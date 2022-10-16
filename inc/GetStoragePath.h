/*************************************************************************
	> File Name: GetStoragePath.h
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Sat 05 Oct 2019 08:24:12 PM PDT
 ************************************************************************/

#ifndef _GETSTORAGEPATH_H
#define _GETSTORAGEPATH_H
#include<time.h>
#define MAX_PATH_LENGTH 512

enum index_category { SIP4 = 0, DIP4, SIP6, DIP6, SPORT, DPORT, PROTO, TS, ATTR_NUM};
static const char *index_category_str[] = { "sip4_","dip4_","sip6_","dip6_","sport","dport","proto","ts" };

extern char dataFilePath[];
extern char sipIndexFilePath[];
extern char dipIndexFilePath[];
extern char sip6IndexFilePath[];
extern char dip6IndexFilePath[];
extern char sportIndexFilePath[];
extern char dportIndexFilePath[];
extern char protoIndexFilePath[];
extern char tsIndexFilePath[];

extern time_t flush_ts;

void getStoragePath();
#endif
