#pragma once
#include "Buffer.h"

//定义状态码枚举
enum HttpStatusCode {
    Unknown,                     //默认为未知
     OK = 200,                   //请求成功
    MovedPermanently = 301,      //重定向
    MovedTemporarily = 302,      //临时重定向
    BadRequest = 400,            //请求失败
    NotFound = 404               //请求资源未找到
};

//定义响应的结构体---存储响应键值对
struct ResponseHeader
{
    char key[32];
    char value[128];
};

//定义一个函数指针，用来组织要回复给客户端的数据块
typedef void (*responseBody)(const char* filename, struct Buffer* sendBuf, int socket);    //filename为请求资源猫，sendBuf为存储http响应的数据，socket为用来通信的文件描述符

//定义回复响应结构体
struct HttpResponse{
    //(1)状态行：状态码，状态描述
    enum HttpStatusCode statusCode;
    char statusMsg[128];
    char fileName[128];
    //(2)响应头 -- 键值对
    struct ResponseHeader* headers;    //定义一个结构体指针数组，存储多个结构体（键值对）
    int headersNum;   //记录结构体指针数组中存储了多少个结构体
    //(3)空行不做处理
    //(4)body数据块---使用回调函数
    responseBody sendDataFunc;
};

//一些列API函数
//初始化
struct HttpResponse* httpResponseInit();
//销毁
void httpResponseDestory(struct HttpResponse* response);
//添加响应头
void httpResponseAddHeader(struct HttpResponse* response, const char* key, const char* value);
//组织http响应数据
void httpResponsePrepareMsg(struct HttpResponse* response, struct Buffer* sendBuf, int socket);