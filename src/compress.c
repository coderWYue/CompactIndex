/*************************************************************************
	> File Name: compress.c
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Thu 26 Dec 2019 11:22:03 AM CST
 ************************************************************************/

#include "compress.h"
#include "common.h"

size_t compressList2Buf(pListHead li,uint64_t *buf,size_t bufLen){
    int i = 0;
    if(li->head == NULL) return 0;
    uint64_t currPrefix = ((pPktPosNode)li->head)->offset & PREFIX_MASK;
    int currBlkStartPos = i;
    buf[i++] = ((pPktPosNode)li->head)->offset;
    li->head = ((pPktPosNode)li->head)->next;
    while(likely(li->head && i < bufLen)){
        uint64_t count = 0;
        int s = 0;
        uint64_t word = 0;
        while(li->head && (((pPktPosNode)li->head)->offset & PREFIX_MASK) == currPrefix){
            uint64_t curr = ((pPktPosNode)li->head)->offset & OFFSET_MASK;
            li->head = ((pPktPosNode)li->head)->next;
            count++;
            word |= (curr << s);
            s += OFFSET_SIZE;
            if(s == 64){
                if(i < bufLen-1) buf[i++] = word;
                else break;
                word = 0;
                s = 0;
            }
        }
        if(word) buf[i++] = word;
        buf[currBlkStartPos] |= (count << 48);
        if(i >= bufLen) break;
        currBlkStartPos = i;
        if(li->head){
            currPrefix = ((pPktPosNode)li->head)->offset & PREFIX_MASK;
            buf[i++] = ((pPktPosNode)li->head)->offset;
            li->head = ((pPktPosNode)li->head)->next;
        }
    }
    return i * 8;
}

