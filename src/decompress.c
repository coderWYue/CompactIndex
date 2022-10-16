/*************************************************************************
	> File Name: decompress.c
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Sun 10 May 2020 11:05:49 AM CST
 ************************************************************************/

#include "compress.h"
#include "common.h"
#include "decompress.h"

uint64_t listTmp[TMP_SIZE];
uint64_t *invertedListBuf;
int dynMallocFlag = 0;

uint32_t decompressIL(uint64_t *compressILBuf,int bufLen){
    uint32_t from = 0, to = 0;
    if(bufLen > TMP_SIZE/4){
        invertedListBuf = (uint64_t *)malloc(4*bufLen*sizeof(uint64_t));
        dynMallocFlag = 1;
    }else invertedListBuf = (uint64_t *)listTmp;
    while(likely(from < bufLen)){
        uint64_t currPrefix = compressILBuf[from] & PREFIX_MASK;
        invertedListBuf[to++] = compressILBuf[from] & FIRST_ELE_MASK;
        uint64_t blkLen = ((compressILBuf[from++] & BLKLEN_MASK) >> 48);
        while(blkLen){
            uint64_t word = compressILBuf[from++];
            int i = 0;
            while(i < 4 && word){
                invertedListBuf[to++] = (currPrefix | (word & OFFSET_MASK));
                word  = (word >> OFFSET_SIZE);
                i++;
                blkLen--;
            }
        }
    }
    return to;
}
