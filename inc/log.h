#ifndef _LOG_H__
#define _LOG_H__

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <stdint.h>

#ifdef __cplusplus 
extern "C" {
#endif


enum log_level
{ 
    DEBUG = 0,
    INFO, 
    WARNING, 
    ERROR,
    UPDATE, 
    APPEND, 
    LOG_TYPE_NUM
};

static enum log_level this_log_level = DEBUG;

static const char *log_level_str[] = { "DEBUG", "INFO", "WARNING", "ERROR", "UPDATE", "APPEND"};


void tdms_init_log();
void tdms_log_reset_burst_cnt(int val);
void tdms_log_set_burst_cnt(int new_val );
int  tdms_log_get_burst_cnt(void );
/*====================================================
函数名: set_log_switch_value
功能:  设置日志开关
入参:unsigned char value
出参: 
返回值: 
作者:  
时间:
说明:I_SET_FLAG 打开日志
     I_RESET_FLAG 关闭日志
======================================================*/
void tdms_set_log_switch_value(unsigned char value);
/*====================================================
函数名: get_log_switch_value
功能:  获取日志开关值
入参:
出参: 
返回值: I_SET_FLAG 打开日志
     I_RESET_FLAG 关闭日志
作者:  
时间:
说明:
======================================================*/
unsigned char tdms_get_log_switch_value();




// #define LOG_DEBUG
/*
#ifdef LOG_DEBUG
	#define log_it(fmt, level_str, ...) \
		fprintf(stderr, "[%s:%u] %s: " fmt  "\n", __FILE__, __LINE__, \
				level_str, ##__VA_ARGS__);
#else
	#define log_it(fmt, level_str, ...) \
		fprintf(stderr, "%s: " fmt "\n", level_str, ##__VA_ARGS__);
#endif
*/

#define log_it_to_file(fd, fmt, level_str, ...) \
    fprintf(fd, "%s: " fmt "\n", level_str, ##__VA_ARGS__);

#define log_it_to_buf(buf, fmt, level_str, ...) \
    sprintf(buf, "%s: " fmt "\n", level_str, ##__VA_ARGS__);

#define log_to_buf(buf, level, fmt, ...) \
	do { \
		if (level < this_log_level || level >= LOG_TYPE_NUM) \
			break; \
		log_it_to_buf(buf, fmt, log_level_str[level], ##__VA_ARGS__); \
	} while (0)

#define log_to_file(fd, level, fmt, ...) \
	do { \
		if (level < this_log_level || level >= LOG_TYPE_NUM) \
			break; \
		log_it_to_file(fd, fmt, log_level_str[level], ##__VA_ARGS__); \
	} while (0)
	
#ifdef LOG_DEBUG 
#define  log(type, format, ...) \
    do { \
        _tdms_log_msg(type, __FILE__, __LINE__, format, ##__VA_ARGS__); \
    } while (0) 
#else 
#define  log(type, format, ...) \
    do { \
        if (type != DEBUG) _tdms_log_msg(type, __FILE__, __LINE__, format, ##__VA_ARGS__); \
    } while (0) 
#endif
                        
void _tdms_log_msg(int32_t type, const char *file, int32_t line, const char *format, ...);
/*
#define log(level, fmt, ...) \
	do { \
		if (level < this_log_level || level >= LOG_TYPE_NUM) \
			break; \
		log_it(fmt, log_level_str[level], ##__VA_ARGS__); \
	} while (0)
*/

#ifdef __cplusplus
}
#endif
#endif
