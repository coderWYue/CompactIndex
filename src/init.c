/*************************************************************************
	> File Name: init.c
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Sun 26 Apr 2020 02:58:41 PM CST
 ************************************************************************/

#include "common.h"
#include "log.h"
#include "toolFunc.h"
#include <mysql.h>
#include <fcntl.h>
#include "tdms.h"
#include "index.h"

#define SQL_CMD_LEN 512

static char *create_table_sql = "\
CREATE TABLE storage_info\
(\
    data_file varchar(256) NOT NULL,\
    pkts_num int NOT NULL,\
    src_ip_index_file varchar(256) NOT NULL,\
    src_ip6_index_file varchar(256) NOT NULL,\
    src_port_index_file varchar(256) NOT NULL,\
    dst_ip_index_file varchar(256) NOT NULL,\
    dst_ip6_index_file varchar(256) NOT NULL,\
    dst_port_index_file varchar(256) NOT NULL,\
    proto_index_file varchar(256) NOT NULL,\
    ts_index_file varchar(256) NOT NULL,\
    timestamp datetime NOT NULL,\
    PRIMARY KEY (timestamp)\
)";

//static char *create_db_sql = "create database if not exists tdms_db";

void set_sz_index_structure(void){
    pktAmount = data_buf_sz/AVG_PKT_SIZE;
    pktNodeAmount = pktAmount*INDEX_NUM;
    listHeadAmount = TRIE_INDEX_NUM*pktAmount/DIFF_IP_RATIO;
    listHeadHkAmount = TRIE_INDEX_NUM*pktAmount/DIFF_IP_RATIO;
    trieNodeAmount = (listHeadAmount + listHeadHkAmount) / TRIE_NODE_RATIO;
}

static inline int cheack_alloc_status(void *allocatedPtr,const char *spaceName){
    if(NULL == allocatedPtr){
        log_to_buf(tdms_init_status,ERROR,"fail to alloc space for \
                %s in tdms_init",spaceName);
       log(ERROR,"fail to alloc space for \
                %s in tdms_init",spaceName);
        return -1;
    }
    return 0;
}

int alloc_space(void){
#if 1
    dataBuf = (BYTE*)malloc(data_buf_sz + PCAP_FILE_HEAD_SIZE);
    if(cheack_alloc_status(dataBuf,"dataBuf") == -1) return -1;

    digestBuf = (pPktDigest)calloc(pktAmount,sizeof(PktDigest));
    if(cheack_alloc_status(digestBuf,"digestBuf") == -1) return -1;
#endif
    
#if 0
    dataTmp1 = (BYTE*)malloc(data_buf_sz + PCAP_FILE_HEAD_SIZE);
    if(cheack_alloc_status(dataTmp1,"dataBuf") == -1) return -1;
    dataTmp2 = (BYTE*)malloc(data_buf_sz + PCAP_FILE_HEAD_SIZE);
    if(cheack_alloc_status(dataTmp2,"dataBuf") == -1) return -1;

    digestTmp1 = (pPktDigest)calloc(pktAmount,sizeof(PktDigest));
    if(cheack_alloc_status(digestTmp1,"digestBuf") == -1) return -1;
    digestTmp2 = (pPktDigest)calloc(pktAmount,sizeof(PktDigest));
    if(cheack_alloc_status(digestTmp2,"digestBuf") == -1) return -1;
#endif

#if 1
    trieNodePool = (pTrieNode)calloc(trieNodeAmount,sizeof(TrieNode));
    if(cheack_alloc_status(trieNodePool,"trieNodePool") == -1) return -1;

    recListPool = (pListHead)calloc(listHeadAmount,sizeof(ListHead));
    if(cheack_alloc_status(recListPool,"recListPool") == -1) return -1;

    recListHKPool = (pListHeadHasKey4)calloc(listHeadHkAmount,sizeof(ListHeadHasKey4));
    if(cheack_alloc_status(recListHKPool,"recListHKPool") == -1) return -1;

    posNodePool = (pPktPosNode)calloc(pktNodeAmount,sizeof(PktPosNode));
    if(cheack_alloc_status(posNodePool,"posNodePool") == -1) return -1;
#endif

    return 0;
}

int free_space(void){
    free(dataBuf);
    free(digestBuf);
    free(trieNodePool);
    free(recListPool);
    free(recListHKPool);
    free(posNodePool);
    return 0;
}

static int use_db(const char *db){

	char create[SQL_CMD_LEN] = {0};
	sprintf(create,"create database if not exists %s",db);
    if(mysql_real_query(&mysql,create,(unsigned long)strlen(create))){
        mysql_close(&mysql);
        return -1;
    }

	char query[SQL_CMD_LEN] = {0};
    sprintf(query,"use %s",db);
    if(mysql_real_query(&mysql,query,(unsigned long)strlen(query))){
        return -1;
    }
    return 0;
}

static int check_table_exist(const char *table_name){
    char query[512];
    sprintf(query,"show tables like '%s'",table_name);
    if(mysql_real_query(&mysql,query,(unsigned long)strlen(query))){
        return -1;
    }
    MYSQL_RES *res = mysql_store_result(&mysql);
    if(NULL == res){
        return -1;
    }
    MYSQL_ROW row;
    if((row = mysql_fetch_row(res))){
        return 1;
    }
    mysql_free_result(res);
    return 0;
}

int connect_db(void){
    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql,db_address,db_uname,db_pwd,NULL,db_port,NULL,0)){
        log_to_buf(tdms_init_status,ERROR,"fail to connect to db in tdms_init\n\
                db_address:%s\n\
                db_port:%u\n\
                db_uname:%s\n\
                db_pwd:%s\n\
                MYSQL ERROR:%s\n",db_address,db_port,db_uname,db_pwd,mysql_error(&mysql));
         log(ERROR,"fail to connect to db in tdms_init\n\
                db_address:%s\n\
                db_port:%u\n\
                db_uname:%s\n\
                db_pwd:%s\n\
                MYSQL ERROR:%s\n",db_address,db_port,db_uname,db_pwd,mysql_error(&mysql));       
        return -1;
    }
    int use_success = use_db(db_name);
    if(use_success == -1){
        log_to_buf(tdms_init_status,ERROR,"MYSQL ERROR:%s,request for creating database is denied",mysql_error(&mysql));
        log(ERROR,"MYSQL ERROR:%s,request for creating database is denied",mysql_error(&mysql));
        mysql_close(&mysql);
        return -1;
    }
    int exist = check_table_exist("storage_info");
    if(exist == -1){
        log_to_buf(tdms_init_status,ERROR,"MYSQL ERROR:%s",mysql_error(&mysql));
        log(ERROR,"MYSQL ERROR:%s",mysql_error(&mysql));
        mysql_close(&mysql);
        return -1;
    }else if(exist == 0){
        if(mysql_real_query(&mysql,create_table_sql,(unsigned long)strlen(create_table_sql))){
            log_to_buf(tdms_init_status,ERROR,"fail to create table in db,MYSQL ERROR:%s",mysql_error(&mysql));
            log(ERROR,"fail to create table in db,MYSQL ERROR:%s",mysql_error(&mysql));

            mysql_close(&mysql);
          
            return -1;
        }
        return 0;
    }else return 0;
}

void init_capture_zone(void){
    uint64_t dataUnit = data_buf_sz / core_num;
    uint64_t digestUnit = pktAmount / core_num;

    //dataBuf = dataTmp1;
    //digestBuf = digestTmp1;

    dataZones[0] = dataBuf + PCAP_FILE_HEAD_SIZE;
    digestZones[0] = digestBuf;
    avais[0] = 0;
    dataPoses[0] = 0;

    for(int i = 1; i < core_num; ++i){
        dataZones[i] = dataBuf + PCAP_FILE_HEAD_SIZE + i*dataUnit;
        digestZones[i] = digestBuf + i*digestUnit;
        avais[i] = 0;
        dataPoses[i] = 0;
    }
    digestBound = digestUnit;
    dataBound = dataUnit - OVERFLOW_BUF;
    //log(INFO,"digestBound is %d,dataBound is %lu\n",digestBound,dataBound);
}


void writeHdrToDataBuf(void){
    pcapFileHead hdr = {
        .magic = {0xd4,0xc3,0xb2,0xa1},
        .major = 2,
        .minor = 4,
        .thiszone = 0,
        .sigfigs = 0,
        .snaplen = 0x0000ffff,
        .linktype = 1
    };
    memcpy(dataBuf,&hdr,PCAP_FILE_HEAD_SIZE);
}

int open_log_file(void){
    char fileName[256];
    getCurrFormatTime(fileName,250);
    strcat(fileName,".log");

    char logPath[512];
    strcpy(logPath,log_storage_dir);
    uint32_t dirLen = strlen(log_storage_dir);
    if(logPath[dirLen-1] != '/'){
        logPath[dirLen] = '/';
        logPath[dirLen+1] = 0;
    }
    strcat(logPath,fileName);

    log_fp = fopen(logPath,"a+"); 
    if(log_fp == NULL){
        log_to_buf(tdms_init_status,ERROR,"fail to open log file: %s",\
                logPath); 
       log(ERROR,"fail to open log file: %s",\
                logPath);
        return -1;
    }
    return 0;
}
