/*************************************************************************
	> File Name: toolFunc.h
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Fri 27 Sep 2019 04:38:35 AM PDT
 ************************************************************************/

#ifndef _TOOLFUNC_H
#define _TOOLFUNC_H

#include <stdint.h>
#include <time.h>


#ifdef __cplusplus
extern "C"{
#endif

unsigned getDecimalLen(int num);
char *decimalToStr(int64_t num,char *str);
void getCurrFormatTime(char formatTime[],int len);
int countFiles(const char *path,const char *filter);
time_t date_to_seconds(const char *date);
uint64_t get_file_sz(const char *path);
uint64_t get_dir_sz(const char *path);
int set_open_files_max(uint32_t);
int get_oldest_file(const char *path,char *target_file);
int get_file_id(const char *fileName);
uint64_t get_free_space_sz(const char *dir);
void seconds_to_split_date(time_t ts,char month[],char day[],char hour[]);
void seconds_to_std_date(time_t ts,char stdDate[],int len);
int check_file_exist(const char *filepath);
int del_oldest_dir(const char *path);

#ifdef __cplusplus
}
#endif

#endif
