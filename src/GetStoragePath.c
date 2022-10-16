/*************************************************************************
	> File Name: GetStoragePath.c
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Sat 05 Oct 2019 08:36:35 PM PDT
 ************************************************************************/

#include "GetStoragePath.h"
#include "log.h"
#include "toolFunc.h"
#include "tdms.h"
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>

static char cDir[64];

char dataFilePath[MAX_PATH_LENGTH];
char sipIndexFilePath[MAX_PATH_LENGTH];
char dipIndexFilePath[MAX_PATH_LENGTH];
char sip6IndexFilePath[MAX_PATH_LENGTH];
char dip6IndexFilePath[MAX_PATH_LENGTH];
char sportIndexFilePath[MAX_PATH_LENGTH];
char dportIndexFilePath[MAX_PATH_LENGTH];
char protoIndexFilePath[MAX_PATH_LENGTH];
char tsIndexFilePath[MAX_PATH_LENGTH];

time_t flush_ts;

int checkDir(const char *dir){
    int dirStatus = access(dir, F_OK);
    if(dirStatus == 0){
        dirStatus = access(dir, R_OK | W_OK | X_OK);
        if(dirStatus != 0){
            log(ERROR,"not adequate permission for %s",dir);
        }
        return dirStatus;
    }else{
        int createRes = mkdir(dir,0755);
        if(createRes != 0) log(ERROR,"fail to create dir %s",dir);
        return createRes;
    }
}

int countFile(const char *dir,const char *toMatch){
    DIR *dp;
    struct dirent *dirp;
    if((dp = opendir(dir)) == NULL){
        log(ERROR,"fail to open dir %s",dir);
        return -1;
    }
    int all = (NULL == toMatch || !strcmp(toMatch,"")); 
    int num = 0;
    while((dirp = readdir(dp)) != NULL){
        if(all || strstr(dirp->d_name,toMatch)) num++;
    }
    return num;
}

static int set_child_dir(const char *dataDir,const char *indexDir){
    char dataPath[256],indexPath[256];
    strcpy(dataPath,dataDir);
    strcpy(indexPath,indexDir);
    char month[16],day[16],hour[16];
    time(&flush_ts);
    seconds_to_split_date(flush_ts,month,day,hour);
    strcat(dataPath,month);
    strcat(dataPath,"/");
    strcat(indexPath,month);
    strcat(indexPath,"/");
    strcpy(cDir,month);
    strcat(cDir,"/");
    if(checkDir(dataPath) != 0 || checkDir(indexPath) != 0){
        log_to_file(log_fp,ERROR,"fail to set storage dir,data dir:%s,index dir:%s",dataPath,indexPath);
        exit(0);
    }
    strcat(dataPath,day);
    strcat(dataPath,"/");
    strcat(indexPath,day);
    strcat(indexPath,"/");
    strcat(cDir,day);
    strcat(cDir,"/");
    if(checkDir(dataPath) != 0 || checkDir(indexPath) != 0){
        log_to_file(log_fp,ERROR,"fail to set storage dir,data dir:%s,index dir:%s",dataPath,indexPath);
        exit(0);
    }
    strcat(dataPath,hour);
    strcat(dataPath,"/");
    strcat(indexPath,hour);
    strcat(indexPath,"/");
    strcat(cDir,hour);
    strcat(cDir,"/");
    if(checkDir(dataPath) != 0 || checkDir(indexPath) != 0){
        log_to_file(log_fp,ERROR,"fail to set storage dir,data dir:%s,index dir:%s",dataPath,indexPath);
        exit(0);
    }
    return 0;
}

void getFilePath(char *path,const char *dir,const char *target){
    strcpy(path,dir);
    strcat(path,cDir);
    if(checkDir(path) != 0){
        log_to_file(log_fp,ERROR,"fail to set storage dir:%s",path);
        exit(0);
    } 
    int id = countFile(path,target);
    if(id < 0){
        log_to_file(log_fp,ERROR,"fail to set storage file name:%s%d",target,id);
        exit(0);
    } 
    strcat(path,target);
    char idStr[16];
    strcat(path,decimalToStr(id,idStr));
}

void getStoragePath(){
    set_child_dir(data_storage_dir,index_storage_dir);
    getFilePath(dataFilePath,data_storage_dir,"data");
    getFilePath(sipIndexFilePath,index_storage_dir,index_category_str[SIP4]);
    getFilePath(dipIndexFilePath,index_storage_dir,index_category_str[DIP4]);
    getFilePath(sip6IndexFilePath,index_storage_dir,index_category_str[SIP6]);
    getFilePath(dip6IndexFilePath,index_storage_dir,index_category_str[DIP6]);
    getFilePath(sportIndexFilePath,index_storage_dir,index_category_str[SPORT]);
    getFilePath(dportIndexFilePath,index_storage_dir,index_category_str[DPORT]);
    getFilePath(protoIndexFilePath,index_storage_dir,index_category_str[PROTO]);
    getFilePath(tsIndexFilePath,index_storage_dir,index_category_str[TS]);
}

