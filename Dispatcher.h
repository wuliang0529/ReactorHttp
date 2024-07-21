#pragma once
#include "Channel.h"
#include "EventLoop.h"

struct EventLoop;   //声明--在EventLoop.h中也使用到了struct Dispatcher

struct Dispatcher
{
    // 初始化---初始化poll、epoll或者select需要的数据块DispatherData
    void* (*init) ();     //返回值是泛型是为了兼容poll的 poll_fd，epoll的epoll_event , select的 fd_set(使用的是时候转化一下)  
    // 添加---待检测的文件描述符添加到poll、epoll或者是select中
    int (*add) (struct Channel* channel, struct EventLoop* evLoop);
    // 修改
    int (*remove) (struct Channel* channel, struct EventLoop* evLoop);
    // 删除
    int (*modify) (struct Channel* channel, struct EventLoop* evLoop);
    // 事件监测，监测三种模型中是否有文件描述符被激活
    int (*dispatch) (struct EventLoop* evLoop, int timeout);   //超时阈值timeout单位：s
    // 清除数据（关闭fd或者释放内存）
    int (*clear) (struct EventLoop* evLoop);
};
