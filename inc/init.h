/*************************************************************************
	> File Name: init.h
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Mon 27 Apr 2020 03:55:04 PM CST
 ************************************************************************/

#ifndef _INIT_H
#define _INIT_H

/*
#ifndef MAX_CORE_NUM
#define MAX_CORE_NUM 256
#endif
*/

void set_sz_index_structure(void);
int alloc_space(void);
int connect_db(void);
void init_capture_zone(void);
void writeHdrToDataBuf(void);
int open_log_file(void);
int free_space(void);

#endif
