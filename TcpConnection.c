#include "TcpConnection.h"
#include "HttpRequest.h"
// #include <strings.h>
#include <stdlib.h>    //free、malloc头文件
#include <stdio.h>    //sprintf头文件
#include "Log.h"     //日志输出

int processRead(void* arg) {
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    //接收数据
    int count = bufferSocketRead(conn->readBuf, conn->channel->fd);    //count为接收到的字节数

    //日志打印
    Debug("接收到的http请求数据：%s", conn->readBuf->data + conn->readBuf->readPos);

    if(count > 0) {
        //接收到了http请求      
        //解析http请求
        int socket = conn->channel->fd;
#ifdef MSG_SEND_AUTO   
        //修改文件描述符的读事件为读写事件---需要等线程执行完毕读回调后才能处理执行回调（弊端：需要等到所有的数据写入到writebuff后才能处理写回调，那如果文件太大写不下岂不是凉凉）
        writeEventEnable(conn->channel, true);
        eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
#endif  
        bool flag = parseHttpRequest(conn->request, conn->readBuf, conn->response, conn->writeBuf, socket);
        Debug("解析http请求成功............");
        if(!flag) {
            //解析失败，回复一个简单的html页面
            char* errMsg = "Http/1.1 400 Bad Request\r\n\r\n";
            bufferAppendString(conn->writeBuf, errMsg);       //只需要写入sendBuf（这里是writeBuf）即可，不需要发送
        }
    }
    else {
        //接收字节数count == 0时;
#ifdef MSG_SEND_AUTO       
        eventLoopAddTask(conn->evLoop, conn->channel, DELETE);  //接收到的数据是空数据，则断开连接
#endif
    }
    //断开连接的三种情况：1.接收字节数count<0;  2.解析失败发送html页面后   3. flag=true时，存储完成并发送Buf通讯结束
    //断开连接--涉及内存的释放  
#ifndef MSG_SEND_AUTO       
    eventLoopAddTask(conn->evLoop, conn->channel, DELETE);  //如果是第一种发送方式，则不能调用该语句（因为将数据写入writeBuf后会直接断开连接，并未发送）    
#endif
    return 0;
}

int processWrite(void* arg) {
    //日志打印
    Debug("开始发送数据了（基于写事件发送）....");

    struct TcpConnection* conn = (struct TcpConnection*)arg;
    //发送数据
    int count = bufferSendData(conn->writeBuf, conn->channel->fd);
    if(count > 0) {
        //判断数据是否被全部发送出去
        if(bufferReadableSize(conn->writeBuf) == 0) {
            //1. 不再检测写事件---修改channel保存的事件
            writeEventEnable(conn->channel, false);
            //2. 修改dispatcher检测集合---添加任务节点
            eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);   //从读写，修改为只读
            //3. 删除这个节点----前两步可写可不写，直接删除节点就行
            eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
        }
    }
}

struct TcpConnection* tcpConnectionInit(int fd, struct EventLoop *evLoop)
{
    struct TcpConnection* conn = (struct TcpConnection*)malloc(sizeof(struct TcpConnection));
    conn->evLoop = evLoop;
    conn->readBuf = bufferInit(10240);
    conn->writeBuf = bufferInit(10240);
    //http协议初始化
    conn->request = httpRequestInit();
    conn->response = httpResponseInit();
    sprintf(conn->name, "Connection-%d", fd);
    //对fd封装成channel，processRead是fd对应的读回调函数
    conn->channel = channelInit(fd, ReadEvent, processRead, processWrite, tcpConnectionDestory, conn);   //processRead是接收客户端的http请求
    //把channel添加到子线程的反应堆模型evLoop，子线层反应堆实例evLoop检测到任务队列中有任务(conn->channel)后，就把任务conn->channel添加的待监测集合
    Debug("Tcp连接初始化完成............");
    eventLoopAddTask(evLoop, conn->channel, ADD);
    Debug("和客户端建立连接，threadName: %s, threadID: %s, connName: %s", 
            evLoop->threadName, evLoop->threadID, conn->name);
    return conn;
}

int tcpConnectionDestory(void* arg)
{
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    if(conn != NULL) {
        if(conn->readBuf && bufferReadableSize(conn->readBuf) == 0 &&
            conn->writeBuf && bufferReadableSize(conn->writeBuf) == 0)
        {
            destroyChannel(conn->evLoop, conn->channel);
            bufferDestory(conn->readBuf);
            bufferDestory(conn->writeBuf);
            httpRequestDestory(conn->request);
            httpResponseDestory(conn->response);
            free(conn);     //不能写到if外边，因为如果buffer有数据则不能释放内存
        }    
    }
    //日志打印
    Debug("连接断开，释放资源，connName: %s", conn->name);
    return 0;
}
