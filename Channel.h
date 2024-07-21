#pragma once
#include<stdbool.h>

//定义函数指针---即事件对应的回调函数
typedef int (*handleFunc) (void* arg);

//定义文件描述符的读写事件标志
enum FDEvent{
    TimeOut = 0x01,         //读写超时
    ReadEvent = 0x02,       //读事件
    WriteEvent = 0x04       //写事件
};

//通道结构体
struct Channel
{
    //文件描述符----监听(只用一个)、连接
    int fd;
    //文件描述符对应的事件---读、写、读写
    int events;
    //回调函数
    handleFunc readCallBack;   //读回调
    handleFunc writeCallBack;  //写回调
    handleFunc destoryCallBack;  //销毁回调
    //回调函数的参数
    void* arg;
};

//初始化一个Channel
struct Channel* channelInit(int fd, int events, handleFunc readFunc, handleFunc writeFunc, handleFunc destoryFunc, void* arg);
//修改fd的写事件（检测 or 不检测）
void writeEventEnable(struct Channel* channel, bool flag);    //flag用于判断是否设置channel的写事件
//判断是否需要检测文件描述符的写事件（读事件一定需要，任何操作都需要读事件）
bool isWriteEventEnable(struct Channel* channel); 