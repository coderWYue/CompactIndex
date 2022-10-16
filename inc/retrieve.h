/*************************************************************************
	> File Name: retrieve.h
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Wed 06 May 2020 09:58:32 PM CST
 ************************************************************************/

#ifndef _RETRIEVE_H
#define _RETRIEVE_H
#define GB 1024*1024*1024
#define MAX_PKT_NUM_PER_RETR 0x1000000
#define MAX_FILE_NAME_LEN 8192
#define MAX_RESULT_LEN 65536
#define MAX_ERR_MSG_LEN 4096
#define MAX_TIME_INTERVAL_NUM 64
#define ID_LEN 256

extern char resFileName[MAX_FILE_NAME_LEN];
extern char resFileNames[MAX_RESULT_LEN];
extern char err_msg[MAX_ERR_MSG_LEN];
extern uint64_t result_sz;
extern int partial_error;
extern int newFlag;

extern uint64_t s_time_arr[MAX_TIME_INTERVAL_NUM];
extern uint64_t e_time_arr[MAX_TIME_INTERVAL_NUM];
extern int time_interval_num;

extern char identifier[ID_LEN];

enum command_type {RETR = 0,TYPE_NUM};
static const char *commands[] = {"retrieve"};

int run_query(const char *query ,int compress_enable);
#endif
