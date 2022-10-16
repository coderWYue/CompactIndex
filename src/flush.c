/*************************************************************************
	> File Name: flush.c
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Mon 07 Oct 2019 01:12:33 AM PDT
 ************************************************************************/

#include "mysql.h"
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include "flush.h"
#include "GetStoragePath.h"
#include "index.h"
#include "log.h"
#include "toolFunc.h"
#include "init.h"
#include "compress.h"
#include "tdms.h"
#include "common.h"

/*
extern MYSQL mysql;    //defined in init
extern uint8_t dbFlag;
extern BYTE dataBuf[];    //defined in capture
*/


int keyLenCounter[16] = {0};
int counted = 0;

static int compress_enable;

char formatTime[128];

char dumped[DUMP_CATE_NUM][MAX_PATH_LENGTH];
uint64_t listStartPoses[DUMP_CATE_NUM];
uint64_t currTrieNodePoses[DUMP_CATE_NUM];
uint64_t currListPoses[DUMP_CATE_NUM];

const uint32_t trieNodeSize = sizeof(TrieNode);

static inline void print_cutline(int n){
    while(n--) putchar('-');
    //printf("\n");
}

static inline void write_cutline(char *buf,int pos,int n){
    for(int i = 0; i < n; i++){
        buf[pos+i] = '-';
    }
}

static void write_log_info(void){
    if(store_strategy == 2){
        log_to_file(log_fp,UPDATE,"%s",formatTime);
    }else log_to_file(log_fp,APPEND,"%s",formatTime);

    char log_info[4096];
    log_info[0] = '+';
    write_cutline(log_info,1,97);
    log_info[98] = '+';
    log_info[99] = '\n';
    sprintf(log_info+100,"| pkts_num   | %-82u |\n",currUsedPosNode/(INDEX_NUM-EXCLUDE_INDEX_NUM));
    sprintf(log_info+200,"| data_size  | %-82lu |\n",currIndexOffset);
    //uint64_t index_size = currUsedTrieNode*sizeof(TrieNode) + \
    //                      currUsedPosNode*sizeof(uint64_t) + \
    //                      sizeof(DirectAddressTable)*2;
    //printf("| index_size | %-32lu |\n",index_size);
    sprintf(log_info+300,"| data_path  | %-82s |\n",dataFilePath);
    log_info[400] = '+';
    write_cutline(log_info,401,97);
    log_info[498] = '+';
    log_info[499] = '\n';
    log_info[500] = 0;

    fprintf(log_fp,"%s",log_info);
}

//free trieNodeBuf
static BYTE* initTrieIndexDump(category cate){
    BYTE *trieNodeBuf;
    currTrieNodePoses[cate-SIP_INDEX] = trieNodeSize;
    if(cate == SIP_INDEX){
        trieNodeBuf = (BYTE *)malloc((currUsedSipTrieNode+1)*trieNodeSize);
        if(NULL == trieNodeBuf){
            log_to_file(log_fp,ERROR,"fail to malloc trieNodeBuf in initTrieIndexDump");
            return NULL;
        }
        listStartPoses[cate-SIP_INDEX] = 8 + currUsedSipTrieNode*trieNodeSize;
    }else if(cate == DIP_INDEX){
        trieNodeBuf = (BYTE *)malloc((currUsedDipTrieNode+1)*trieNodeSize);
        if(NULL == trieNodeBuf){
            log_to_file(log_fp,ERROR,"fail to malloc trieNodeBuf in initTrieIndexDump");
            return NULL;
        }
        listStartPoses[cate-SIP_INDEX] = 8 + currUsedDipTrieNode*trieNodeSize;
    }else if(cate == TS_INDEX){
        trieNodeBuf = (BYTE *)malloc((currUsedTsTrieNode+1)*trieNodeSize);
        if(NULL == trieNodeBuf){
            log_to_file(log_fp,ERROR,"fail to malloc trieNodeBuf in initTrieIndexDump");
            return NULL;
        }
        listStartPoses[cate-SIP_INDEX] = 8 + currUsedTsTrieNode*trieNodeSize;
    }else if(cate == SIP6_INDEX){
        trieNodeBuf = (BYTE *)malloc((currUsedSip6TrieNode+1)*trieNodeSize);
        if(NULL == trieNodeBuf){
            log_to_file(log_fp,ERROR,"fail to malloc trieNodeBuf in initTrieIndexDump");
            return NULL;
        }
        listStartPoses[cate-SIP_INDEX] = 8 + currUsedSip6TrieNode*trieNodeSize;
    }else if(cate == DIP6_INDEX){
        trieNodeBuf = (BYTE *)malloc((currUsedDip6TrieNode+1)*trieNodeSize);
        if(NULL == trieNodeBuf){
            log_to_file(log_fp,ERROR,"fail to malloc trieNodeBuf in initTrieIndexDump");
            return NULL;
        }
        listStartPoses[cate-SIP_INDEX] = 8 + currUsedDip6TrieNode*trieNodeSize;
    }

    currListPoses[cate-SIP_INDEX] = listStartPoses[cate-SIP_INDEX];
    return trieNodeBuf;
}

static inline void rollBack(){
    for(int i = 0; i < DUMP_CATE_NUM; ++i){
        if(strlen(dumped[i]) > 0){
            if(remove(dumped[i]) != 0) 
                log_to_file(log_fp,ERROR,"fail to remove %s in rollBack",dumped[i]);
        }
    }
}

static inline void rollBackExit(int arg){
    for(int i = 0; i < DUMP_CATE_NUM; ++i){
        if(strlen(dumped[i]) > 0){
            if(remove(dumped[i]) != 0) 
                log_to_file(log_fp,ERROR,"fail to remove %s in rollBack",dumped[i]);
        }
    }
    exit(0);
}

static inline void openFileError(const char *fileName){
    log_to_file(log_fp,ERROR,"fail to open %s,cause of this:%s",fileName,strerror(errno));
    rollBack();
    exit(0);
}

static inline void dumpError(int fd){
    close(fd);
    rollBack();
    exit(0);
}

static int64_t write_loop(int fd,const void *buf,uint64_t sz){
    int64_t writed = 0;

    while(writed < sz){
        int64_t thisWr = write(fd,buf+writed,sz-writed);
        if(thisWr == -1) return -1;
        else writed += thisWr;
    }
    return writed;

    return sz;
}

void* dumpDataToFile(void *args){
    int fd = open(dataFilePath,O_WRONLY | O_CREAT | O_TRUNC | O_SYNC,0666);    
    if(fd == -1) openFileError(dataFilePath);
    
    strcpy(dumped[DATA],dataFilePath);

    uint64_t expected = PCAP_FILE_HEAD_SIZE;
    int64_t dumpedBytes = write_loop(fd,dataBuf,expected);
    if(dumpedBytes != PCAP_FILE_HEAD_SIZE){
        close(fd);
        log_to_file(log_fp,ERROR,"write pcap head error in dumpDataToFile,expected:%lu,real:%ld,cause of this:%s",expected,dumpedBytes,strerror(errno));
        rollBack();
        exit(0);
    }

    for(int i = 0; i < core_num; ++i){
        int64_t dumpedBytes = write_loop(fd,dataZones[i],dataPoses[i]);
        if(dumpedBytes != dataPoses[i]){
            close(fd);
            log_to_file(log_fp,ERROR,"write data error in dumpDataToFile,expected:%lu real:%ld,cause of this:%s",dataPoses[i],dumpedBytes,strerror(errno));
            rollBack();
            exit(0);
        }
    }
    close(fd);
}

static inline size_t list2Buf(pListHead li,uint64_t *buf,size_t bufLen){
    size_t i = 0;
    while(li->head && i < bufLen){
        buf[i++] = ((pPktPosNode)li->head)->offset;
        li->head = ((pPktPosNode)li->head)->next;
    }
    return i*8;
}

static int64_t list2FileWithKey(pListHead li, int fd, uint64_t keyBuf[], int len){
    uint64_t buf[DUMP_BUF_SIZE/8+4];
    int64_t nBytes,totalBytes = 0;
    int i;
    for(i = 0; i < len; ++i) buf[i] = keyBuf[i];
    if(!compress_enable)
    {
        nBytes = list2Buf(li,buf+i,DUMP_BUF_SIZE/8+4-len);
        if(write(fd,buf,nBytes+len*8) != nBytes+len*8){
            log_to_file(log_fp,ERROR,"write error in list2File,cause of this:%s",strerror(errno));
            return -1;
        }
        totalBytes += nBytes;

         while((nBytes = list2Buf(li,buf,DUMP_BUF_SIZE/8)) != 0)
    	{

            if(write(fd,buf,nBytes) != nBytes){
                log_to_file(log_fp,ERROR,"write error in list2File,cause of this:%s",strerror(errno));
                return -1;
            }
            totalBytes += nBytes;
        }
    }
    else
    {
        nBytes = compressList2Buf(li,buf+i,DUMP_BUF_SIZE/8+4-len);
        if(write(fd,buf,nBytes+len*8) != nBytes+len*8){
            log_to_file(log_fp,ERROR,"write error in list2File,cause of this:%s",strerror(errno));
            return -1;
        }
        totalBytes += nBytes;

        while((nBytes = compressList2Buf(li,buf,DUMP_BUF_SIZE/8)) != 0)
    	{

            if(write(fd,buf,nBytes) != nBytes){
                log_to_file(log_fp,ERROR,"write error in list2File,cause of this:%s",strerror(errno));
                return -1;
            }
            totalBytes += nBytes;
        }
    }
    return totalBytes;

}

static int64_t list2BigBuf(pListHead li, uint64_t *bigBuf, uint64_t capa){
    int64_t nBytes, totalBytes = 0;
    if(!compress_enable){
        while((nBytes = list2Buf(li,bigBuf,capa)) != 0){
            totalBytes += nBytes;
        }
    }else{
        while((nBytes = compressList2Buf(li,bigBuf,capa)) != 0){
            totalBytes += nBytes;
        }
    }
    return totalBytes;
}

static int64_t list2File(pListHead li, int fd){
    uint64_t buf[DUMP_BUF_SIZE/8];
    int64_t nBytes,totalBytes = 0;
    if(!compress_enable)
    {
         while((nBytes = list2Buf(li,buf,DUMP_BUF_SIZE/8)) != 0)
    	{

            if(write(fd,buf,nBytes) != nBytes){
                log_to_file(log_fp,ERROR,"write error in list2File,cause of this:%s",strerror(errno));
                return -1;
            }
            totalBytes += nBytes;
        }
    }
    else
    {
        while((nBytes = compressList2Buf(li,buf,DUMP_BUF_SIZE/8)) != 0)
    	{

            if(write(fd,buf,nBytes) != nBytes){
                log_to_file(log_fp,ERROR,"write error in list2File,cause of this:%s",strerror(errno));
                return -1;
            }
            totalBytes += nBytes;
        }
    }
    return totalBytes;
}

//not complete,need to record pkts per port
int dumpShortIndex(pDirectAddressTable pDATable,int fd,int flag){
    uint64_t offset;
    if((offset = lseek(fd,sizeof(DirectAddressTable),SEEK_SET)) == -1){
        log_to_file(log_fp,ERROR,"fail to lseek in dumpShortIndex");
        return -1;
    }

    uint64_t *bigBuf = (uint64_t *)malloc(currUsedPosNode/(INDEX_NUM-EXCLUDE_INDEX_NUM)*sizeof(uint64_t)*2);
    if(NULL == bigBuf){
        log_to_file(log_fp,ERROR,"fail to allocate big buf for offset in dumpShortIndex");
        return -1;
    }

    uint64_t capa = currUsedPosNode/(INDEX_NUM-EXCLUDE_INDEX_NUM), used = 0;

    for(int i = 0; i < DA_TABLE_SIZE; ++i){
        if(pDATable->table[i]){
            pListHead li = (pListHead)pDATable->table[i];
            int64_t totalBytes = list2BigBuf(li,bigBuf+used,capa-used);

            pDATable->table[i] = (void *)(offset | totalBytes << 32);

            /*
            if(flag && i == 80){
                uint64_t *buf = (uint64_t *)calloc(totalBytes/8+2,sizeof(uint64_t));
                buf[0] = totalBytes/8;
                buf[1] = bigBuf[used+totalBytes/8-1];
                memcpy(buf+2,bigBuf+used,totalBytes);

                int fd = open("IL80",O_CREAT | O_TRUNC | O_WRONLY,0777);
                write(fd,buf,totalBytes+16);
                close(fd);
            }
            */

            offset += totalBytes;
            used += totalBytes/8;
        }
    }

    uint64_t expected = offset - sizeof(DirectAddressTable);
    int64_t dumped = write_loop(fd,bigBuf,expected);

    if(dumped != expected){
        log_to_file(log_fp,ERROR,"write offset error in dumpShortIndex,expected:%lu,real:%ld,cause of this:%s",expected,dumped,strerror(errno));
        return -1;
    }

    free(bigBuf);

    if(lseek(fd,0,SEEK_SET) == -1){
        log_to_file(log_fp,ERROR,"fail to lseek in dumpShortIndex");
        return -1;
    }

    expected = sizeof(DirectAddressTable);
    dumped = write_loop(fd,pDATable,expected);

    if(dumped != expected){
        log_to_file(log_fp,ERROR,"write DATable error in dumpShortIndex,expected:%lu,real:%ld,cause of this:%s",expected,dumped,strerror(errno));
        return -1;
    }

    return 0;
}

void* dumpShortIndexToFile(void *args){
    int flag = 0;
    category cate = *(category *)args;
    pDirectAddressTable pDATable;
    char *filePath;
    if(cate == SPORT_INDEX){
        filePath = sportIndexFilePath;
        pDATable = &sPortDATable;
    }else if(cate == DPORT_INDEX){
        filePath = dportIndexFilePath;
        pDATable = &dPortDATable;
        flag = 1;
    }else if(cate == PROTO_INDEX){
        filePath = protoIndexFilePath;
        pDATable = &protoDATable;
    }

    int fd = open(filePath, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC,0666);
    if(fd == -1) openFileError(filePath);
    strcpy(dumped[cate],filePath);

    int r = dumpShortIndex(pDATable,fd,flag);
    if(r == -1) dumpError(fd);
    close(fd);
}

//not complete,need to record pkts per port
void dumpTrie(pTrieNode pCurr,int level,int maxHeight,int fd,void *trieNodeBuf,category cate,uint64_t* bigBuf){
    for(int i = 0; i < SEGMENT_SIZE; ++i){
        void *pNext = pCurr->pArr[i];
        if(pNext != NULL){
            if(isTrieNode(pNext) || isExtTrieNodeCommVersion(pNext)){
                dumpTrie((pTrieNode)pNext,level+1,maxHeight,fd,trieNodeBuf,cate,bigBuf);
                pCurr->pArr[i] = (void *)(currTrieNodePoses[cate-SIP_INDEX] - trieNodeSize + 8);
            }else{

                if(!counted){
                    /*
                    if(level == 1) keyLenCounter[0]++;
                    else if(level == 2) keyLenCounter[1]++;
                    else if(level == 3) keyLenCounter[2]++;
                    else if(level == 4) keyLenCounter[3]++;
                    */
                    keyLenCounter[level-1]++;
                }

                if(level == maxHeight){
                    uint64_t tot = 16*currUsedPosNode/(INDEX_NUM-EXCLUDE_INDEX_NUM), used = (currListPoses[cate-SIP_INDEX] - listStartPoses[cate-SIP_INDEX])/8;
                    int64_t totalBytes = list2BigBuf((pListHead)pNext,bigBuf+used,tot-used);

                    pCurr->pArr[i] = (void *)(currListPoses[cate-SIP_INDEX] | totalBytes << 32);
                    currListPoses[cate-SIP_INDEX] += totalBytes;
                }else{
                    uint64_t tot = 16*currUsedPosNode/(INDEX_NUM-EXCLUDE_INDEX_NUM), used = (currListPoses[cate-SIP_INDEX] - listStartPoses[cate-SIP_INDEX])/8;
                    int keyLen = ((maxHeight%8==0)?maxHeight:(maxHeight+8-maxHeight%8));
                    bigBuf[used++] = 0xffffffff00000000 | keyLen;
                    memcpy(bigBuf+used,((pListHeadHasKey4)pNext)->key,maxHeight);
                    used += keyLen/8;
                    
                    currListPoses[cate-SIP_INDEX] += 8;
                    
                    pListHead pli= &(((pListHeadHasKey4)pNext)->li);
                    //int64_t totalBytes = list2BigBuf((pListHead)pNext,bigBuf+used,tot-used);
                    int64_t totalBytes = list2BigBuf(pli,bigBuf+used,tot-used);

                    pCurr->pArr[i] = (void *)(currListPoses[cate-SIP_INDEX] | totalBytes << 32);
                    currListPoses[cate-SIP_INDEX] += (totalBytes + keyLen);
                }
            }
        }
    }
    memcpy(trieNodeBuf+currTrieNodePoses[cate-SIP_INDEX],pCurr,trieNodeSize);
    currTrieNodePoses[cate-SIP_INDEX] += trieNodeSize;
}

void* dumpLongIndexToFile(void *args){
    category cate = *(category *)args;
    pTrieNode root;
    char *filePath;
    if(cate == SIP_INDEX){
        filePath = sipIndexFilePath;
        root = sipRoot;
    }else if(cate == DIP_INDEX){
        filePath = dipIndexFilePath;
        root = dipRoot;
    }else if(cate == TS_INDEX){
        filePath = tsIndexFilePath;
        root = tsRoot;
    }else if(cate == SIP6_INDEX){
        filePath = sip6IndexFilePath;
        root = sip6Root;
    }else if(cate == DIP6_INDEX){
        filePath = dip6IndexFilePath;
        root = dip6Root;
    }

    int fd = open(filePath, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC,0666);
    if(fd == -1) openFileError(filePath);
    strcpy(dumped[cate],filePath);

    BYTE *trieNodeBuf = initTrieIndexDump(cate);
    if(NULL == trieNodeBuf) dumpError(fd);
    if(lseek(fd,listStartPoses[cate-SIP_INDEX],SEEK_SET) == -1){
        log_to_file(log_fp,ERROR,"lseek error in dumpLongIndexToFile");
        dumpError(fd);
    }

    uint64_t *bigBuf = (uint64_t *)malloc(currUsedPosNode/(INDEX_NUM-EXCLUDE_INDEX_NUM)*sizeof(uint64_t)*16);
    if(NULL == bigBuf){
        log_to_file(log_fp,ERROR,"fail to allocate big buf for offset in dumpLongIndexToFile");
        dumpError(fd);
    }

    if(cate == SIP_INDEX || cate == DIP_INDEX || cate == TS_INDEX) dumpTrie(root,1,4,fd,trieNodeBuf,cate,bigBuf);
    else if(cate == SIP6_INDEX || cate == DIP6_INDEX) dumpTrie(root,1,16,fd,trieNodeBuf,cate,bigBuf);

    uint64_t expected = currListPoses[cate-SIP_INDEX] - listStartPoses[cate-SIP_INDEX];
    int64_t dumped = write_loop(fd,bigBuf,expected);

    if(dumped != expected){
        log_to_file(log_fp,ERROR,"write offset error in dumpLongIndexToFile,index:%s,expected:%lu,real:%ld,cause of this:%s",index_category_str[cate-1],expected,dumped,strerror(errno));
        dumpError(fd);
    }

    free(bigBuf);

    memcpy(trieNodeBuf,trieNodeBuf+listStartPoses[cate-SIP_INDEX]-8,trieNodeSize);

    if(lseek(fd,0,SEEK_SET) == -1){
        log_to_file(log_fp,ERROR,"lseek error in dumpLongIndexToFile");
        dumpError(fd);
    }
    if((dumped = write(fd,&listStartPoses[cate-SIP_INDEX],8)) != 8){
        log_to_file(log_fp,ERROR,"write bound error in dumpLongIndexToFile,index:%s,expected:8,real:%ld,cause of this:%s",index_category_str[cate-1],dumped,strerror(errno));
        dumpError(fd);
    }

    expected = listStartPoses[cate-SIP_INDEX] - 8;
    dumped = write_loop(fd,trieNodeBuf,expected);

    if(dumped != expected){
        log_to_file(log_fp,ERROR,"write trie error in dumpLongIndexToFile,index:%s,expected:%lu,real:%ld,cause of this:%s",index_category_str[cate-1],expected,dumped,strerror(errno));
        dumpError(fd);
    }

    free(trieNodeBuf);
    trieNodeBuf = NULL;
    close(fd);
}

// implementation of SAA dump
void splitIndexArr(void *indexArr,void *keyArr,uint64_t *offArr,category cate,uint32_t len){
    if(cate == SPORT_INDEX || cate == DPORT_INDEX || cate == PROTO_INDEX){
        pK16V k16vArr = indexArr;
        uint16_t *k16Arr = keyArr;
        for(int i = 0; i < len; ++i){
            k16Arr[i] = k16vArr[i].key;
            offArr[i] = k16vArr[i].pos;
        }
    }else if(cate == SIP_INDEX || cate == DIP_INDEX || cate == TS_INDEX){
        pK32V k32vArr = indexArr;
        uint32_t *k32Arr = keyArr;
        for(int i = 0; i < len; ++i){
            k32Arr[i] = k32vArr[i].key;
            offArr[i] = k32vArr[i].pos;
        }
    }else{
        pK128V k128vArr = indexArr;
        uint128_t *k128Arr = keyArr;
        for(int i = 0; i < len; ++i){
            k128Arr[i] = k128vArr[i].key;
            offArr[i] = k128vArr[i].pos;
        }
    }
}

void *dumpIndexArrToFile(void *args){
    IndexFileHeader indFileHdr;
    char *path;
    void *indexArr;
    void *keyArr;
    void *offArr;
    int64_t writed,expected;

    category cate = *(category *)args;

    if(cate == SIP_INDEX){
        path = sipIndexFilePath;
        indFileHdr.keyLen = 4;
        indFileHdr.eleNum = ipv4PktNum;
        indexArr = srcIpIndexArr;
    }else if(cate == DIP_INDEX){
        path = dipIndexFilePath;
        indFileHdr.keyLen = 4;
        indFileHdr.eleNum = ipv4PktNum;
        indexArr = dstIpIndexArr;
    }else if(cate == TS_INDEX){
        path = tsIndexFilePath;
        indFileHdr.keyLen = 4;
        indFileHdr.eleNum = realPktNum;
        indexArr = tsIndexArr;
    }else if(cate == SIP6_INDEX){
        path = sip6IndexFilePath;
        indFileHdr.keyLen = 16;
        indFileHdr.eleNum = ipv6PktNum;
        indexArr = srcIp6IndexArr;
    }else if(cate == DIP6_INDEX){
        path = dip6IndexFilePath;
        indFileHdr.keyLen = 16;
        indFileHdr.eleNum = ipv6PktNum;
        indexArr = dstIp6IndexArr;
    }else if(cate == PROTO_INDEX){
        path = protoIndexFilePath;
        indFileHdr.keyLen = 2;
        indFileHdr.eleNum = realPktNum;
        indexArr = protoIndexArr;
    }else if(cate == SPORT_INDEX){
        path = sportIndexFilePath;
        indFileHdr.keyLen = 2;
        indFileHdr.eleNum = realPktNum;
        indexArr = srcPortIndexArr;
    }else if(cate == DPORT_INDEX){
        path = dportIndexFilePath;
        indFileHdr.keyLen = 2;
        indFileHdr.eleNum = realPktNum;
        indexArr = dstPortIndexArr;
    }

    keyArr = malloc(indFileHdr.keyLen*indFileHdr.eleNum);
    offArr = malloc(sizeof(uint64_t)*indFileHdr.eleNum);

    splitIndexArr(indexArr,keyArr,offArr,cate,indFileHdr.eleNum);

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC,0666);
    if(fd == -1) openFileError(path);
    strcpy(dumped[cate],path);

    if((writed = write(fd,&indFileHdr,sizeof(IndexFileHeader))) != sizeof(IndexFileHeader)){
        log_to_file(log_fp,ERROR,"write header error,index:%s,expected:8,real:%ld,cause of this:%s",index_category_str[cate-1],writed,strerror(errno));
        dumpError(fd);
    }

    expected = indFileHdr.keyLen * indFileHdr.eleNum;
    writed = write_loop(fd,keyArr,expected);
    if(writed != expected){
        log_to_file(log_fp,ERROR,"write key error,index:%s,expected:%ld,real:%ld,cause of this:%s",index_category_str[cate-1],expected,writed,strerror(errno));
        dumpError(fd);
    }

    expected = sizeof(uint64_t) * indFileHdr.eleNum;
    writed = write_loop(fd,offArr,expected);
    if(writed != expected){
        log_to_file(log_fp,ERROR,"write off error,index:%s,expected:%ld,real:%ld,cause of this:%s",index_category_str[cate-1],expected,writed,strerror(errno));
        dumpError(fd);
    }

    free(keyArr);
    free(offArr);

    close(fd);
}

static inline const char* getFileName(const char *path){
    const char *fileName = strrchr(path,'/');
    if(fileName) fileName++;
    else fileName = path;
    return fileName;
}

static int del_oldest_records_in_db(int n){
    char query[512];
    char nStr[16];
    sprintf(query,"DELETE FROM storage_info WHERE 1=1 ORDER BY timestamp limit %s",decimalToStr(n,nStr));
    if(mysql_real_query(&mysql,query,(unsigned long)strlen(query))){
        return -1;
    }
    return 0;
}

void writeInfoToDB(){
    char pkts_num[16];
    char query[512];
    seconds_to_std_date(flush_ts,formatTime,128);
    //decimalToStr(realPktNum,pkts_num);
    decimalToStr(currUsedPosNode/(INDEX_NUM-EXCLUDE_INDEX_NUM),pkts_num);
    //decimalToStr(realPktNum,pkts_num);
    const char *dataFileName = dataFilePath+strlen(data_storage_dir);
    int indexDirLen = strlen(index_storage_dir);
    const char *sipIndexFileName = sipIndexFilePath + indexDirLen;
    const char *dipIndexFileName = dipIndexFilePath + indexDirLen;
    const char *sip6IndexFileName = sip6IndexFilePath + indexDirLen;
    const char *dip6IndexFileName = dip6IndexFilePath + indexDirLen;
    const char *sportIndexFileName = sportIndexFilePath + indexDirLen;
    const char *dportIndexFileName = dportIndexFilePath + indexDirLen;
    const char *protoIndexFileName = protoIndexFilePath + indexDirLen;
    const char *tsIndexFileName = tsIndexFilePath + indexDirLen;
    if(delNum != 0 && del_oldest_records_in_db(delNum) == -1){
        log_to_file(log_fp,ERROR,"fail to delete %d expired records in db!",delNum);
    }
    sprintf(query,"INSERT INTO storage_info(data_file,pkts_num,src_ip_index_file,src_ip6_index_file,src_port_index_file,dst_ip_index_file,\
            dst_ip6_index_file,dst_port_index_file,proto_index_file,ts_index_file,timestamp)values('%s','%s','%s','%s','%s','%s','%s','%s','%s','%s','%s')",\
            dataFileName,pkts_num,sipIndexFileName,sip6IndexFileName,sportIndexFileName,dipIndexFileName,dip6IndexFileName,dportIndexFileName,\
            protoIndexFileName,tsIndexFileName,formatTime);
    if(mysql_real_query(&mysql,query,(unsigned long)strlen(query))){
	log_to_file(log_fp,ERROR,"fail to write info to DB.");
        log_to_file(log_fp,DEBUG,"dataFilePath:%s\n\
                pkts_num:%s\n\
                sipIndexFilePath:%s\n\
                dipIndexFilePath:%s\n\
                sportIndexFilePath:%s\n\
                dportIndexFilePath:%s\n\
                protoIndexFilePath:%s\n\
                tsIndexFilePath:%s\n\
                timestamp:%s\
                ",dataFilePath,pkts_num,sipIndexFilePath,dipIndexFilePath,sportIndexFilePath,dportIndexFilePath,protoIndexFilePath,tsIndexFilePath,formatTime);
    }
}

void persistence(){
    getStoragePath();
    category sport = SPORT_INDEX, dport = DPORT_INDEX, proto = PROTO_INDEX, sip = SIP_INDEX, dip = DIP_INDEX, sip6 = SIP6_INDEX, dip6 = DIP6_INDEX, ts = TS_INDEX;
    pthread_t pids[INDEX_NUM+1];

    struct timeval start,end;
    gettimeofday(&start,NULL);

    /* dump with single thread
    dumpDataToFile(NULL);
    //free(dataBuf);
    //free(digestBuf);
    dumpShortIndexToFile(&sport);
    dumpShortIndexToFile(&dport);
    dumpShortIndexToFile(&proto);
    dumpLongIndexToFile(&sip6);
    counted = 1;
    dumpLongIndexToFile(&dip6);
    dumpLongIndexToFile(&sip);
    dumpLongIndexToFile(&dip);
    dumpLongIndexToFile(&ts);
    */
    pthread_create(&pids[0],NULL,dumpDataToFile,NULL);
    pthread_create(&pids[1],NULL,dumpShortIndexToFile,&sport);
    pthread_create(&pids[2],NULL,dumpShortIndexToFile,&dport);
    pthread_create(&pids[3],NULL,dumpShortIndexToFile,&proto);
    pthread_create(&pids[4],NULL,dumpLongIndexToFile,&sip);
    pthread_create(&pids[5],NULL,dumpLongIndexToFile,&dip);
    pthread_create(&pids[6],NULL,dumpLongIndexToFile,&sip6);
    pthread_create(&pids[7],NULL,dumpLongIndexToFile,&dip6);
    pthread_create(&pids[8],NULL,dumpLongIndexToFile,&ts);
    for(int i = 0; i <= INDEX_NUM; ++i){
        pthread_join(pids[i],NULL);
    }

    /*
     * SAA implementation
    pthread_create(&pids[0],NULL,dumpDataToFile,NULL);
    pthread_create(&pids[1],NULL,dumpIndexArrToFile,&sport);
    pthread_create(&pids[2],NULL,dumpIndexArrToFile,&dport);
    pthread_create(&pids[3],NULL,dumpIndexArrToFile,&proto);
    pthread_create(&pids[4],NULL,dumpIndexArrToFile,&sip);
    pthread_create(&pids[5],NULL,dumpIndexArrToFile,&dip);
    pthread_create(&pids[6],NULL,dumpIndexArrToFile,&sip6);
    pthread_create(&pids[7],NULL,dumpIndexArrToFile,&dip6);
    pthread_create(&pids[8],NULL,dumpIndexArrToFile,&ts);
    for(int i = 0; i <= INDEX_NUM; ++i){
        pthread_join(pids[i],NULL);
    }
    */

    gettimeofday(&end,NULL);
    uint64_t dura = (end.tv_sec - start.tv_sec)*1000000 + end.tv_usec - start.tv_usec;
    log_to_file(log_fp,INFO,"flush cost %lu us",dura);
    
    writeInfoToDB();
}

void flush(int cps_flag){
    //atexit(persistence);
    signal(SIGTERM,rollBackExit);
    compress_enable = cps_flag;

    struct timeval start,end;
    gettimeofday(&start,NULL);
    index_process();
    gettimeofday(&end,NULL);
    uint64_t dura = (end.tv_sec - start.tv_sec)*1000000 + end.tv_usec - start.tv_usec;
    log(INFO,"index %lu pkts cost %lu us",currUsedPosNode/(INDEX_NUM-EXCLUDE_INDEX_NUM), dura);

    persistence();


    log(INFO,"flush OK!\n");
    write_log_info();
    //_exit(0);    //avoid to invoke persistence again()
}
