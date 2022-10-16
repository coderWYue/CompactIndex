/*************************************************************************
	> File Name: compress.h
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Thu 26 Dec 2019 10:15:02 AM CST
 ************************************************************************/

#ifndef _COMPRESS_H
#define _COMPRESS_H

#include "structure.h"

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

size_t compressList2Buf(pListHead li,uint64_t *buf,size_t bufLen);

#ifdef __cplusplus
}
#endif
#endif
