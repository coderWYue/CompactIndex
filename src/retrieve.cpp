/*************************************************************************
	> File Name: retrieve.cpp
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Wed 06 May 2020 10:10:31 PM CST
 ************************************************************************/

#include "common.h"
#include "log.h"
#include "tdms.h"
#include "toolFunc.h"
#include "QueryInterpreter.h"
#include "Driver.h"
#include "fileOperation.h"
#include "retrieve.h"

static const char *retr_all = "sip == *.*.*.*";

char resFileName[MAX_FILE_NAME_LEN];
char resFileNames[MAX_RESULT_LEN];
char err_msg[MAX_ERR_MSG_LEN];
uint64_t result_sz = 0;
int partial_error = 0;
int newFlag = 1;

uint64_t s_time_arr[MAX_TIME_INTERVAL_NUM];
uint64_t e_time_arr[MAX_TIME_INTERVAL_NUM];

int time_interval_num = 0;

char identifier[ID_LEN];

uint64_t resIndexBuf[MAX_PKT_NUM_PER_RETR];


static void initPara(void){
    memset(identifier,0,ID_LEN);

    result_sz = 0;
    resFileName[0] = 0;
    resFileNames[0] = 0;
    err_msg[0] = 0;
    partial_error = 0;
    newFlag = 1;
}

static int checkResultDir(){
    DIR *dp;
    if((dp=opendir(result_storage_dir))==NULL){
        int mkRes=mkdir(result_storage_dir,0777);
        if(mkRes==-1){
            log_to_buf(err_msg,ERROR,"result dir %s does not exist,and create failed!",result_storage_dir);
            log(ERROR,"result dir %s does not exist,and create failed!",result_storage_dir);
            return -1;
        }
        return 0;
    }
    return 0;
}

static const char *checkQueryType(const char *query,char type[]){
    const char *tmp = query;
    while(*tmp==' ') tmp++;
    if(memcmp(tmp,commands[RETR],strlen(commands[RETR]))==0){
        memcpy(type,commands[RETR],strlen(commands[RETR]));
        tmp+=strlen(commands[RETR]);
    }else{
        log_to_buf(err_msg,ERROR,"not supported command type");
        log(ERROR,"not supported command type");
        return NULL;
    }
    while(*tmp==' ') tmp++;
    return tmp;
}

static const char *getOptPara(const char *query,char *sTimeStrArr[],char *eTimeStrArr[],int *s_time_num, int *e_time_num,\
        char *numLimitStr,char *id){
    const char *tmp = query;
    while(*tmp==' ') tmp++;
    while(*tmp=='-'){
        if((*(tmp+1)=='n')){
            tmp += 2;
            while(*tmp==' ') tmp++;
            int i = 0;
            while(isdigit(*tmp)) numLimitStr[i++] = *(tmp++);
            numLimitStr[i] = 0;
            while(*tmp==' ') tmp++;
        }else if(*(tmp+1)=='s'){
            tmp+=2;
            while(*tmp==' ') tmp++;

            char *sTimeStr = sTimeStrArr[*s_time_num];

            int i=0;
            while(isdigit(*tmp)||*tmp=='-'||*tmp==':'){
                sTimeStr[i++]=*(tmp++);
            }
            if(*tmp==' '&&isdigit(*(tmp+1))){
                sTimeStr[i++] = *(tmp++);
                while(isdigit(*tmp)||*tmp==':'){
                    sTimeStr[i++]=*(tmp++);
                }
            }
            sTimeStr[i]=0;

            (*s_time_num)++;

            if(*s_time_num > *e_time_num + 1){
                char *eTimeStr = eTimeStrArr[(*e_time_num)++];
                strcpy(eTimeStr,"2100-12-31 23:59:59");
            }

            while(*tmp==' ') tmp++;
        }else if(*(tmp+1)=='e'){
            tmp+=2;
            while(*tmp==' ') tmp++;

            char *eTimeStr = eTimeStrArr[*e_time_num];

            int i=0;
            while(isdigit(*tmp)||*tmp=='-'||*tmp==':'){
                eTimeStr[i++]=*(tmp++);
            }
            if(*tmp==' '&&isdigit(*(tmp+1))){
                eTimeStr[i++] = *(tmp++);
                while(isdigit(*tmp)||*tmp==':'){
                    eTimeStr[i++]=*(tmp++);
                }
            }
            eTimeStr[i]=0;

            (*e_time_num)++;
            
            if(*e_time_num > *s_time_num){
                char *sTimeStr = sTimeStrArr[(*s_time_num)++];
                strcpy(eTimeStr,"1970-01-01 08:00:00");
            }

            while(*tmp==' ') tmp++;
        }else if(*(tmp+1)=='c'){
            tmp+=2;
            while(*tmp==' ') tmp++;
            int i = 0;
            while(i < ID_LEN-1 && *tmp != ' ') id[i++] = *(tmp++);
            id[i] = 0;
            while(*tmp==' ') tmp++;
        }else if(*(tmp+1)=='-'){
            tmp+=2;
            while(*tmp==' ') tmp++;
            break;
        }else{
            log_to_buf(err_msg,ERROR,"unknown option:%c",*(tmp+1));
            log(ERROR,"unknown option:%c",*(tmp+1));
            
            return NULL;
        }
    }
    return tmp;
}

static inline void replace_char(char *str,char s,char t){
    char *tmp = str;
    while(*tmp){
        if(*tmp == s) *tmp = t;
        tmp++;
    }
}

static inline void freeQueryFileName(pQueryFile pqf){
    delete pqf->dataFileName;
    delete pqf->srcIpIndexFileName;
    delete pqf->srcIp6IndexFileName;
    delete pqf->dstIpIndexFileName;
    delete pqf->dstIp6IndexFileName;
    delete pqf->srcPortIndexFileName;
    delete pqf->dstPortIndexFileName;
    delete pqf->protoIndexFileName;
    delete pqf->tsIndexFileName;
}

static int processTimeInterval(const char *sTimeStr,const char *eTimeStr,int ind){
    char fSTimeStr[64];
    char fETimeStr[64];
    int len = getQueryTime(sTimeStr,fSTimeStr);
    if(len == -1){
        log_to_buf(err_msg,ERROR,"incorrect time format:%s",sTimeStr);
        return -1;
    }

    len = getQueryTime(eTimeStr,fETimeStr);
    if(len == -1){
        log_to_buf(err_msg,ERROR,"incorrect time format:%s",eTimeStr);
        return -1;
    }

    s_time_arr[ind] = date_to_seconds(fSTimeStr);
    log(INFO,"specified start time is %s, seconds is %lu",fSTimeStr,s_time_arr[ind]);
    if(s_time_arr[ind] == UINT64_MAX){
        log_to_buf(err_msg,ERROR,"incorrect time format:%s",sTimeStr);
        return -1;
    }

    e_time_arr[ind] = date_to_seconds(fETimeStr);
    log(INFO,"specified start time is %s,seconds is %lu",fETimeStr,e_time_arr[ind]);
    if(e_time_arr[ind] == UINT64_MAX){
        log_to_buf(err_msg,ERROR,"incorrect time format:%s",eTimeStr);
        return -1;
    }

    int fileNum = getQueryFileSet(&mysql,fSTimeStr,fETimeStr);
    if(fileNum == -1){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"database access error,fail to get data file and index file");
        return -1;
    }else if(fileNum == 0){
        return 0;
    }
    return fileNum;
    
}

int run_query(const char *query,int compress_enable){
    initPara();
    if(checkResultDir() == -1) return -1;

    char type[64];
    const char *tmp = checkQueryType(query,type);
    if(tmp == NULL) return -1;

    char *sTimeStrArr[MAX_TIME_INTERVAL_NUM];
    char *eTimeStrArr[MAX_TIME_INTERVAL_NUM];

    char numLimitStr[64];

    for(int i = 0; i < MAX_TIME_INTERVAL_NUM; ++i){
        sTimeStrArr[i] = (char *)malloc(64);
        eTimeStrArr[i] = (char *)malloc(64);
        sTimeStrArr[i][0] = 0;
        eTimeStrArr[i][0] = 0;
        s_time_arr[i] = 0;
        e_time_arr[i] = UINT64_MAX;

     }     
    numLimitStr[0] = 0;
  
    int s_time_num = 0, e_time_num = 0;
    
    tmp = getOptPara(tmp,sTimeStrArr,eTimeStrArr,&s_time_num,&e_time_num,numLimitStr,identifier);
    if(tmp == NULL) return -1;

    time_interval_num = s_time_num > e_time_num ? s_time_num : e_time_num;

    if(s_time_num == 0 && e_time_num == 0){
        strcpy(eTimeStrArr[e_time_num],"2100-12-31 23:59:59");
        strcpy(sTimeStrArr[s_time_num],"1971-01-01 08:00:00");
        s_time_num = e_time_num = time_interval_num = 1;
    }
        
    if(s_time_num > e_time_num)
    {
        char *eTimeStr = eTimeStrArr[e_time_num++];
        strcpy(eTimeStr,"2100-12-31 23:59:59");
    }
    
    int totFileNum = 0;
    for(int i = 0; i < time_interval_num; ++i)
    {
        int fileNum = processTimeInterval(sTimeStrArr[i],eTimeStrArr[i],i);
        if(fileNum == -1) return -1;
        totFileNum += fileNum;
    }
    if(totFileNum == 0)  return 0;

    long limits = INT64_MAX;
    if(strlen(numLimitStr)){
        limits = atol(numLimitStr);
    }
    if(limits == 0) return 0;

    int file_id = 0;
    char q_time[64];
    char file_name_prefix[MAX_FILE_NAME_LEN];
    char path_prefix[MAX_FILE_NAME_LEN];
    char query_t[MAX_FILE_NAME_LEN];
    //strcpy(query_t,query);
    //replace_char(query_t,'/','%');
    getCurrFormatTime(q_time,64);
    //strcpy(file_name_prefix,query_t);
    //strcat(file_name_prefix,"_");
    strcpy(file_name_prefix,q_time);
    strcat(file_name_prefix,"_");
    getPath(path_prefix,file_name_prefix,RESULT);
    strcpy(file_name_prefix,path_prefix);
    strcpy(resFileName,file_name_prefix);
    char file_id_str[8];
    strcat(resFileName,decimalToStr(file_id,file_id_str));

    long retrieved = 0;
    int partial_error = 0;

    if(*tmp == 0){
        tmp = retr_all;
    }

    while(!queryFileSet.empty()){
        pQueryFile pqf = queryFileSet.front();

        char dataPath[TDMS_FILE_DIR_MAX_LEN];
        getPath(dataPath,pqf->dataFileName,DATA);

        if(!check_file_exist(dataPath)){
            freeQueryFileName(pqf);
            delete pqf;
            queryFileSet.pop();
            continue;
        } 

        struct timeval start,end;
        gettimeofday(&start,NULL);

        long num = computeQuery(tmp,pqf,resIndexBuf);
        if(num == -1){
            log_to_buf(err_msg+strlen(err_msg),ERROR,"logic computation error");
            log(ERROR,"logic computation error");
            clearQueue();
            return -1;
        }

        gettimeofday(&end,NULL);
        uint64_t dura = (end.tv_sec - start.tv_sec)*1000000 + end.tv_usec - start.tv_usec;
        log(INFO,"retrieve %ld pkts cost %lu us",num, dura);
        
        num = (num > (limits - retrieved) ? (limits - retrieved) : num);
        if(num <= 0){
            freeQueryFileName(pqf);
            delete pqf;
            queryFileSet.pop();
            continue;
        }

        int curr_get_num;
        if((curr_get_num = getDataByIL(dataPath,resIndexBuf,num,compress_enable)) == -1){
            log_to_buf(err_msg+strlen(err_msg),ERROR,"packet retrieval error");
            log(ERROR,"packet retrieval error");
            result_sz += get_file_sz(resFileName);
            partial_error = 1;
            clearQueue();
            return retrieved;
        }
        retrieved += curr_get_num;

        uint64_t curr_get_sz;
        //char resultPath[MAX_FILE_NAME_LEN];
        //getPath(resultPath,resFileName,RESULT);
        curr_get_sz = get_file_sz(resFileName);
        if(curr_get_sz >= frame_sz * GB){
            newFlag = 1;
            result_sz += curr_get_sz;
            int len = strlen(resFileNames);
            resFileNames[len] = '\n';
            resFileNames[len+1] = 0;
            strcat(resFileNames,resFileName);
            strcpy(resFileName,file_name_prefix);
            file_id++;
            strcat(resFileName,decimalToStr(file_id,file_id_str));
        }

        freeQueryFileName(pqf);
        delete pqf;
        queryFileSet.pop();
    }

    if(retrieved == 0) return 0;

    uint64_t curr_get_sz;
    //char resultPath[MAX_FILE_NAME_LEN];
    //getPath(resultPath,resFileName,RESULT);
    curr_get_sz = get_file_sz(resFileName);
    if(curr_get_sz > 0){
        result_sz += curr_get_sz;
        int len = strlen(resFileNames);
        resFileNames[len] = '\n';
        resFileNames[len+1] = 0;
        strcat(resFileNames,resFileName);
    }

    return retrieved;
}
