/*************************************************************************
	> File Name: retrieve_c_api.h
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Mon 11 May 2020 06:02:49 PM CST
 ************************************************************************/

#ifndef _RETRIEVE_C_API_H
#define _RETRIEVE_C_API_H
#define MAX_RESULT_PATH_ELN 4096
#define MAX_ERROR_MESSAGE_LEN 4096

extern char result_pathes[MAX_RESULT_PATH_ELN];
extern char error_msg[MAX_ERROR_MESSAGE_LEN];
extern uint64_t res_sz;
extern int p_error;

#ifdef __cplusplus
extern "C"{
#endif

int exe_query(const char *query,int compress_enable);

#ifdef __cplusplus
}
#endif
#endif
