/*************************************************************************
	> File Name: decompress.h
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Sun 10 May 2020 11:02:48 AM CST
 ************************************************************************/

#ifndef _DECOMPRESS_H
#define _DECOMPRESS_H

#include <stddef.h>
#include <stdint.h>

#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

#define TMP_SIZE 0x400000
#define PREFIX_MASK 0x0000ffffffff0000
#define OFFSET_MASK 0x000000000000ffff
#define BLKLEN_MASK 0xffff000000000000
#define FIRST_ELE_MASK 0x0000ffffffffffff
#define OFFSET_SIZE 16

#ifdef __cplusplus
extern "C"{
#endif

extern uint64_t *invertedListBuf;
extern int dynMallocFlag;
uint32_t decompressIL(uint64_t *compressILBuf, int bufLen);

#ifdef __cplusplus
}
#endif

#endif
