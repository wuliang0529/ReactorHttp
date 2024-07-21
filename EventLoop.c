#include "EventLoop.h"
#include <stdlib.h>   //free、malloc、exit头文件
#include <string.h>   //strcpy头文件
#include <sys/socket.h>
#include <unistd.h>   //close、write头文件
#include <stdio.h>    //perror头文件
#include <assert.h>   //assert头文件
#include "Log.h"
 
struct EventLoop *eventLoopInit()
{
    return eventLoopInitEx(NULL);
}

//写数据(向evLoop->socketPair[0]中写，发送到evLoop->socketPair[1]，从而激活evLoop->socketPair[1])
void taskWakeup(struct EventLoop* evLoop) {
    const char* msg = "我的目的就是唤醒evLoop->socketPair[1]";
    write(evLoop->socketPair[0], msg, strlen(msg));
}
//读数据
int readLocalMessage(void* arg) {
    struct EventLoop* evLoop = (struct EventLoop*)arg;
    char buf[256];    //接收数据
    read(evLoop->socketPair[1], buf, sizeof(buf));
    return 0;
}

struct EventLoop *eventLoopInitEx(const char *threadName)
{
    struct EventLoop* evLoop = (struct EventLoop*)malloc(sizeof(struct EventLoop));
    evLoop->isQuit = false;
    evLoop->threadID = pthread_self();   //获取线程id
    pthread_mutex_init(&evLoop->mutex, NULL);   //初始化锁---保护任务队列，多线程对任务队列的操作
    strcpy(evLoop->threadName, threadName == NULL ? "MainThread" : threadName);    //给线程初始化名字，因为threadName是数组，所以不能使用等号赋值
    
    //初始化dispatcher和dispatcherData----三个dispatcher取其一
    // evLoop->dispatcher = &EpollDispatcher;    //为结构体dispatcher初始化一个实例
    // evLoop->dispatcher = &PollDispatcher;    
    evLoop->dispatcher = &SelectDispatcher;    

    evLoop->dispatcherData = evLoop->dispatcher->init();   //init()返回的是一个void*类型的函数指针，根据dispatcher的实例调用对应的初始化函数
    //初始化链表
    evLoop->head = evLoop->tail = NULL;
    //初始化map
    evLoop->channelMap = ChannelMapInit(128);
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, evLoop->socketPair);
    if(ret == -1) {
        perror("socketpair");
        exit(0);    //退出整个进程
    }
    // 指定规则: evLoop->socketPair[0]发送数据， evLoop->socketPair[1]接收数据
    //创建evLoop->socketPair[1]对应的channel节点并添加到任务队列
    struct Channel* channel = channelInit(evLoop->socketPair[1], ReadEvent, readLocalMessage, NULL, NULL, evLoop);   //evLoop是传入readLocalMessage的参数
    // Debug("到这了..............");
    eventLoopAddTask(evLoop, channel, ADD);
    return evLoop;
}

/**关于exit
 * （1）exit和return的区别
 * 如果main()在一个递归程序中，exit()仍然会终止程序；但return将控制权移交给递归的前一级，直到最初的那一级，此时return才会终止程序。
 * return和exit()的另一个区别在于，即使在除main()之外的函数中调用exit()，它也将终止程序。 
 * （2）exit() 里面的参数，是传递给其父进程的。
 * （3）在 main() 函数里面 return 会导致程序永远不退出。exit(3) 是C标准库函数，也是最常用的进程退出函数。
 */

int eventLoopRun(struct EventLoop *evLoop)
{
    assert(evLoop != NULL);    //断言判断实例指针是否为空
    //取出事件分发和检测模型
    struct Dispatcher* dispatcher = evLoop->dispatcher;
    //比较线程ID是否正常
    if(evLoop->threadID != pthread_self()) {
        perror("thread_id");
        return -1;
    }
    Debug("反应堆模型启动............");
    //循环处理事件
    while(!evLoop->isQuit) {
        //监测的是evLoop->dispatcher的实例化对象(上边初始化的是EpollDispatcher)
        dispatcher->dispatch(evLoop, 2);   //超时时长 2s
        eventLoopProcessTask(evLoop);    //子线程解除阻塞后执行任务队列的处理函数（只有子线程可以处理任务队列中的任务）
    }
    return 0;
}

int eventActivate(struct EventLoop *evLoop, int fd, int event)
{
    // Debug("能不能到这嘞。。。。。。。。。");--------可以到这
    if(fd < 0 || evLoop == NULL) {   //判断无效数据
        return -1;
    }
    struct Channel* channel = evLoop->channelMap->list[fd];
    assert(channel->fd == fd);   //断言判断传入的fd与取出的channel中的fd是否相同，不相同则异常退出
    // Debug("event & ReadEvent: %d", (event & ReadEvent));
    Debug("event & WriteEvent: %d", (event & WriteEvent));
    if((event & ReadEvent) && channel->readCallBack) {
        channel->readCallBack(channel->arg);
    }
    if((event & WriteEvent) && channel->writeCallBack) {
        channel->writeCallBack(channel->arg);
    }
    return 0;
}

int eventLoopAddTask(struct EventLoop *evLoop, struct Channel *channel, int type)
{
    pthread_mutex_lock(&evLoop->mutex);      //加锁
    //创建节点并初始化---不管type是什么操作，都得先添加到任务队列
    struct ChannelElement* node = (struct ChannelElement*)malloc(sizeof(struct ChannelElement));
    node->channel = channel;
    node->next = NULL;
    node->type = type;
    //添加节点到任务队列
    if(evLoop->head == NULL) {
        evLoop->head = evLoop->tail = node;
    }
    else{
        evLoop->tail->next = node;   //添加
        evLoop->tail = node;   //后移tail指针
    }
    // Debug("任务添加成功.............., node-type: %d", node->type);
    pthread_mutex_unlock(&evLoop->mutex);       //解锁

    //处理节点
    /**
     * 处理的细节分析
     * 1. 对于链表节点的添加：可能是当前的线程也可能是其他线程（主线程）
     *     1). 修改fd的事件，当前子线程发起，当前子线程处理
     *     2). 添加新的fd，添加任务节点的操作是由主线程发起的
     * 2. 不能让主线程处理任务队列，需要由当前的子线程去处理
     */
    if(evLoop->threadID == pthread_self()) {
        //当前线程ID等于evLoop对应的子线程ID---直接处理任务队列中的任务
        eventLoopProcessTask(evLoop);
        Debug("子线程处理任务....");
    }
    else{
        //主线程---需要告诉子线程去处理任务队列中的任务
        //子线程的两种状态：1.在工作  2. 在阻塞（此时需要自定义一个fd（在EventLoop中），通过该fd发送数据给当前节点的文件描述符（激活）去唤醒子线程）
        Debug("主线程告诉子线程去处理任务....");
        taskWakeup(evLoop);   //如果线程被阻塞，主线程调用此函数即可唤醒子线程，子线程是在dispacth函数（eventLoopRun函数那里）那结束阻塞
    }
    return 0;
}

int eventLoopProcessTask(struct EventLoop *evLoop)
{
    pthread_mutex_lock(&evLoop->mutex);      //加锁
    //取出头节点
    struct ChannelElement* head = evLoop->head;
    while(head != NULL) {    //遍历任务队列
        struct Channel* channel = head->channel;
        if(head->type == ADD) {            //添加---将文件描述符添加到dispatcher（poll/epoll/select）的监测集合中
            eventLoopAdd(evLoop, channel);
            // Debug("文件描述符添加成功.........");
        }
        else if(head->type == DELETE) {    //删除---将文件描述符从dispatcher的监测集合中删除
            eventLoopRemove(evLoop, channel);
            // destroyChannel(evLoop, channel);     释放channel，最后分析
        }
        else if(head->type == MODIFY) {    //修改事件---读/写
            eventLoopModify(evLoop, channel);
        }
        struct ChannelElement* tmp = head;
        head = head->next;
        free(tmp);   //删除节点
    }
    evLoop->head = evLoop->tail = NULL;    //所有的任务都处理完毕后，任务队列重新初始化为空
    pthread_mutex_unlock(&evLoop->mutex);    //解锁
    return 0;
}

int eventLoopAdd(struct EventLoop *evLoop, struct Channel *channel)
{
    int fd = channel->fd;
    struct ChannelMap* channelMap = evLoop->channelMap;
    if(fd >= channelMap->size) {   //如果fd大于channelMap的数量
        if(!makeMapRoom(channelMap, fd, sizeof(struct Channel*))) {
            return -1;   //如果扩容失败
        }
    }
    //找到fd对应的数组元素位置，并存储
    if(channelMap->list[fd] == NULL) {
        channelMap->list[fd] = channel;       //添加到channelMap
        evLoop->dispatcher->add(channel, evLoop);     //添加到dispatcher监测集合
    }
    // Debug("fd: %d", fd);
    return 0;
}
int eventLoopRemove(struct EventLoop* evLoop, struct Channel* channel)
{
    int fd = channel->fd;
    struct ChannelMap* channelmap = evLoop->channelMap;
    if(fd >= channelmap->size) {
        return -1;   //要删除的文件描述符及对应的channel并不在channelmap中存储（证明一定不会再dispatcher监测集合）
    }
    int ret = evLoop->dispatcher->remove(channel, evLoop);
    return ret;    
}
int eventLoopModify(struct EventLoop* evLoop, struct Channel* channel)
{
    int fd = channel->fd;
    struct ChannelMap* ChannelMap = evLoop->channelMap;
    if(fd >= ChannelMap->size || ChannelMap->list[fd] == NULL) {
        return -1;
    }
    int ret = evLoop->dispatcher->modify(channel, evLoop);
    return ret;
}

int destroyChannel(struct EventLoop *evLoop, struct Channel *channel)
{
    //删除channel 和 fd 的对应关系
    evLoop->channelMap->list[channel->fd] = NULL;
    //关闭fd
    close(channel->fd);
    //释放channel指针指向的地址
    free(channel);
    return 0;
}
