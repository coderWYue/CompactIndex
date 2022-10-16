/*************************************************************************
	> File Name: index.c
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Thu 19 Sep 2019 06:48:37 PM PDT
 ************************************************************************/
#define _GNU_SOURCE

#include <sys/time.h>
#include <pthread.h>
#include <sched.h>
#include <xmmintrin.h>
#include "index.h"
#include "tdms.h"
#include "log.h"
#include "common.h"
#include "atomic.h"

//typedef void (*IndexFunc)(int,uint32_t,uint64_t);
typedef void (*IndexFunc)(PktDigest *,int);

IndexFunc indexFuncArr[I_NUM] = {NULL};

extern uint16_t core_num;

uint64_t *dataBase = NULL;

//TrieNode trieNodePool[TRIE_NODE_AMOUNT];
pTrieNode trieNodePool;
pTrieNode extTrieNodePools[EXT_TRIE_NODE_POOL_NUM];

DirectAddressTable sPortDATable;
DirectAddressTable dPortDATable;
DirectAddressTable protoDATable;

//ListHead recListPool[LIST_HEAD_AMOUNT];
pListHead recListPool;
//ListHeadHasKey4 recListHKPool[LIST_HEAD_HK_AMOUNT];
pListHeadHasKey4 recListHKPool;

//PktPosNode posNodePool[PKT_NODE_AMOUNT];
pPktPosNode posNodePool;

pK16V srcPortIndexArr,dstPortIndexArr,protoIndexArr;
pK32V srcIpIndexArr,dstIpIndexArr,tsIndexArr;
pK128V srcIp6IndexArr,dstIp6IndexArr;

uint32_t realPktNum = 0;
uint32_t ipv4PktNum = 0;
uint32_t ipv6PktNum = 0;

uint32_t trieNodeAmount;
uint32_t trieNodeAmountArr[INDEX_NUM] = {0};
uint32_t listHeadAmount;
uint32_t listHeadAmountArr[INDEX_NUM] = {0};
uint32_t listHeadHkAmount;
uint32_t listHeadHkAmountArr[INDEX_NUM] = {0};
uint64_t pktNodeAmount;
uint64_t pktNodeAmountArr[INDEX_NUM] = {0};

pTrieNode sipRoot;
pTrieNode dipRoot;
pTrieNode tsRoot;
pTrieNode sip6Root;
pTrieNode dip6Root;

uint32_t currUsedTrieNode = 0;
uint32_t currUsedTrieNodeArr[INDEX_NUM] = {0};
uint32_t currUsedSipTrieNode = 0;
uint32_t currUsedDipTrieNode = 0;
uint32_t currUsedTsTrieNode = 0;
uint32_t currUsedSip6TrieNode = 0;
uint32_t currUsedDip6TrieNode = 0;
uint32_t trieNodeExtUnit = 65536;

uint32_t currUsedList = 0;
uint32_t currUsedListArr[INDEX_NUM] = {0};
uint32_t currUsedListHk = 0;
uint32_t currUsedListHKArr[INDEX_NUM] = {0};

uint32_t currUsedPosNode = 0;
uint32_t currUsedPosNodeArr[INDEX_NUM] = {0};

uint32_t tNodeBase[INDEX_NUM];
uint32_t lNodeBase[INDEX_NUM];
uint32_t lHKNodeBase[INDEX_NUM];
uint32_t pNodeBase[INDEX_NUM];
uint32_t eTNodeBase[INDEX_NUM] = {0,6,-1,8,13,-1,15,-1};

uint32_t eTNodeShare[INDEX_NUM] = {6, 2, 0, 5, 2, 0, 1, 0};
uint32_t tNodeShare[INDEX_NUM] = {5, 2, 0, 6, 2, 0, 1, 0};
uint32_t pNodeShare[INDEX_NUM] = {2, 2, 2, 2, 2, 2, 2, 2};
uint32_t lHKNodeShare[INDEX_NUM] = {5, 2, 0, 5, 2, 0, 2, 0};
uint32_t lNodeShare[INDEX_NUM] = {5, 1, 1, 5, 1, 1, 1, 1};

void *trieNodeLowAddr;
void *trieNodeHighAddr;

uint32_t ready = 0;    //need to be initialized
uint32_t readyArr[INDEX_NUM] = {0};
uint64_t currIndexOffset = 0;    //need to be initialized
uint64_t currIndexOffsetArr[INDEX_NUM];
uint64_t perCoreOffset = 0;
uint64_t perCoreOffsetArr[INDEX_NUM] = {0};

uint32_t readTail = 0;

#if 1
static inline void updateCurrIndexOffset(int i,IndexCate cate){
    BYTE *dataBuf = (BYTE *)dataZones[i];
    uint32_t len = *(uint32_t *)(dataBuf + perCoreOffsetArr[cate] + 8);
    perCoreOffsetArr[cate] = perCoreOffsetArr[cate] + 16 + len;
    currIndexOffsetArr[cate] += (len + 16);
}
#endif

#if 0
static inline void updateCurrIndexOffset(int i){
    BYTE *dataBuf = (BYTE *)dataZones[i];
    uint32_t len = *(uint32_t *)(dataBuf + perCoreOffset+ 8);
    perCoreOffset = perCoreOffset + 16 + len;
    currIndexOffset += (len + 16);
}
#endif
/*
static inline int8_t indexStruRunOut(){
    if(unlikely(currUsedTrieNode >= TRIE_NODE_THRE ||\
            currUsedList >= LIST_HEAD_THRE ||\
           currUsedListHk >= LIST_HEAD_HK_THRE ||\
            currUsedPosNode >= PKT_NODE_THRE)) return 1;
    else return 0;
}
*/

static inline int8_t needExtTrieNodePool(IndexCate cate){
    return ((currUsedTrieNodeArr[cate] - trieNodeAmountArr[cate]) % trieNodeExtUnit == 0) ? 1 : 0;
}

static inline int8_t doExtTrieNodePool(IndexCate cate){
    int ind = (currUsedTrieNodeArr[cate] - trieNodeAmountArr[cate]) / trieNodeExtUnit;
    if(ind >= eTNodeShare[cate]) return -1;
    extTrieNodePools[ind + eTNodeBase[cate]] = (pTrieNode)calloc(trieNodeExtUnit,sizeof(TrieNode));
    if(extTrieNodePools[ind + eTNodeBase[cate]] == NULL){
        return -2;
    }
    return 0;
}

static inline TrieNode *allocTrieNode(IndexCate cate){
    if(likely(currUsedTrieNodeArr[cate] < trieNodeAmountArr[cate])){
        /*
        uint32_t tmp = tNodeBase[cate] + currUsedTrieNodeArr[cate];
        currUsedTrieNodeArr[cate]++;
        void * next = &trieNodePool[tmp+1];
        for(int i = 0; i < 16; ++i) _mm_prefetch(next+64*i,_MM_HINT_T0);
        return &trieNodePool[tmp];
        */
        return &trieNodePool[tNodeBase[cate] + currUsedTrieNodeArr[cate]++];
        /*
        uint32_t tmp = ATOMIC_GET(currUsedTrieNode);
        while(!ATOMIC_CAS(currUsedTrieNode,tmp,tmp+1)){
            tmp = ATOMIC_GET(currUsedTrieNode);
        }
        currUsedTrieNodeArr[cate]++;
        return &trieNodePool[tmp];
        */
    }else{
        if(unlikely(needExtTrieNodePool(cate))){
            if(unlikely(doExtTrieNodePool(cate))){
                log(ERROR,"extend trie node pool failed");
                exit(0);
            }else{
                log(INFO,"extend trie node pool success");
            }
        }
        int ind = (currUsedTrieNodeArr[cate] - trieNodeAmountArr[cate]) / trieNodeExtUnit;
        return &extTrieNodePools[ind+eTNodeBase[cate]][(currUsedTrieNodeArr[cate]++ - trieNodeAmountArr[cate])%trieNodeExtUnit];

        #if 0
        pTrieNode newTrieNode = (pTrieNode)calloc(1,sizeof(TrieNode));
        if(NULL == newTrieNode){
            log(ERROR,"fail to alloc TrieNode");
            exit(0);
        }else{
            currUsedTrieNode++;
            return newTrieNode;
        }
        #endif
    }
}

static inline int8_t isExtTrieNode(void *addr,IndexCate cate){
    int i;
    int bound = (currUsedTrieNodeArr[cate] - trieNodeAmountArr[cate]) / trieNodeExtUnit;
    bound += eTNodeBase[cate];
    for(i = eTNodeBase[cate]; i <= bound && i < EXT_TRIE_NODE_POOL_NUM; ++i){
        if(addr >= extTrieNodePools[i] && addr < (extTrieNodePools[i] + trieNodeExtUnit))
            return 1;
    }
    return 0;
}

static inline ListHead *allocListHead(IndexCate cate){
    if(likely(currUsedListArr[cate] < listHeadAmountArr[cate])){
        /*
        uint32_t tmp = ATOMIC_GET(currUsedList);
        while(!ATOMIC_CAS(currUsedList,tmp,tmp+1)){
            tmp = ATOMIC_GET(currUsedList);
        }
        currUsedListArr[cate]++;
        return &recListPool[tmp];
        */
        return &recListPool[lNodeBase[cate] + currUsedListArr[cate]++];
    }else{
        pListHead newList = (pListHead)calloc(1,sizeof(ListHead));    
        if(NULL == newList){
            log(ERROR,"fail to alloc list");
            exit(0);
        }else{
            currUsedListArr[cate]++;
            return newList;
        } 
    }
}

static inline ListHeadHasKey4 *allocListHeadHk(IndexCate cate){
    if(likely(currUsedListHKArr[cate] < listHeadHkAmountArr[cate])){
        /*
        uint32_t tmp = ATOMIC_GET(currUsedListHk);
        while(!ATOMIC_CAS(currUsedListHk,tmp,tmp+1)){
            tmp = ATOMIC_GET(currUsedListHk);
        }
        currUsedListHKArr[cate]++;
        return &recListHKPool[tmp];
        */
        return &recListHKPool[lHKNodeBase[cate] + currUsedListHKArr[cate]++];
    }else{
        pListHeadHasKey4 newListHK = (pListHeadHasKey4)calloc(1,sizeof(ListHeadHasKey4));
        if(NULL == newListHK){
            log(ERROR,"fail to alloc listHK");
            exit(0);
        }else{
            currUsedListHKArr[cate]++;
            return newListHK;
        } 
    } 
}

static inline PktPosNode *allocPosNode(IndexCate cate){
    if(likely(currUsedPosNodeArr[cate] < pktNodeAmountArr[cate])){
        //return &posNodePool[pNodeBase[cate] + currUsedPosNodeArr[cate]++];
        return (posNodePool+pNodeBase[cate] + currUsedPosNodeArr[cate]++);
        /*
        uint32_t tmp = ATOMIC_GET(currUsedPosNode);
        while(!ATOMIC_CAS(currUsedPosNode,tmp,tmp+1)){
            tmp = ATOMIC_GET(currUsedPosNode);
        }
        currUsedPosNodeArr[cate]++;
        return &posNodePool[tmp];
        */
        //return &posNodePool[cate + (currUsedPosNodeArr[cate]++)*I_NUM];
    }else{
        pPktPosNode newPosNode = (pPktPosNode)calloc(1,sizeof(PktPosNode));
        if(NULL == newPosNode){
            log(ERROR,"fail to alloc posNode");
            exit(0);
        }else{
            currUsedPosNodeArr[cate]++;
            return newPosNode;
        } 
    } 
}

static inline void appendPosNodeToList(pListHead li,pPktPosNode pPkt){
    if(li->head == NULL) li->head = li->tail = pPkt;
    else{
        ((pPktPosNode)li->tail)->next = pPkt;
        li->tail = pPkt;
    }
}

void initIndexStructure(){
#if 1 
    //memset(trieNodePool,0,sizeof(TrieNode)*trieNodeAmount);
    //memset(&sPortDATable,0,sizeof(sPortDATable));
    //memset(&dPortDATable,0,sizeof(dPortDATable));
    //memset(&protoDATable,0,sizeof(protoDATable));
    //memset(recListPool,0,sizeof(ListHead)*listHeadAmount);
    //memset(recListHKPool,0,sizeof(ListHeadHasKey4)*listHeadHkAmount);
    //memset(posNodePool,0,sizeof(PktPosNode)*pktNodeAmount);
    currUsedTrieNode = 0;
    trieNodeExtUnit = 65536;
    currUsedSipTrieNode = 0;
    currUsedDipTrieNode = 0;
    currUsedTsTrieNode = 0;
    currUsedList = 0;
    currUsedListHk = 0;
    currUsedPosNode = 0;

    for(int i = 0; i < I_NUM; ++i){
        trieNodeAmountArr[i] = trieNodeAmount / 16 * tNodeShare[i];
        listHeadAmountArr[i] = listHeadAmount / 16 * lNodeShare[i];
        listHeadHkAmountArr[i] = listHeadHkAmount / 16 * lHKNodeShare[i];
        pktNodeAmountArr[i] = pktNodeAmount / 16 * pNodeShare[i];   
    }

    for(int i = 1; i < I_NUM; ++i){
        tNodeBase[i] = tNodeBase[i-1] + trieNodeAmountArr[i-1];
        lNodeBase[i] = lNodeBase[i-1] + listHeadAmountArr[i-1];
        lHKNodeBase[i] = lHKNodeBase[i-1] + listHeadHkAmountArr[i-1];
        pNodeBase[i] = pNodeBase[i-1] + pktNodeAmountArr[i-1];
    }

    sipRoot = allocTrieNode(I_SIP);
    //currUsedSipTrieNode = currUsedTrieNode - currUsedDipTrieNode;
    dipRoot = allocTrieNode(I_DIP);
    //currUsedDipTrieNode = currUsedTrieNode - currUsedSipTrieNode;
    //int tmp = currUsedTrieNode;
    sip6Root = allocTrieNode(I_SIP6);
    //currUsedSip6TrieNode = currUsedTrieNode - tmp;
    //tmp = currUsedTrieNode;
    dip6Root = allocTrieNode(I_DIP6);
    //currUsedDip6TrieNode = currUsedTrieNode - tmp;
    //tmp = currUsedTrieNode;
    tsRoot = allocTrieNode(I_TS);
    //currUsedTsTrieNode = currUsedTrieNode - tmp;

    /*
    sipRoot = allocTrieNode();
    currUsedSipTrieNode = currUsedTrieNode - currUsedDipTrieNode;
    dipRoot = allocTrieNode();
    currUsedDipTrieNode = currUsedTrieNode - currUsedSipTrieNode;
    uint32_t tmp = currUsedTrieNode;
    tsRoot = allocTrieNode();
    currUsedTsTrieNode = currUsedTrieNode - tmp;
    */

    trieNodeLowAddr = trieNodePool;
    trieNodeHighAddr = trieNodePool + trieNodeAmount;
    int i;
    for(i = 0; i < EXT_TRIE_NODE_POOL_NUM; ++i) extTrieNodePools[i] = NULL;
#endif

#if 0
    srcPortIndexArr = (pK16V)calloc(pktAmount,sizeof(K16V));
    dstPortIndexArr = (pK16V)calloc(pktAmount,sizeof(K16V));
    protoIndexArr = (pK16V)calloc(pktAmount,sizeof(K16V));

    srcIpIndexArr = (pK32V)calloc(pktAmount,sizeof(K32V));
    dstIpIndexArr = (pK32V)calloc(pktAmount,sizeof(K32V));
    tsIndexArr = (pK32V)calloc(pktAmount,sizeof(K32V));

    srcIp6IndexArr = (pK128V)calloc(pktAmount,sizeof(K128V));
    dstIp6IndexArr = (pK128V)calloc(pktAmount,sizeof(K128V));
#endif

    dataBase = (uint64_t *)malloc(core_num*sizeof(uint64_t));

    dataBase[0] = PCAP_FILE_HEAD_SIZE;

    ready = 0;
    perCoreOffset = 0;
    currIndexOffset = 0;
    for(int i = 0; i < I_NUM; ++i) currIndexOffsetArr[i] = PCAP_FILE_HEAD_SIZE;
}

void buildIndexForLongKey(uint8_t key[],uint8_t len,pTrieNode currLevelNode,IndexCate cate,uint64_t off){
    pPktPosNode pPkt = allocPosNode(cate);
    pPkt->offset = off;
    uint8_t i = 0;
    while(i < len - 1){
        void *pNode = currLevelNode->pArr[key[i]];
        if(pNode == NULL){
            pListHeadHasKey4 liHK = allocListHeadHk(cate);
            memcpy(liHK->key,key,len);
            appendPosNodeToList(&liHK->li,pPkt);
            currLevelNode->pArr[key[i]] = liHK;
            return;
        }else if(isTrieNode(pNode)){
            ++i;
            currLevelNode = (pTrieNode)pNode;
        }else if(currUsedTrieNodeArr[cate] > trieNodeAmountArr[cate] && isExtTrieNode(pNode,cate)){
            ++i;
            currLevelNode = (pTrieNode)pNode;
        }else{
            if(memcmp(((pListHeadHasKey4)pNode)->key,key,len) == 0){
                appendPosNodeToList(&((pListHeadHasKey4)pNode)->li,pPkt);
                return;
            }else{
                pTrieNode newTrieNode = allocTrieNode(cate);
                currLevelNode->pArr[key[i++]] = newTrieNode;
                newTrieNode->pArr[((pListHeadHasKey4)pNode)->key[i]] = pNode;
                currLevelNode = newTrieNode;
            }
        }
    }
    void *pNode = currLevelNode->pArr[key[i]];
    if(pNode == NULL){
        pListHead li = allocListHead(cate);
        appendPosNodeToList(li,pPkt);
        currLevelNode->pArr[key[i]] = li;
    }else appendPosNodeToList((pListHead)pNode,pPkt); //pNode maybe point to ListHeadHasKey4
    return;
}

void buildIndexForLongKeyNative(uint8_t key[],uint8_t len,pTrieNode currLevelNode,IndexCate cate,uint64_t off){
    pPktPosNode pPkt = allocPosNode(cate);
    pPkt->offset = off;
    uint8_t i = 0;
    while(i < len - 1){
        void *pNode = currLevelNode->pArr[key[i]];
        if(pNode == NULL){
            pTrieNode newTrieNode = allocTrieNode(cate);
            currLevelNode->pArr[key[i++]] = newTrieNode;
            currLevelNode = newTrieNode;
        }else{
            ++i;
            currLevelNode = (pTrieNode)pNode;
        }
    }
    void *pNode = currLevelNode->pArr[key[i]];
    if(pNode == NULL){
        pListHead li = allocListHead(cate);
        appendPosNodeToList(li,pPkt);
        currLevelNode->pArr[key[i]] = li;
    }else appendPosNodeToList((pListHead)pNode,pPkt); //pNode maybe point to ListHeadHasKey4
    return;
}

static void buildIndexForShortKey(uint16_t key,pDirectAddressTable pTable,IndexCate cate,uint64_t off){
    pPktPosNode pPkt = allocPosNode(cate);
    pPkt->offset = off;
    if(pTable->table[key] == NULL){
        pListHead li = allocListHead(cate);
        appendPosNodeToList(li,pPkt);
        pTable->table[key] = li;
    }else appendPosNodeToList((pListHead)pTable->table[key],pPkt);    
}

//static inline void buildIndexForSip4(int i,uint32_t j,uint64_t off){
static inline void buildIndexForSip4(PktDigest *dig,int core_id){
    uint64_t off = dig->off + dataBase[core_id];
    if(dig->af == 4) buildIndexForLongKey(dig->src_ip.ip4,4,sipRoot,I_SIP,off);
    //if(dig->af == 4) buildIndexForLongKeyNative(dig->src_ip.ip4,4,sipRoot,I_SIP,off);
}
//static inline void buildIndexForDip4(int i,uint32_t j,uint64_t off){
static inline void buildIndexForDip4(PktDigest *dig,int core_id){
    uint64_t off = dig->off + dataBase[core_id];
    if(dig->af == 4) buildIndexForLongKey(dig->dst_ip.ip4,4,dipRoot,I_DIP,off);
    //if(dig->af == 4) buildIndexForLongKeyNative(dig->dst_ip.ip4,4,dipRoot,I_DIP,off);
}

//static inline void buildIndexForSip6(int i,uint32_t j,uint64_t off){
static inline void buildIndexForSip6(PktDigest *dig,int core_id){
    uint64_t off = dig->off + dataBase[core_id];
    if(dig->af == 6) buildIndexForLongKey(dig->src_ip.ip6,16,sip6Root,I_SIP6,off);
    //if(dig->af == 6) buildIndexForLongKeyNative(dig->src_ip.ip6,16,sip6Root,I_SIP6,off);
}

//static inline void buildIndexForDip6(int i,uint32_t j,uint64_t off){
static inline void buildIndexForDip6(PktDigest *dig,int core_id){
    uint64_t off = dig->off + dataBase[core_id];
    if(dig->af == 6) buildIndexForLongKey(dig->dst_ip.ip6,16,dip6Root,I_DIP6,off);
    //if(dig->af == 6) buildIndexForLongKeyNative(dig->dst_ip.ip6,16,dip6Root,I_DIP6,off);
}

//static inline void buildIndexForTs(int i,uint32_t j,uint64_t off){
static inline void buildIndexForTs(PktDigest *dig,int core_id){
    uint64_t off = dig->off + dataBase[core_id];
    buildIndexForLongKey(dig->ts,4,tsRoot,I_TS,off);
    //buildIndexForLongKeyNative(dig->ts,4,tsRoot,I_TS,off);
}

//static inline void buildIndexForSPort(int i,uint32_t j, uint64_t off){
static inline void buildIndexForSPort(PktDigest *dig,int core_id){
    uint64_t off = dig->off + dataBase[core_id];
    buildIndexForShortKey(dig->src_port,&sPortDATable,I_SPORT,off);
}

//static inline void buildIndexForDPort(int i,uint32_t j, uint64_t off){
static inline void buildIndexForDPort(PktDigest *dig,int core_id){
    uint64_t off = dig->off + dataBase[core_id];
    buildIndexForShortKey(dig->dst_port,&dPortDATable,I_DPORT,off);
}

//static inline void buildIndexForProto(int i,uint32_t j, uint64_t off){
static inline void buildIndexForProto(PktDigest *dig,int core_id){
    uint64_t off = dig->off + dataBase[core_id];
    buildIndexForShortKey(dig->proto,&protoDATable,I_PROTO,off);
}

/*
static inline void buildIndex(int i){
    PktDigest *digestBuf = (PktDigest *)digestZones[i];
    if(digestBuf[ready].af == 6){
        buildIndexForSip6(i);
        buildIndexForDip6(i);
    }else{
        buildIndexForSip4(i);
        buildIndexForDip4(i);
    }
    buildIndexForSPort(i);
    buildIndexForDPort(i);
    buildIndexForProto(i);
    buildIndexForTs(i);
    ++ready;
    updateCurrIndexOffset(i);
}
*/

#if 0
static inline void be2le(void *beNum,int width){
    if(width & 0x1) return;

    uint8_t *num = beNum;
    for(int i = 0; i < width/2; ++i){
        uint8_t tmp = num[width-1-i];
        num[width-1-i] = num[i];
        num[i] = tmp;
    }
}

static inline void genIndexArr(int i){
    PktDigest *digestBuf = (PktDigest *)digestZones[i];

    srcPortIndexArr[realPktNum].key = digestBuf[ready].src_port;
    srcPortIndexArr[realPktNum].pos = currIndexOffset;
    dstPortIndexArr[realPktNum].key = digestBuf[ready].dst_port;
    dstPortIndexArr[realPktNum].pos = currIndexOffset;

    protoIndexArr[realPktNum].key = digestBuf[ready].proto;
    protoIndexArr[realPktNum].pos = currIndexOffset;

    tsIndexArr[realPktNum].key = *(uint32_t *)(void *)(digestBuf[ready].ts);
    tsIndexArr[realPktNum].pos = currIndexOffset;
    be2le(&(tsIndexArr[realPktNum].key),4);

    if(digestBuf[ready].af == 6){
        srcIp6IndexArr[realPktNum].key = *(uint128_t *)(void *)(digestBuf[ready].src_ip.ip6);
        srcIp6IndexArr[realPktNum].pos = currIndexOffset;
        dstIp6IndexArr[realPktNum].key = *(uint128_t *)(void *)(digestBuf[ready].dst_ip.ip6);
        dstIp6IndexArr[realPktNum].pos = currIndexOffset;
        be2le(&(srcIp6IndexArr[realPktNum].key),TDMS_IPV6_LEN);
        be2le(&(dstIp6IndexArr[realPktNum].key),TDMS_IPV6_LEN);
        ipv6PktNum++;
    }else{
        srcIpIndexArr[realPktNum].key = *(uint32_t *)(void *)(digestBuf[ready].src_ip.ip4);
        srcIpIndexArr[realPktNum].pos = currIndexOffset;
        dstIpIndexArr[realPktNum].key = *(uint32_t *)(void *)(digestBuf[ready].dst_ip.ip4);
        dstIpIndexArr[realPktNum].pos = currIndexOffset;
        be2le(&(srcIpIndexArr[realPktNum].key),TDMS_IPV4_LEN);
        be2le(&(dstIpIndexArr[realPktNum].key),TDMS_IPV4_LEN);
        ipv4PktNum++;
    }

    ++realPktNum;
    ++ready;
    updateCurrIndexOffset(i);

}

static inline int compK16V(const void *a,const void *b){
    K16V ele1 = *(pK16V)a, ele2 = *(pK16V)b;
    if(ele1.key < ele2.key) return -1;
    else if(ele1.key == ele2.key) return 0;
    else return 1;
}

static inline int compK32V(const void *a,const void *b){
    K32V ele1 = *(pK32V)a, ele2 = *(pK32V)b;
    if(ele1.key < ele2.key) return -1;
    else if(ele1.key == ele2.key) return 0;
    else return 1;
}


static inline int compK128V(const void *a,const void *b){
    K128V ele1 = *(pK128V)a, ele2 = *(pK128V)b;
    if(ele1.key < ele2.key) return -1;
    else if(ele1.key == ele2.key) return 0;
    else return 1;
}
#endif

#if 0
//implementation of SAA index
void index_process(void){
    for(int i = 0; i < core_num; ++i){
        while(ready < avais[i]){
            genIndexArr(i);
            //buildIndex(i);
        }
        ready = 0;
        perCoreOffset = 0;
    }
    qsort(srcPortIndexArr,realPktNum,sizeof(K16V),compK16V);
    qsort(dstPortIndexArr,realPktNum,sizeof(K16V),compK16V);

    qsort(protoIndexArr,realPktNum,sizeof(K16V),compK16V);

    qsort(tsIndexArr,realPktNum,sizeof(K32V),compK32V);

    qsort(srcIpIndexArr,ipv4PktNum,sizeof(K32V),compK32V);
    qsort(dstIpIndexArr,ipv4PktNum,sizeof(K32V),compK32V);

    qsort(srcIp6IndexArr,ipv6PktNum,sizeof(K128V),compK128V);
    qsort(dstIp6IndexArr,ipv6PktNum,sizeof(K128V),compK128V);
}
#endif

#if 1
void *index_thread(void *args){
    IndexCate cate = *(IndexCate *)args;
    cpu_set_t mask;
    CPU_ZERO(&mask);

    CPU_SET(cate,&mask);
    
    if(-1 == pthread_setaffinity_np(pthread_self(),sizeof(mask),&mask)){
        log(ERROR,"fail to bind core");
    }

    for(int i = 0; i < core_num; ++i){
        PktDigest *digestBuf = (PktDigest *)digestZones[i];
        for(int j = 0; j < avais[i]; ++j){
            indexFuncArr[cate](digestBuf+j,i);
        }
    }

}
#endif

static inline void buildIndex(PktDigest *dig,int core_id){
    if(digestBuf[ready].af == 6){
        buildIndexForSip6(dig,core_id);
        buildIndexForDip6(dig,core_id);
    }else{
        buildIndexForSip4(dig,core_id);
        buildIndexForDip4(dig,core_id);
    }
    buildIndexForSPort(dig,core_id);
    buildIndexForDPort(dig,core_id);
    buildIndexForProto(dig,core_id);
    buildIndexForTs(dig,core_id);
}

//build index with single thread
#if 0
void index_process(void){
    IndexCate sip = I_SIP, sip6 = I_SIP6, sport = I_SPORT, dip = I_DIP, dip6 = I_DIP6, dport = I_DPORT, ts = I_TS, proto = I_PROTO;

    int k = 1;
    for(; k < core_num; ++k) dataBase[k] = dataBase[k-1] + dataPoses[k-1];

    for(int i = 0; i < core_num; ++i){
        PktDigest *digestBuf = (PktDigest *)digestZones[i];
        for(int j = 0; j < avais[i]; ++j){
            buildIndex(digestBuf+j,i);
        }
    }

    currUsedSipTrieNode = currUsedTrieNodeArr[sip];
    currUsedDipTrieNode = currUsedTrieNodeArr[dip];
    currUsedSip6TrieNode = currUsedTrieNodeArr[sip6];
    currUsedDip6TrieNode = currUsedTrieNodeArr[dip6];
    currUsedTsTrieNode = currUsedTrieNodeArr[ts];

    currUsedPosNode = 0;
    currUsedTrieNode = 0;
    currUsedList = 0;
    currUsedListHk = 0;

    for(int i = 0; i < I_NUM; ++i){
        currUsedPosNode += currUsedPosNodeArr[i];
        currUsedTrieNode += currUsedTrieNodeArr[i];
        currUsedList += currUsedListArr[i];
        currUsedListHk += currUsedListHKArr[i];
    }

    currIndexOffset = dataBase[core_num-1] + dataPoses[core_num-1];
}
#endif

//build index with multiple threads
#if 1
void index_process(void){

    int k = 1;
    for(; k < core_num; ++k) dataBase[k] = dataBase[k-1] + dataPoses[k-1];

    indexFuncArr[I_SIP] = buildIndexForSip4;
    indexFuncArr[I_SIP6] = buildIndexForSip6;
    indexFuncArr[I_SPORT] = buildIndexForSPort;
    indexFuncArr[I_DIP] = buildIndexForDip4;
    indexFuncArr[I_DIP6] = buildIndexForDip6;
    indexFuncArr[I_DPORT] = buildIndexForDPort;
    indexFuncArr[I_TS] = buildIndexForTs;
    indexFuncArr[I_PROTO] = buildIndexForProto;

    IndexCate sip = I_SIP, sip6 = I_SIP6, sport = I_SPORT, dip = I_DIP, dip6 = I_DIP6, dport = I_DPORT, ts = I_TS, proto = I_PROTO;

    pthread_t tids[I_NUM];
    pthread_create(&tids[sip],NULL,index_thread,&sip);
    pthread_create(&tids[sip6],NULL,index_thread,&sip6);
    pthread_create(&tids[sport],NULL,index_thread,&sport);
    pthread_create(&tids[dip],NULL,index_thread,&dip);
    pthread_create(&tids[dip6],NULL,index_thread,&dip6);
    pthread_create(&tids[dport],NULL,index_thread,&dport);
    pthread_create(&tids[ts],NULL,index_thread,&ts);
    pthread_create(&tids[proto],NULL,index_thread,&proto);

    for(int i = 0; i < I_NUM; ++i){
        pthread_join(tids[i],NULL);
    }

    currUsedSipTrieNode = currUsedTrieNodeArr[sip];
    currUsedDipTrieNode = currUsedTrieNodeArr[dip];
    currUsedSip6TrieNode = currUsedTrieNodeArr[sip6];
    currUsedDip6TrieNode = currUsedTrieNodeArr[dip6];
    currUsedTsTrieNode = currUsedTrieNodeArr[ts];

    currUsedPosNode = 0;
    currUsedTrieNode = 0;
    currUsedList = 0;
    currUsedListHk = 0;

    for(int i = 0; i < I_NUM; ++i){
        currUsedPosNode += currUsedPosNodeArr[i];
        currUsedTrieNode += currUsedTrieNodeArr[i];
        currUsedList += currUsedListArr[i];
        currUsedListHk += currUsedListHKArr[i];
    }

    currIndexOffset = dataBase[core_num-1] + dataPoses[core_num-1];

}
#endif
