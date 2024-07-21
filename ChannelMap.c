#include "ChannelMap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>    //memset头文件

struct ChannelMap *ChannelMapInit(int size)    //map使用数组实现，数组的容量为size
{
    struct ChannelMap* map = (struct ChannelMap*)malloc(sizeof(struct ChannelMap));   //初始化结构体
    map->size = size;
    map->list = (struct Channel **)malloc(size * sizeof(struct Channel*));      //初始化map中的list数组
    return map;
}

void ChannelMapClear(struct ChannelMap *map)
{
    if(map != NULL) {      //判断当前map是否为空
        for(int i=0; i<map->size; ++i) {    //循环遍历map中存储的Channel* 地址
            if(map->list[i] != NULL) {    //判断当前的Channel*指向的地址是否为空
                free(map->list[i]);
            }
        }
        free(map->list);   
        map->list = NULL;    //避免野指针的存在
    }
    map->size = 0;
}

bool makeMapRoom(struct ChannelMap *map, int newSize, int uniteSize)
{
    if(map->size < newSize) {    //如果当前内存小于需要的内存大小再进行重新分配
        int curSize = map->size;    
        while(curSize < newSize) {  
            curSize *= 2;    //每次增大分配内存的一倍，直至够用
        }
        struct Channel** temp = realloc(map->list, curSize * uniteSize);    
        if(temp == NULL) {     //重新分配内存失败
            return false;
        }
        map->list = temp;
        memset(&map->list[map->size], 0, (curSize - map->size) * uniteSize);   //初始化新分配的内存为0
        map->size = curSize;
    }
    return true;
}
