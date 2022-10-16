/*************************************************************************
	> File Name: QueryInterpreter.h
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Fri 22 Nov 2019 12:44:03 AM PST
 ************************************************************************/

#ifndef _QUERYINTERPRETER_H
#define _QUERYINTERPRETER_H
#include "Driver.h"

#define QUERYLEN 4096
#define VAL_LEN 64

enum relation_optr {EQ = 0, GE, LE, GT, LT, R_OPTR_NUM};
static const char *relation_optr_str[] = {"==",">=","<=",">","<"};

int computeQuery(const char *queryLang,pQueryFile pqf,uint64_t resIndexBuf[]);

#endif
