#pragma once     //无论这个头文件被其他源文件引用多少次, 编译器只会对这个文件编译一次。
#include "EventLoop.h"
#include "Channel.h"
#include "Buffer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"

struct TcpConnection
{
    struct EventLoop* evLoop;
    struct Channel* channel;
    struct Buffer* readBuf;
    struct Buffer* writeBuf;
    char name[32];
    //http协议
    struct HttpRequest* request;
    struct HttpResponse* response;
};

//初始化
struct TcpConnection* tcpConnectionInit(int fd, struct EventLoop* evLoop);
//资源释放---在TcpConnection中释放出EventLoop之外的其它资源，因为EventLoop并不属于TcpConnetcion，而是属于子线程
int tcpConnectionDestory(void* arg);