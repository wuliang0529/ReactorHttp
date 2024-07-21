#include "WorkerThread.h"
#include <stdio.h>      //sprintf对应的头文件
#include <pthread.h>    //pthread_create头文件
#include "Log.h"

int workerThreadInit(struct WorkerThread *thread, int index)
{
    thread->threadID = 0;
    sprintf(thread->name, "SubThread-%d", index);
    pthread_mutex_init(&thread->mutex, NULL);   //指定为NULL是使用默认属性
    pthread_cond_init(&thread->cond, NULL);
    thread->evLoop = NULL;
    return 0;
}

//子线程的回调函数
void* subThreadRunning(void* arg) {     //注意：这是子线程执行的函数
    struct WorkerThread* thread = (struct WorkerThread*)arg;
    pthread_mutex_lock(&thread->mutex);
    thread->evLoop = eventLoopInitEx(thread->name);
    pthread_mutex_unlock(&thread->mutex);
    pthread_cond_signal(&thread->cond);    //子线程创建完毕，唤醒主线程---让主线程获得锁，并继续执行
    eventLoopRun(thread->evLoop);
    return NULL;
}

void workerThreadRun(struct WorkerThread *thread)    //注意：这是主线程执行的函数
{
    //创建子线程
    pthread_create(&thread->threadID, NULL, subThreadRunning, thread);
    //检查子线程是否创建完毕
    pthread_mutex_lock(&thread->mutex);   //主线程和子线程访问共享资源，需要加锁
    while (thread->evLoop == NULL)   //循环检查子线程的反应堆模型是否创建完毕
    {
        pthread_cond_wait(&thread->cond, &thread->mutex);    //阻塞主线程并释放thread_mutex锁，以便让子线程获取并执行
    }
    // Debug("子线程创建完毕。。。。。。。。。。。");-------可以到这
    pthread_mutex_unlock(&thread->mutex);
}
