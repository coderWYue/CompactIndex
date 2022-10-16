/*************************************************************************
	> File Name: fileOperation.c
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Wed 20 Nov 2019 06:20:33 PM PST
 ************************************************************************/

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<lz4.h>
#include"fileOperation.h"
#include"retrieve.h"

#include"toolFunc.h"
#include"common.h"
#include"log.h"
#include"tdms.h"

#define B_SEARCH(type) B_SEARCH_##type
#define SEARCH_POS(type) SEARCH_POS_##type

#define B_SEARCH_IMPL(type) \
        int B_SEARCH(type)(const type *nums,int len,type target,int lower){ \
            int l = 0, r = len - 1, ans = len; \
            while(l <= r){ \
                int m = (l + r) / 2; \
                if(nums[m] > target || (lower && nums[m] >= target)){ \
                    r = m - 1; \
                    ans = m; \
                }else{ \
                    l = m + 1; \
                } \
            } \
            return ans; \
        }

#define SEARCH_POS_IMPL(type) \
        int SEARCH_POS(type)(const type *nums,int len,type target){ \
            int l = 0, r = len - 1; \
            while(l <= r){ \
                int m = (l + r) / 2; \
                if(nums[m] > target){ \
                    r = m - 1; \
                }else if(nums[m] == target){ \
                    return m; \
                }else{ \
                    l = m + 1; \
                } \
            } \
            return l; \
        }

B_SEARCH_IMPL(uint16_t)
B_SEARCH_IMPL(uint32_t)
B_SEARCH_IMPL(uint64_t)
B_SEARCH_IMPL(uint128_t)
SEARCH_POS_IMPL(uint16_t)
SEARCH_POS_IMPL(uint32_t)
SEARCH_POS_IMPL(uint64_t)
SEARCH_POS_IMPL(uint128_t)

byte compressedInvertedListBuf[COMPRESSED_BUF_SIZE];
uint64_t posArr[BUF_ELE_NUM];
uint64_t *matchInvertList;

static pcapFileHead hdr = {
    .magic = {0xd4,0xc3,0xb2,0xa1},
    .major = 2,
    .minor = 4,
    .thiszone = 0,
    .sigfigs = 0,
    .snaplen = 0x0000ffff,
    .linktype = 1
};

static byte dataBuff[DATA_BUF_SIZE];
static byte cpsedBuf[DATA_BUF_SIZE-DATA_THRESHOLD];

void getPath(char *path,const char *fileName,FileCategory cate){
    switch(cate){
        case DATA:
            strcpy(path,data_storage_dir);
            break;
        case INDEX:
            strcpy(path,index_storage_dir);
            break;
        case RESULT:
            strcpy(path,result_storage_dir);
            break;
        default:
            log_to_buf(err_msg+strlen(err_msg),ERROR,"unknown file category!");
            log(ERROR,"unknown file category!");
            return;
    }
    strcat(path,fileName);
}

static inline int checkLseek(const char *path,off_t off){
    if(off == -1){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"lseek failed in %s",path);
        log(ERROR,"lseek failed in %s",path);
        return -1;
    }
    return 0;
}

static inline int checkOpen(const char *path,int fd){
    if(fd == -1){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to open %s",path);
        log(ERROR,"fail to open %s",path);
        return -1;
    }
    return 0;
}

static inline int checkRead(const char *path,ssize_t sz,int64_t expected){
    if(sz != expected){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"read error in %s",path);
        log(ERROR,"read error in %s",path);
        return -1;
    }
    return 0;
}


Range searchRange(const void *arr,int len,const void *target,KeyType type){
    Range res = {-1,-1};
    int lIdx, rIdx;

    if(type == key16){
        uint16_t t = *(uint16_t *)target;
        uint16_t *nums = arr;

        lIdx = B_SEARCH(uint16_t)(nums,len,t,1);
        rIdx = B_SEARCH(uint16_t)(nums,len,t,0) - 1;
        if(lIdx <= rIdx && rIdx < len && t == nums[lIdx] && t == nums[rIdx]){
            res.rInd = rIdx;
            res.lInd = lIdx;
        }
    }else if(type == key32){
        uint32_t t = *(uint32_t *)target;
        uint32_t *nums = arr;

        lIdx = B_SEARCH(uint32_t)(nums,len,t,1);
        rIdx = B_SEARCH(uint32_t)(nums,len,t,0) - 1;
        if(lIdx <= rIdx && rIdx < len && t == nums[lIdx] && t == nums[rIdx]){
            res.rInd = rIdx;
            res.lInd = lIdx;
        }
    }else if(type == key64){
        uint64_t t = *(uint64_t *)target;
        uint64_t *nums = arr;

        lIdx = B_SEARCH(uint64_t)(nums,len,t,1);
        rIdx = B_SEARCH(uint64_t)(nums,len,t,0) - 1;
        if(lIdx <= rIdx && rIdx < len && t == nums[lIdx] && t == nums[rIdx]){
            res.rInd = rIdx;
            res.lInd = lIdx;
        }
    }else if(type == key128){
        uint128_t t = *(uint128_t *)target;
        uint128_t *nums = arr;

        lIdx = B_SEARCH(uint128_t)(nums,len,t,1);
        rIdx = B_SEARCH(uint128_t)(nums,len,t,0) - 1;
        if(lIdx <= rIdx && rIdx < len && t == nums[lIdx] && t == nums[rIdx]){
            res.rInd = rIdx;
            res.lInd = lIdx;
        }
    }

    return res;
}

static int64_t readIL(const char *path,int fd,uint64_t start,uint64_t matchSz,uint64_t *buf){
    off_t r = lseek(fd,start,SEEK_CUR);
    if(checkLseek(path,r) == -1) return -1;

    ssize_t readed = read(fd,buf,matchSz);
    if(checkRead(path,readed,matchSz) == -1) return -1;

    return matchSz;
}

int getILFromIndexArr(const void *path,const void *target,int dir){
    IndexFileHeader hdr;
    uint64_t start = 0, matchSz = 0;
    int matched = 0;
    int pos = -1;
    Range r = {-1,-1};
    
    int fd = open(path,O_RDONLY);
    if(checkOpen(path,fd) == -1) return -1;

    ssize_t readed;
    readed = read(fd,&hdr,sizeof(IndexFileHeader));
    if(checkRead(path,readed,sizeof(IndexFileHeader)) == -1) return -1;

    if(hdr.keyLen == key16){
        uint64_t keySz = key16*(uint64_t)hdr.eleNum;

        uint16_t *keyBuf = (uint16_t *)malloc(keySz);

        readed = read(fd,keyBuf,keySz);
        if(checkRead(path,readed,keySz) == -1) return -1;

        r = searchRange(keyBuf,hdr.eleNum,target,key16);

        if(r.lInd == -1 && dir){
            pos = SEARCH_POS(uint16_t)(keyBuf,hdr.eleNum,*(uint16_t *)target);
        }
    }else if(hdr.keyLen == key32){
        uint64_t keySz = key32*(uint64_t)hdr.eleNum;

        uint32_t *keyBuf = (uint32_t *)malloc(keySz);

        readed = read(fd,keyBuf,keySz);
        if(checkRead(path,readed,keySz) == -1) return -1;

        r = searchRange(keyBuf,hdr.eleNum,target,key32);

        if(r.lInd == -1 && dir){
            pos = SEARCH_POS(uint32_t)(keyBuf,hdr.eleNum,*(uint16_t *)target);
        }
    }else if(hdr.keyLen == key64){
        uint64_t keySz = key64*(uint64_t)hdr.eleNum;

        uint64_t *keyBuf = (uint64_t *)malloc(keySz);

        readed = read(fd,keyBuf,keySz);
        if(checkRead(path,readed,keySz) == -1) return -1;

        r = searchRange(keyBuf,hdr.eleNum,target,key64);

        if(r.lInd == -1 && dir){
            pos = SEARCH_POS(uint64_t)(keyBuf,hdr.eleNum,*(uint16_t *)target);
        }
    }else if(hdr.keyLen == key128){
        uint64_t keySz = key128*(uint64_t)hdr.eleNum;

        uint128_t *keyBuf = (uint128_t *)malloc(keySz);

        readed = read(fd,keyBuf,keySz);
        if(checkRead(path,readed,keySz) == -1) return -1;

        r = searchRange(keyBuf,hdr.eleNum,target,key128);

        if(r.lInd == -1 && dir){
            pos = SEARCH_POS(uint128_t)(keyBuf,hdr.eleNum,*(uint16_t *)target);
        }
    }

    if(r.lInd != -1){
        if(dir == 0){
            start = (uint64_t)r.lInd*8;
            matchSz = (uint64_t)(r.rInd - r.lInd + 1)*8;
        }else if(dir == 1){
            start = (uint64_t)r.lInd*8;
            matchSz = (uint64_t)(hdr.eleNum - r.lInd)*8;    
        }else if(dir == -1){
            start = 0;
            matchSz = (uint64_t)(r.rInd + 1)*8;
        }
    }else if(pos != -1){
        if(dir == 1 && pos < hdr.eleNum){
            start = (uint64_t)pos * 8;
            matchSz = (uint64_t)(hdr.eleNum - pos) * 8;
        }else if(dir == -1 && pos > 0){
            start = 0;
            matchSz = (uint64_t)pos * 8;
        }
    }

    if(matchSz > BUF_ELE_NUM*8){
        matchInvertList = (uint64_t *)malloc(matchSz);
    }else matchInvertList = posArr;

    readed = readIL(path,fd,start,matchSz,matchInvertList);
    if(readed == -1) return -1;

    close(fd);
    return readed / 8;
}

int getIntervalILFromIndexArr(const char *path,const void *lo, const void *hi){
    IndexFileHeader hdr;
    uint64_t start = 0, matchSz = 0;
    int pos = -1;
    Range r = {-1,-1};
    
    int fd = open(path,O_RDONLY);
    if(checkOpen(path,fd) == -1) return -1;

    ssize_t readed;
    readed = read(fd,&hdr,sizeof(IndexFileHeader));
    if(checkRead(path,readed,sizeof(IndexFileHeader)) == -1) return -1;
    
    if(hdr.keyLen == key16){
        uint64_t keySz = key16*(uint64_t)hdr.eleNum;

        uint16_t *keyBuf = (uint16_t *)malloc(keySz);

        readed = read(fd,keyBuf,keySz);
        if(checkRead(path,readed,keySz) == -1) return -1;

        r = searchRange(keyBuf,hdr.eleNum,lo,key16);

        if(r.lInd == -1){
            pos = SEARCH_POS(uint16_t)(keyBuf,hdr.eleNum,*(uint16_t *)lo);
            if(pos < hdr.eleNum) start = (uint64_t)pos*8;
        }else start = (uint64_t)r.lInd*8;

        r = searchRange(keyBuf,hdr.eleNum,hi,key16);

        if(r.lInd == -1){
            pos = SEARCH_POS(uint16_t)(keyBuf,hdr.eleNum,*(uint16_t *)hi);
            if(start/8 < pos) matchSz = pos*8 - start;
        }else{
            if(start/8 <= r.rInd) matchSz = (uint64_t)(r.rInd+1)*8 - start;
        }
    }else if(hdr.keyLen == key32){
        uint64_t keySz = key32*(uint64_t)hdr.eleNum;

        uint32_t *keyBuf = (uint32_t *)malloc(keySz);

        readed = read(fd,keyBuf,keySz);
        if(checkRead(path,readed,keySz) == -1) return -1;

        r = searchRange(keyBuf,hdr.eleNum,lo,key32);

        if(r.lInd == -1){
            pos = SEARCH_POS(uint32_t)(keyBuf,hdr.eleNum,*(uint32_t *)lo);
            if(pos < hdr.eleNum) start = (uint64_t)pos*8;
        }else start = (uint64_t)r.lInd*8;

        r = searchRange(keyBuf,hdr.eleNum,hi,key32);

        if(r.lInd == -1){
            pos = SEARCH_POS(uint32_t)(keyBuf,hdr.eleNum,*(uint32_t *)hi);
            if(start/8 < pos) matchSz = pos*8 - start;
        }else{
            if(start/8 <= r.rInd) matchSz = (uint64_t)(r.rInd+1)*8 - start;
        }
    }else if(hdr.keyLen == key64){
        uint64_t keySz = key64*(uint64_t)hdr.eleNum;

        uint64_t *keyBuf = (uint64_t *)malloc(keySz);

        readed = read(fd,keyBuf,keySz);
        if(checkRead(path,readed,keySz) == -1) return -1;

        r = searchRange(keyBuf,hdr.eleNum,lo,key64);

        if(r.lInd == -1){
            pos = SEARCH_POS(uint64_t)(keyBuf,hdr.eleNum,*(uint64_t *)lo);
            if(pos < hdr.eleNum) start = (uint64_t)pos*8;
        }else start = (uint64_t)r.lInd*8;

        r = searchRange(keyBuf,hdr.eleNum,hi,key64);

        if(r.lInd == -1){
            pos = SEARCH_POS(uint64_t)(keyBuf,hdr.eleNum,*(uint64_t *)hi);
            if(start/8 < pos) matchSz = pos*8 - start;
        }else{
            if(start/8 <= r.rInd) matchSz = (uint64_t)(r.rInd+1)*8 - start;
        }
    }else if(hdr.keyLen == key128){
        uint64_t keySz = key128*(uint64_t)hdr.eleNum;

        uint128_t *keyBuf = (uint128_t *)malloc(keySz);

        readed = read(fd,keyBuf,keySz);
        if(checkRead(path,readed,keySz) == -1) return -1;

        r = searchRange(keyBuf,hdr.eleNum,lo,key128);

        if(r.lInd == -1){
            pos = SEARCH_POS(uint128_t)(keyBuf,hdr.eleNum,*(uint128_t *)lo);
            if(pos < hdr.eleNum) start = (uint64_t)pos*8;
        }else start = (uint64_t)r.lInd*8;

        r = searchRange(keyBuf,hdr.eleNum,hi,key128);

        if(r.lInd == -1){
            pos = SEARCH_POS(uint128_t)(keyBuf,hdr.eleNum,*(uint128_t *)hi);
            if(start/8 < pos) matchSz = pos*8 - start;
        }else{
            if(start/8 <= r.rInd) matchSz = (uint64_t)(r.rInd+1)*8 - start;
        }
    }

    if(matchSz > BUF_ELE_NUM*8){
        matchInvertList = (uint64_t *)malloc(matchSz);
    }else matchInvertList = posArr;

    readed = readIL(path,fd,start,matchSz,matchInvertList);
    if(readed == -1) return -1;

    close(fd);
    return readed / 8;
}


static int readCIL(const char *path,int fd,uint64_t posAndLen,byte **more){
    uint64_t pos = (posAndLen & POS_MASK);
    uint64_t len = (posAndLen >> LIST_LEN_SHIFT);

    off_t off;
    off = lseek(fd,pos,SEEK_SET);
    if(checkLseek(path,off) == -1) return -1;

    ssize_t sz;
    uint64_t len1 = (len < COMPRESSED_BUF_SIZE ? len : COMPRESSED_BUF_SIZE);
    sz = read(fd,compressedInvertedListBuf,len1);
    if(checkRead(path,sz,len1) == -1) return -1;

    if(len > COMPRESSED_BUF_SIZE){
        *more = (byte *)malloc(len - COMPRESSED_BUF_SIZE);
        if(NULL == *more){
            log_to_buf(err_msg+strlen(err_msg),ERROR,"malloc error in getCILFromTrie!");
            log(ERROR,"malloc error in getCILFromTrie!");
            return -1;
        }
        sz = read(fd,*more,len - COMPRESSED_BUF_SIZE);
        if(checkRead(path,sz,len - COMPRESSED_BUF_SIZE) == -1) return -1;
    }

    return len;
}

/* CIL denotes: compressed inverted list*/
int getCILFromDATable(const char *path,uint16_t key,byte **more){
    int fd = open(path,O_RDONLY);
    if(checkOpen(path,fd) == -1) return -1;

    off_t off = lseek(fd,key*sizeof(uint64_t),SEEK_SET);
    if(checkLseek(path,off) == -1){
        close(fd);
        return -1;
    }

    uint64_t posAndLen;
    ssize_t sz = read(fd,&posAndLen,sizeof(uint64_t));
    if(checkRead(path,sz,sizeof(uint64_t)) == -1) return -1;
    if(posAndLen == 0){
        close(fd);
        return 0;
    }

    int res = readCIL(path,fd,posAndLen,more);

    close(fd);
    return res;
}

int getCILFromDATable_range(const char *path,uint16_t low,uint16_t high,byte **more){
    int fd = open(path,O_RDONLY);
    if(checkOpen(path,fd) == -1) return -1;
    
    DATable tab;
    ssize_t sz = read(fd,&tab,sizeof(DATable));
    if(checkRead(path,sz,sizeof(DATable)) == -1) return -1;

    uint64_t lowPos = 0, highPos = 0;
    for(int i = low; i <= high; ++i){
        if(tab.offsetArr[i]){
            lowPos = tab.offsetArr[i];
            break;
        }
    }
    for(int i = high; i >= low; --i){
        if(tab.offsetArr[i]){
            highPos = tab.offsetArr[i];
            break;
        }
    }
    if(lowPos == 0){
        close(fd);
        return 0;
    }

    uint64_t totalBytes = (highPos & POS_MASK) - (lowPos & POS_MASK) + (highPos >> LIST_LEN_SHIFT);
    uint64_t posAndLen = (lowPos & POS_MASK) | (totalBytes << LIST_LEN_SHIFT);
    int res = readCIL(path,fd,posAndLen,more);

    close(fd);
    return res;
}

static inline int isTrieNode(uint64_t off,uint64_t bound){
    return off < bound;
}


static inline int isKeyMatch(byte real[],int16_t target[]){
    int i = 0;
    while(target[i] != -1){
        if(target[i] == SEGMENT_SIZE || real[i] == target[i]) i++;
        else return 0;
    }
    return 1;
}

static inline int isKeyLe(byte real[],int16_t target[]){
    int i = 0;
    while(target[i] != -1){
        if(real[i] < target[i]) return 1;
        else if(real[i] > target[i]) return 0;
        i++;
    }
    return 1;
}

static inline int isKeyGe(byte real[],int16_t target[]){
    int i = 0;
    while(target[i] != -1){
        if(real[i] > target[i]) return 1;
        else if(real[i] < target[i]) return 0;
        i++;
    }
    return 1;
}

static int testAndReadCIL(const char *path,int fd,uint64_t posAndLen,int16_t key[],int keyLen,byte **more){
    uint64_t pos = (posAndLen & POS_MASK);
    uint64_t len = (posAndLen >> LIST_LEN_SHIFT);
    
    off_t off;
    off = lseek(fd,pos,SEEK_SET);
    if(checkLseek(path,off) == -1) return -1;

    ssize_t sz;
    byte buf[64];
    int alignLen = ((keyLen%8==0)?keyLen:(keyLen+8-keyLen%8));
    sz = read(fd,buf,alignLen);
    if(checkRead(path,sz,alignLen) == -1) return -1;

    if(isKeyMatch(buf,key)){
        uint64_t len1 = (len < COMPRESSED_BUF_SIZE ? len : COMPRESSED_BUF_SIZE);
        sz = read(fd,compressedInvertedListBuf,len1);
        if(checkRead(path,sz,len1) == -1) return -1;

        if(len > COMPRESSED_BUF_SIZE){
            *more = (byte *)malloc(len - COMPRESSED_BUF_SIZE);
            if(NULL == *more){
                log_to_buf(err_msg+strlen(err_msg),ERROR,"malloc error in getCILFromTrie!");
                log(ERROR,"malloc error in getCILFromTrie!");
                
                return -1;
            }
            sz = read(fd,*more,len - COMPRESSED_BUF_SIZE);
            if(checkRead(path,sz,len - COMPRESSED_BUF_SIZE) == -1) return -1;
        }

        return len;
        
    }else return 0;
}

static inline int readTrieNode(int fd,const char *path,pFileTrieNode pTmp,uint64_t pos){
    off_t off = lseek(fd,pos,SEEK_SET);
    if(checkLseek(path,off) == -1) return -1;

    ssize_t sz = read(fd,pTmp,sizeof(FileTrieNode));
    if(checkRead(path,sz,sizeof(FileTrieNode)) == -1) return -1;

    return 0;
}

int getCILFromTrie_prefix(const char *path,int16_t key[],int keyLen,int lo,int hi,byte **more)
{
    int fd = open(path,O_RDONLY);
    if(checkOpen(path,fd) == -1) return -1;
        
    byte buf[sizeof(uint64_t)+sizeof(FileTrieNode)];
    ssize_t sz = read(fd,buf,sizeof(uint64_t)+sizeof(FileTrieNode));
    if(checkRead(path,sz,sizeof(uint64_t)+sizeof(FileTrieNode)) == -1){
        close(fd);
        return -1;
    }

    uint64_t *bound = (uint64_t *)buf;
    pFileTrieNode pTNode = (pFileTrieNode)(buf + sizeof(uint64_t));
    int depth = 0;
    uint64_t pos = sizeof(uint64_t);

    int res = -1;

    while(key[depth] != -1){
        int16_t ind = key[depth++];
        if(ind < SEGMENT_SIZE){
            pos = pTNode->offsetArr[ind];
            if(pos == 0){
                close(fd);
                return 0;
            }
            else if(isTrieNode(pos,BOUND)){
                if(readTrieNode(fd,path,pTNode,pos) == -1){
                    close(fd);
                    return -1;
                }
            }else{
                break;
            }
        }else{
            //TODO
        }
    }

    if(isTrieNode(pos,BOUND)){
        int loInd, hiInd;
        uint64_t lowPos,highPos;

        depth++;
        int t_depth = depth;

        for(loInd = lo; loInd <= hi; ++loInd){
            pos = pTNode->offsetArr[loInd];
            if(pos != 0) break;
        }
        if(loInd > hi){
            close(fd);
            return 0;
        }

        if(isTrieNode(pos,BOUND)){
            FileTrieNode tmp1;
            if(readTrieNode(fd,path,&tmp1,pos) == -1){
                close(fd);
                return -1;
            }
            while(1){
                depth++;
                int i;
                for(i = 0; i < SEGMENT_SIZE; ++i){
                    if(tmp1.offsetArr[i]){
                        pos = tmp1.offsetArr[i];
                        break;
                    }
                }
                if(i == SEGMENT_SIZE){
                    close(fd);
                    return 0;
                } 
                if(isTrieNode(pos,BOUND)){
                    if(readTrieNode(fd,path,&tmp1,pos) == -1){
                        close(fd);
                        return -1;
                    }
                }else break;
            }
        }
        lowPos = pos;
        int alignLen = ((keyLen%8==0)?keyLen:(keyLen+8-keyLen%8));
        if(depth < keyLen) lowPos += alignLen;

        for(hiInd = hi; hiInd >= lo; --hiInd){
            pos = pTNode->offsetArr[hiInd];
            if(pos != 0) break;
        }

        if(isTrieNode(pos,BOUND)){
            FileTrieNode tmp2;
            if(readTrieNode(fd,path,&tmp2,pos) == -1){
                close(fd);
                return -1;
            }
            while(1){
                t_depth++;
                int i;
                for(i = SEGMENT_SIZE - 1; i >= 0; --i){
                    if(tmp2.offsetArr[i]){
                        pos = tmp2.offsetArr[i];
                        break;
                    }
                }
                if(i < 0){
                    close(fd);
                    return 0;
                } 
                if(isTrieNode(pos,BOUND)){
                    if(readTrieNode(fd,path,&tmp2,pos) == -1){
                        close(fd);
                        return -1;
                    }
                }else break;
            }
        }
        highPos = pos;
        if(t_depth < keyLen) highPos += alignLen;

        if((highPos & POS_MASK) < (lowPos & POS_MASK)) highPos = lowPos;
        uint64_t totalBytes = (highPos & POS_MASK) - (lowPos & POS_MASK) + (highPos >> LIST_LEN_SHIFT);
        uint64_t posAndLen = (lowPos & POS_MASK) | (totalBytes << LIST_LEN_SHIFT);
        res = readCIL(path,fd,posAndLen,more);
    }else{
        if(depth == keyLen || key[depth] == -1){
            res = readCIL(path,fd,pos,more);
        }else{
            res = testAndReadCIL(path,fd,pos,key,keyLen,more);
        }
    }

    close(fd);
    return res;

}

int getLowPartCILFromTrie(const char *path,int16_t key[],int keyLen,byte **more){
    int fd = open(path,O_RDONLY);
    if(checkOpen(path,fd) == -1) return -1;
        
    byte buf[sizeof(uint64_t)+sizeof(FileTrieNode)];
    ssize_t sz = read(fd,buf,sizeof(uint64_t)+sizeof(FileTrieNode));
    if(checkRead(path,sz,sizeof(uint64_t)+sizeof(FileTrieNode)) == -1){
        close(fd);
        return -1;
    }

    uint64_t CILStartPos = *(uint64_t *)(void *)buf;
    pFileTrieNode pTNode = (void *)(buf + sizeof(uint64_t));
    int depth = 0;
    uint64_t pos = sizeof(uint64_t);
    uint64_t closest = pos;
    int cLevel = 0;
    int flag = 0;

    int res = -1;

    while(key[depth] != -1){
        int i = key[depth++];
        if(pTNode->offsetArr[i--] == 0){
            flag = 1;
            for(; i >= 0; --i){
                pos = pTNode->offsetArr[i];
                if(pos != 0) break;
            }
            if(i < 0){
                if(closest == sizeof(uint64_t)){
                    close(fd);
                    return 0;
                }else{
                    pos = closest;
                    depth = cLevel;
                    break;
                }
            }else{
                break;
            }
        }else{
            pos = pTNode->offsetArr[i+1];
            for(; i >= 0; --i){
                if(pTNode->offsetArr[i]){
                    closest = pTNode->offsetArr[i];
                    cLevel = depth;
                } 
            }
        }
        if(isTrieNode(pos,BOUND)){
            if(readTrieNode(fd,path,pTNode,pos) == -1){
                close(fd);
                return -1;
            }
        }else break;
    }

    while(isTrieNode(pos,BOUND)){
        depth++;
        if(readTrieNode(fd,path,pTNode,pos) == -1){
            close(fd);
            return -1;
        }
        int i = SEGMENT_SIZE-1;
        for(; i >= 0; --i){
            pos = pTNode->offsetArr[i];
            if(pos != 0) break;
        }
        if(i < 0 ){
            close(fd);
            return 0;
        }
    }

    if(depth < keyLen){
        int alignLen = ((keyLen%8==0)?keyLen:(keyLen+8-keyLen%8));
        if(flag){
            uint64_t totalBytes = (pos & POS_MASK) - (CILStartPos & POS_MASK) + (pos >> LIST_LEN_SHIFT) + alignLen;
            uint64_t posAndLen = (CILStartPos & POS_MASK) | (totalBytes << LIST_LEN_SHIFT);
            res = readCIL(path,fd,posAndLen,more);
        }else{
            off_t off;
            off = lseek(fd,pos & POS_MASK,SEEK_SET);
            if(checkLseek(path,off) == -1) return -1;
            byte realKey[64];
            sz = read(fd,realKey,alignLen);
            if(checkRead(path,sz,alignLen) == -1) return -1;

            uint64_t totalBytes = (pos & POS_MASK) - (CILStartPos & POS_MASK);
            if(isKeyLe(realKey,key)){
                totalBytes += (pos >> LIST_LEN_SHIFT);
                totalBytes += alignLen;
            }
            uint64_t posAndLen = (CILStartPos & POS_MASK) | (totalBytes << LIST_LEN_SHIFT);
            res = readCIL(path,fd,posAndLen,more);
        }

    }else{
        uint64_t totalBytes = (pos & POS_MASK) - (CILStartPos & POS_MASK) + (pos >> LIST_LEN_SHIFT);
        uint64_t posAndLen = (CILStartPos & POS_MASK) | (totalBytes << LIST_LEN_SHIFT);
        res = readCIL(path,fd,posAndLen,more);
    }

    return res;
}

int getHighPartCILFromTrie(const char *path,int16_t key[],int keyLen,byte **more){
    struct stat st;
    if(stat(path,&st) == -1){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get index file:%s size",path);
        return -1;
    } 
    uint64_t CILEndPos = st.st_size;

    int fd = open(path,O_RDONLY);
    if(checkOpen(path,fd) == -1) return -1;
        
    byte buf[sizeof(uint64_t)+sizeof(FileTrieNode)];
    ssize_t sz = read(fd,buf,sizeof(uint64_t)+sizeof(FileTrieNode));
    if(checkRead(path,sz,sizeof(uint64_t)+sizeof(FileTrieNode)) == -1){
        close(fd);
        return -1;
    }

    pFileTrieNode pTNode = (void *)(buf + sizeof(uint64_t));
    int depth = 0;
    uint64_t pos = sizeof(uint64_t);
    uint64_t closest = pos;
    int cLevel = 0;
    int flag = 0;

    int res = -1;

    while(key[depth] != -1){
        int i = key[depth++];
        if(pTNode->offsetArr[i++] == 0){
            flag = 1;
            for(; i < SEGMENT_SIZE; ++i){
                pos = pTNode->offsetArr[i];
                if(pos != 0) break;
            }
            if(i >= SEGMENT_SIZE){
                if(closest == sizeof(uint64_t)){
                    close(fd);
                    return 0;
                }else{
                    pos = closest;
                    depth = cLevel;
                    break;
                }
            }else{
                break;
            }
        }else{
            pos = pTNode->offsetArr[i-1];
            for(; i < SEGMENT_SIZE; ++i){
                if(pTNode->offsetArr[i]){
                    closest = pTNode->offsetArr[i];
                    cLevel = depth;
                } 
            }
        }
        if(isTrieNode(pos,BOUND)){
            if(readTrieNode(fd,path,pTNode,pos) == -1){
                close(fd);
                return -1;
            }
        }else break;
    }

    while(isTrieNode(pos,BOUND)){
        if(readTrieNode(fd,path,pTNode,pos) == -1){
            close(fd);
            return -1;
        }
        int i = 0;
        for(; i < SEGMENT_SIZE; ++i){
            pos = pTNode->offsetArr[i];
            if(pos != 0) break;
        }
        if(i >= SEGMENT_SIZE ){
            close(fd);
            return 0;
        }
    }

    if(depth < keyLen){
        int alignLen = ((keyLen%8==0)?keyLen:(keyLen+8-keyLen%8));
        if(flag){
            uint64_t totalBytes = (CILEndPos & POS_MASK) - (pos & POS_MASK) - alignLen;
            pos += alignLen;
            uint64_t posAndLen = (pos & POS_MASK) | (totalBytes << LIST_LEN_SHIFT);
            res = readCIL(path,fd,posAndLen,more);
        }else{
            off_t off;
            off = lseek(fd,pos & POS_MASK,SEEK_SET);
            if(checkLseek(path,off) == -1) return -1;
            byte realKey[64];
            sz = read(fd,realKey,alignLen);
            if(checkRead(path,sz,alignLen) == -1) return -1;

            uint64_t totalBytes = (CILEndPos & POS_MASK) - (pos & POS_MASK);
            if(isKeyGe(realKey,key)){
                totalBytes -= alignLen;
                pos += alignLen;
            }else{
                totalBytes -= alignLen;
                uint64_t len = (pos >> LIST_LEN_SHIFT);
                totalBytes -= len;
                pos += alignLen;
                pos += len;
            }
            uint64_t posAndLen = (pos & POS_MASK) | (totalBytes << LIST_LEN_SHIFT);
            res = readCIL(path,fd,posAndLen,more);
        }

    }else{
        uint64_t totalBytes = (CILEndPos & POS_MASK) - (pos & POS_MASK);
        uint64_t posAndLen = (pos & POS_MASK) | (totalBytes << LIST_LEN_SHIFT);
        res = readCIL(path,fd,posAndLen,more);
    }

    return res;
}

int getCILFromTrie(const char *path,int16_t key[],int keyLen,byte **more){
    int fd = open(path,O_RDONLY);
    if(checkOpen(path,fd) == -1) return -1;
        
    byte buf[sizeof(uint64_t)+sizeof(FileTrieNode)];
    ssize_t sz = read(fd,buf,sizeof(uint64_t)+sizeof(FileTrieNode));
    if(checkRead(path,sz,sizeof(uint64_t)+sizeof(FileTrieNode)) == -1){
        close(fd);
        return -1;
    }

    uint64_t *bound = (uint64_t *)buf;
    pFileTrieNode pTNode = (pFileTrieNode)(buf + sizeof(uint64_t));
    int depth = 0;
    uint64_t pos = sizeof(uint64_t);

    int res = -1;

    while(key[depth] != -1){
        int16_t ind = key[depth++];
        if(ind < SEGMENT_SIZE){
            pos = pTNode->offsetArr[ind];
            if(pos == 0){
                close(fd);
                return 0;
            }
            else if(isTrieNode(pos,BOUND)){
                if(readTrieNode(fd,path,pTNode,pos) == -1){
                    close(fd);
                    return -1;
                }
            }else{
                break;
            }
        }else{
            //TODO
        }
    }

    if(isTrieNode(pos,BOUND)){
        FileTrieNode tmp1,tmp2;
        memcpy(&tmp1,pTNode,sizeof(tmp1));
        memcpy(&tmp2,pTNode,sizeof(tmp2));
        uint64_t lowPos,highPos;
        int t_depth = depth;
        while(1){
            depth++;
            int i;
            for(i = 0; i < SEGMENT_SIZE; ++i){
                if(tmp1.offsetArr[i]){
                    pos = tmp1.offsetArr[i];
                    break;
                }
            }
            if(i == SEGMENT_SIZE){
                close(fd);
                return 0;
            } 
            if(isTrieNode(pos,BOUND)){
                if(readTrieNode(fd,path,&tmp1,pos) == -1){
                    close(fd);
                    return -1;
                }
            }else break;
        }
        lowPos = pos;
        int alignLen = ((keyLen%8==0)?keyLen:(keyLen+8-keyLen%8));
        if(depth < keyLen) lowPos += alignLen;
        while(1){
            t_depth++;
            int i;
            for(i = SEGMENT_SIZE - 1; i >= 0; --i){
                if(tmp2.offsetArr[i]){
                    pos = tmp2.offsetArr[i];
                    break;
                }
            }
            if(i < 0){
                close(fd);
                return 0;
            } 
            if(isTrieNode(pos,BOUND)){
                if(readTrieNode(fd,path,&tmp2,pos) == -1){
                    close(fd);
                    return -1;
                }
            }else break;
        }
        highPos = pos;
        if(t_depth < keyLen) highPos += alignLen;
        if((highPos & POS_MASK) < (lowPos & POS_MASK)) highPos = lowPos;
        uint64_t totalBytes = (highPos & POS_MASK) - (lowPos & POS_MASK) + (highPos >> LIST_LEN_SHIFT);
        uint64_t posAndLen = (lowPos & POS_MASK) | (totalBytes << LIST_LEN_SHIFT);
        res = readCIL(path,fd,posAndLen,more);
    }else{
        if(depth == keyLen || key[depth] == -1){
            res = readCIL(path,fd,pos,more);
        }else{
            res = testAndReadCIL(path,fd,pos,key,keyLen,more);
        }
    }

    close(fd);
    return res;
}

static inline uint64_t getFileSize(const char *path){
    struct stat statBuf;
    if(stat(path,&statBuf)<0){
        return -1;
    }else{
        return statBuf.st_size;
    }
}

static int chkSumUnMatch(void *buf,uint32_t len,char *chksum,uint32_t clen){
    if(strlen(chksum) == 0) return 0;
}

static inline int chkTimeRange_single(pPcapPacketHead pHdr,uint64_t s_time,uint64_t e_time){
#ifdef BIG_ENDIAN
    uint8_t *ts = (void *)&(pHdr->ts.sec);
    uint32_t seg1 = ts[3], seg2 = ts[2], seg3 = ts[1], seg4= ts[0];
    uint32_t tmp = (seg4 << 24) | (seg3 << 16) | (seg2 << 8) | seg1;
    return tmp >= s_time && tmp <= e_time;
#else
    return pHdr->ts.sec >= s_time && pHdr->ts.sec <= e_time;

#endif
}
static inline int chkTimeRange(pPcapPacketHead pHdr){
    for(int i = 0; i < time_interval_num; ++i){
        if(chkTimeRange_single(pHdr,s_time_arr[i],e_time_arr[i])) return 1;
    }
    return 0;
}

int getDataByIL(const char *path,uint64_t invertedList[],int pktNum,int compress_enable){
    if(pktNum == 0) return 0;
    int rfd,wfd;
    rfd = open(path,O_RDONLY);
    if(checkOpen(path,rfd) == -1) return -1;

    int totBytes = 0;

    //char resultPath[MAX_PATH_LEN];
    //getPath(resultPath,resFileName,RESULT);

    wfd = open(resFileName,O_RDWR | O_CREAT | O_APPEND,666);
    if(checkOpen(resFileName,wfd) == -1) return -1;

    if(newFlag){
        memcpy(dataBuff,&hdr,PCAP_FILE_HEAD_SIZE);
        totBytes += PCAP_FILE_HEAD_SIZE;
        newFlag = 0;
    }

    off_t off;
    ssize_t sz;

    int realNum = 0;

    for(int i = 0; i < pktNum; ++i){
        //if(invertedList[i] > file_size) continue;
        off = lseek(rfd,invertedList[i],SEEK_SET);
        if(checkLseek(path,off) == -1){
            close(rfd);
            close(wfd);
            return -1;
        }

        sz = read(rfd,dataBuff+totBytes,sizeof(pcapPacketHead));
        pPcapPacketHead pPktHdr = (pPcapPacketHead)(dataBuff + totBytes);
        //if(!chkTimeRange(pPktHdr)) continue;
        totBytes += sizeof(pcapPacketHead);
        if(0)
        {
             sz = read(rfd,cpsedBuf,pPktHdr->caplen);
            if(checkRead(path,sz,pPktHdr->caplen) == -1){
                close(rfd);
                close(wfd);
                return -1;
            }
        
             int uncpsed = LZ4_decompress_safe((char *)cpsedBuf,(char *)(dataBuff+totBytes),sz,DATA_BUF_SIZE-DATA_THRESHOLD);
            if(uncpsed < 0){
                log_to_buf(err_msg+strlen(err_msg),ERROR,"decompress error");
                log(ERROR,"decompress error");
                close(rfd);
                close(wfd);
                return -1;
            }
            pPktHdr->caplen = pPktHdr->len = uncpsed;
        }else{
            sz = read(rfd,dataBuff+totBytes,pPktHdr->caplen);
            if(checkRead(path,sz,pPktHdr->caplen) == -1){
                close(rfd);
                close(wfd);
                return -1;
            }
        }

        /*
        if(chkSumUnMatch(dataBuff+totBytes,uncpsed,identifier,16)){
            totBytes -= sizeof(pcapPacketHead);
            continue;
        }
        */
        
        realNum++;


        totBytes += pPktHdr->caplen;

        if(totBytes >= DATA_THRESHOLD){
            sz = write(wfd,dataBuff,totBytes);
            if(checkRead(resFileName,sz,totBytes) == -1){
                close(rfd);
                close(wfd);
                return -1;
            }
            totBytes = 0;
        }
    }

    if(totBytes != 0){
        sz = write(wfd,dataBuff,totBytes);
        if(checkRead(resFileName,sz,totBytes) == -1){
            realNum = -1;
        }
        totBytes = 0;
    }

    close(rfd);
    close(wfd);
    return realNum;
}
