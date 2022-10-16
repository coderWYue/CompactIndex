#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>

#include "common.h"
#include "log.h"
#include "tdms_atomic.h"



#define LOG_TO_STDOUT      0
#define LOG_TO_STDERR      1
#define LOG_TO_LOGFILE     2
#define LOG_TO_DIRMAX      3

#define LOG_OPT_TIME       1
#define LOG_OPT_POS        2
#define LOG_OPT_BAR        4

struct tdms_log_config {
    uint16_t log_flag;
    uint16_t log_opt;
};

TDMS_ATOMIC_DECLARE (int,tdms_log_burst_per_second);   //每秒钟支持的日志个数
static int log_cnt_cfg   = 80;
static char *logfile_file = "/home/istore/logstore";
static char *logfile_name = "/home/istore/logstore/tdms.log";
static unsigned char g_tdms_log_switch = I_SET_FLAG; // I_SET_FLAG 开启日志
								                     // I_RESET_FLAG 关闭日志

static FILE * log_output_fps[LOG_TO_DIRMAX] = {NULL,NULL,NULL };

static struct tdms_log_config log_conf[LOG_TYPE_NUM][LOG_TO_DIRMAX] = 
{
    /* ERROR*/
    {
        { 0, 0 },                                         // stdout
        { 1, LOG_OPT_TIME | LOG_OPT_POS | LOG_OPT_BAR },  // stderr
        { 1, LOG_OPT_TIME | LOG_OPT_POS | LOG_OPT_BAR },  // file
    }, 
    /* ERROR*/
    {
        { 0, 0 },
        { 1, LOG_OPT_TIME | LOG_OPT_POS | LOG_OPT_BAR },
        { 1, LOG_OPT_TIME | LOG_OPT_POS | LOG_OPT_BAR },
    },
    /*INFO*/
    { 
        { 1, LOG_OPT_TIME },
        { 0, 0 },
        { 1, LOG_OPT_TIME },
    },
    /*DEBUG*/
    {
        { 1, LOG_OPT_TIME | LOG_OPT_BAR },
        { 0, 0 },
        { 0, 0 },
    },
    /* tdms_WARN */
    {
        { 0, 0 },
        { 1, LOG_OPT_TIME | LOG_OPT_POS | LOG_OPT_BAR },
        { 1, LOG_OPT_TIME | LOG_OPT_POS | LOG_OPT_BAR },
    },
};

void tdms_init_log()
{
	char str[64] = {0};

    /*open the log file*/
	if(0 != access(logfile_file, F_OK))//判断目录是否存在
	{
		snprintf(str, 64, "mkdir -p %s", logfile_file); //不存在时创建目录
		if(0 != system(str))
		{
			perror("create log dir failed:");
			printf("%s: fail to create dir %s.\n", __func__, logfile_file);
			exit(1);
		}
	}

    FILE *logfile = fopen(logfile_name, "a+");
    if (!logfile) {
        perror("open log file failed");
        exit(errno);
    }
    log_output_fps[LOG_TO_STDOUT]   =   stdout;
    log_output_fps[LOG_TO_STDERR]   =   stderr;
    log_output_fps[LOG_TO_LOGFILE]  =   logfile;

    fprintf(logfile, "\n****************************************");
    fprintf(logfile, "****************************************\n\n");
    fflush(logfile);
   
    TDMS_ATOMIC_SET(tdms_log_burst_per_second, -1);
}


void tdms_log_reset_burst_cnt(int val)
{
    if(val != -1)
        TDMS_ATOMIC_SET(tdms_log_burst_per_second, log_cnt_cfg);
    else     
        TDMS_ATOMIC_SET(tdms_log_burst_per_second, -1);
}

void tdms_log_set_burst_cnt(int new_val )
{
    log_cnt_cfg = new_val;
}

int tdms_log_get_burst_cnt(void )
{
    return log_cnt_cfg ;
}


static inline void log_with_option(int32_t type, int32_t fpno, char *time, 
        const char *pos, const char *msg) 
{
    /* To avoid interleaving output from multiple threads. */
    flockfile(log_output_fps[fpno]);
    
    if (log_conf[type][fpno].log_opt & LOG_OPT_TIME) 
        fprintf(log_output_fps[fpno], "%s  ", time); 

    if (log_conf[type][fpno].log_opt & LOG_OPT_BAR) 
        fprintf(log_output_fps[fpno], "[%s] ", log_level_str[type]); 

    fprintf(log_output_fps[fpno], "%s ", msg); 

    if (log_conf[type][fpno].log_opt & LOG_OPT_POS) 
        fprintf(log_output_fps[fpno], "(%s)", pos); 

    fprintf(log_output_fps[fpno], "\n"); 

    fflush(log_output_fps[fpno]);

    funlockfile(log_output_fps[fpno]);

    return;
}

static inline void log_to_tdmslog_file(int32_t type, char *time, const char *pos,
        const char *msg) 
{
    if (log_conf[type][LOG_TO_STDOUT].log_flag)  
        log_with_option(type, LOG_TO_STDOUT, time, pos, msg); 

    if (log_conf[type][LOG_TO_STDERR].log_flag) 
        log_with_option(type, LOG_TO_STDERR, time, pos, msg); 

    if (log_conf[type][LOG_TO_LOGFILE].log_flag)
        log_with_option(type, LOG_TO_LOGFILE, time, pos, msg); 
}

void _tdms_log_msg(int32_t type, const char *file, int32_t  line, const char *format, ...) 
{
	/* ERROR 输出日志 */
	if (I_RESET_FLAG == g_tdms_log_switch )
	{	
		return;
	}
    
    /* theck the burst*/
    int cnt = TDMS_ATOMIC_GET(tdms_log_burst_per_second);
    if(0 == cnt &&(-1 != cnt))
        return;

    if(cnt >0)
        TDMS_ATOMIC_SUB(tdms_log_burst_per_second, 1);
     
    /* get time string */
    struct tm local_time;
    char time_str[32]   = {0};
    time_t now = time(NULL);
    localtime_r(&now, &local_time);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S ", &local_time);

    /* get position string */
    char pos_str[32]  = {0};
    char line_str[16] = {0};
    const int32_t pos_max_size = 30;

    snprintf(line_str, 16, "%i", line);
    int32_t diff = strlen(file) + strlen(line_str) + 1 - pos_max_size;
    if (diff > 0) 
        snprintf(pos_str, 32, "%s:%i", file + diff, line);
    else 
        snprintf(pos_str, 32, "%s:%i", file, line);

    /* put va_list into msg buffer */
    char log_msg[1024] = {0};
    va_list args;
    va_start(args, format);
    vsnprintf(log_msg, sizeof(log_msg), format, args);
    va_end(args);

    log_to_tdmslog_file(type, time_str, pos_str, log_msg);

}

/* re-entrant version */

static inline void log_with_option_r(int32_t type, int32_t fpno, char *time, 
        const char *pos, const char *msg) 
{
    if (log_conf[type][fpno].log_opt & LOG_OPT_TIME) 
        fprintf(log_output_fps[fpno], "%s  ", time); 

    if (log_conf[type][fpno].log_opt & LOG_OPT_BAR) 
        fprintf(log_output_fps[fpno], "[%s] ", log_level_str[type]); 

    fprintf(log_output_fps[fpno], "%s ", msg); 

    if (log_conf[type][fpno].log_opt & LOG_OPT_POS) 
        fprintf(log_output_fps[fpno], "(%s)", pos); 

    fprintf(log_output_fps[fpno], "\n"); 

    return;
}

static inline void log_to_file_r(int32_t type, char *time, const char *pos,
        const char *msg) 
{
    if (log_conf[type][LOG_TO_STDOUT].log_flag)  
        log_with_option_r(type, LOG_TO_STDOUT, time, pos, msg); 

    if (log_conf[type][LOG_TO_STDERR].log_flag) 
        log_with_option_r(type, LOG_TO_STDERR, time, pos, msg); 

    if (log_conf[type][LOG_TO_LOGFILE].log_flag)
        log_with_option_r(type, LOG_TO_LOGFILE, time, pos, msg); 
}

       
void tdms_set_log_switch_value(unsigned char value)
{
	if (I_SET_FLAG != value && I_RESET_FLAG != value)
	{
		log(ERROR,"log_swtich value invalid.\n");
		return;
	}
	g_tdms_log_switch = value;
	return;
}

unsigned char tdms_get_log_switch_value()
{
	return g_tdms_log_switch;
}







