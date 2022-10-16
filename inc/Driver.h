/*************************************************************************
	> File Name: Driver.h
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Fri 22 Nov 2019 01:53:51 AM PST
 ************************************************************************/

#ifndef _DRIVER_H
#define _DRIVER_H
#include<mysql.h>
#include<queue>

#define ELEMENT_NUM 0x100000
#define VALID_OFF_BOUND 0xffffffff00000000

typedef struct queryFile{
    char *dataFileName;
    char *srcIpIndexFileName;
    char *srcIp6IndexFileName;
    char *dstIpIndexFileName;
    char *dstIp6IndexFileName;
    char *srcPortIndexFileName;
    char *dstPortIndexFileName;
    char *protoIndexFileName;
    char *tsIndexFileName;
    queryFile(){
        dataFileName=NULL;
        srcIpIndexFileName=NULL;
        srcIp6IndexFileName=NULL;
        dstIpIndexFileName=NULL;
        dstIp6IndexFileName=NULL;
        srcPortIndexFileName=NULL;
        dstPortIndexFileName=NULL;
        protoIndexFileName=NULL;
        tsIndexFileName=NULL;
    }
}queryFile,*pQueryFile;

enum index_category { SIP = 0, SIP6, DIP, DIP6, SPORT, DPORT, PROTO, TS, IND_NUM };
static const char *index_category_str[] = { "sip","sip6","dip","dip6","sport","dport","proto","ts"};


extern std::queue<pQueryFile> queryFileSet;

int getOffsetArr_range(const char *path,const char *indexName,const char *low,const char *high,int lb,int hb);
int getOffsetArr(const char *path,const char *indexName,char *indexValue);
int tGetOffsetArr_range(const char *path,const char *indexName,const char *low,const char *high,int lb,int hb);
int tGetOffsetArr(const char *path,const char *indexName,char *indexValue);
void clearQueue();
int getQueryFileSet_no_db(bool flag[]);
int getQueryFileSet(MYSQL *mysgl,const char *startTime,const char *endTime);
int getQueryTime(const char *raw,char *formated);

#endif
