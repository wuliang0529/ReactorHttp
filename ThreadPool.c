#include "ThreadPool.h"
#include <assert.h>    //断言函数assert的头文件
#include <stdlib.h>    //malloc的头文件
#include <pthread.h>   //thread_self头文件
#include "Log.h"

//该函数必须被主线程执行
struct ThreadPool *threadPoolInit(struct EventLoop *mainLoop, int count)
{
    struct ThreadPool* pool = (struct ThreadPool*)malloc(sizeof(struct ThreadPool));
    pool->index = 0;
    pool->isStart = false;
    pool->mainLoop = mainLoop;
    pool->threadNum = count;
    pool->workerThreads = (struct WorkerThread*)malloc(sizeof(struct WorkerThread) * count);
    return pool;
}

//该函数必须被主线程执行
void threadPoolRun(struct ThreadPool *pool)
{
    assert(pool && !pool->isStart);   //断言判断是否存在线程池或者线程池是否启动（不存在或者已经启动则直接返回）
    if(pool->mainLoop->threadID != pthread_self()) {   //指向threadPoolRun函数的线程不是主线程
        exit(0);
    }
    pool->isStart = true;
    if(pool->threadNum) {   //判断子线程数量是否大于0
        for(int i=0; i<pool->threadNum; ++i) {
            workerThreadInit(&pool->workerThreads[i], i);    //创建子线程   
            workerThreadRun(&pool->workerThreads[i]);        //启动子线程
        }
    }
}

//该函数必须被主线程执行
struct EventLoop *takeWorkerEventLoop(struct ThreadPool *pool) 
{
    assert(pool->isStart);   //断言判断线程池是否启动
    if(pool->mainLoop->threadID != pthread_self()) {      //如果不是主线程
        exit(0);
    }
    //从线程池中找一个子线程，然后取出里边的反应堆实例
    Debug("主线程%d，要开始从线程池取出子线程了............", pool->mainLoop->threadID);
    struct EventLoop* evLoop = pool->mainLoop;    //初始化为主线程的反应堆实例是因为，如果没有子线程，则任务有主线程执行，这种情况为单反应堆模型
    if(pool->threadNum > 0) {
        evLoop = pool->workerThreads[pool->index].evLoop;
        pool->index = ++pool->index % pool->threadNum;     //更新pool->index，保证任务分配雨露均沾
    }
    return evLoop;
}
