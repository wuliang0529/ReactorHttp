#include "Channel.h"
#include <stdlib.h>   //malloc头文件


struct Channel *channelInit(int fd, int events, handleFunc readFunc, handleFunc writeFunc, handleFunc destoryFunc, void *arg)
{
    struct Channel* channel = (struct Channel*)malloc(sizeof(struct Channel));
    channel->fd = fd;
    channel->events = events;
    channel->readCallBack = readFunc;
    channel->writeCallBack = writeFunc;
    channel->destoryCallBack = destoryFunc;
    channel->arg = arg;
    return channel;
}

void writeEventEnable(struct Channel *channel, bool flag)
{
    if(flag) {   //检测写事件
        channel->events |= WriteEvent;
    }
    else {       //不检测写事件
         //把第三位清零，原始为000100，取反后变成111011，第三位0与任何数相与都为0
        channel->events = channel->events & ~WriteEvent;  
    }
}

bool isWriteEventEnable(struct Channel *channel)
{
    //writeEventEnable中已经设置了是否检测写事件
    return channel->events & WriteEvent;    //返回值大于0即存在写事件，返回值为0则不存在写事件
}
