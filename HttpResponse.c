#include "HttpResponse.h"
#include <strings.h>    //bzero对挺的头文件
#include <string.h>
#include <stdlib.h>    //malloc对应的头文件
#include <stdio.h>     //sprintf对应的头文件
#include "Log.h"

#define ResHeaderSize 16   //定义一个response响应请求中headers存储容量的宏（用于开辟内存）


struct HttpResponse *httpResponseInit()
{
    struct HttpResponse* response = (struct HttpResponse*)malloc(sizeof(struct HttpResponse));
    response->headersNum = 0;
    int size = sizeof(struct ResponseHeader) * ResHeaderSize;
    response->headers = (struct ResponseHeader*)malloc(size);   //malloc申请堆内存，并未初始化
    //初始化 headers结构体数组----使用bzero或memset
    bzero(response->headers, size);
    //初始化 statusMsg字符数组---注意与指针数组的区别哦！！！
    response->statusCode = Unknown;
    bzero(response->statusMsg, sizeof(response->statusMsg));
    bzero(response->fileName, sizeof(response->fileName));
    //初始化函数指针
    response->sendDataFunc = NULL;
    return response;
}

void httpResponseDestory(struct HttpResponse *response)
{
    if(response != NULL) {
        free(response->headers);   //注意：它是结构体数组，而不是指针数组
        free(response);
    }
}

void httpResponseAddHeader(struct HttpResponse *response, const char *key, const char *value)
{
    if(response == NULL || key == NULL || value == NULL) {
        return;
    }
    //添加key和value到response中的headers结构体数组中
    strcpy(response->headers[response->headersNum].key, key);
    strcpy(response->headers[response->headersNum].value, value);
    response->headersNum++;
}

void httpResponsePrepareMsg(struct HttpResponse *response, struct Buffer *sendBuf, int socket)
{
    //状态行
    char tmp[1024] = { 0 };
    sprintf(tmp, "HTTP/1.1 %d, %s\r\n", response->statusCode, response->statusMsg);
    bufferAppendString(sendBuf, tmp);
    Debug("%s", tmp);
    // Debug("写入响应了吗？ : %d", sendBuf->writePos);    
    //响应头
    for (int i = 0; i < response->headersNum; i++)
    {
        sprintf(tmp, "%s: %s\r\n", response->headers[i].key, response->headers[i].value);
        bufferAppendString(sendBuf, tmp);
        Debug("写入响应了吗？ : %d", sendBuf->writePos); 
    }
    //空行
    bufferAppendString(sendBuf, "\r\n");
#ifndef MSG_SEND_AUTO
    bufferSendData(sendBuf, socket);    //第二种数据发送方式-----写一部分发送一部分
#endif
    //回复的数据
    response->sendDataFunc(response->fileName, sendBuf, socket);
}
