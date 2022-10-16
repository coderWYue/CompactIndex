/*************************************************************************
	> File Name: index.h
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Wed 18 Sep 2019 01:38:40 AM PDT
 ************************************************************************/

#ifndef _INDEX_H
#define _INDEX_H
#include "structure.h"

#define EXT_TRIE_NODE_POOL_NUM 16

typedef enum IndexCate {I_SIP = 0, I_SIP6, I_SPORT, I_DIP, I_DIP6, I_DPORT,\
                        I_TS, I_PROTO, I_NUM} IndexCate;
//extern TrieNode trieNodePool[TRIE_NODE_AMOUNT];
extern pTrieNode extTrieNodePools[EXT_TRIE_NODE_POOL_NUM];
extern uint32_t trieNodeExtUnit;
extern pTrieNode trieNodePool;

extern DirectAddressTable sPortDATable;
extern DirectAddressTable dPortDATable;
extern DirectAddressTable protoDATable;

//extern ListHead recListPool[LIST_HEAD_AMOUNT];
extern pListHead recListPool;
//extern ListHeadHasKey4 recListHKPool[LIST_HEAD_HK_AMOUNT];
extern pListHeadHasKey4 recListHKPool;

//extern PktPosNode posNodePool[PKT_NODE_AMOUNT];
extern pPktPosNode posNodePool;

extern pK16V srcPortIndexArr,dstPortIndexArr,protoIndexArr;
extern pK32V srcIpIndexArr,dstIpIndexArr,tsIndexArr;
extern pK128V srcIp6IndexArr,dstIp6IndexArr;

extern uint32_t realPktNum;
extern uint32_t ipv4PktNum;
extern uint32_t ipv6PktNum;

extern uint32_t trieNodeAmount;
extern uint32_t listHeadAmount;
extern uint32_t listHeadHkAmount;
extern uint64_t pktNodeAmount;

extern pTrieNode sipRoot;
extern pTrieNode dipRoot;
extern pTrieNode sip6Root;
extern pTrieNode dip6Root;
extern pTrieNode tsRoot;

extern uint32_t currUsedTrieNode;
extern uint32_t currUsedSipTrieNode;
extern uint32_t currUsedDipTrieNode;
extern uint32_t currUsedSip6TrieNode;
extern uint32_t currUsedDip6TrieNode;
extern uint32_t currUsedTsTrieNode;

extern uint32_t currUsedList;
extern uint32_t currUsedListHk;

extern uint32_t currUsedPosNode;

extern void *trieNodeLowAddr;
extern void *trieNodeHighAddr;

extern uint32_t ready;
extern uint64_t currIndexOffset;

void initIndexStructure(void);

void index_process(void);

static inline int8_t isTrieNode(void *addr){
    if(addr >= trieNodeLowAddr && addr < trieNodeHighAddr) return 1;
    else return 0;
}

static inline int8_t isExtTrieNodeCommVersion(void *addr){
    int i;
    for(int i = 0; i < EXT_TRIE_NODE_POOL_NUM; ++i){
        if(extTrieNodePools[i] && addr >= extTrieNodePools[i] && addr < (extTrieNodePools[i]+\
                    trieNodeExtUnit))
            return 1;
    }
    return 0;
}
#endif
