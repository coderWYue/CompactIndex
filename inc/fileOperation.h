/*************************************************************************
	> File Name: fileOperation.h
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Wed 20 Nov 2019 06:06:50 PM PST
 ************************************************************************/

#ifndef _FILEOPERATION_H
#define _FILEOPERATION_H
#include <stdint.h>

#define BUF_ELE_NUM 0x200000
#define COMPRESSED_BUF_SIZE 0x800000
#define DATA_BUF_SIZE 0xc00000
#define DATA_THRESHOLD 0x800000
#define PCAP_FILE_HEAD_SIZE 24
#define MAX_PATH_LEN 256
#define POS_MASK 0x00000000ffffffff
#define LIST_LEN_SHIFT 32
#define SEGMENT_SIZE 256
#define DATABLE_SIZE 65536
#define BOUND 0x100000000
#define LEN_BYTES 8 

typedef uint8_t byte;

enum FileCategory {DATA = 0, INDEX, RESULT};
typedef enum FileCategory FileCategory;

enum KeyType {key16 = 2, key32 = 4, key64 = 8, key128 = 16};
typedef enum KeyType KeyType;

typedef struct FileTrieNode{
    uint64_t offsetArr[SEGMENT_SIZE];
}FileTrieNode,*pFileTrieNode;

typedef struct DATable{
    uint64_t offsetArr[DATABLE_SIZE];
}DATable,*pDATable;

typedef struct Range{
    int lInd;
    int rInd;
}Range,*pRange;
 
/*
static const char * const dataStorageDir = "../dataFileDir/";
static const char * const indexStorageDir = "../indexFileDir/";
static const char * const resultStorageDir = "./resultFileDir/";
*/

extern byte compressedInvertedListBuf[COMPRESSED_BUF_SIZE];
extern uint64_t *matchInvertList;

#ifdef __cplusplus
extern "C"{
#endif
void getPath(char *path,const char *fileName,FileCategory cate);
int getCILFromDATable(const char *path,uint16_t key,byte **more);
int getCILFromDATable_range(const char *path,uint16_t low,uint16_t high,byte **more);
int getCILFromTrie(const char *path,int16_t key[],int keyLen,byte **more);
int getCILFromTrie_prefix(const char *path,int16_t key[],int keyLen,int lo,int hi,byte **more);
int getLowPartCILFromTrie(const char *path,int16_t key[],int keyLen,byte **more);
int getHighPartCILFromTrie(const char *path,int16_t key[],int keyLen,byte **more);
int getDataByIL(const char *path,uint64_t invertedList[],int pktNum,int compress_enable);
int getILFromIndexArr(const void *path,const void *target,int dir);
int getIntervalILFromIndexArr(const char *path,const void *lo, const void *hi);
#ifdef __cplusplus
}
#endif
#endif
