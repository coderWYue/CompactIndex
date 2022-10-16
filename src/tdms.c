/*************************************************************************
	> File Name: tdms.c
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Sun 26 Apr 2020 02:57:32 PM CST
 ************************************************************************/

#include "common.h"
#include "log.h"
#include <lz4.h>
#include "tdms.h"
#include "init.h"
#include "toolFunc.h"
#include "retrieve_c_api.h"
#include "index.h"
#include "flush.h"
#include "atomic.h"
#include "release.h"


//#include "retrieval/retrieve.h"
#define DISCARD_THRE 2

static int run_cores[MAX_CORE_NUM] = {0};
static int pause_proc = 0;
int folder_del_flag = 0;

const double index_ratio = 0.08;
const uint64_t max_open_files = 65536;

static uint64_t free_space_warn = 0;
static uint64_t free_space_lowest = 0;

uint16_t core_num;
uint16_t db_port;

uint64_t data_buf_sz;
uint64_t pktAmount;

uint64_t disk_cap;
uint64_t disk_used;
uint64_t frame_sz;
uint64_t warn_threshold;
uint64_t overlap_threshold;

uint8_t store_strategy;

uint32_t oldest_file_id;

char log_storage_dir[TDMS_FILE_DIR_MAX_LEN];

char db_address[TDMS_FILE_DIR_MAX_LEN];
char db_uname[TDMS_FILE_DIR_MAX_LEN];
char db_pwd[TDMS_FILE_DIR_MAX_LEN];
char db_name[TDMS_FILE_DIR_MAX_LEN];

char tdms_init_status[STATUS_MSG_LEN]; 
char tdms_store_status[STATUS_MSG_LEN];
char tdms_free_status[STATUS_MSG_LEN];

BYTE* dataBuf;
pPktDigest digestBuf;

BYTE* dataTmp1;
BYTE* dataTmp2;

pPktDigest digestTmp1;
pPktDigest digestTmp2;

void *dataZones[MAX_CORE_NUM];
void *digestZones[MAX_CORE_NUM];
uint32_t avais[MAX_CORE_NUM];
uint64_t dataPoses[MAX_CORE_NUM];
uint32_t digestBound;
uint64_t dataBound;

uint64_t global_data_off = 0;
uint64_t global_digest_counter = 0;

uint64_t data_frame_sz;
uint64_t digest_frame_sz;

MYSQL mysql;

int delNum = 0;

uint64_t counter = 0;

FILE* log_fp;

pid_t flush_pid;
static int discard = 0;
//defined in other source file
//extern uint64_t disk_cap;
char data_storage_dir[TDMS_FILE_DIR_MAX_LEN];
char index_storage_dir[TDMS_FILE_DIR_MAX_LEN];
char result_storage_dir[TDMS_FILE_DIR_MAX_LEN];
char *libtdms_version_str()
{
    static char version_str[256] = "";

    if (version_str[0] == '\0') {
        snprintf(version_str, sizeof(version_str),
                "libtdms %s %s (svn-revison: %s, build-time: %s)",
                LIBTDMS_VERSION,
#ifdef NDEBUG
                "release",
#else
                "debug",
#endif
                LIBTDMS_SVN_REV, LIBTDMS_BUILD_TIME);
    }
    return version_str;
}

void tdms_print_init_param(tdmsInitParam *init_param)
{
	if (NULL == init_param)
		return;
	log(INFO,"%s", libtdms_version_str());
    
	log(INFO,"core_num: %d", init_param->core_num);
	log(INFO,"db_port: %d", init_param->db_port);
	log(INFO,"search_ret_sz: %ld", init_param->search_ret_sz);
	log(INFO,"warning_thres: %lf", init_param->warning_thres);
	log(INFO,"rewrite_thres: %lf", init_param->rewrite_thres);
	log(INFO,"data_buf_sz: %ld", init_param->data_buf_sz);
	log(INFO,"disk_cap: %ld", init_param->disk_cap);

	log(INFO,"data_storage_dir: %s", init_param->data_storage_dir);
	log(INFO,"index_storage_dir: %s", init_param->index_storage_dir);
	log(INFO,"result_storage_dir: %s", init_param->result_storage_dir);
	log(INFO,"log_storage_dir: %s", init_param->log_storage_dir);

	log(INFO,"db_address: %s", init_param->db_address);
	log(INFO,"db_uname: %s", init_param->db_uname);
	log(INFO,"db_pwd: %s", init_param->db_pwd);
	log(INFO,"db_name: %s", init_param->db_name);
	
}

static inline int check_null_ptr(const char *desc,const void *ptr){
    if(NULL == ptr){
        log_to_buf(tdms_init_status,ERROR,"invalid para:%s is null ptr",desc);
        log(ERROR,"invalid para:%s is null ptr",desc);
        return -1;
    }
    return 0;
}

static int check_dir_format(const char *dirFunc,char *dir){
    int len = strlen(dir);
    if(len == 0){
        log_to_buf(tdms_init_status,ERROR,"%s can not be null str",dirFunc);
        log(ERROR,"%s can not be null str",dirFunc);
        return -1;
    }
    if(dir[len-1] != '/'){
        dir[len] = '/';
        dir[len+1] = 0;
    }
    return 0;
}

static int check_dir_exist(const char *dir){
    int dirStatus = access(dir, F_OK);
    if(dirStatus == 0){
        dirStatus = access(dir, R_OK | W_OK | X_OK);
        if(dirStatus != 0){
            log_to_buf(tdms_init_status,ERROR,"not adequate permission for %s",dir);
            log(ERROR,"not adequate permission for %s",dir);
            
        }
        return dirStatus;
    }else{
        int createRes = mkdir(dir,0755);
        if(createRes != 0) 
        {
            log_to_buf(tdms_init_status,ERROR,"%s do not exist,and fail to create",dir);
            log(ERROR,"%s do not exist,and fail to create",dir);
        }
        return createRes;
    }
}

int tdms_init(tdmsInitParam *init_param,int which)
{
    tdms_init_log();
	if (NULL == init_param)
	{
		log_to_buf(tdms_init_status,ERROR,"tdms_init param is null");
        log(ERROR,"tdms_init param is null");
		return -1;
	}

	tdms_print_init_param(init_param);
	
    db_port = init_param->db_port;

    if(check_null_ptr("db_address",init_param->db_address) == -1) return -1;
    strcpy(db_address,init_param->db_address);

    if(check_null_ptr("db_uname",init_param->db_uname) == -1) return -1;
    strcpy(db_uname,init_param->db_uname);

    if(check_null_ptr("db_pwd",init_param->db_pwd) == -1) return -1;
    strcpy(db_pwd,init_param->db_pwd);

    if(check_null_ptr("db_name",init_param->db_name) == -1) return -1;
    strcpy(db_name,init_param->db_name);

    if(check_null_ptr("log_storage_dir",init_param->log_storage_dir) == -1) return -1;
    strcpy(log_storage_dir,init_param->log_storage_dir);
    if(check_dir_format("log_storage_dir",log_storage_dir) == -1 || check_dir_exist(log_storage_dir) != 0) return -1;

    if(check_null_ptr("data_storage_dir",init_param->data_storage_dir) == -1) return -1;
    strcpy(data_storage_dir,init_param->data_storage_dir);
    if(check_dir_format("data_storage_dir",data_storage_dir) == -1 || check_dir_exist(data_storage_dir) != 0) return -1;

    if(check_null_ptr("index_storage_dir",init_param->index_storage_dir) == -1) return -1;
    strcpy(index_storage_dir,init_param->index_storage_dir);
    if(check_dir_format("index_storage_dir",index_storage_dir) == -1 || check_dir_exist(index_storage_dir) != 0) return -1;

    if(which == 1 || which == 2){
        frame_sz = init_param->search_ret_sz;

        if(check_null_ptr("result_storage_dir",init_param->result_storage_dir) == -1) return -1;
        strcpy(result_storage_dir, init_param->result_storage_dir);
        if(check_dir_format("result_storage_dir",result_storage_dir) == -1 || check_dir_exist(result_storage_dir) != 0) return -1;

    }

    if(connect_db() == -1) return -1;

    if(which == 0 || which == 2){
        core_num = init_param->core_num;
        data_buf_sz = init_param->data_buf_sz;
        disk_cap = init_param->disk_cap;
        delNum = 0;
        //last_ts = 0;

        if(set_open_files_max(max_open_files) == -1){
            log_to_buf(tdms_init_status,ERROR,"fail to set max open files");
            log(ERROR,"fail to set max open files");
            return -1;
        }

        disk_used = get_dir_sz(data_storage_dir) + get_dir_sz(index_storage_dir);
        disk_cap = (disk_cap > get_free_space_sz(data_storage_dir)+disk_used ? get_free_space_sz(data_storage_dir)+disk_used : disk_cap);

        warn_threshold = disk_cap * init_param->warning_thres;
        overlap_threshold = disk_cap * init_param->rewrite_thres;

        free_space_warn = (disk_cap - warn_threshold);
        free_space_lowest = (disk_cap - overlap_threshold);

        //if(disk_cap < 4*data_buf_sz){
        if(disk_cap < 0){
            log_to_buf(tdms_init_status,ERROR,"available disk space is %lu,do not suggest run this program",disk_cap);
            log(ERROR,"available disk space is %lu,do not suggest run this program",disk_cap);
	    return -1;

        }

        free_space_warn = (free_space_warn < 8*data_buf_sz ? 8*data_buf_sz : free_space_warn);
        free_space_lowest = (free_space_lowest < 4*data_buf_sz ? 4*data_buf_sz : free_space_lowest);

        store_strategy = 0;

        if(open_log_file() == -1) return -1;


        set_sz_index_structure();

        if(alloc_space() == -1) return -1;

        //init_atomic_vars();
        
        flush_pid = getpid();

        init_capture_zone();

        writeHdrToDataBuf();

        initIndexStructure();
    }

    return 0; 
}

void swapBuf(BYTE *dataTmp,pPktDigest digestTmp){
    uint64_t dataUnit = data_buf_sz / core_num;
    uint64_t digestUnit = pktAmount / core_num;

    dataBuf = dataTmp;
    digestBuf = digestTmp;

    dataZones[0] = dataBuf + PCAP_FILE_HEAD_SIZE;
    digestZones[0] = digestBuf;
    avais[0] = 0;
    dataPoses[0] = 0;

    for(int i = 1; i < core_num; ++i){
        dataZones[i] = dataBuf + PCAP_FILE_HEAD_SIZE + i*dataUnit;
        digestZones[i] = digestBuf + i*digestUnit;
        avais[i] = 0;
        dataPoses[i] = 0;
    }
}

static inline int8_t isDataRunOut(unsigned i){
    uint64_t dataPos = dataPoses[i];
    uint32_t avai = avais[i];
    return (dataPos >= dataBound) || (avai >= digestBound);
}

static inline void updateOffset(unsigned i){
    avais[i]++;
    uint64_t dataPos = dataPoses[i];
    BYTE *dataBuf = (BYTE *)dataZones[i];
    uint32_t len = ((pPcapPacketHead)(dataBuf + dataPos))->caplen;
    dataPoses[i] = dataPos + PCAP_PACKET_HEAD_SIZE + len;
}

static int write_to_data_buf(uint8_t *packet,uint32_t pkt_len, tdmsPktDigest *dig,uint16_t core_id,int compress_enable){
    uint64_t dataPos = dataPoses[core_id];
    BYTE *dataBuf = (BYTE*)dataZones[core_id];
    int cpsed_len= 0;
	
    if(0)
    {
        cpsed_len = LZ4_compress_default((char*)packet,(char*)(dataBuf+dataPos+PCAP_PACKET_HEAD_SIZE),pkt_len,pkt_len+OVERFLOW_BUF);
        if(0 == cpsed_len){
            log_to_buf(tdms_store_status,ERROR,"compress error in tdms_store_proc!");
            log(ERROR,"compress error in tdms_store_proc!");
            return 0;
        }
    }
    else
    {
        memcpy((dataBuf+dataPos+PCAP_PACKET_HEAD_SIZE),packet,pkt_len);
	    cpsed_len = pkt_len;
    }
    pPcapPacketHead pHdr = (pPcapPacketHead)(dataBuf+dataPos);
    pHdr->ts.sec = dig->ts.tv_sec;
    pHdr->ts.usec = dig->ts.tv_usec;
    pHdr->caplen = cpsed_len;
    pHdr->len = cpsed_len;
    return cpsed_len + PCAP_PACKET_HEAD_SIZE;

}

static void get_digest(tdmsPktDigest *dig, uint16_t core_id){
    uint32_t avai = avais[core_id];
    PktDigest *digestBuf = (PktDigest *)digestZones[core_id];
    digestBuf[avai].proto = dig->proto;
    digestBuf[avai].src_port = dig->src_port;
    digestBuf[avai].dst_port = dig->dst_port;
    void *ts = digestBuf[avai].ts;
    *(uint32_t *)ts = dig->ts.tv_sec;
    digestBuf[avai].off = dataPoses[core_id];
    digestBuf[avai].af = dig->af;
    if(dig->af == 6){
        memcpy(digestBuf[avai].src_ip.ip6,dig->src_ip.ip6,TDMS_IPV6_LEN);
        memcpy(digestBuf[avai].dst_ip.ip6,dig->dst_ip.ip6,TDMS_IPV6_LEN);
    }else{
        void *sip = digestBuf[avai].src_ip.ip4;
        void *dip = digestBuf[avai].dst_ip.ip4;
        *(uint32_t *)sip = *(uint32_t *)(void *)(dig->src_ip.ip4);
        *(uint32_t *)dip = *(uint32_t *)(void *)(dig->dst_ip.ip4);
    }
}

static inline uint32_t count_packets_num(void){
    uint32_t pkt_num = 0;
    for(int i = 0; i < core_num; ++i){
        pkt_num += avais[i];
    }
    return pkt_num;
}

static inline uint64_t count_packets_sz(void){
    uint64_t pkts_sz = 0;
    for(int i = 0; i < core_num; ++i){
        pkts_sz += dataPoses[i];
    }
    return pkts_sz;
}

static inline void initDataBuf(unsigned i){
    avais[i] = 0;
    dataPoses[i] = 0;
}

int tdms_store_proc(uint8_t *packet, uint32_t pkt_len, tdmsPktDigest *dig, uint16_t core_id,int compress_enable){
    if (NULL == packet || NULL == dig){
        log(ERROR, "[%s,%d] param is null .\n", __func__,__LINE__);
        return -1;
    }
		
    ATOMIC_SET(run_cores[core_id],1);
    if(ATOMIC_GET(pause_proc)){
        ATOMIC_SET(run_cores[core_id],0);
        while(ATOMIC_GET(pause_proc));
        ATOMIC_SET(run_cores[core_id],1);
    }

    int status = 0;
    int writed_len = write_to_data_buf(packet,pkt_len,dig,core_id,compress_enable);   
    if(writed_len == 0){
        ATOMIC_SET(run_cores[core_id],0);
        return -1;
    } 

    get_digest(dig,core_id);

    updateOffset(core_id);

    if(unlikely(isDataRunOut(core_id))){
        unsigned curr_pause = 0;
        if(ATOMIC_CAS(pause_proc,curr_pause,1)){
            pid_t cStatus = waitpid(flush_pid,NULL,WNOHANG);
            if(cStatus == 0){
                char currTime[256];
                getCurrFormatTime(currTime,256);
                log_to_file(log_fp,WARNING,"%s: disk write can not support net link,%lu bytes %u packets discarded!",\
                        currTime,count_packets_sz(),count_packets_num());

		fflush(log_fp);
                for(int i = 0; i < core_num; ++i){
                    initDataBuf(i);
                }

                discard++;
                if(discard > DISCARD_THRE){
                    kill(flush_pid,SIGTERM);
                    discard = 0;
                }
            }else{
                
                uint64_t dataSize = 0;
                for(int i = 0; i < core_num; ++i) dataSize += dataPoses[i];
                disk_used += dataSize*(1+index_ratio);

                log_to_file(log_fp,INFO,"this write:%lu,free_space_lowest:%lu free_space_warn:%lu disk_used:%lu overlap_threshold:%lu warn_threshold:%lu store_strategy:%d\n\
		    diskcap:%lu",\
                   dataSize,free_space_lowest,free_space_warn,disk_used,overlap_threshold,warn_threshold,store_strategy,disk_cap);
		fflush(log_fp);

                status = store_strategy;

                while(1){
                    int not_run = 0;
                    for(int i = 0; i < core_num; ++i){
                        if(i != core_id && ATOMIC_GET(run_cores[i])==0) not_run++;
                    }
                    if(not_run == core_num-1) break;
                }

                flush_pid = fork();
                if(flush_pid < 0){
                    log_to_buf(tdms_store_status,ERROR,"fail to fork flush process,%lu bytes %u packets discarded!",count_packets_sz(),count_packets_num());
                    log(ERROR,"fail to fork flush process,%lu bytes %u packets discarded!",count_packets_sz(),count_packets_num());
                    for(int i = 0; i < core_num; ++i){
                        initDataBuf(i);
                    }
                    status = -1;
                }else if(flush_pid == 0){
                    flush(compress_enable);
                    exit(0);
                }else{
                    for(int i = 0; i < core_num; ++i){
                        initDataBuf(i);
                    }
                    delNum = 0;
                    discard = 0;
                }
            }
            ATOMIC_SET(pause_proc,0);
        }   
    }
    ATOMIC_SET(run_cores[core_id],0);
    return status;
}

static inline void init_result(tdmsRetrieveResult *retrive_ret){
    retrive_ret->error_msg[0] = 0;
    retrive_ret->result_path = NULL;
    retrive_ret->reslut_sz = 0;
    retrive_ret->pkt_num = 0;
}

int tdms_retrieve_proc(const char *query,tdmsRetrieveResult *retrive_ret,int compress_enable){
    init_result(retrive_ret);

    if(NULL == query || strlen(query) == 0){
        log_to_buf(retrive_ret->error_msg,ERROR,"query is null");
        log(ERROR,"query is null");
        return -1;
    }
    
    int matched_num = exe_query(query,compress_enable);

    if(matched_num == -1){
        strcpy(retrive_ret->error_msg,error_msg);
        return -1;
    }else if(matched_num == 0 && p_error){
        strcpy(retrive_ret->error_msg,error_msg);
        return -1;
    }else if(p_error){
        int pathes_len = strlen(result_pathes);
        retrive_ret->result_path = (char *)malloc(pathes_len+8);
        if(retrive_ret->result_path == NULL){
            log_to_buf(error_msg,ERROR,"fail to malloc space for result path");
            log(ERROR,"fail to malloc space for result path");
            strcpy(retrive_ret->error_msg,error_msg);
            return -1;
        }

        strcpy(retrive_ret->result_path,result_pathes);
        strcpy(retrive_ret->error_msg,error_msg);
        retrive_ret->reslut_sz = res_sz;
        retrive_ret->pkt_num = matched_num;
        return -2;
    }else{
        int pathes_len = strlen(result_pathes);
        retrive_ret->result_path = (char *)malloc(pathes_len+8);
        if(retrive_ret->result_path == NULL){
            log_to_buf(error_msg,ERROR,"fail to malloc space for result path");
            log(ERROR,"fail to malloc space for result path");
            strcpy(retrive_ret->error_msg,error_msg);
            return -1;
        }

        strcpy(retrive_ret->result_path,result_pathes);
        retrive_ret->reslut_sz = res_sz;
        retrive_ret->pkt_num = matched_num;
        return 0;
    }
}

int tdms_uninit(int which,int compress_enable){
    
    printf("tdms_uninit\n");

    if(which == 0 || which == 2){
        int hasLeft = 0;
        for(int i = 0; i < core_num; ++i){
            if(avais[i]){
                hasLeft = 1;
                break;
            }
        }
        if(hasLeft) flush(compress_enable);


        fclose(log_fp);
    }

    mysql_close(&mysql);
    return 0;
}

void tdms_set_folder_del_flag(int flag)
{
	ATOMIC_SET(folder_del_flag,flag);
}

//wait for flush process terminate
void tdms_wait_child(void) {
    waitpid(flush_pid, NULL, 0);
}

int tdms_get_folder_del_flag(void)
{
	return ATOMIC_GET(folder_del_flag);
}
