#pragma once

#include "EventLoop.h"
#include <pthread.h>

struct WorkerThread
{
    pthread_t threadID;
    char name[24];
    pthread_mutex_t mutex;    //互斥锁，实现线程同步
    pthread_cond_t cond;   //条件变量，阻塞线程
    struct EventLoop* evLoop;   //反应堆模型--每个线程都有一个反应堆模型
};

//线程初始化
int workerThreadInit(struct WorkerThread* thread, int index);     //index记录是线程池中的第几个线程
//启动线程
void workerThreadRun(struct WorkerThread* thread);