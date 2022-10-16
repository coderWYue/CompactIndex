/*************************************************************************
	> File Name: test.c
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Sat 09 May 2020 10:06:11 PM CST
 ************************************************************************/

#include "common.h"
#include "log.h"
#include "./branch/tdms.h"
#include "net.h"
#include <fcntl.h>
#include <pthread.h>

void init_init_param(tdmsInitParam *para){
    para->core_num = 1;
    para->db_port = 3307;
    para->warning_thres = 0.7;
    para->rewrite_thres = 0.85;
    para->search_ret_sz = 5;
    para->data_buf_sz = (uint64_t)1024*1024*1024;
    para->disk_cap = (uint64_t)1024*1024*1024*7;
    strcpy(para->data_storage_dir,"/home/tdms_b/test/data/");
    strcpy(para->index_storage_dir,"/home/tdms_b/test/ind/");
    strcpy(para->log_storage_dir,"/home/tdms_b/test/log/");
    strcpy(para->result_storage_dir,"/home/tdms_b/test/result/");
    strcpy(para->db_address,"localhost");
    strcpy(para->db_uname,"test");
    strcpy(para->db_pwd,"135246");
    strcpy(para->db_name,"tdms_db");
}

int main(int argc,char *argv[]){
    tdmsInitParam init_param;
    init_init_param(&init_param);
    int init_res = tdms_init(&init_param,1);

    /*
    int qid = atoi(argv[2]);

    FILE *fp = fopen(argv[1],"r");
    if(NULL == fp){
        printf("fail to open file %s\n",argv[1]);
        return 0;
    }

    int i = 0;
    char query[512] = {0};
    while(fgets(query,512,fp)){
        if(i == qid) break;
        else i++;
    }

    query[strlen(query)-1] = 0;

    printf("query:%s\n",query);
    */

    tdmsRetrieveResult retrive_ret;
    struct timeval start,end;
    gettimeofday(&start,NULL);

    char *query = "retrieve -- sip == 192.168.0.1";
    int query_res = tdms_retrieve_proc(query,&retrive_ret,0);

    gettimeofday(&end,NULL);

    uint64_t dura = (end.tv_sec - start.tv_sec)*1000000 + end.tv_usec - start.tv_usec;
    printf("retrieve cost %lu us\n",dura);

    if(query_res == -1){
        printf("%s",retrive_ret.error_msg);
        tdms_uninit(1,1);
        return 0;
    }else{
        printf("%d packets retrieved, total size: %lu, result path:%s\n",retrive_ret.pkt_num,retrive_ret.reslut_sz,retrive_ret.result_path);
    }

    tdms_uninit(1,0);
}
