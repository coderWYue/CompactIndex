/*************************************************************************
	> File Name: Driver.cpp
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Fri 22 Nov 2019 02:00:13 AM PST
 ************************************************************************/

#include<iostream>
#include<queue>
#include<vector>
#include<map>
#include<cstring>
#include<algorithm>
#include<mysql.h>
#include<sys/time.h>
#include<arpa/inet.h>

#include "common.h"
#include "decompress.h"
#include "toolFunc.h"
#include "log.h"
#include "tdms.h"
#include "fileOperation.h"
#include "Driver.h"
#include "retrieve.h"

using namespace std;

typedef union IpInt{
    uint32_t ip4;
    uint128_t ip6;
}IpInt;

uint64_t cListTmp[ELEMENT_NUM];
queue<pQueryFile> queryFileSet;

static int atoi_16(const char *str){
    int result = 0;
    while(*str){
        if (*str >= '0' && *str <= '9')
            result = result * 16 + (*str - '0');
        else if (*str >= 'a' && *str <= 'f')
            result = result * 16 + (*str - 'a' + 10);
        else if (*str >= 'A' && *str <= 'F')
            result = result * 16 + (*str - 'A' + 10);
        else {
            return -1;
        }
        str++;
    }
    return result;
}

static int atoi_10(const char *str){
    int result = 0;
    while(*str){
        if (*str >= '0' && *str <= '9')
            result = result * 10 + (*str - '0');
        else {
            return -1;
        }
        str++;
    }
    return result;
}

static int str2int(const char *numStr){
    if(NULL == numStr){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"nullptr parameter in str2int");
        log(ERROR,"nullptr parameter in str2int");
        return -1;
    }
    int res = -1;
    const char *start = numStr;
    if(*numStr == '-' || *numStr == '+') numStr++;
    if(*numStr == 0){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"str has no valid number in str2int");
        log(ERROR,"str has no valid number in str2int");
        return -1;
    }
    if(*numStr == '0' && *(numStr+1) == 'x'){
        if(*(numStr+2) != 0){
            res = atoi_16(numStr+2);
        }
    }else{
        res = atoi_10(numStr);
    }
    if(res == -1){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"invalid number str:%s",start);
        log(ERROR,"invalid number str:%s",start);

    
        return -1;
    }
    if(*start == '-') return -res;
    else return res;
}

static int portStr2Int(const char *portStr){
    int portInt = str2int(portStr);
    if(portInt == -1){
        if(portStr) 
        {
            log_to_buf(err_msg+strlen(err_msg),ERROR,"invalid port: %s",portStr);
        
            log(ERROR,"invalid port: %s",portStr);
        }
        return -1;
    }else if(portInt > 65535 || portInt < 0){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"%d out range of port",portInt);
        log(ERROR,"%d out range of port",portInt);
        return -1;
    }else return portInt;
}

static int protoStr2Int(const char *protoStr){
    int proto = str2int(protoStr);
    if(proto == -1){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"invalid proto: %s",protoStr);
        log(ERROR,"invalid proto: %s",protoStr);
        return -1;
    }else if(proto > 255 || proto < 0){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"%d out range of proto",proto);
        log(ERROR,"%d out range of proto",proto);
        return -1;
    }else return proto;
}

static int ipStr2IntArr(const char *ipStr,int16_t ip[]){
    int i=0;
    char ipStrBuf[128];
    strcpy(ipStrBuf,ipStr);
    const char *deli = ".";
    char *numStr=strtok(ipStrBuf,deli);
    while(numStr&&(i<4)){
        if(strcmp(numStr,"*") == 0){
            ip[i++] = 256;
            numStr=strtok(NULL,deli);
            continue;
        }
        int ipNum=str2int(numStr);
        if(ipNum==-1){
            log_to_buf(err_msg+strlen(err_msg),ERROR,"invalid ip: %s",ipStr);
            log(ERROR,"invalid ip: %s",ipStr);
            return -1;
        }else if(ipNum>255 || ipNum<0){
            log_to_buf(err_msg+strlen(err_msg),ERROR,"ip out of range: %s",ipStr);
            log(ERROR,"ip out of range: %s",ipStr);
            return -1;
        }else{
            ip[i++]=ipNum;
        }
        numStr=strtok(NULL,deli);
    }
    if(i<4){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"incomplete ip address: %s",ipStr);
        log(ERROR,"incomplete ip address: %s",ipStr);
        return -1;
    }else if(numStr != NULL){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"incorrect format ipv4: %s",ipStr);
        log(ERROR,"incorrect format ipv4: %s",ipStr);
        return -1;
    }
    ip[i] = -1;
    while(--i >= 0 && ip[i]==256) ip[i] = -1;
    return 0;
}

static int ip6Str2IntArr(const char *ip6Str,int16_t ip6[],int len){
    uint8_t ip6_addr[16];
    int i;
    if(NULL == ip6Str){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"invalid para : ip6Str");
        return -1;
    }
    if(inet_pton(AF_INET6,ip6Str,ip6_addr) == 1){
        for(i = 0; i < 16; ++i) ip6[i] = ip6_addr[i];
        for(;i < len; ++i) ip6[i] = -1;
        return 0;
    }else{
        log_to_buf(err_msg+strlen(err_msg),ERROR,"invalid ipv6 addr str:%s",ip6Str);
        for(i = 0; i < len; ++i) ip6[i] = -1;
        return -1;
    }
}

static int process_ip_netmask(char *indexValue,char *mask_num_str,int16_t ip[],int &mask_num,uint8_t &lo,uint8_t &hi,uint8_t af){
    *mask_num_str = 0;
    mask_num_str++;
    if(*mask_num_str == 0){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"invaild ip format:%s/",indexValue);
        return -1;
    }
    if(strchr(indexValue,'/')){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"invaild ip format:%s/%s",indexValue,mask_num_str);
        return -1;
    }

    if(af == 4){
        if(ipStr2IntArr(indexValue,ip) == -1) return -1;
        mask_num = atoi_10(mask_num_str);
        if(mask_num < 0 || mask_num > IPV4_BIT_LEN){
            log_to_buf(err_msg+strlen(err_msg),ERROR,"invaild ip format:%s/%s,mask number must >=0 and <= 32",indexValue,mask_num_str);
            return -1;
        }
    }else{
        if(ip6Str2IntArr(indexValue,ip,2*IPV6_ADDR_LEN) == -1) return -1;
        mask_num = atoi_10(mask_num_str);
        if(mask_num < 0 || mask_num > IPV6_BIT_LEN){
            log_to_buf(err_msg+strlen(err_msg),ERROR,"invaild ipv6 format:%s/%s,mask number must >=0 and <= 128",indexValue,mask_num_str);
            return -1;
        }
    }

    int b = mask_num/8;
    uint8_t tmp = ip[b];
    uint8_t off = 8-mask_num%8;
    uint8_t mask = 0xff;
    lo = tmp & (mask << off);
    hi = lo | ~(mask << off);
    for(int i = b; i < 8; ++i) ip[i] = -1;
    return 0;
}

int getOffsetArr_range(const char *path,const char *indexName,const char *low,const char *high,int lb,int hb){
    int sizeOfCIL = 0;
    byte *more = NULL;
    if(!strcmp(indexName,index_category_str[SPORT]) || \
            !strcmp(indexName,index_category_str[DPORT])){
        uint16_t lo = 0, hi = 65535;
        if(low != NULL){
            lo = portStr2Int(low);
            if(lo == -1) return -1;
            lo += lb;
            if(lo > 65535){
                log_to_buf(err_msg+strlen(err_msg),ERROR,"port out of range");
                log(ERROR,"port out of range");
                return -1;
            }
        }
        if(high != NULL){
            hi = portStr2Int(high);
            if(hi == -1) return -1;
            hi -= hb;
            if(hi < 0){
                log_to_buf(err_msg+strlen(err_msg),ERROR,"port out of range");
                log(ERROR,"port out of range");
                return -1;
            }
        }
        if((sizeOfCIL = getCILFromDATable_range(path,lo,hi,&more)) == -1){
            log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s range",indexName);
            log(ERROR,"fail to get offset array for %s range",indexName);
            return -1;
        }
    }else if(!strcmp(indexName,index_category_str[SIP]) || !strcmp(indexName,index_category_str[DIP]) || \
            !strcmp(indexName,index_category_str[SIP6]) || !strcmp(indexName,index_category_str[DIP6])){

        int af = 4;
        if(!strcmp(indexName,index_category_str[SIP6]) || !strcmp(indexName,index_category_str[DIP6])) af = 6;
        int ip_len = (af == 4 ? 4 : 16);

        int16_t ip[2*IPV6_ADDR_LEN];
        char indexValue[256];

        if(high){
            strcpy(indexValue,high);
            char *mask_num_str = strrchr(indexValue,'/');
            if(mask_num_str){
                uint8_t lo = 0, hi = 255;
                //int mask_num = 32;
                int mask_num = (af == 4 ? IPV4_BIT_LEN : IPV6_BIT_LEN);
                if(process_ip_netmask(indexValue,mask_num_str,ip,mask_num,lo,hi,af) == -1) return -1;
                int b = mask_num / 8;
                if(hb){
                    int lift = 8 - mask_num%8;
                    if(lo == 0){
                        int i = b - 1;
                        ip[i+1] = 255;
                        while(i >= 0){
                            if(ip[i]){
                                ip[i]--;
                                break;
                            }else ip[i--] = 255; 
                        }
                        if(i < 0) return 0;
                    }else{
                        ip[b++] = (lo - (1 << lift));
                    }
                }else ip[b++] = lo;
                for(;b < ip_len; ++b) ip[b] = 0;
                if((sizeOfCIL = getLowPartCILFromTrie(path,ip,ip_len,&more)) == -1){
                    log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s range",indexName);
                    return -1;
                }
            }else{
                if(af == 4){
                    if(ipStr2IntArr(indexValue,ip) == -1) return -1;
                }
                else{
                    if(ip6Str2IntArr(indexValue,ip,2*IPV6_ADDR_LEN) == -1) return -1;
                }
                if(hb){
                    int i = (af == 4 ? 3 : 15);
                    if(ip[i] == 0){
                        ip[i--] = 255;
                        while(i >= 0){
                            if(ip[i]){
                                ip[i]--;
                                break;
                            }else ip[i--] = 255;
                        }
                        if(i < 0) return 0;
                    }else ip[i]--;
                }
                if((sizeOfCIL = getLowPartCILFromTrie(path,ip,ip_len,&more)) == -1){
                    log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s range",indexName);
                    return -1;
                }
            }
        }else if(low){
            strcpy(indexValue,low);
            char *mask_num_str = strrchr(indexValue,'/');
            if(mask_num_str){
                uint8_t lo = 0, hi = 255;
                int mask_num = (af == 4 ? IPV4_BIT_LEN : IPV6_BIT_LEN);
                //int mask_num = 32;
                if(process_ip_netmask(indexValue,mask_num_str,ip,mask_num,lo,hi,af) == -1) return -1;
                int b = mask_num / 8;
                if(lb){
                    int lift = 8 - mask_num%8;
                    uint16_t tmp = hi;
                    tmp++;
                    if(tmp >= 256){
                        int i = b - 1;
                        ip[i+1] = 0;
                        while(i >= 0){
                            if(ip[i] < 255){
                                ip[i]++;
                                break;
                            }else ip[i--] = 0; 
                        }
                        if(i < 0) return 0;
                    }else{
                        ip[b++] = tmp; 
                    }
                }else ip[b++] = lo;
                for(;b < ip_len; ++b) ip[b] = 0;
                if((sizeOfCIL = getHighPartCILFromTrie(path,ip,ip_len,&more)) == -1){
                    log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s range",indexName);
                    return -1;
                }
            }else{
                if(af == 4){
                    if(ipStr2IntArr(indexValue,ip) == -1) return -1;    
                }
                else{
                    if(ip6Str2IntArr(indexValue,ip,2*IPV6_ADDR_LEN) == -1) return -1;
                }
                if(lb){
                    //int i = 3;
                    int i = (af == 4 ? 3 : 15);
                    if(ip[i] == 255){
                        ip[i--] = 0;
                        while(i >= 0){
                            if(ip[i] < 255){
                                ip[i]++;
                                break;
                            }else ip[i--] = 0;
                        }
                        if(i < 0) return 0;
                    }else ip[i]++;
                }
                if((sizeOfCIL = getHighPartCILFromTrie(path,ip,ip_len,&more)) == -1){
                    log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s range",indexName);
                    return -1;
                }
            }
        }else{
            log_to_buf(err_msg+strlen(err_msg),ERROR,"not specify ip range %s",indexName);
            return -1;
        }
        int from = 0;
        int to = 0;
        uint64_t *pCListTmp;
        if(sizeOfCIL > ELEMENT_NUM*8){
            pCListTmp = reinterpret_cast<uint64_t *>(malloc(sizeOfCIL));
        }else pCListTmp = reinterpret_cast<uint64_t *>((void *)cListTmp);
        while(from < sizeOfCIL && from < COMPRESSED_BUF_SIZE){
            uint64_t *curr = reinterpret_cast<uint64_t *>(compressedInvertedListBuf+from);
            if(*curr > VALID_OFF_BOUND){
                uint64_t skip = *curr ^ VALID_OFF_BOUND;
                from += (skip + 8);
            }else{
                pCListTmp[to++] = *curr;
                from += 8;
            }
        }
        from = from > COMPRESSED_BUF_SIZE ? 8 : 0;
        if(more != NULL){
            while(from < sizeOfCIL - COMPRESSED_BUF_SIZE){
                uint64_t *curr = reinterpret_cast<uint64_t *>(more + from);
                if(*curr > VALID_OFF_BOUND){
                    uint64_t skip = *curr ^ VALID_OFF_BOUND;
                    from += (skip + 8);
                }else{
                    pCListTmp[to++] = *curr;
                    from += 8;
                }
            }
            free(more);
        }
        uint32_t N = decompressIL(pCListTmp,to);
        sort(invertedListBuf,invertedListBuf+N);
        if(sizeOfCIL > ELEMENT_NUM*8) free(pCListTmp);
        return N;
    }else{
        log_to_buf(err_msg+strlen(err_msg),ERROR,"not supported range index: %s",indexName);
        log(ERROR,"not supported range index: %s",indexName);
        return -1;
    }
    uint32_t N = 0;
    if(more != NULL){
        uint64_t *pCListTmp = reinterpret_cast<uint64_t *>(malloc(sizeOfCIL));
        memcpy(pCListTmp,compressedInvertedListBuf,COMPRESSED_BUF_SIZE);
        memcpy(pCListTmp+COMPRESSED_BUF_SIZE/8,more,sizeOfCIL-COMPRESSED_BUF_SIZE);
        free(more);
        N = decompressIL(pCListTmp,sizeOfCIL/8);
        free(pCListTmp);
    }else{
        uint64_t *pCListTmp = reinterpret_cast<uint64_t *>((void *)compressedInvertedListBuf);
        N = decompressIL(pCListTmp,sizeOfCIL/8);
    }
    sort(invertedListBuf,invertedListBuf+N);
    return N;
}

int getOffsetArr(const char *path,const char *indexName,char *indexValue){
    int sizeOfCIL = 0;
    byte *more = NULL;
    bool hasWildCard = false;
    if(!strcmp(indexName,index_category_str[SIP]) || \
            !strcmp(indexName,index_category_str[DIP])){
        int16_t ip[8];
        char *mask_num_str = strrchr(indexValue,'/');
        if(mask_num_str){
            uint8_t lo = 0, hi = 255;
            int mask_num = IPV4_BIT_LEN;
            if(process_ip_netmask(indexValue,mask_num_str,ip,mask_num,lo,hi,4) == -1) return -1;
            if((sizeOfCIL = getCILFromTrie_prefix(path,ip,4,lo,hi,&more))==-1){
                log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s",indexName);
                log(ERROR,"fail to get offset array for %s",indexName);
               return -1;
            }
            if(mask_num != IPV4_BIT_LEN) hasWildCard = true;
        }else{
            if(ipStr2IntArr(indexValue,ip) == -1) return -1;
            if((sizeOfCIL = getCILFromTrie(path,ip,4,&more)) == -1){
                log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s",indexName);
                log(ERROR,"fail to get offset array for %s",indexName);
                return -1;
            }
            int i = 0;
            while(ip[i++] != -1);
            if(i < 4) hasWildCard = true;
        }
    }else if(!strcmp(indexName,index_category_str[SIP6]) || !strcmp(indexName,index_category_str[DIP6])){
        int16_t ip6[32];
        char *mask_num_str = strrchr(indexValue,'/');
        if(mask_num_str){
            uint8_t lo = 0, hi = 255;
            int mask_num = IPV6_BIT_LEN;
            if(process_ip_netmask(indexValue,mask_num_str,ip6,mask_num,lo,hi,6) == -1) return -1;
            if((sizeOfCIL = getCILFromTrie_prefix(path,ip6,IPV6_ADDR_LEN,lo,hi,&more))==-1){
                log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s",indexName);
                log(ERROR,"fail to get offset array for %s",indexName);
               return -1;
            }
            if(mask_num != IPV6_BIT_LEN) hasWildCard = true;
        }else{
            if(ip6Str2IntArr(indexValue,ip6,2*IPV6_ADDR_LEN) == -1) return -1;
            if((sizeOfCIL = getCILFromTrie(path,ip6,IPV6_ADDR_LEN,&more)) == -1){
                log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s",indexName);
                log(ERROR,"fail to get offset array for %s",indexName);
                return -1;
            }
        }
    }else if(!strcmp(indexName,index_category_str[TS])){
        char formatedTime[64];
        int len = getQueryTime(indexValue,formatedTime);
        if(len == -1){
            log_to_buf(err_msg+strlen(err_msg),ERROR,"invalid timestamp: %s",indexValue);
            log(ERROR,"invalid timestamp: %s",indexValue);
            return -1;
        }
        uint64_t t = date_to_seconds(formatedTime);
        if(t == UINT64_MAX){
            
            log_to_buf(err_msg+strlen(err_msg),ERROR,"invalid timestamp: %s",indexValue);
            log(ERROR,"invalid timestamp: %s",indexValue);
            return -1;
        }
        uint8_t tsArrRaw[8];
        memcpy(tsArrRaw,&t,sizeof(t));
        int16_t tsArr[8];
        for(int i = 0; i < 4; ++i){
            tsArr[i] = tsArrRaw[i];
        }
        tsArr[4] = -1;
        if((sizeOfCIL = getCILFromTrie(path,tsArr,4,&more)) == -1){
            log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s",indexName);
            log(ERROR,"fail to get offset array for %s",indexName);
       
            return -1;
        }
    }else if(!strcmp(indexName,index_category_str[SPORT]) || \
            !strcmp(indexName,index_category_str[DPORT])){
        int port = portStr2Int(indexValue);
        if(port == -1) return -1;
        if((sizeOfCIL = getCILFromDATable(path,port,&more)) == -1){
            log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s",indexName);
            log(ERROR,"fail to get offset array for %s",indexName);
            return -1;
        }
    }else if(!strcmp(indexName,index_category_str[PROTO])){
        int proto  = protoStr2Int(indexValue);
        if(proto == -1) return -1;
        if((sizeOfCIL = getCILFromDATable(path,proto,&more)) == -1){
            log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s",indexName);
            log(ERROR,"fail to get offset array for %s",indexName);
            return -1;
        }
    }else{
        log_to_buf(err_msg+strlen(err_msg),ERROR,"unknown index type: %s",indexName);
        log(ERROR,"unknown index type: %s",indexName);
        return -1;
    }
    if(hasWildCard){
        int from = 0;
        int to = 0;
        uint64_t *pCListTmp;
        if(sizeOfCIL > ELEMENT_NUM*8){
            pCListTmp = reinterpret_cast<uint64_t *>(malloc(sizeOfCIL));
        }else pCListTmp = reinterpret_cast<uint64_t *>((void *)cListTmp);
        while(from < sizeOfCIL && from < COMPRESSED_BUF_SIZE){
            uint64_t *curr = reinterpret_cast<uint64_t *>(compressedInvertedListBuf+from);
            if(*curr > VALID_OFF_BOUND){
                uint64_t skip = *curr ^ VALID_OFF_BOUND;
                from += (skip + 8);
            }else{
                pCListTmp[to++] = *curr;
                from += 8;
            }
        }
        from = from > sizeOfCIL ? 8 : 0;
        if(more != NULL){
            while(from < sizeOfCIL - COMPRESSED_BUF_SIZE){
                uint64_t *curr = reinterpret_cast<uint64_t *>(more + from);
                if(*curr > VALID_OFF_BOUND){
                    uint64_t skip = *curr ^ VALID_OFF_BOUND;
                    from += (skip + 8);
                }else{
                    pCListTmp[to++] = *curr;
                    from += 8;
                }
            }
            free(more);
        }
        uint32_t N = decompressIL(pCListTmp,to);
        sort(invertedListBuf,invertedListBuf+N);
        if(sizeOfCIL > ELEMENT_NUM*8) free(pCListTmp);
        return N;
    }
    uint32_t N = 0;
    if(more != NULL){
        uint64_t *pCListTmp = reinterpret_cast<uint64_t *>(malloc(sizeOfCIL));
        memcpy(pCListTmp,compressedInvertedListBuf,COMPRESSED_BUF_SIZE);
        memcpy(pCListTmp+COMPRESSED_BUF_SIZE/8,more,sizeOfCIL-COMPRESSED_BUF_SIZE);
        free(more);
        N = decompressIL(pCListTmp,sizeOfCIL/8);
        free(pCListTmp);
    }else{
        uint64_t *pCListTmp = reinterpret_cast<uint64_t *>((void *)compressedInvertedListBuf);
        N = decompressIL(pCListTmp,sizeOfCIL/8);
    }
    return N;
}


int getQueryFileSet_no_db(bool flag[]){
    int num=countFiles(index_storage_dir,index_category_str[SPORT]);
    for(int i=0;i<num;i++){
        pQueryFile pqf=new queryFile();
        pqf->dataFileName=new char[48];
        strcpy(pqf->dataFileName,"data");
        char id[8];
        strcat(pqf->dataFileName,decimalToStr(i,id));

        pqf->srcIpIndexFileName=new char[48];
        strcpy(pqf->srcIpIndexFileName,index_category_str[SIP]);
        strcpy(pqf->srcIpIndexFileName,"4_");
        strcat(pqf->srcIpIndexFileName,id);
        
        pqf->srcIp6IndexFileName=new char[48];
        strcpy(pqf->srcIp6IndexFileName,index_category_str[SIP6]);
        strcpy(pqf->srcIp6IndexFileName,"_");
        strcat(pqf->srcIp6IndexFileName,id);

        pqf->srcPortIndexFileName=new char[48];
        strcpy(pqf->srcPortIndexFileName,index_category_str[SPORT]);
        strcat(pqf->srcPortIndexFileName,id);

        pqf->dstIpIndexFileName=new char[48];
        strcpy(pqf->dstIpIndexFileName,index_category_str[DIP]);
        strcpy(pqf->dstIpIndexFileName,"4_");
        strcat(pqf->dstIpIndexFileName,id);

        pqf->dstIp6IndexFileName=new char[48];
        strcpy(pqf->dstIp6IndexFileName,index_category_str[DIP6]);
        strcpy(pqf->dstIp6IndexFileName,"_");
        strcat(pqf->dstIp6IndexFileName,id);

        pqf->dstPortIndexFileName=new char[48];
        strcpy(pqf->dstPortIndexFileName,index_category_str[DPORT]);
        strcat(pqf->dstPortIndexFileName,id);

        pqf->protoIndexFileName=new char[48];
        strcpy(pqf->protoIndexFileName,index_category_str[PROTO]);
        strcat(pqf->protoIndexFileName,id);

        pqf->tsIndexFileName=new char[48];
        strcpy(pqf->tsIndexFileName,index_category_str[TS]);
        strcat(pqf->tsIndexFileName,id);

        queryFileSet.push(pqf);
    }
    return num;
}

int getQueryFileSet(MYSQL *mysql,const char *startTime,const char *endTime){
    int num=0;    
    MYSQL_RES *res;
    MYSQL_ROW row;
    char query1[512];
    sprintf(query1,"SELECT data_file,src_ip_index_file,src_ip6_index_file,src_port_index_file,dst_ip_index_file,dst_ip6_index_file,\
            dst_port_index_file,proto_index_file,ts_index_file FROM storage_info WHERE timestamp > '%s' AND timestamp <= '%s'",startTime,endTime);
    if(mysql_real_query(mysql,query1,(unsigned long)strlen(query1))){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"mysql_real_query failed!query:%s",query1);
        log(ERROR,"mysql_real_query failed!query:%s",query1);
        return -1;
    }
    res=mysql_store_result(mysql);
    if(res==NULL){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"mysql_store_result failed!query:%s",query1);
        log(ERROR,"mysql_store_result failed!query:%s",query1);
        return -1;
    }
    while(row=mysql_fetch_row(res)){
        pQueryFile pqf=new queryFile();
        pqf->dataFileName=new char[48];
        strcpy(pqf->dataFileName,row[0]);

        pqf->srcIpIndexFileName=new char[48];
        strcpy(pqf->srcIpIndexFileName,row[1]);

        pqf->srcIp6IndexFileName=new char[48];
        strcpy(pqf->srcIp6IndexFileName,row[2]);

        pqf->srcPortIndexFileName=new char[48];
        strcpy(pqf->srcPortIndexFileName,row[3]);

        pqf->dstIpIndexFileName=new char[48];
        strcpy(pqf->dstIpIndexFileName,row[4]);

        pqf->dstIp6IndexFileName=new char[48];
        strcpy(pqf->dstIp6IndexFileName,row[5]);

        pqf->dstPortIndexFileName=new char[48];
        strcpy(pqf->dstPortIndexFileName,row[6]);
        
        pqf->protoIndexFileName=new char[48];
        strcpy(pqf->protoIndexFileName,row[7]);

        pqf->tsIndexFileName=new char[48];
        strcpy(pqf->tsIndexFileName,row[8]);

        queryFileSet.push(pqf);
        num++;
    }
    mysql_free_result(res);
    if(num==0){
        char query2[512];
        sprintf(query2,"SELECT data_file,src_ip_index_file,src_ip6_index_file,src_port_index_file,dst_ip_index_file,dst_ip6_index_file,\
                dst_port_index_file,proto_index_file,ts_index_file FROM storage_info WHERE timestamp > '%s' ORDER BY timestamp LIMIT 1",endTime);
        if(mysql_real_query(mysql,query2,(unsigned long)strlen(query2))){
            log_to_buf(err_msg+strlen(err_msg),ERROR,"mysql_real_query failed!query:%s",query2);
            log(ERROR,"mysql_real_query failed!query:%s",query2);
            return -1;
        }
        res=mysql_store_result(mysql);
        if(res==NULL){
            log_to_buf(err_msg+strlen(err_msg),ERROR,"mysql_store_result failed!query:%s",query2);
            log(ERROR,"mysql_store_result failed!query:%s",query2);
            return -1;
        }
        if(row=mysql_fetch_row(res)){
            pQueryFile pqf=new queryFile();
            pqf->dataFileName=new char[48];
            strcpy(pqf->dataFileName,row[0]);

            pqf->srcIpIndexFileName=new char[48];
            strcpy(pqf->srcIpIndexFileName,row[1]);

            pqf->srcIp6IndexFileName=new char[48];
            strcpy(pqf->srcIp6IndexFileName,row[2]);

            pqf->srcPortIndexFileName=new char[48];
            strcpy(pqf->srcPortIndexFileName,row[3]);

            pqf->dstIpIndexFileName=new char[48];
            strcpy(pqf->dstIpIndexFileName,row[4]);

            pqf->dstIp6IndexFileName=new char[48];
            strcpy(pqf->dstIp6IndexFileName,row[5]);

            pqf->dstPortIndexFileName=new char[48];
            strcpy(pqf->dstPortIndexFileName,row[6]);
            
            pqf->protoIndexFileName=new char[48];
            strcpy(pqf->protoIndexFileName,row[7]);

            pqf->tsIndexFileName=new char[48];
            strcpy(pqf->tsIndexFileName,row[8]);

            queryFileSet.push(pqf);
            num++;
        }
        mysql_free_result(res);
    }
    return num;
}

void clearQueue(){
    while(!queryFileSet.empty()){
        pQueryFile pqf=queryFileSet.front();
        if(pqf->dataFileName) delete pqf->dataFileName;
        if(pqf->srcIpIndexFileName) delete pqf->srcIpIndexFileName;
        if(pqf->srcIp6IndexFileName) delete pqf->srcIp6IndexFileName;
        if(pqf->srcPortIndexFileName) delete pqf->srcPortIndexFileName;
        if(pqf->dstIpIndexFileName) delete pqf->dstIpIndexFileName;
        if(pqf->dstIp6IndexFileName) delete pqf->dstIp6IndexFileName;
        if(pqf->dstPortIndexFileName) delete pqf->dstPortIndexFileName;
        if(pqf->protoIndexFileName) delete pqf->protoIndexFileName;
        if(pqf->tsIndexFileName) delete pqf->tsIndexFileName;
        delete pqf;
        queryFileSet.pop();
    }    
}

int getQueryTime(const char *raw,char *formated){
    time_t now = time(0);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    int len=strlen(raw);
    if(len==19){
        memcpy(formated,raw,19);
    }else if(len==14){
        strftime(formated,48,"%Y-",&tm_now);
        memcpy(formated+5,raw,14);
    }else if(len==8){
        strftime(formated,48,"%Y-%m-%d ",&tm_now);
        memcpy(formated+11,raw,8);
    }else if(len==10){
        if(!strchr(raw,':')){
            memcpy(formated,raw,10);
            memcpy(formated+10," 00:00:00",9);
        }else{
            formated[0]=0;
            return -1;
        }
    }else if(len==5){
        if(!strchr(raw,':')){
            strftime(formated,48,"%Y-",&tm_now);
            memcpy(formated+5,raw,5);
            memcpy(formated+10," 00:00:00",9);
        }else{
            formated[0]=0;
            return -1;
        }
    }else{
        formated[0]=-1;
        return -1;
    }
    formated[19]='\0';
    return len;
}

IpInt ipArr2Int(int16_t ipArr[],int af){
    IpInt r;
    if(af == 4){
        uint32_t tmp[4] = {0};
        uint32_t num = 0;
        for(int i = 0; i < 4 && ipArr[i] != -1; ++i) tmp[i] = ipArr[i];
        for(int i = 0; i < 4; ++i) num |= (tmp[i] << (3-i)*8);
        r.ip4 = num;
    }else if(af == 6){
        uint128_t tmp[16] = {0};
        uint128_t num = 0;
        for(int i = 0; i < 16 && ipArr[i] != -1; ++i) tmp[i] = ipArr[i];
        for(int i = 0; i < 16; ++i) num |= (tmp[i] << (15-i)*8);
        r.ip6 = num;
    }
    return r;
}

int tGetOffsetArr(const char *path,const char *indexName,char *indexValue){
    int matchNum = 0;
    if(!strcmp(indexName,index_category_str[SIP]) || \
            !strcmp(indexName,index_category_str[DIP])){
        int16_t ip[8];
        char *mask_num_str = strrchr(indexValue,'/');
        if(mask_num_str){
            uint8_t lo = 0, hi = 255;
            int mask_num = IPV4_BIT_LEN;
            if(process_ip_netmask(indexValue,mask_num_str,ip,mask_num,lo,hi,4) == -1) return -1;
            ip[mask_num/8] = lo;
            IpInt ipi = ipArr2Int(ip,4);
            uint32_t low = ipi.ip4;

            ip[mask_num/8] = hi;
            for(int i = mask_num/8+1; i < 4; ++i) ip[i] = 255;
            ipi = ipArr2Int(ip,4);
            uint32_t high = ipi.ip4;

            if((matchNum = getIntervalILFromIndexArr(path,&low,&high))==-1){
                log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s",indexName);
                log(ERROR,"fail to get offset array for %s",indexName);
               return -1;
            }
        }else{
            if(ipStr2IntArr(indexValue,ip) == -1) return -1;
            if(ip[3] != -1){
                IpInt ipi = ipArr2Int(ip,4);
                uint32_t target = ipi.ip4;
                matchNum = getILFromIndexArr(path,&target,0);
                if(matchNum == -1){
                    log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s",indexName);
                    log(ERROR,"fail to get offset array for %s",indexName);
                    return -1;
                }
            }else{
                int idx = 0;
                while(ip[idx] != -1) idx++;
                IpInt ipi = ipArr2Int(ip,4);
                uint32_t low = ipi.ip4;
                while(idx < 4) ip[idx++] = 255;
                ipi = ipArr2Int(ip,4);
                uint32_t high = ipi.ip4;
                matchNum = getIntervalILFromIndexArr(path,&low,&high);
                if(matchNum == -1){
                    log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s",indexName);
                    log(ERROR,"fail to get offset array for %s",indexName);
                    return -1;
                }
            }
        }
    }else if(!strcmp(indexName,index_category_str[SIP6]) || !strcmp(indexName,index_category_str[DIP6])){
        int16_t ip6[32];
        char *mask_num_str = strrchr(indexValue,'/');
        if(mask_num_str){
            uint8_t lo = 0, hi = 255;
            int mask_num = IPV6_BIT_LEN;
            if(process_ip_netmask(indexValue,mask_num_str,ip6,mask_num,lo,hi,6) == -1) return -1;

            ip6[mask_num/8] = lo;
            IpInt ipi = ipArr2Int(ip6,6);
            uint128_t low = ipi.ip6;

            ip6[mask_num/8] = hi;
            for(int i = mask_num/8+1; i < 16; ++i) ip6[i] = 255;
            ipi = ipArr2Int(ip6,6);
            uint128_t high = ipi.ip6;

            if((matchNum = getIntervalILFromIndexArr(path,&low,&high))==-1){
                log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s",indexName);
                log(ERROR,"fail to get offset array for %s",indexName);
               return -1;
            }
        }else{
            if(ip6Str2IntArr(indexValue,ip6,2*IPV6_ADDR_LEN) == -1) return -1;

            IpInt ip6i = ipArr2Int(ip6,6);
            uint128_t target = ip6i.ip6;

            if((matchNum = getILFromIndexArr(path,&target,0)) == -1){
                log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s",indexName);
                log(ERROR,"fail to get offset array for %s",indexName);
                return -1;
            }
        }
    }else if(!strcmp(indexName,index_category_str[TS])){
        char formatedTime[64];
        int len = getQueryTime(indexValue,formatedTime);
        if(len == -1){
            log_to_buf(err_msg+strlen(err_msg),ERROR,"invalid timestamp: %s",indexValue);
            log(ERROR,"invalid timestamp: %s",indexValue);
            return -1;
        }

        uint64_t t = date_to_seconds(formatedTime);
        if(t == UINT64_MAX){
            log_to_buf(err_msg+strlen(err_msg),ERROR,"invalid timestamp: %s",indexValue);
            log(ERROR,"invalid timestamp: %s",indexValue);
            return -1;
        }
        
        uint32_t target = t;
        if((matchNum = getILFromIndexArr(path,&target,0)) == -1){
            log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s",indexName);
            log(ERROR,"fail to get offset array for %s",indexName);
            return -1;
        }
    }else if(!strcmp(indexName,index_category_str[SPORT]) || \
            !strcmp(indexName,index_category_str[DPORT])){
        int port = portStr2Int(indexValue);
        if(port == -1) return -1;
        
        uint16_t target = port;
        if((matchNum = getILFromIndexArr(path,&target,0)) == -1){
            log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s",indexName);
            log(ERROR,"fail to get offset array for %s",indexName);
            return -1;
        }
    }else if(!strcmp(indexName,index_category_str[PROTO])){
        int proto  = protoStr2Int(indexValue);
        if(proto == -1) return -1;

        uint16_t target = proto;
        if((matchNum = getILFromIndexArr(path,&target,0)) == -1){
            log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s",indexName);
            log(ERROR,"fail to get offset array for %s",indexName);
            return -1;
        }
    }else{
        log_to_buf(err_msg+strlen(err_msg),ERROR,"unknown index type: %s",indexName);
        log(ERROR,"unknown index type: %s",indexName);
        return -1;
    }

    return matchNum;
}


int tGetOffsetArr_range(const char *path,const char *indexName,const char *low,const char *high,int lb,int hb){
    int matchNum = 0;

    if(!strcmp(indexName,index_category_str[SPORT]) || \
            !strcmp(indexName,index_category_str[DPORT])){
        uint16_t lo = 0, hi = 65535;
        if(low != NULL){
            lo = portStr2Int(low);
            if(lo == -1) return -1;
            lo += lb;
            if(lo > 65535){
                log_to_buf(err_msg+strlen(err_msg),ERROR,"port out of range");
                log(ERROR,"port out of range");
                return -1;
            }
        }
        if(high != NULL){
            hi = portStr2Int(high);
            if(hi == -1) return -1;
            hi -= hb;
            if(hi < 0){
                log_to_buf(err_msg+strlen(err_msg),ERROR,"port out of range");
                log(ERROR,"port out of range");
                return -1;
            }
        }

        uint16_t low = lo, high = hi;
        if((matchNum = getIntervalILFromIndexArr(path,&low,&high)) == -1){
            log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s range",indexName);
            log(ERROR,"fail to get offset array for %s range",indexName);
            return -1;
        }
    }else if(!strcmp(indexName,index_category_str[SIP]) || !strcmp(indexName,index_category_str[DIP]) || \
            !strcmp(indexName,index_category_str[SIP6]) || !strcmp(indexName,index_category_str[DIP6])){

        uint32_t target4 = 0;
        uint128_t target6 = 0;
        int af = 4;
        if(!strcmp(indexName,index_category_str[SIP6]) || !strcmp(indexName,index_category_str[DIP6])) af = 6;
        int ip_len = (af == 4 ? 4 : 16);

        int16_t ip[2*IPV6_ADDR_LEN];
        char indexValue[256];

        if(high){
            strcpy(indexValue,high);
            char *mask_num_str = strrchr(indexValue,'/');
            if(mask_num_str){
                uint8_t lo = 0, hi = 255;
                int mask_num = (af == 4 ? IPV4_BIT_LEN : IPV6_BIT_LEN);
                if(process_ip_netmask(indexValue,mask_num_str,ip,mask_num,lo,hi,af) == -1) return -1;
                int b = mask_num / 8;
                if(hb){
                    int lift = 8 - mask_num%8;
                    if(lo == 0){
                        int i = b - 1;
                        ip[i+1] = 255;
                        while(i >= 0){
                            if(ip[i]){
                                ip[i]--;
                                break;
                            }else ip[i--] = 255; 
                        }
                        if(i < 0) return 0;
                    }else{
                        ip[b++] = (lo - (1 << lift));
                    }
                }else ip[b++] = lo;
                for(;b < ip_len; ++b) ip[b] = 0;

                IpInt ipi = ipArr2Int(ip,af);

                if(af == 4){
                    target4 = ipi.ip4;
                    matchNum = getILFromIndexArr(path,&target4,-1);
                }
                else{
                    target6 = ipi.ip6;
                    matchNum = getILFromIndexArr(path,&target6,-1);
                } 

                if(matchNum == -1){
                    log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s range",indexName);
                    return -1;
                }
            }else{
                if(af == 4){
                    if(ipStr2IntArr(indexValue,ip) == -1) return -1;
                }
                else{
                    if(ip6Str2IntArr(indexValue,ip,2*IPV6_ADDR_LEN) == -1) return -1;
                }
                if(hb){
                    int i = (af == 4 ? 3 : 15);
                    if(ip[i] == 0){
                        ip[i--] = 255;
                        while(i >= 0){
                            if(ip[i]){
                                ip[i]--;
                                break;
                            }else ip[i--] = 255;
                        }
                        if(i < 0) return 0;
                    }else ip[i]--;
                }

                IpInt ipi = ipArr2Int(ip,af);

                if(af == 4){
                    target4 = ipi.ip4;
                    matchNum = getILFromIndexArr(path,&target4,-1);
                }
                else{
                    target6 = ipi.ip6;
                    matchNum = getILFromIndexArr(path,&target6,-1);
                } 

                if(matchNum == -1){
                    log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s range",indexName);
                    return -1;
                }
            }
        }else if(low){
            strcpy(indexValue,low);
            char *mask_num_str = strrchr(indexValue,'/');
            if(mask_num_str){
                uint8_t lo = 0, hi = 255;
                int mask_num = (af == 4 ? IPV4_BIT_LEN : IPV6_BIT_LEN);
                //int mask_num = 32;
                if(process_ip_netmask(indexValue,mask_num_str,ip,mask_num,lo,hi,af) == -1) return -1;
                int b = mask_num / 8;
                if(lb){
                    int lift = 8 - mask_num%8;
                    uint16_t tmp = hi;
                    tmp++;
                    if(tmp >= 256){
                        int i = b - 1;
                        ip[i+1] = 0;
                        while(i >= 0){
                            if(ip[i] < 255){
                                ip[i]++;
                                break;
                            }else ip[i--] = 0; 
                        }
                        if(i < 0) return 0;
                    }else{
                        ip[b++] = tmp; 
                    }
                }else ip[b++] = lo;
                for(;b < ip_len; ++b) ip[b] = 0;

                IpInt ipi = ipArr2Int(ip,af);

                if(af == 4){
                    target4 = ipi.ip4;
                    matchNum = getILFromIndexArr(path,&target4,1);
                }
                else{
                    target6 = ipi.ip6;
                    matchNum = getILFromIndexArr(path,&target6,1);
                } 

                if(matchNum == -1){
                    log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s range",indexName);
                    return -1;
                }
            }else{
                if(af == 4){
                    if(ipStr2IntArr(indexValue,ip) == -1) return -1;    
                }
                else{
                    if(ip6Str2IntArr(indexValue,ip,2*IPV6_ADDR_LEN) == -1) return -1;
                }
                if(lb){
                    //int i = 3;
                    int i = (af == 4 ? 3 : 15);
                    if(ip[i] == 255){
                        ip[i--] = 0;
                        while(i >= 0){
                            if(ip[i] < 255){
                                ip[i]++;
                                break;
                            }else ip[i--] = 0;
                        }
                        if(i < 0) return 0;
                    }else ip[i]++;
                }

                IpInt ipi = ipArr2Int(ip,af);

                if(af == 4){
                    target4 = ipi.ip4;
                    matchNum = getILFromIndexArr(path,&target4,1);
                }
                else{
                    target6 = ipi.ip6;
                    matchNum = getILFromIndexArr(path,&target6,1);
                } 

                if(matchNum == -1){
                    log_to_buf(err_msg+strlen(err_msg),ERROR,"fail to get offset array for %s range",indexName);
                    return -1;
                }
            }
        }else{
            log_to_buf(err_msg+strlen(err_msg),ERROR,"not specify ip range %s",indexName);
            return -1;
        }
    }else{
        log_to_buf(err_msg+strlen(err_msg),ERROR,"not supported range index: %s",indexName);
        log(ERROR,"not supported range index: %s",indexName);
        return -1;
    }

    return matchNum;
}
