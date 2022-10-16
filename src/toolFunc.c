/*************************************************************************
	> File Name: toolFunc.c
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Fri 27 Sep 2019 04:45:10 AM PDT
 ************************************************************************/
#include "toolFunc.h"
#include <time.h>
#include <sys/time.h>
#include "common.h"
#include "log.h"
#include <sys/resource.h>
#include <sys/statfs.h>
#include <dirent.h>
#include <ctype.h>

unsigned getDecimalLen(int num){
    unsigned len = 1;
    if(num < 0) ++len;
    while((num /= 10) != 0) ++len;
    return len;
}

char *decimalToStr(int64_t num,char *str){
    char decTable[] = "0123456789";
    uint64_t unum;
    int i = 0, f = 0;
    if(num < 0){
        unum = -num;
        f = 1;
        str[i++] = '-';
    }else unum = num;
    do{
        str[i++] = decTable[unum%10];
        unum /= 10;
    }while(unum);
    str[i] = 0;
    for(int j = f; j <= (i-1)/2; ++j){
        char tmp = str[j];
        str[j] = str[i-1+f-j];
        str[i-1+f-j] = tmp;
    }
    return str;
}

void getCurrFormatTime(char formatTime[],int len){
    time_t sec = time(0);
    struct tm tm_now; 
    localtime_r(&sec, &tm_now);
    strftime(formatTime,len,"%Y-%m-%d %H:%M:%S",&tm_now);
}

time_t date_to_seconds(const char *date){
    struct tm tm;
    //strptime(date,"%Y-%m-%d %H:%M:%S",&tm);
    if(strptime(date,"%Y-%m-%d %H:%M:%S",&tm) == NULL){
        return UINT64_MAX;
    }
    
    time_t tt;
    tt = mktime(&tm);

    return tt;
}

void seconds_to_split_date(time_t ts,char month[],char day[],char hour[]){
    time_t sec = time(0);
    struct tm tm_date;
    localtime_r(&sec, &tm_date);
    decimalToStr(tm_date.tm_year+1900,month);
    decimalToStr(tm_date.tm_mon+1,month+strlen(month));
    decimalToStr(tm_date.tm_mday,day);
    decimalToStr(tm_date.tm_hour,hour);
}

void seconds_to_std_date(time_t ts,char stdDate[],int len){
    time_t sec = time(0);
    struct tm tm_date;
    localtime_r(&sec, &tm_date);
    strftime(stdDate,len,"%Y-%m-%d %H:%M:%S",&tm_date);
}

int set_open_files_max(uint32_t num){
    struct rlimit r_limit;  
    r_limit.rlim_cur = num;  
    r_limit.rlim_max = num;  
    if (setrlimit(RLIMIT_NOFILE, &r_limit) == -1) {  
        log(ERROR,"fail to set max open files");
        return -1;  
    }
    return 0;
}

uint64_t get_file_sz(const char *path){
    struct stat st_buf;
    //memset(&st_buf,0,sizeof(st_buf));
    int res = stat(path,&st_buf);
    if(res == -1){
        log(ERROR,"fail to get size of %s",path);
        return 0;
    }
    return st_buf.st_size;
}

uint64_t get_dir_sz(const char *path){
    if(NULL == path || strlen(path) == 0){
        log(ERROR,"invaild parameter: path in get_dir_sz");
        return 0;
    }
    
    char buf[4096];
    strcpy(buf,path);
    uint64_t dirLen = strlen(path);

    uint64_t tot_sz = 0;

    DIR *dp;
    struct dirent *dirp;
    if((dp=opendir(path))==NULL){
        log(ERROR,"%s doesn't exist",path);
        return tot_sz;
    }

    while((dirp=readdir(dp))!=NULL){
        if(strcmp(dirp->d_name,".") == 0 || strcmp(dirp->d_name,"..") == 0) continue;
        strcpy(buf+dirLen,dirp->d_name);
        struct stat st_buf;
        //memset(&st_buf,0,sizeof(st_buf));
        int res = stat(buf,&st_buf);
        if(res == -1){
            log(ERROR,"fail to get state of %s",buf);
            continue;
        }
        if(S_ISREG(st_buf.st_mode)){
            tot_sz += st_buf.st_size;
        }else if(S_ISDIR(st_buf.st_mode)){
            uint32_t len = strlen(buf);
            buf[len] = '/';
            buf[len+1] = 0;
            tot_sz += get_dir_sz(buf);
        }
    }

    return tot_sz;
}

int get_oldest_file(const char *path,char *target_file){
    if(NULL == target_file || NULL == path || strlen(path) == 0){
        log(ERROR,"invaild parameter: path in get_oldest_file");
        return -1;
    }
    
    char buf[4096];
    strcpy(buf,path);
    uint64_t dirLen = strlen(path);

    DIR *dp;
    struct dirent *dirp;
    if((dp=opendir(path))==NULL){
        log(ERROR,"%s doesn't exist",path);
        return -1;
    }

    time_t oldest = INT_MAX;
    
    while((dirp=readdir(dp))!=NULL){
        if(strcmp(dirp->d_name,".") == 0 || strcmp(dirp->d_name,"..") == 0) continue;
        strcpy(buf+dirLen,dirp->d_name);
        struct stat st_buf;
        //memset(&st_buf,0,sizeof(st_buf));
        int res = stat(buf,&st_buf);
        if(res == -1){
            log(ERROR,"fail to get state of %s",buf);
            continue;
        }
        if(S_ISREG(st_buf.st_mode)){
            if(oldest < st_buf.st_mtime){
                oldest = st_buf.st_mtime;
                strcpy(target_file,dirp->d_name);
            }
        }
    }

    if(oldest == INT_MAX) return 0;

    return 1;
}

static int count_num_in_dir(const char *dir){
    DIR *dp;
    struct dirent *dirp;
    if((dp=opendir(dir))==NULL){
        log(ERROR,"%s does not exist",dir);
        return -1;
    }

    int r = 0;
    while((dirp=readdir(dp))!=NULL){
        if(strcmp(dirp->d_name,".") == 0 || strcmp(dirp->d_name,"..") == 0) continue;
        r++;
    }

    return r;
}

int comp_time_dir(const char *dir1,const char *dir2){
    if(NULL == dir1 || NULL == dir2) return -2;
    int len1 = strlen(dir1), len2 = strlen(dir2);
    if(len1 == 0 || len2 == 0) return -2;
    if(len1 > 3 && len2 > 3){
        int i = 0; 
        for(; i < 4; ++i){
            if(dir1[i] < dir2[i]) return -1;
            else if(dir1[i] > dir2[i]) return 1;
        }
        if(len1 < len2) return -1;
        else if(len1 > len2) return 1;
        for(;i < len1;++i){
            if(dir1[i] < dir2[i]) return -1;
            else if(dir1[i] > dir2[i]) return 1;
        }
        return 0;
    }else{
        if(len1 < len2) return -1;
        else if(len1 > len2) return 1;
        for(int i = 0;i < len1;++i){
            if(dir1[i] < dir2[i]) return -1;
            else if(dir1[i] > dir2[i]) return 1;
        }
        return 0;
    }
}

int del_oldest_dir(const char *path){
    if(NULL == path || strlen(path) == 0){
        log(ERROR,"invaild parameter: path in del_oldest_dir");
        return -1;
    }
    
    char buf[4096];
    strcpy(buf,path);
    uint64_t dirLen = strlen(buf);
    if(buf[dirLen-1] != '/'){
        buf[dirLen]='/';
        buf[dirLen+1]=0;
        dirLen++;
    }

    DIR *dp;
    struct dirent *dirp;
    if((dp=opendir(buf))==NULL){
        log(ERROR,"%s doesn't exist",path);
        return -1;
    }

    uint64_t oldest = UINT64_MAX;
    int dNum = 0;
    char subDir[16] = "999999999";
    
    while((dirp=readdir(dp))!=NULL){
        if(strcmp(dirp->d_name,".") == 0 || strcmp(dirp->d_name,"..") == 0) continue;
        strcpy(buf+dirLen,dirp->d_name);
        struct stat st_buf;
        int res = stat(buf,&st_buf);
        if(res == -1){
            log(ERROR,"fail to get state of %s",buf);
            continue;
        }
        if(S_ISREG(st_buf.st_mode)){
            if(remove(buf) != 0){
                log(ERROR,"fail to delete %s in del_oldest_dir",buf);
            }else dNum++;
        }else if(S_ISDIR(st_buf.st_mode)){
            if(comp_time_dir(dirp->d_name,subDir) < 0){
                strcpy(subDir,dirp->d_name);
            }
            /*
            if(st_buf.st_mtime < oldest){
                oldest = st_buf.st_mtime;
                strcpy(subDir,dirp->d_name);
            }
            */
        }
    }

    //if(oldest < UINT64_MAX){
    if(strcmp(subDir,"999999999") != 0){
        strcpy(buf+dirLen,subDir);
        dNum = del_oldest_dir(buf);
        if(count_num_in_dir(buf)==0){
            if(rmdir(buf)!=0){
                log(ERROR,"%s is empty,but fail to delete it",buf);
                return -1;
            }
        }
    }

    return dNum;
}

int get_file_id(const char *fileName){
    if(NULL == fileName || strlen(fileName) == 0) return -1;
    int len = strlen(fileName);
    const char *s_ptr = fileName + len - 1;
    while(s_ptr >= fileName && isdigit(*s_ptr)) s_ptr--;
    s_ptr++;
    return atoi(s_ptr);
}

uint64_t get_free_space_sz(const char *dir){
	struct statfs diskInfo;
	statfs(dir, &diskInfo);
	uint64_t blocksize = diskInfo.f_bsize; 
    return blocksize * diskInfo.f_bavail;
}

int countFiles(const char *path,const char *filter){
    DIR *dp;
    struct dirent *dirp;
    int fileNum=0;
    if((dp=opendir(path))==NULL){
        int mkRes=mkdir(path,0777);
        if(mkRes==-1){
            log(ERROR,"%s doesn't exist,and create failed!",path);
        }
        return 0;
    }
    while((dirp=readdir(dp))!=NULL){
        if(strstr(dirp->d_name,filter)){
            fileNum++;
        }
    }
    return fileNum;
}

int check_file_exist(const char *filepath){
    int status = access(filepath,F_OK | R_OK);
    if(status == 0) return 1;
    else return 0;
}
