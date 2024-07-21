//不需要有"SelectDispatch.h"文件，因为只要保存函数的地址即可调用，不用把函数声明为全局函数
#include "Dispatcher.h"
#include <sys/select.h>
#include <stdio.h>
// #include <cstddef>
#include <stddef.h>
#include <stdlib.h>    //malloc头文件
#include "Log.h"

#define Max 1024

struct SelectData
{
    fd_set readSet;    // 读集合，对内核的操作小于等于传入的文件描述符（有些未被激活）
    fd_set writeSet;    //写从内核传出的数据
};


static void* selectInit();   
static int selectAdd(struct Channel* channel, struct EventLoop* evLoop);
static int selectRemove(struct Channel* channel, struct EventLoop* evLoop);
static int selectModify(struct Channel* channel, struct EventLoop* evLoop);
static int selectDispatch(struct EventLoop* evLoop, int timeout);
static int selectClear(struct EventLoop* evLoop);
//避免代码冗余，定义额外函数
static void setFdSet(struct Channel* channel, struct SelectData* data);
static void clearFdSet(struct Channel* channel, struct SelectData* data);




struct Dispatcher SelectDispatcher = {    //初始化Dispatcher
    selectInit,
    selectAdd,
    selectRemove,
    selectModify,
    selectDispatch,
    selectClear
};

static void *selectInit()    //初始化PollData
{
    struct SelectData* data = (struct SelectData*)malloc(sizeof(struct SelectData));
    //为data成员赋值---读写集合均赋值为0(使用宏函数)
    FD_ZERO(&data->readSet);
    FD_ZERO(&data->writeSet);
    return data;
}

static void setFdSet(struct Channel* channel, struct SelectData* data)
{
    if(channel->events & ReadEvent) {
        FD_SET(channel->fd, &data->readSet);     //调用FD_SET函数时，会在集合相应位置设置为1
    }
    if(channel->events & WriteEvent) {   //用if是因为有可能读写同时发生哦！！！
        FD_SET(channel->fd, &data->writeSet);
    }
}
static void clearFdSet(struct Channel* channel, struct SelectData* data)
{
    if(channel->events & ReadEvent) {
        FD_CLR(channel->fd, &data->readSet);     //调用FD_CLR函数时，会在集合相应位置设置为1
    }
    if(channel->events & WriteEvent) {   //用if是因为有可能读写同时发生哦！！！
        FD_CLR(channel->fd, &data->writeSet);
    }
}

static int selectAdd(struct Channel *channel, struct EventLoop *evLoop)
{
    struct SelectData* data = (struct SelectData*)evLoop->dispatcherData;    
    if(channel->fd >= Max) {   //如果fd大于最大的可操作位置，返回-1
        return -1;
    }
    setFdSet(channel, data);
    return 0;
}

static int selectRemove(struct Channel *channel, struct EventLoop *evLoop)
{
    struct SelectData* data = (struct SelectData*)evLoop->dispatcherData;
    clearFdSet(channel, data);
    
    //删除了文件描述符之后即可释放channel所在的TcpConnection模块的内存
    //思路---在channel结构体中定义的函数指针指向TcpConnection对应的销毁函数的地址即可（销毁类型的回调函数）
    channel->destoryCallBack(channel->arg);
    return 0;
}

static int selectModify(struct Channel *channel, struct EventLoop *evLoop)
{
    struct SelectData* data = (struct SelectData*)evLoop->dispatcherData;
    setFdSet(channel, data);
    clearFdSet(channel, data);
    return 0;
}

static int selectDispatch(struct EventLoop *evLoop, int timeout)
{
    // 把data数据块取出来，需要进行类型转换---veLoop中使用的是泛型指针void*
    struct SelectData* data = (struct SelectData*)evLoop->dispatcherData;
    struct timeval val;
    val.tv_sec = timeout;   //ms
    val.tv_usec = 0;     //s，必须初始化为0，因为使用的时候是让结构体里的两个时间单位相加，不初始化会产生一个随机数相加
    fd_set rdtmp = data->readSet;
    fd_set wrtmp = data->writeSet;   //因为select函数的第二、三、四参数为传入传出参数，会修改原始数据，所以创建临时集合
    int count = select(Max, &rdtmp, &wrtmp, NULL, &val);   //设置NULL的位置为异常接收处理，这里不需要 
    if(count == -1) {
        perror("select");
        exit(0);
    }
    for(int i=0; i<Max; ++i) {   //i代表读写集合的第i个标志位
        // Debug("i: %d", i);
        if(FD_ISSET(i, &rdtmp)) {    //读集合的第i位有效
            eventActivate(evLoop, i, ReadEvent);
        }
        if(FD_ISSET(i, &wrtmp)) {    //写集合的第i位有效
            eventActivate(evLoop, i, WriteEvent);
        }
    }
    return 0;
}

static int selectClear(struct EventLoop *evLoop)
{
    struct SelectData* data = (struct SelectData*)evLoop->dispatcherData;
    free(data);
    return 0;
}
