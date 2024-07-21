//不需要有"EpollDispatch.h"文件，因为只要保存函数的地址即可调用，不用把函数声明为全局函数
#include "Dispatcher.h"
#include <sys/epoll.h>
#include <unistd.h>   //close头文件
#include <stdlib.h>   //free、exit头文件
#include <stdio.h>    //perror头文件
 
#define Max 520

struct EpollData
{
    int epfd;   //根节点
    struct epoll_event* events;
};


static void* epollInit();   
static int epollAdd(struct Channel* channel, struct EventLoop* evLoop);
static int epollRemove(struct Channel* channel, struct EventLoop* evLoop);
static int epollModify(struct Channel* channel, struct EventLoop* evLoop);
static int epollDispatch(struct EventLoop* evLoop, int timeout);
static int epollClear(struct EventLoop* evLoop);
//由于epoll_ADD  epoll_Remove  epoll_Modify实现基本相同，故自定义epoll_ctll减少代码冗余
static int epollCtl(struct Channel* channel, struct EventLoop* evLoop, int op);  


struct Dispatcher EpollDispatcher = {    //初始化Dispatcher
    epollInit,
    epollAdd,
    epollRemove,
    epollModify,
    epollDispatch,
    epollClear
};


static void *epollInit()    //初始化EpollData
{
    struct EpollData* data = (struct EpollData*)malloc(sizeof(struct EpollData));
    //为data成员赋值
    data->epfd = epoll_create(10);   //大于0的数即可，无实际意义
    if(data->epfd == -1) {
        perror("epoll_create");
        exit(0);
    }
    data->events = (struct epoll_event*)calloc(Max, sizeof(struct epoll_event));
    return data;
}

static int epollCtl(struct Channel* channel, struct EventLoop* evLoop, int op) {
    //把data数据块取出来，需要进行类型转换---veLoop中使用的是泛型指针void*
    struct EpollData* data = (struct EpollData*)evLoop->dispatcherData;

    struct epoll_event ev;   //epoll_event结构体并初始化，用于epoll_ctll的最后一个参数传递
    ev.data.fd = channel->fd;
    int events = 0;   //表示内核的读、写
    if(channel->events & ReadEvent) {
        events |= EPOLLIN;
    }
    if(channel->events & WriteEvent) {   //用if是因为有可能读写同时发生哦！！！
        events |= EPOLLOUT;
    }
    ev.events = events;   //不能直接赋值为channel->events，因为channel的events是自己定义的，epoll_event的events是linux内核定义
    //向文件描述符epfd引用的epoll实例执行控制操作(op：添加、修改或者删除)
    int ret = epoll_ctl(data->epfd, op, channel->fd, &ev);
    return ret;
}

static int epollAdd(struct Channel *channel, struct EventLoop *evLoop)
{
    int ret = epollCtl(channel, evLoop, EPOLL_CTL_ADD);
    if(ret == -1) {
        perror("epoll_ctl add");
        exit(0);
    }
    return ret;
}

static int epollRemove(struct Channel *channel, struct EventLoop *evLoop)
{
    int ret = epollCtl(channel, evLoop, EPOLL_CTL_DEL);
    if(ret == -1) {
        perror("epoll_ctl delete");
        exit(0);
    }
    //删除了文件描述符之后即可释放channel所在的TcpConnection模块的内存
    //思路---在channel结构体中定义的函数指针指向TcpConnection对应的销毁函数的地址即可（销毁类型的回调函数）
    channel->destoryCallBack(channel->arg);
    return ret;
}

static int epollModify(struct Channel *channel, struct EventLoop *evLoop)
{
    int ret = epollCtl(channel, evLoop, EPOLL_CTL_MOD);
    if(ret == -1) {
        perror("epoll_ctl modify");
        exit(0);
    }
    return ret;
}

static int epollDispatch(struct EventLoop *evLoop, int timeout)
{
    // 把data数据块取出来，需要进行类型转换---veLoop中使用的是泛型指针void*
    struct EpollData* data = (struct EpollData*)evLoop->dispatcherData;
    int count = epoll_wait(data->epfd, data->events, Max, timeout * 1000);   //timeout自定义为s，但epoll_wait参数为ms
    for(int i=0; i<count; ++i) {
        int events = data->events[i].events;   // (struct epoll_event*) events 是结构体类型
        int fd = data->events[i].data.fd;     //data也是个结构体
        if(events & EPOLLERR || events & EPOLLHUP) {   //客户端断开连接  或  客户端已经断开连接，但服务器仍在发送数据
            //删除fd
            // epollRemove(Channel, evLoop);
            continue;
        }
        if(events & EPOLLIN) {    //读事件
            eventActivate(evLoop, fd, ReadEvent);
        }
        if(events & EPOLLOUT) {   //写事件
            eventActivate(evLoop, fd, WriteEvent);
        }    
    }
    return 0;
}

static int epollClear(struct EventLoop *evLoop)
{
    struct EpollData* data = (struct EpollData*)evLoop->dispatcherData;
    free(data->events);
    close(data->epfd);
    free(data);
    return 0;
}
