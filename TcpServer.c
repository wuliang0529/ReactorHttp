#include "TcpServer.h"
#include <arpa/inet.h>    //socket头文件
#include "TcpConnection.h"
#include <stdio.h>        //perror头文件
#include <stdlib.h>       //malloc头文件
#include "Log.h"     //日志输出
 
struct TcpServer *tcpServerInit(unsigned short port, int threadNum)
{
    struct TcpServer* tcp = (struct TcpServer*)malloc(sizeof(struct TcpServer));
    tcp->listener =  listenerInit(port);
    tcp->mainLoop = eventLoopInit();
    tcp->threadNum = threadNum;
    tcp->threadPool = threadPoolInit(tcp->mainLoop, threadNum);        //注意***************视频中用的threadNum
    return tcp;
}

struct Listener *listenerInit(unsigned short port)
{
    struct Listener* listener = (struct Listener*)malloc(sizeof(struct Listener));
    //1. 创建监听的fd
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if(lfd == -1) {
        perror("socket");
        return NULL;
    }
    //2. 设置端口复用
    int opt = 1;
    int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    if(ret == -1) {
        perror("setsockopt");
        return NULL;
    }
    //3. 绑定
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;    //绑定本地ip地址：设置为零地址的宏（INADDR_ANY），表示可以绑定本机的任意IP地址
    ret = bind(lfd, (struct sockaddr*)&addr, sizeof addr);
    if(ret == -1){
        perror("bind");
        return NULL;
    }
    //4. 设置监听
    ret = listen(lfd, 128);
    if(ret == -1) {
        perror("listern");
        return NULL;
    }
    //初始化listener
    listener->lfd = lfd;
    listener->port = port;
    return listener;
}

//定义连接函数
int acceptConnection(void* arg) {
    struct TcpServer* server = (struct TcpServer*)arg;
    Debug("到acceptConnection这了");
    //和客户端建立连接
    int cfd = accept(server->listener->lfd, NULL, NULL);
    //从线程池中取出一个子线程的反应堆实例，去处理cfd
    Debug("和客户端建立连接成功............");
    struct EventLoop* evLoop = takeWorkerEventLoop(server->threadPool);   //需要把这个封装到tcpConntion
    //将cfd放到TcpConnection中处理
    tcpConnectionInit(cfd, evLoop);     //函数里面实现了对cfd的封装
    return 0;
}

void tcpServerRun(struct TcpServer *server)
{
    Debug("服务器的程序已经启动了...");
    //启动线程池---启动服务器之前先启动线程池
    threadPoolRun(server->threadPool);
    //处理监听文件描述符fd（只有这一个文件描述符需要执行）,添加检测的任务
    //初始化一个channel
    struct Channel* channel = channelInit(server->listener->lfd, ReadEvent, acceptConnection, NULL, NULL, server);
    eventLoopAddTask(server->mainLoop, channel, ADD);
    //启动反应堆模型
    eventLoopRun(server->mainLoop);
}
