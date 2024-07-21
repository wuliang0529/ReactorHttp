#pragma once
#include "Buffer.h"
#include "HttpResponse.h"
#include <stdbool.h>

//请求键值对
struct RequestHeader{
    char* key;          //想要长期有效必须存储在全局区或者是堆内存中，释放内存时需要循环释放（设置成静态数组可以避免内存泄漏，但局限了大小）
    char* value;
};

//当前解析状态
enum HttpRequestState{
    ParseReqLine,      //请求行  （GET  ./1.jpg  http/1.1）
    ParseReqHeaders,   //请求头（key ： value  键值对）
    ParseReqBody,      //请求的数据块(get请求这部分为空)
    ParseReqDone       //请求解析完毕
};

//定义http请求结构体
struct HttpRequest{
    char* method;
    char* url;
    char* version;
    struct RequestHeader* reqHeaders;
    int reqHeardersNum; 
    enum HttpRequestState curState;
};

//初始化
struct HttpRequest* httpRequestInit();
//重置HttpRequest实例----只需要重置指针指向的内存中的额数据，而不需要销毁整个结构体的内存地址
void httpRequestReset(struct HttpRequest* req);     //不释放堆内存
void httpRequestResetEx(struct HttpRequest* req);   //释放三个char指针指向的堆内存，防止内存泄漏
void httpRequestDestory(struct HttpRequest* req);   //内存释放
//获取处理状态
enum HttpRequestState httpRequestState(struct HttpRequest* request);   //enum HttpRequestState是上边定义的枚举类型
//添加请求头---把键值对添加到reqHeaders结构体数组中
void httpRequestAddHeader(struct HttpRequest* request, const char* key, const char* value);
//从reqHeaders结构体数组中根据key值取出value值
char* httpRequestGetHeader(struct HttpRequest* request, const char* key);
//解析请求行
bool parseHttpRequestLine(struct HttpRequest* request, struct Buffer* readBuf);
//解析请求头
bool parseHttpRequestHeader(struct HttpRequest* request, struct Buffer* readBuf);
//解析http请求协议
bool parseHttpRequest(struct HttpRequest* request, struct Buffer* readBuf,
                     struct HttpResponse* response, struct Buffer* sendBuf, int socket);
//处理http请求
bool processHttpRequest(struct HttpRequest* request, struct HttpResponse* response);
//解码特殊字符--to存储解码之后的数据，传出参数，from被解码的数据，传入参数
void decodeMsg(char *to, char *from); 
//解析文件类型
const char* getFileType(const char* name);
//发送目录
void sendDir(const char *dirName, struct Buffer* sendBuf, int cfd);
//发送文件
void sendFile(const char *filename, struct Buffer* sendBuf, int cfd);