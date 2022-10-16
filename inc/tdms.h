#ifndef _TDMS_H_
#define _TDMS_H_
#include <mysql.h>
#include <stdio.h>
#include <sys/time.h>

#ifdef __cplusplus 
extern "C" {
#endif

typedef uint8_t BYTE;

#define TDMS_FILE_DIR_MAX_LEN 256
#define TDMS_NAME_MAX_LEN 256
#define TDMS_IPV4_LEN 4
#define TDMS_IPV6_LEN 16
#define MAX_ERROR_MSG_LEN 512
#define STATUS_MSG_LEN 4096

#define AVG_PKT_SIZE 70
#define DIFF_IP_RATIO 32
#define TRIE_NODE_RATIO 8
#define INDEX_NUM 8
#define EXCLUDE_INDEX_NUM 2
#define TRIE_INDEX_NUM 2

#define MAX_CORE_NUM 256
#define OVERFLOW_BUF 65536

#define TDMS_R_OK 0
#define TDMS_R_ERR -1
#define TDMS_R_WARN 1
#define TDMS_R_REWRITE 2

typedef struct tdms_init_param_s
{
	uint16_t core_num;   /*存储使用的核心数*/
	uint16_t db_port;   /*mysql服务器端口*/
	uint16_t reserved[2];
	
    double warning_thres;/*磁盘存储空间告警阈值*/
    double rewrite_thres;/*磁盘存储空间滚动重写阈值*/
	
	uint64_t data_buf_sz;/*内存数据缓存大小(建议小于实际内存大小的1/4),单位bytes*/
	uint64_t disk_cap;   /*存储数据与索引的磁盘容量,单位bytes*/
	uint64_t search_ret_sz;/*检索结果大小，单位bytes*/

	char data_storage_dir[TDMS_FILE_DIR_MAX_LEN];/*数据文件存储目录*/
	char index_storage_dir[TDMS_FILE_DIR_MAX_LEN];/*索引文件存储目录*/
	char result_storage_dir[TDMS_FILE_DIR_MAX_LEN];/*结果文件存储目录*/
	char log_storage_dir[TDMS_FILE_DIR_MAX_LEN];/*日志文件存储目录*/

	char db_address[TDMS_FILE_DIR_MAX_LEN];/*mysql服务器主机地址*/
	char db_uname[TDMS_FILE_DIR_MAX_LEN];/*mysql服务器用户登录名*/
	char db_pwd[TDMS_FILE_DIR_MAX_LEN];/*mysql服务器用户密码*/
	char db_name[TDMS_FILE_DIR_MAX_LEN];/*mysql服务器数据库名*/
	
}tdmsInitParam;

typedef struct tdms_pkt_digest_s
{
	
	uint8_t proto;
	uint8_t reserved[3];
	
	uint16_t src_port;
	uint16_t dst_port;
	
	struct timeval ts;
	
	//uint8_t src_ip[TDMS_IPV4_LEN];	  //大端序，ip:192.168.1.1->src_ip[0]=192,…
	//uint8_t dst_ip[TDMS_IPV4_LEN];
    union {
        uint8_t ip4[TDMS_IPV4_LEN];
        uint8_t ip6[TDMS_IPV6_LEN];
    }src_ip;
    union {
        uint8_t ip4[TDMS_IPV4_LEN];
        uint8_t ip6[TDMS_IPV6_LEN];
    }dst_ip;
    uint8_t af;    //4 for ipv4,6 for ipv6
}tdmsPktDigest;

typedef struct PktDigest{
    uint8_t proto;
    uint8_t reserved[3];

    uint16_t src_port;
    uint16_t dst_port;

    uint8_t ts[4];

    union {
        uint8_t ip4[TDMS_IPV4_LEN];
        uint8_t ip6[TDMS_IPV6_LEN];
    }src_ip;
    union {
        uint8_t ip4[TDMS_IPV4_LEN];
        uint8_t ip6[TDMS_IPV6_LEN];
    }dst_ip;

    uint64_t off;

    uint8_t af;
    uint8_t padding[11];
}__attribute__((__aligned__(64))) PktDigest,*pPktDigest;

typedef struct _retrieve_result {   
    char error_msg[MAX_ERROR_MSG_LEN];/*查询失败原因*/
    char *result_path;/*结果文件路径*/
    uint64_t  reslut_sz;/*结果文件大小*/
	int  pkt_num;/*检索到匹配条件的包数量*/
} tdmsRetrieveResult;

typedef struct IndexFileHeader{
    uint32_t keyLen;
    uint32_t eleNum;
}IndexFileHeader,*pIndexFileHeader;

/*====================================================
函数名: tdms_init
功能:  设置相关参数，分配资源，初始化相关数据结构
入参:  init_param （配置参数），which 选择初始化，
       0：初始化存储部分资源
       1：初始化检索部分资源
       2：初始化全部资源
出参：char tdms_init_status[],全局变量,描述初始化结果，
包含头文件后可直接引用。
返回值:TDMS_R_OK:初始化成功
       TDMS_R_ERR:初始化失败
======================================================*/
int tdms_init(tdmsInitParam *init_param,int which);

/*====================================================
函数名: tdms_store_proc
功能:  存储数据包到缓存，缓存满时，建索引并导出到硬盘
入参:  uint8_t *packet，待存储的数据包；
       uint32_t pkt_len，数据包长度；
       PktDigest *dig，五元组与时间戳信息摘要，PktDigest
	   tdmsPktDigest *dig
       uint16_t core_id，调用存储函数的cpu核心编号（或者线程编号）
       int compress_enable 是否压缩
出参：char tdms_store_status[],全局变量,描述存储结果，
包含头文件后可直接引用。
返回值: TDMS_R_OK:成功
        TDMS_R_ERR：出错 
        TDMS_R_WARN:达到磁盘存储空间告警阈值
        TDMS_R_REWRITE:达到磁盘存储空间滚动重写阈值
======================================================*/
int tdms_store_proc(uint8_t *packet, uint32_t pkt_len, tdmsPktDigest *dig, uint16_t core_id,int compress_enable);

/*====================================================
函数名: tdms_retrieve_proc
功能:  根据查询语句，返回匹配结果
入参:  const char *query,
	查询语句举例：
	retrieve -n 100 -s 2020-01-01 20:00:00 -e 2020-02-01 20:00:00 [-c val] -- sip == 192.*.*.* &&!(dport == 80 || dport == 53) && ts == 2020-04-22 12:00:00
    int compress_enable 是否压缩

出参： tdmsRetrieveResult *retrive_ret

包含头文件后可直接引用。
返回值:成功返回匹配包数目；失败返回TDMS_R_ERR
======================================================*/
int tdms_retrieve_proc(const char *query, tdmsRetrieveResult *retrive_ret,int compress_enable);

/*====================================================
函数名: tdms_uninit
功能:  释放资源
入参:  which:选择释放，需与初始化函数选项对应
       0：释放存储部分资源
       1：释放检索部分资源
       2：释放所有资源
出参： char tdms_free_status[],全局变量,描述释放结果，
包含头文件后可直接引用。
返回值: TDMS_R_OK:释放成功；TDMS_R_ERR:释放失败
======================================================*/
int tdms_uninit(int which,int fcompress_enable);

/*====================================================
函数名: tdms_set_folder_del_flag
功能:  设置文件夹删除成功标记
入参:  删除的data文件个数
======================================================*/
void tdms_set_folder_del_flag(int flag);
/*====================================================
函数名: tdms_get_folder_del_flag
功能:  获得  删除的data文件个数
返回值:   删除的data文件个数
======================================================*/

int tdms_get_folder_del_flag(void);
char *libtdms_version_str();

/*====================================================
函数名: tdms_wait_child
功能:   在主进程退出时回收flush子进程资源
返回值:   void
======================================================*/
void tdms_wait_child(void);



extern uint16_t core_num;
extern uint16_t db_port;

extern uint64_t data_buf_sz;/*内存数据缓存大小(建议小于实际内存大小的1/4)*/
extern uint64_t disk_cap;
extern uint64_t pktAmount;

extern char log_storage_dir[TDMS_FILE_DIR_MAX_LEN];
extern char data_storage_dir[TDMS_FILE_DIR_MAX_LEN];
extern char index_storage_dir[TDMS_FILE_DIR_MAX_LEN];
extern char result_storage_dir[TDMS_FILE_DIR_MAX_LEN];

extern char db_address[TDMS_FILE_DIR_MAX_LEN];/*mysql服务器主机地址*/
extern char db_uname[TDMS_FILE_DIR_MAX_LEN];/*mysql服务器用户登录名*/
extern char db_pwd[TDMS_FILE_DIR_MAX_LEN];/*mysql服务器用户密码*/
extern char db_name[TDMS_FILE_DIR_MAX_LEN];/*mysql服务器数据库名*/

extern char tdms_init_status[STATUS_MSG_LEN]; 
extern char tdms_store_status[STATUS_MSG_LEN];
extern char tdms_free_status[STATUS_MSG_LEN];

extern BYTE* dataBuf;
extern pPktDigest digestBuf;

extern BYTE* dataTmp1;
extern BYTE* dataTmp2;

extern pPktDigest digestTmp1;
extern pPktDigest digestTmp2;

extern uint64_t frame_sz;

extern void *dataZones[MAX_CORE_NUM];
extern void *digestZones[MAX_CORE_NUM];
extern uint32_t avais[MAX_CORE_NUM];
extern uint64_t dataPoses[MAX_CORE_NUM];
extern uint32_t digestBound;
extern uint64_t dataBound;

extern MYSQL mysql;
extern int delNum;

extern uint32_t oldest_file_id;
extern uint8_t store_strategy;

extern FILE* log_fp;

#ifdef __cplusplus
}
#endif

#endif

