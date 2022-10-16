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
	uint16_t core_num;   /*�洢ʹ�õĺ�����*/
	uint16_t db_port;   /*mysql�������˿�*/
	uint16_t reserved[2];
	
    double warning_thres;/*���̴洢�ռ�澯��ֵ*/
    double rewrite_thres;/*���̴洢�ռ������д��ֵ*/
	
	uint64_t data_buf_sz;/*�ڴ����ݻ����С(����С��ʵ���ڴ��С��1/4),��λbytes*/
	uint64_t disk_cap;   /*�洢�����������Ĵ�������,��λbytes*/
	uint64_t search_ret_sz;/*���������С����λbytes*/

	char data_storage_dir[TDMS_FILE_DIR_MAX_LEN];/*�����ļ��洢Ŀ¼*/
	char index_storage_dir[TDMS_FILE_DIR_MAX_LEN];/*�����ļ��洢Ŀ¼*/
	char result_storage_dir[TDMS_FILE_DIR_MAX_LEN];/*����ļ��洢Ŀ¼*/
	char log_storage_dir[TDMS_FILE_DIR_MAX_LEN];/*��־�ļ��洢Ŀ¼*/

	char db_address[TDMS_FILE_DIR_MAX_LEN];/*mysql������������ַ*/
	char db_uname[TDMS_FILE_DIR_MAX_LEN];/*mysql�������û���¼��*/
	char db_pwd[TDMS_FILE_DIR_MAX_LEN];/*mysql�������û�����*/
	char db_name[TDMS_FILE_DIR_MAX_LEN];/*mysql���������ݿ���*/
	
}tdmsInitParam;

typedef struct tdms_pkt_digest_s
{
	
	uint8_t proto;
	uint8_t reserved[3];
	
	uint16_t src_port;
	uint16_t dst_port;
	
	struct timeval ts;
	
	//uint8_t src_ip[TDMS_IPV4_LEN];	  //�����ip:192.168.1.1->src_ip[0]=192,��
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
    char error_msg[MAX_ERROR_MSG_LEN];/*��ѯʧ��ԭ��*/
    char *result_path;/*����ļ�·��*/
    uint64_t  reslut_sz;/*����ļ���С*/
	int  pkt_num;/*������ƥ�������İ�����*/
} tdmsRetrieveResult;

typedef struct IndexFileHeader{
    uint32_t keyLen;
    uint32_t eleNum;
}IndexFileHeader,*pIndexFileHeader;

/*====================================================
������: tdms_init
����:  ������ز�����������Դ����ʼ��������ݽṹ
���:  init_param �����ò�������which ѡ���ʼ����
       0����ʼ���洢������Դ
       1����ʼ������������Դ
       2����ʼ��ȫ����Դ
���Σ�char tdms_init_status[],ȫ�ֱ���,������ʼ�������
����ͷ�ļ����ֱ�����á�
����ֵ:TDMS_R_OK:��ʼ���ɹ�
       TDMS_R_ERR:��ʼ��ʧ��
======================================================*/
int tdms_init(tdmsInitParam *init_param,int which);

/*====================================================
������: tdms_store_proc
����:  �洢���ݰ������棬������ʱ����������������Ӳ��
���:  uint8_t *packet�����洢�����ݰ���
       uint32_t pkt_len�����ݰ����ȣ�
       PktDigest *dig����Ԫ����ʱ�����ϢժҪ��PktDigest
	   tdmsPktDigest *dig
       uint16_t core_id�����ô洢������cpu���ı�ţ������̱߳�ţ�
       int compress_enable �Ƿ�ѹ��
���Σ�char tdms_store_status[],ȫ�ֱ���,�����洢�����
����ͷ�ļ����ֱ�����á�
����ֵ: TDMS_R_OK:�ɹ�
        TDMS_R_ERR������ 
        TDMS_R_WARN:�ﵽ���̴洢�ռ�澯��ֵ
        TDMS_R_REWRITE:�ﵽ���̴洢�ռ������д��ֵ
======================================================*/
int tdms_store_proc(uint8_t *packet, uint32_t pkt_len, tdmsPktDigest *dig, uint16_t core_id,int compress_enable);

/*====================================================
������: tdms_retrieve_proc
����:  ���ݲ�ѯ��䣬����ƥ����
���:  const char *query,
	��ѯ��������
	retrieve -n 100 -s 2020-01-01 20:00:00 -e 2020-02-01 20:00:00 [-c val] -- sip == 192.*.*.* &&!(dport == 80 || dport == 53) && ts == 2020-04-22 12:00:00
    int compress_enable �Ƿ�ѹ��

���Σ� tdmsRetrieveResult *retrive_ret

����ͷ�ļ����ֱ�����á�
����ֵ:�ɹ�����ƥ�����Ŀ��ʧ�ܷ���TDMS_R_ERR
======================================================*/
int tdms_retrieve_proc(const char *query, tdmsRetrieveResult *retrive_ret,int compress_enable);

/*====================================================
������: tdms_uninit
����:  �ͷ���Դ
���:  which:ѡ���ͷţ������ʼ������ѡ���Ӧ
       0���ͷŴ洢������Դ
       1���ͷż���������Դ
       2���ͷ�������Դ
���Σ� char tdms_free_status[],ȫ�ֱ���,�����ͷŽ����
����ͷ�ļ����ֱ�����á�
����ֵ: TDMS_R_OK:�ͷųɹ���TDMS_R_ERR:�ͷ�ʧ��
======================================================*/
int tdms_uninit(int which,int fcompress_enable);

/*====================================================
������: tdms_set_folder_del_flag
����:  �����ļ���ɾ���ɹ����
���:  ɾ����data�ļ�����
======================================================*/
void tdms_set_folder_del_flag(int flag);
/*====================================================
������: tdms_get_folder_del_flag
����:  ���  ɾ����data�ļ�����
����ֵ:   ɾ����data�ļ�����
======================================================*/

int tdms_get_folder_del_flag(void);
char *libtdms_version_str();

/*====================================================
������: tdms_wait_child
����:   ���������˳�ʱ����flush�ӽ�����Դ
����ֵ:   void
======================================================*/
void tdms_wait_child(void);



extern uint16_t core_num;
extern uint16_t db_port;

extern uint64_t data_buf_sz;/*�ڴ����ݻ����С(����С��ʵ���ڴ��С��1/4)*/
extern uint64_t disk_cap;
extern uint64_t pktAmount;

extern char log_storage_dir[TDMS_FILE_DIR_MAX_LEN];
extern char data_storage_dir[TDMS_FILE_DIR_MAX_LEN];
extern char index_storage_dir[TDMS_FILE_DIR_MAX_LEN];
extern char result_storage_dir[TDMS_FILE_DIR_MAX_LEN];

extern char db_address[TDMS_FILE_DIR_MAX_LEN];/*mysql������������ַ*/
extern char db_uname[TDMS_FILE_DIR_MAX_LEN];/*mysql�������û���¼��*/
extern char db_pwd[TDMS_FILE_DIR_MAX_LEN];/*mysql�������û�����*/
extern char db_name[TDMS_FILE_DIR_MAX_LEN];/*mysql���������ݿ���*/

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

