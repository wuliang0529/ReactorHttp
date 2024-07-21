#define _GNU_SOURCE     //memmem需要
#include "HttpRequest.h"
#include <stdio.h>      //null的头文件
#include <strings.h>    //strncasecmp头文件
#include <string.h>     //strlen头文件
#include <stdlib.h>     //malloc头文件
#include <sys/stat.h>   //stat函数头文件
#include <dirent.h>     //scandir函数 与 dirent结构体 的头文件
#include <fcntl.h>
#include <unistd.h>     //lseek函数头文件
#include <assert.h>     //assert头文件
#include <pthread.h>    //pthread_create头文件
#include <ctype.h>      //isxdigit头文件
#include "Log.h"
  
#define HeaderSize 12

struct HttpRequest* httpRequestInit()
{
    struct HttpRequest* request = (struct HttpRequest*)malloc(sizeof(struct HttpRequest));
    httpRequestReset(request);
    request->reqHeaders = (struct RequestHeader*)malloc(sizeof(struct RequestHeader) * HeaderSize);
    return request;
}

void httpRequestReset(struct HttpRequest *req)
{
    req->curState = ParseReqLine;
    req->method = NULL;        //重置指针
    req->url = NULL;
    req->version = NULL;
    req->reqHeardersNum = 0;   //在request->reqHeaders指向的数组中从0号位置开始存储
}

void httpRequestResetEx(struct HttpRequest *req)
{
    free(req->method);        //释放内存
    free(req->url);
    free(req->version);
    //释放request->reqHeaders指针数组的内存
    if(req->reqHeaders != NULL) {
        for(int i=0; i<req->reqHeardersNum; ++i) {
            free(req->reqHeaders[i].key);
            free(req->reqHeaders[i].value);
        }
        free(req->reqHeaders);
    }
    httpRequestReset(req);    //当重置指针时，如果之前没有释放对应的内存，会导致内存泄漏
}

void httpRequestDestory(struct HttpRequest *req)
{
    if(req != NULL) {
        //先释放请求行申请内存
        httpRequestResetEx(req);
        free(req);
    }
}

enum HttpRequestState httpRequestState(struct HttpRequest *request)
{
    return request->curState;
}

void httpRequestAddHeader(struct HttpRequest *request, const char *key, const char *value)
{
    request->reqHeaders[request->reqHeardersNum].key = (char*)key;      //这里并没有分配内存，所以需要在外部申请并释放（单独释放key 和 value）
    request->reqHeaders[request->reqHeardersNum].value = (char*)value;  //给key和value赋值时需要把地址传递给key和value
    request->reqHeardersNum++;
}

//只有客户端发送的是基于post的http请求，此函数才会用到（需要得到content-type等类型）
char *httpRequestGetHeader(struct HttpRequest *request, const char *key)
{
    if(request != NULL) {                                         //注意************个人感觉应该判断的是request->reqHeaders是否为空
        for(int i=0; i<request->reqHeardersNum; ++i) {
            //str代表字符串，n代表要比较的长度，case代表不缺分大小写，cmp代表compare
            if(strncasecmp(request->reqHeaders[i].key, key, strlen(key)) == 0) {
                return request->reqHeaders[i].value;
            }
        }
    }
    return NULL;
}

//分割请求行辅助函数
char* splitRequestLine(const char* start, const char* end, const char* sub, char** ptr)   //sub是指要匹配的字符串
{
    char* space = (char*)end;   //初始化为end是因为如果sub等于null，length可以正确计算
    if(sub != NULL) {
        space = memmem(start, end-start, sub, strlen(sub));  //返回的结果是匹配子串的起始位置
        assert(space != NULL);  
    }
    int length = space - start;
    //---注意：给指针分配堆内存，就要传递二级指针; （二级指针就是一级指针的地址）
    //---注意：形参传递指针时，传递的是原指针的副本，如果要给原指针分配内存，就要传递原指针的地址，传递地址是不会发生拷贝的
    char* tmp = (char*)malloc(length + 1);   //加1是为了存储“\0”； 
    strncpy(tmp, start, length);
    tmp[length] = '\0';
    *ptr = tmp;
    return space + 1; //方便下一次调用此函数（加1是为了跳过空格）
}

bool parseHttpRequestLine(struct HttpRequest *request, struct Buffer *readBuf)
{
    // 读出请求行,保存字符串结束位置
    char* end = bufferFindCRLF(readBuf);
    //保存字符串起始位置
    char* start = readBuf->data + readBuf->readPos;
    //请求行总长度
    int lineSize = end - start;
    if(lineSize) {
        //请求行：get xxx/xxx.txt http/1.1
        //得到请求方式 get
#if 0
        char* space = memmem(start, lineSize, " ", 1);  //返回的结果是匹配子串的起始位置
        assert(space != NULL);
        int methodSize = space - start;
        request->method = (char*)malloc(sizeof(methodSize +1 ));   //加1是为了存储“\0”； 
        strncpy(request->method, start, methodSize);
        request->method[methodSize] = '\0';
        // 请求静态资源 xxx/xxx.txt
        start = space + 1;
        char* space =  memmem(start, end-start, " ", 1);
        assert(space != NULL);
        int urlSize = space - start;
        request->url = (char*)malloc(sizeof(urlSize + 1));
        strncpy(request->url, start, urlSize);
        request->url[urlSize] = '\0';
        // http版本
        start = space + 1;
        request->version = (char*)malloc(sizeof(end - start + 1));
        strncpy(request->version, start, end - start);
        request->version[end-start] = '\0';
#endif
        start = splitRequestLine(start, end, " ", &request->method);
        start = splitRequestLine(start, end, " ", &request->url);
        splitRequestLine(start, end, NULL, &request->version);
        //移动Buffer中的readPos指针---为解析请求头做准备
        readBuf->readPos += lineSize;
        readBuf->readPos += 2;   //跳过"\r\n",2个字符;
        //解析状态修改
        request->curState = ParseReqHeaders;
        return true;
    }
    return false;
}

bool parseHttpRequestHeader(struct HttpRequest *request, struct Buffer *readBuf)
{
    char* end = bufferFindCRLF(readBuf);   //找到\r, 注意：每行的结尾是\r\n
    if(end != NULL){
        char* start = readBuf->data + readBuf->readPos;
        int lineSize = end - start;
        //基于键值对的“: ”取搜索key和value 
        char* middle = memmem(start, lineSize, ": ", 2);
        if(middle != NULL) {
            //key值
            char* key = malloc(middle - start + 1);    //加1是为了在字符串末尾存储'\0'
            strncpy(key, start, middle-start);
            key[middle-start] = '\0';
            //value值
            char* value = malloc(end - middle - 2 + 1);   
            strncpy(value, middle + 2, end - middle - 2);
            value[end - middle - 2] = '\0';

            //把key和value传递给request中的reqHeaders
            httpRequestAddHeader(request, key, value);
            //移动读数据的位置
            readBuf->readPos += lineSize;
            readBuf->readPos += 2;
        }   
        else {  //如果middle为空，则证明解析到了get请求的第三部分--->空行，忽略post请求
            //跳过空行
            readBuf->readPos += 2;
            //修改解析状态
            request->curState = ParseReqDone;   //忽略了post请求，如果是post的请求就得解析第四部分的数据块
        }
        return true;
    }
    return false;
}

bool parseHttpRequest(struct HttpRequest *request, struct Buffer *readBuf, 
           struct HttpResponse* response, struct Buffer* sendBuf, int socket)
{
    bool flag = true;
    while(request->curState != ParseReqDone) {
        switch (request->curState)
        {
        case ParseReqLine:
            flag = parseHttpRequestLine(request, readBuf);
            break;
        case ParseReqHeaders:
            flag = parseHttpRequestHeader(request, readBuf);
            break;
        case ParseReqBody:
            break;
        default:
            break;
        }
        if(!flag) {
            return flag;
        }
        //判断是否解析完毕了，如果解析完毕了，需要准备回复的数据
        if(request->curState == ParseReqDone) {
            //1. 根据解析出的原始数据，对客户端的请求做出处理
            // Debug("到这一步了没呢。。。。。。。。。。。。");    //到这步没问题
            processHttpRequest(request, response);
            //2. 组织响应数据并发送给客户端
            Debug("sendbuf中数据量: %d", sendBuf->writePos - sendBuf->readPos);
            httpResponsePrepareMsg(response, sendBuf, socket);
        }
    }
    request->curState = ParseReqLine;   //状态还原，保证还能继续处理第二条及之后的客户端请求
    return flag;
}

//处理基于get的http请求
bool processHttpRequest(struct HttpRequest *request, struct HttpResponse* response)
{
    if(strcasecmp(request->method, "get") != 0) {
        return -1;
    }
    decodeMsg(request->url, request->url);
    //处理客户端请求的静态资源（目录或者文件）
    //（1）获取文件的属性---是根目录，还是子目录或文件
    char* file = NULL;
    if(strcmp(request->url, "/") == 0) {
        file = "./";
    }
    else{
        file = request->url + 1;
    }
    //（2）判断请求的资源在服务器中是否存在
    struct stat st;    //用于存储获取到的文件属性信息
    int ret = stat(file, &st);    //获取文件的属性并存储至st
    // Debug("是否成功获取到了请求资源的属性..........");      //可以到达这里

    if(ret == -1) {              //如果文件不存在--即属性获取失败
        //文件不存在---回复404
        strcpy(response->fileName, "404.html");
        response->statusCode = NotFound;
        strcpy(response->statusMsg, "Not Found");
        //响应头
        httpResponseAddHeader(response, "Content-type", getFileType(".html"));
        response->sendDataFunc = sendFile;
        return 0;
    }
    strcpy(response->fileName, file);
    response->statusCode = OK;
    strcpy(response->statusMsg, "Ok");
    //（3）判断资源类型(目录 or 非目录)
    if(S_ISDIR(st.st_mode)) {
        //把这个目录中的内容发送给客户端
        // strcpy(response->fileName, file);   //与else里面的代码冗余，拿到if-else外边
        // response->statusCode = OK;
        // strcpy(response->statusMsg, "Ok");
        //响应头
        // Debug("可以判断请求的是目录吗。。。。。。。。。。。。。。。");    //程序可以运行到这
        httpResponseAddHeader(response, "Content-type", getFileType(".html"));   //发送目录网页
        response->sendDataFunc = sendDir;
    }
    else {
        //把文件的内容发送给客户端
        // strcpy(response->fileName, file);
        // response->statusCode = OK;
        // strcpy(response->statusMsg, "Ok");
        //响应头
        char tmp[12] = { 0 };
        sprintf(tmp, "%ld", st.st_size);    //发送content-length
        httpResponseAddHeader(response, "Content-type", getFileType(file));    //发送文件的内容
        httpResponseAddHeader(response, "Content-length", tmp);   //发送文件大小
        response->sendDataFunc = sendFile;
    }
    
    return false;
}

int hexToDec(char c)
{
    if(c >= '0' && c <= '9') {
        return c - '0';
    }
    if(c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if(c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return 0;
}

//to存储解码之后的数据，传出参数，from被解码的数据，传入参数
void decodeMsg(char *to, char *from)    //字符串指针
{
    //Linux%E5%86%85%E6%A0%B8.jpg
    for(; *from != '\0'; ++to, ++from) {
        //isxdigit判断是不是16进制格式，取值在0-f
        if(from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])){
            //16进制  --->  10进制int   ----> 字符型char （整型到字符型存在隐式的自动类型转换）
            *to = hexToDec(from[1])*16 + hexToDec(from[2]);
            from += 2;
        }
        else{
            //拷贝字符，复制
            *to = *from;
        }
    }
    *to = '\0';    //字符串结尾
}

const char* getFileType(const char* name){

    //判断的类型如：a.jpg、a.mp4、a.html
    //从左向右找“.”，找到之后从左向右就可以得到完整的后缀名
    const char* dot = strrchr(name, '.');    //strrchr是从右向左找
    if(dot == NULL) {
        return "text/plain; charset=utf-8";   //纯文本
    }
    if(strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0){
        return "text/html; charset=utf-8";
    }
    if(strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0){
        return "image/jpeg";
    }
    if(strcmp(dot, ".gif") == 0){
        return "image/gif";
    }
    if(strcmp(dot, ".png") == 0){
        return "image/png";
    }
    if(strcmp(dot, ".css") == 0){
        return "text/css";
    }
    if(strcmp(dot, ".au") == 0){
        return "audio/basic";
    }
    if(strcmp(dot, ".wav") == 0){
        return "audio/wav";
    }
    if(strcmp(dot, ".avi") == 0){
        return "video/x-msvideo";
    }
    if(strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0){
        return "video/quicktime";
    }
    if(strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0){
        return "video/mpeg";
    }
    if(strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0){
        return "model/vrml";
    }
    if(strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0){
        return "audio/quicktime";
    }
    if(strcmp(dot, ".mp3") == 0){
        return "audio/mpeg";
    }
    if(strcmp(dot, ".mp4") == 0) {
        return "video/mp4";
    }
    if(strcmp(dot, ".ogg") == 0) {
        return "application/ogg";
    }
    if(strcmp(dot, ".pac") == 0) {
        return "application/x-ns-proxy-autoconfig";
    }
    return "text/plain; charset=utf-8";
}

/**html网页结构  ----如果客户端请求的资源为目录，则需要组织成html网页发送给客户端
 * <html>
 *      <head>
 *          <title>name</title>
 *          <meta http-equiv="content-type" content="text/html; charset=UTF-8">   //解决网页乱码问题
 *      </head>
 *      <body>
 *          <table>
 *              <tr>
 *                  <td>内容</td>
 *              </tr>
 *              <tr>
 *                  <td>内容</td>
 *              </tr>
 *          </table> 
 *      </body>
 * </html>
 */

void sendDir(const char *dirName, struct Buffer* sendBuf, int cfd)   //需要把他定义为与 组织回复给客户端数据块的函数指针 相同的类型，这样才能让函数指针指向该函数的地址，调用该函数
{
    char buf[4096] = { 0 };
    sprintf(buf, "<html><head><title>%s</title><meta charset=\"%s\"></head><body><table>", dirName, "UTF-8"); 
    struct dirent** namelist;
    int num = scandir(dirName, &namelist, NULL, alphasort);
    for(int i=0; i<num; ++i) {
        //取出文件名namelist，指向的是一个指针数组struct diren* tmp[]
        char* name = namelist[i]->d_name;    //取出来的是相对于dirName的相对路径，需要拼接一下
        struct stat st;
        char subPath[1024] = { 0 };
        sprintf(subPath, "%s/%s", dirName, name);
        stat(subPath, &st);   //用于判断subPath是文件还是目录
        if(S_ISDIR(st.st_mode)) {
            sprintf(buf + strlen(buf), "<tr><td><a href=\"%s/\">%s</a></td>%ld<td></td></tr>",name, name, st.st_size);
        }
        else{
            sprintf(buf + strlen(buf), "<tr><td><a href=\"%s\">%s</a></td>%ld<td></td></tr>",name, name, st.st_size);
        }
        // send(cfd, buf, strlen(buf), 0);    
        bufferAppendString(sendBuf, buf);   //将要发送的数据写入sendBuf
#ifndef MSG_SEND_AUTO
        bufferSendData(sendBuf, cfd);    //第二种数据发送方式-----写一部分发送一部分(第一种是全部写入sendBuf后再发送：存在可能写不下程序死掉的问题)
#endif
        memset(buf, 0, sizeof(buf));
        free(namelist[i]);     //释放一级指针
    }
    sprintf(buf, "</table></body></html>");
    // send(cfd, buf, strlen(buf), 0);
    bufferAppendString(sendBuf, buf);   //将要发送的数据写入sendBuf
#ifndef MSG_SEND_AUTO
    bufferSendData(sendBuf, cfd);   //第二种数据发送方式-----写一部分发送一部分
#endif
    free(namelist);   //释放一下二级指针
    // return 0;
}

void sendFile(const char *filename, struct Buffer* sendBuf, int cfd)
{
    
    //1.打开文件
    int fd = open(filename, O_RDONLY);   //只读的方式打开文件
    assert(fd > 0);   //断言判断文件是否打开成功
#if 1
    while(1){
        char buf[1024] = { 0 };
        int len = read(fd, buf, sizeof buf);
        if(len > 0) {
            // send(cfd, buf, len, 0);
            bufferAppendData(sendBuf, buf, len);   //不能使用bufferAppendString，因为这里的buf不一定是以\0结尾，会出错
#ifndef MSG_SEND_AUTO
            bufferSendData(sendBuf, cfd);     //第二种数据发送方式-----写一部分发送一部分
#endif            
            // usleep(1);    //已经在bufferSendData中让程序休眠了1s
        }
        else if(len == 0) {
            break;
        }
        else{
            close(cfd);      //读文件失败，把文件描述符关掉
            perror("read");
            // return -1;
        }
    }
#else      //我们需要把文件存储到buf中，而不是直接发送给客户端
    //使用这种方式发送数据只适合发送文件，不适合发送目录（所以如果要使用这种高效率发送数据的方式，需要使用另外一个函数来发送目录）
    //使用sendfile效率更高---号称“零拷贝函数”，减少了在内核态的拷贝
    int size = lseek(fd, 0, SEEK_END);   //通过指针从头到尾的偏移量获得文件的大小---此时指针已经移动到文件尾部了
    lseek(fd, 0, SEEK_SET);
    off_t offset = 0;      //设置偏移量位置
    while(offset < size) {
        int ret = sendfile(cfd, fd, &offset, size-offset);    //从偏移处开始发数据，发完之后更新偏移量
        if(ret == -1) {
            perror("sendfile");
        }
    }
#endif
    close(fd);
    // return 0;
}