#pragma once
#include "EventLoop.h"
#include "WorkerThread.h"
#include <stdbool.h>

//定义线程池
struct ThreadPool
{
    //主线程的反应堆模型--线程池中子线程数量为0时调用这个模型
    struct EventLoop* mainLoop;
    bool isStart;
    int threadNum;
    struct WorkerThread* workerThreads;   //子线程数组指针
    int index;
};

//初始化
struct ThreadPool* threadPoolInit(struct EventLoop* mainLoop, int count);
//启动线程池
void threadPoolRun(struct ThreadPool* pool);
//从线程池中取出某个线程的反应堆实例（存储对任务的一系列处理操作）---即选择处理任务的子线程，把任务队列中的任务放置到所选子线程的反应堆模型中
struct EventLoop* takeWorkerEventLoop(struct ThreadPool* pool);
