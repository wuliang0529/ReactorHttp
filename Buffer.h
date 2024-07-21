#pragma once

//需要在TcpConnection.c   HttpRequest.c 和 HttpResponse.c多个文件中使用这个宏---所以选择定义在这
//控制服务器向客户端发送数据的方式：1.全部写入sendBuf再发送给客户端   2. 写一部分发送一部分
// #define MSG_SEND_AUTO     //如果MSG_SEND_AUTO有效则为第一种发送方式    


//定义Buffer结构体
struct Buffer
{
    char* data;
    int capacity;
    int readPos;
    int writePos;
};

//初始化结构体
struct Buffer* bufferInit(int size);
//销毁模块儿buffer内存（Buffer存储了好多data块儿）
void bufferDestory(struct Buffer* buf);
//扩容buffer
void bufferExtendRoom(struct Buffer* buffer, int size);    //size表示需要写进内存的数据大小
//得到剩余的可写的内存容量（未写---不包含已读过的内存哦）
int bufferWriteableSize(struct Buffer* buffer);
//得到剩余的可读的内存容量（未读）
int bufferReadableSize(struct Buffer* buffer);
//把数据写入内存：1.直接写  2.接收套接字数据（也需要写入buffer）
int bufferAppendData(struct Buffer* buffer, const char* data, int size);   //适用于所有的data数据（不一定是string）
int bufferAppendString(struct Buffer* buffer, const char* data);   //适用于data为string类型的数据
int bufferSocketRead(struct Buffer* buffer, int fd);    //接收套接字数据并写入buffer
//根据\r\n取出一行，找到其在数据块中的位置，返回该位置
char* bufferFindCRLF(struct Buffer* buffer);
//发送数据
int bufferSendData(struct Buffer* buffer, int socket);