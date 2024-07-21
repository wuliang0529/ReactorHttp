#pragma once
#include "Dispatcher.h"
#include "ChannelMap.h"
#include <stdbool.h>
#include <pthread.h>

extern struct Dispatcher EpollDispatcher;   //必须加extern才能使用全局数据
extern struct Dispatcher SelectDispatcher;
extern struct Dispatcher PollDispatcher;

//处理节点中channel的方式
enum ElemType{ADD, DELETE, MODIFY};

//定义任务队列的节点
struct ChannelElement
{
    int type;       //如何处理该节点中的channel----使用枚举
    struct Channel* channel;     
    struct ChannelElement* next;   //指向下一个任务队列节点的指针
};
struct Dispatcher;   //声明--在Dispatcher.h中也使用到了struct EventLoop
struct EventLoop
{
    bool isQuit;   //标识是否在工作
    struct Dispatcher* dispatcher;
    void* dispatcherData;
    //任务队列
    struct ChannelElement* head;
    struct ChannelElement* tail;
    //map---文件描述符和channel的映射关系
    struct ChannelMap* channelMap;
    //线程id，name
    pthread_t threadID;
    char threadName[32];
    pthread_mutex_t mutex;
    int socketPair[2];    //存储本地通信的fd，通过socketpair初始化
};

//初始化----c语言中没有重载，因此需要定义两个初始化函数
struct EventLoop* eventLoopInit();   //初始化主线程---主线程名字固定
struct EventLoop* eventLoopInitEx(const char* threadName);    //初始化子线程，子线程名字需要区分
//启动反应堆模型
int eventLoopRun(struct EventLoop* evLoop);     //启动传入的实例
//处理被激活的文件描述符fd
int eventActivate(struct EventLoop* evLoop, int fd, int event);    //处理模型对象，被激活的文件描述符，读/写事件
//添加任务到任务队列
int eventLoopAddTask(struct EventLoop* evLoop, struct Channel* channel, int type);
//处理任务队列中的任务
int eventLoopProcessTask(struct EventLoop* evLoop);
//处理dispatcher中的节点
int eventLoopAdd(struct EventLoop* evLoop, struct Channel* channel);      //任务队列中的任务节点添加到dispatcher监测集合里边
int eventLoopRemove(struct EventLoop* evLoop, struct Channel* channel);   //把任务节点从dispatcher监测集合中移除
int eventLoopModify(struct EventLoop* evLoop, struct Channel* channel);   //修改任务节点的处理事件
//释放channel（删除channel的后序处理）
int destroyChannel(struct EventLoop* evLoop, struct Channel* channel);