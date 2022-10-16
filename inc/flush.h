/*************************************************************************
	> File Name: flush.h
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Mon 07 Oct 2019 01:11:59 AM PDT
 ************************************************************************/

#ifndef _FLUSH_H
#define _FLUSH_H

#include <stdint.h>

#define DUMP_CATE_NUM 9 
#define DUMP_BUF_SIZE 8192

enum dump_category { DATA = 0, SIP_INDEX, DIP_INDEX, SIP6_INDEX, DIP6_INDEX, SPORT_INDEX, DPORT_INDEX, PROTO_INDEX, TS_INDEX};

typedef enum dump_category category;

void flush(int compress_enable);


#endif
