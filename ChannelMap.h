#pragma once
#include <stdbool.h>

struct ChannelMap {
    int size;   //记录指针指向的数组的元素总个数
    struct Channel** list;    //存储的是struct Channel* list
};

//初始化函数
struct ChannelMap* ChannelMapInit(int size);

//清空map函数
void ChannelMapClear(struct ChannelMap* map);

//重新分配内存空间--扩容
bool makeMapRoom(struct ChannelMap* map, int newSize, int uniteSize);  //newSize代表重新分配list存储的元素个数，uniteSize代表每个指针所占字节数