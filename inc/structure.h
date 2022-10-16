/*************************************************************************
	> File Name: structure.h
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Wed 11 Sep 2019 01:08:54 AM PDT
 ************************************************************************/

#ifndef _STRUCTURE_H
#define _STRUCTURE_H

#include<stdint.h>

#define SEGMENT_SIZE 256
#define DA_TABLE_SIZE 65536    //the size of direct addressing table

typedef unsigned __int128 uint128_t;

typedef struct TrieNode{
    void *pArr[SEGMENT_SIZE];
}__attribute__((__packed__)) TrieNode,*pTrieNode;

typedef struct DirectAddressTable{
    void *table[DA_TABLE_SIZE];
}__attribute__((__packed__)) DirectAddressTable,*pDirectAddressTable;

typedef struct ListHead{
    void *head;
    void *tail;
}__attribute__((__packed__)) ListHead,*pListHead;

typedef struct ListHeadHasKey4{
    ListHead li;
    uint8_t key[16];
}__attribute__((__packed__)) ListHeadHasKey4,*pListHeadHasKey4;

typedef struct PktPosNode{
    uint64_t offset;
    struct PktPosNode *next;
}__attribute__((__packed__)) PktPosNode,*pPktPosNode;

typedef struct K16V{
    uint16_t key;
    uint64_t pos;
}__attribute__((__packed__))K16V, *pK16V;

typedef struct K32V{
    uint32_t key;
    uint64_t pos;
}__attribute__((__packed__))K32V, *pK32V;

typedef struct K128V{
    uint128_t key;
    uint64_t pos;
}__attribute__((__packed__))K128V, *pK128V;
#endif
