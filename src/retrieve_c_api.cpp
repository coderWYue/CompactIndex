/*************************************************************************
	> File Name: retrieve_c_api.c
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Mon 11 May 2020 06:04:12 PM CST
 ************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "retrieve_c_api.h"
#include "retrieve.h"

char result_pathes[MAX_RESULT_PATH_ELN];
char error_msg[MAX_ERROR_MESSAGE_LEN];
uint64_t res_sz;
int p_error;

#ifdef __cplusplus
extern "C"{
#endif

int exe_query(const char *query,int compress_enable ){
    int matched = run_query(query,compress_enable);
    strcpy(result_pathes,resFileNames);
    strcpy(error_msg,err_msg);
    res_sz = result_sz;
    p_error = partial_error;
    printf("retrieve OK!\n");
    return matched;
}

#ifdef __cplusplus
}
#endif
