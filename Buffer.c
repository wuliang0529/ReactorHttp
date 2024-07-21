// //buffer用于实现套接字通信时的数据传入和传出（存储通讯时的数据）
// //整体结构：Buffer[data, readPos, writePos, capacity]
#define _GNU_SOURCE    //memmem需要的宏定义（查看man文档可看）
#include "Buffer.h"
#include <stdio.h>     //NULL对应的头文件
#include <stdlib.h>    //free对应的头文件
#include <sys/uio.h>   //readv对应的头文件
#include <string.h>    
#include <strings.h>   ////memmem头文件
#include <sys/socket.h>   //send头文件
#include <unistd.h>   //usleep头文件
#include "Log.h"

struct Buffer *bufferInit(int size)
{
    struct Buffer* buffer = (struct Buffer*)malloc(sizeof(struct Buffer));   //也可以使用calloc直接申请 + 初始化，省去了memset的调用
    if(buffer != NULL) {
        buffer->data = (char*)malloc(size);   //其实是sizeof(size * char);  申请一块儿堆内存（buffer里的一块儿）
        buffer->capacity = size;              
        buffer->readPos = buffer->writePos = 0;
        memset(buffer->data, 0, size);        //初始化
    }
    return buffer;
}

void bufferDestory(struct Buffer *buf)
{
    if(buf != NULL) {
        if(buf->data != NULL) {
            free(buf->data);
        }
    }
    free(buf);
}

//向buffer内存中写数据前先调用此函数
void bufferExtendRoom(struct Buffer *buffer, int size)
{
    //1.内存够用且不存在内存需要合并的问题---不需要扩容
    if(bufferWriteableSize(buffer) >= size) {
        return;
    }
    //2.内存够用但需要内存合并后才可以---不需要扩容
    else if(buffer->readPos + bufferWriteableSize(buffer) >= size) {
        //得到未读的内存大小
        int readable = bufferReadableSize(buffer);   
        //移动内存
        memcpy(buffer->data, buffer->data + buffer->readPos, readable);    //buffer->data指向buffer的起始地址
        //更新位置
        buffer->readPos = 0;
        buffer->writePos = readable;
    }
    //3.内存不够用---需要扩容
    else {
        void* temp = realloc(buffer->data, buffer->capacity + size);   //temp保存扩容后的buffer地址
        if(temp == NULL) {
            return;  //扩容失败
        }
        memset(temp + buffer->capacity, 0, size);    //从temp到buffer->capacity的内存无需初始化
        //更新数据
        buffer->data = temp;
        buffer->capacity += size;
    }
}

int bufferWriteableSize(struct Buffer *buffer)
{
    return buffer->capacity - buffer->writePos;
}

int bufferReadableSize(struct Buffer *buffer)
{
    return buffer->writePos - buffer->readPos;
}

//适用于向buffer中写入任何类型的data数据
int bufferAppendData(struct Buffer *buffer, const char *data, int size)
{
    if(buffer == NULL || data == NULL || size <= 0) {    
        return -1;
    }
    //扩容---不一定需要扩容
    bufferExtendRoom(buffer, size);
    //数据拷贝
    Debug("buffer->writePos起始位置: %d", buffer->writePos);
    memcpy(buffer->data + buffer->writePos, data, size);
    buffer->writePos += size;
    Debug("buffer->writePos的结束位置: %d", buffer->writePos);
    return 0;
}
//适用于向buffer写入string字符串的情况
int bufferAppendString(struct Buffer* buffer, const char* data)
{   
    int size = strlen(data);
    // Debug("数据成功添加到sendbuf,可发送数量: %d", size);  //有数据
    int ret = bufferAppendData(buffer, data, size);
    Debug("数据是否写入了buffer中呢: %d", buffer->writePos - buffer->readPos);
    return ret;
}
//接收套接字数据并写入buffer
int bufferSocketRead(struct Buffer* buffer, int fd)
{
    //接收套接字数据read/recv/readv（readv更高级）
    struct iovec vec[2];

    int writeable = bufferWriteableSize(buffer);
    vec[0].iov_base = buffer->data + buffer->writePos;
    vec[0].iov_len = writeable;

    char* temp = (char*)malloc(40960);   //40k字节
    vec[1].iov_base = buffer->data + buffer->writePos;
    vec[1].iov_len = 40960;

    int result = readv(fd, vec, 2);
    if(result == -1) {
        return -1;    //函数调用失败
    }
    else if(result <= writeable) {   //未使用新申请的temp内存空间
        buffer->writePos += result;
    }
    else{
        buffer->writePos = buffer->capacity;    //capacity已使用完毕
        bufferAppendData(buffer, temp, result-writeable);   //把temp中的数据写入到buffer（buffAppendData中有扩容逻辑）
    }
    free(temp);
    return result;
}

char *bufferFindCRLF(struct Buffer *buffer)
{
    //strstr --> 大字符串中匹配子字符串（遇到\0结束）  char *strstr(const char *haystack, const char *needle);
    //memmem --> 大数据块中匹配子数据块（需要制定数据库大小） void *memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen); 
    char* ptr = memmem(buffer->data + buffer->readPos, bufferReadableSize(buffer), "\r\n", 2);
    return ptr;
}

int bufferSendData(struct Buffer *buffer, int socket)
{
    //1.判断有无数据
    int readable = bufferReadableSize(buffer);
    Debug("可读数据量是多少呢：%d", readable);
    if(readable > 0) {
        int count = send(socket, buffer->data + buffer->readPos, readable, MSG_NOSIGNAL);
        if(count > 0) {
            buffer->readPos += count;
            usleep(1);
        }
        return count;
    }
    return 0;
}

