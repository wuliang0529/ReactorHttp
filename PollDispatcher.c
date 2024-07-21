//不需要有"PollDispatch.h"文件，因为只要保存函数的地址即可调用，不用把函数声明为全局函数
#include "Dispatcher.h"
#include <poll.h>
#include <stdlib.h>   //malloc头文件
#include <stdio.h>    //perror头文件

#define Max 1024

struct PollData
{
    int maxfd;    // 记录fds的最大字符位置
    struct pollfd fds[Max];    //每个fds[i]都是一个结构体，包括fd, events, revents
};


static void* pollInit();   
static int pollAdd(struct Channel* channel, struct EventLoop* evLoop);
static int pollRemove(struct Channel* channel, struct EventLoop* evLoop);
static int pollModify(struct Channel* channel, struct EventLoop* evLoop);
static int pollDispatch(struct EventLoop* evLoop, int timeout);
static int pollClear(struct EventLoop* evLoop);

struct Dispatcher PollDispatcher = {    //初始化Dispatcher
    pollInit,
    pollAdd,
    pollRemove,
    pollModify,
    pollDispatch,
    pollClear
};

static void *pollInit()    //初始化PollData
{
    struct PollData* data = (struct PollData*)malloc(sizeof(struct PollData));
    //为data成员赋值
    data->maxfd = 0;
    for(int i=0; i<Max; ++i) {
        data->fds[i].fd = -1;   //有效的文件描述符从0开始
        data->fds[i].events = 0;   //无事件(可以包括读写)
        data->fds[i].revents = 0;   //无触发时间（如果只触发读，则这里只是读）
    }
    return data;
}


static int pollAdd(struct Channel *channel, struct EventLoop *evLoop)
{
    struct PollData* data = (struct PollData*)evLoop->dispatcherData;
    int events = 0;   //表示内核的读、写
    if(channel->events & ReadEvent) {
        events |= POLLIN;
    }
    if(channel->events & WriteEvent) {   //用if是因为有可能读写同时发生哦！！！
        events |= POLLOUT;
    }
    //找空闲位置把 events 及对应的 fd 存储到data（PollData对象）中
    int i=0;
    for(; i<Max; ++i) {
        if(data->fds[i].fd == -1) {
            data->fds[i].fd = channel->fd;
            data->fds[i].events = events;
            data->maxfd = i > data->maxfd ? i : data->maxfd;
            break;
        }
    } 
    if(i >= Max) {   //证明超出了内存管理范围
        return -1;
    }
    return 0;
}

static int pollRemove(struct Channel *channel, struct EventLoop *evLoop)
{
    struct PollData* data = (struct PollData*)evLoop->dispatcherData;
    //找到 events 及对应的 fd ，在data（PollData对象）中删除
    int i=0;
    for(; i<Max; ++i) {
        if(data->fds[i].fd == channel->fd) {
            data->fds[i].fd = -1;
            data->fds[i].events = 0;
            data->fds[i].revents = 0;
            // data->maxfd = i == data->maxfd ? i-1 : data->maxfd;    //更新data中最大的数据位置，不适用所有情况，可不写
            break;
        }
    } 
    //删除了文件描述符之后即可释放channel所在的TcpConnection模块的内存
    //思路---在channel结构体中定义的函数指针指向TcpConnection对应的销毁函数的地址即可（销毁类型的回调函数）
    channel->destoryCallBack(channel->arg);

    if(i >= Max) {   //证明超出了内存管理范围
        return -1;
    }
    return 0;
}

static int pollModify(struct Channel *channel, struct EventLoop *evLoop)
{
    struct PollData* data = (struct PollData*)evLoop->dispatcherData;
    int events = 0;   //表示内核的读、写
    if(channel->events & ReadEvent) {   //判断用户是否需要读事件
        events |= POLLIN;
    }
    if(channel->events & WriteEvent) {   //判断用户是否需要写事件，用if是因为有可能读写同时发生哦！！！
        events |= POLLOUT;
    }
    //找空闲位置把 events 及对应的 fd 存储到data（PollData对象）中
    int i=0;
    for(; i<Max; ++i) {
        if(data->fds[i].fd == channel->fd) {
            data->fds[i].events = events;   //只需要修改读写时间即可
            break;
        }
    } 
    if(i >= Max) {   //证明超出了内存管理范围
        return -1;
    }
    return 0;
}

static int pollDispatch(struct EventLoop *evLoop, int timeout)
{
    // 把data数据块取出来，需要进行类型转换---veLoop中使用的是泛型指针void*
    struct PollData* data = (struct PollData*)evLoop->dispatcherData;
    int count = poll(data->fds, data->maxfd+1, timeout * 1000);   //timeout自定义为s，但epoll_wait参数为ms
    if(count == -1) {
        perror("poll");
        exit(0);
    }
    for(int i=0; i<=data->maxfd; ++i) {

        if(data->fds[i].fd == -1) {    //未被激活的文件描述符
            continue;
        }
        if(data->fds[i].revents & POLLIN) {    //读事件
            //调用被激活的文件描述符对应的“读回调函数”---在EventLoop中的channel中存储
            eventActivate(evLoop, data->fds[i].fd, ReadEvent);
        }
        if(data->fds[i].revents & POLLOUT) {   //写事件
            //调用被激活的文件描述符对应的“写回调函数”
            eventActivate(evLoop, data->fds[i].fd, WriteEvent);
        }   
    }
    return 0;
}

static int pollClear(struct EventLoop *evLoop)
{
    struct PollData* data = (struct PollData*)evLoop->dispatcherData;
    free(data);
    return 0;
}
