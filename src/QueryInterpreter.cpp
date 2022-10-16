/*************************************************************************
	> File Name: QueryInterpreter.cpp
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Fri 22 Nov 2019 12:47:11 AM PST
 ************************************************************************/

#include<iostream>
#include <vector>
#include <string>
#include <stack>
#include <cctype>
#include <cstdio>
#include <cstring>

#include"fileOperation.h"
#include"Driver.h"
#include"QueryInterpreter.h"
#include"retrieve.h"
#include"decompress.h"
#include"log.h"

using namespace std;

static char priority[6][6]={
    {'>','<','<','<','>','>'},
    {'>','>','<','<','>','>'},
    {'>','>','x','<','>','>'},
    {'<','<','<','<','=','x'},
    {'>','>','x','x','>','>'},
    {'<','<','<','<','x','='}
};


static vector<uint64_t> getIntersection(vector<uint64_t> arr1,vector<uint64_t> arr2){
    vector<uint64_t> res;
    int i=0,j=0;
    int len1=arr1.size();
    int len2=arr2.size();
    while((i<len1)&&(j<len2)){
        if(arr1[i]>arr2[j]){
            j++;
        }else if(arr1[i]<arr2[j]){
            i++;
        }else{
            res.push_back(arr1[i]);
            i++;
            j++;
        }
    }
    return res;
}

static vector<uint64_t> getUnion(vector<uint64_t> arr1,vector<uint64_t> arr2){
    if(arr1.size() == 0) return arr2;
    if(arr2.size() == 0) return arr1;
    vector<uint64_t> merge;
    vector<uint64_t>::size_type  i = 0;
    vector<uint64_t>::size_type  j = 0;
    while(i < arr1.size() && j < arr2.size()){
        if(arr1[i] < arr2[j]) merge.push_back(arr1[i++]);
        else if(arr1[i] > arr2[j]) merge.push_back(arr2[j++]);
        else i++;
    }
    while(i < arr1.size()) merge.push_back(arr1[i++]);
    while(j < arr2.size()) merge.push_back(arr2[j++]);
    return merge;
}

static vector<uint64_t> getDiff(vector<uint64_t> arr1,vector<uint64_t> arr2){
    uint32_t len1 = arr1.size(), len2 = arr2.size();
    if(len1 == 0 || len2 == 0) return arr1;
    vector<uint64_t> diff;
    uint32_t i = 0, j = 0;
    while(i < len1 && j < len2){
        if(arr1[i] < arr2[j]) diff.push_back(arr1[i++]);
        else if(arr1[i] > arr2[j]) j++;
        else{
            i++;
            j++;
        }
    }
    while(i < len1) diff.push_back(arr1[i++]);
    return diff;
}

static vector<uint64_t> logicCompute(vector<uint64_t> arr1,vector<uint64_t> arr2,string optr){
    if(optr.compare("&&")==0){
        return getIntersection(arr1,arr2);
    }else if(optr.compare("||")==0){
        return getUnion(arr1,arr2);
    }else if(optr.compare("!")==0){
        return getDiff(arr1,arr2);
    }else{
        log_to_buf(err_msg+strlen(err_msg),ERROR,"unknown opreator:%s",optr.c_str());
        log(ERROR,"unknown opreator:%s",optr.c_str());
        return vector<uint64_t>();
    }
}

static int getPos(string optr){
    if(optr.compare("||")==0){
        return 0;
    }else if(optr.compare("&&")==0){
        return 1;
    }else if(optr.compare("!")==0){
        return 2;
    }else if(optr.compare("(")==0){
        return 3;
    }else if(optr.compare(")")==0){
        return 4;
    }else if(optr.compare("#")==0){
        return 5;
    }else{
        return -1;
    }
}

static void clearStack(stack<string> &optrStack,stack<vector<uint64_t> > &opndStack){
    while(!optrStack.empty()){
        optrStack.pop();
    }    
    while(!opndStack.empty()){
        opndStack.pop();
    }
}

static string getIndexName(const char *tmp){
    string res;
    //while(isalpha(*tmp)){
    while(*tmp != ' '){
        res+=*tmp;
        tmp++;
    }
    return res;
}

static int checkEqualSymbol(const char *tmp){
    int fowardNum=0;
    while(*tmp==' '){
        tmp++;
        fowardNum++;
    }
    if((*tmp!='=')||(*(tmp+1)!='=')){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"expect == before %s",tmp);
        log(ERROR,"expect == before %s",tmp);
        return -1;
    }
    fowardNum+=2;
    tmp+=2;
    while(*tmp==' '){
        tmp++;
        fowardNum++;
    }
    return fowardNum;
}

static int checkRelationOptr(const char *tmp,int *optr){
    int fowardNum = 0;
    while(*tmp==' '){
        tmp++;
        fowardNum++;
    }
    if(memcmp(relation_optr_str[EQ],tmp,strlen(relation_optr_str[EQ]))==0){
        fowardNum += strlen(relation_optr_str[EQ]);
        tmp += strlen(relation_optr_str[EQ]);
        *optr = EQ;
    }else if(memcmp(relation_optr_str[GE],tmp,strlen(relation_optr_str[GE]))==0){
        fowardNum += strlen(relation_optr_str[GE]);
        tmp += strlen(relation_optr_str[GE]);
        *optr = GE;
    }else if(memcmp(relation_optr_str[LE],tmp,strlen(relation_optr_str[LE]))==0){
        fowardNum += strlen(relation_optr_str[LE]);
        tmp += strlen(relation_optr_str[LE]);
        *optr = LE;
    }else if(memcmp(relation_optr_str[GT],tmp,strlen(relation_optr_str[GT]))==0){
        fowardNum += strlen(relation_optr_str[GT]);
        tmp += strlen(relation_optr_str[GT]);
        *optr = GT;
    }else if(memcmp(relation_optr_str[LT],tmp,strlen(relation_optr_str[LT]))==0){
        fowardNum += strlen(relation_optr_str[LT]);
        tmp += strlen(relation_optr_str[LT]);
        *optr = LT;
    }else{
        log_to_buf(err_msg+strlen(err_msg),ERROR,"expect == or >= or <= or > or < before %s",tmp);
        log(ERROR,"expect == or >= or <= or > or < before %s",tmp);
        *optr = -1;
        return -1;
    }
    while(*tmp==' '){
        tmp++;
        fowardNum++;
    }
    return fowardNum;
}

static inline int isHexChar(char ch){
    if(ch >= '0' && ch <= '9') return 1;
    if(ch >= 'a' && ch <= 'f') return 1;
    if(ch >= 'A' && ch <= 'F') return 1;
    return 0;
}

static int getIpValue(const char *tmp,char indexValue[],int af){
    int i=0;
    if(af == 4){
        while((isdigit(*tmp)||(*tmp=='.')||(*tmp=='*')||(*tmp=='/'))&&i<VAL_LEN-2){
            indexValue[i++]=*(tmp++);
        }
    }else{
        while((isHexChar(*tmp)||(*tmp==':')||(*tmp=='/'))&&i<VAL_LEN-2){
            indexValue[i++]=*(tmp++);
        }
    }
    indexValue[i]=0;
    if((indexValue[0]=='.')||(indexValue[strlen(indexValue)-1]=='.')){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"incorrect ip format: %s",indexValue);
        log(ERROR,"incorrect ip format: %s",indexValue);
        return -1;
    }
    if(i >= VAL_LEN-2){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"too long index value:%s",indexValue);
        log(ERROR,"too long index value:%s",indexValue);
        return -1;
    }
    return i;
}

static int getTsValue(const char *tmp,char indexValue[]){
    int i = 0;
    while((isdigit(*tmp)||(*tmp=='-')||(*tmp==':'))&&i<VAL_LEN-2) indexValue[i++] = *(tmp++);
    if((*tmp==' ') && (isdigit(*(tmp+1)))){
        indexValue[i++] = *(tmp++);
        while((isdigit(*tmp)||(*tmp==':'))&&i<VAL_LEN-2) indexValue[i++] = *(tmp++);
    }
    indexValue[i]=0;
    if(i >= VAL_LEN-2){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"too long index value:%s",indexValue);
        log(ERROR,"too long index value:%s",indexValue);
        return -1;
    }
    return i;
}

static int getShortIntValue(const char *tmp,char indexValue[]){
    int i = 0;
    if(*tmp == '0' && *tmp == 'x'){
        indexValue[i++] = '0';
        indexValue[i++] = 'x';
        tmp+=2;
    }
    while(isdigit(*tmp) && i<VAL_LEN-2) indexValue[i++] = *(tmp++);
    indexValue[i]=0;
    if(i >= VAL_LEN-2){
        log_to_buf(err_msg+strlen(err_msg),ERROR,"too long index value:%s",indexValue);
        log(ERROR,"too long index value:%s",indexValue);
        return -1;
    }
    return i;
}

static int getIndexValue(const char *tmp,char indexValue[],string indexName){
    if(indexName.compare(index_category_str[SIP])==0 || indexName.compare(index_category_str[DIP])==0){
        return getIpValue(tmp,indexValue,4);
    }else if(indexName.compare(index_category_str[SIP6])==0 || indexName.compare(index_category_str[DIP6])==0){
        return getIpValue(tmp,indexValue,6);
    }else if(indexName.compare(index_category_str[TS])==0){
        return getTsValue(tmp,indexValue);
    }else{
        return getShortIntValue(tmp,indexValue);
    }
}

static string getOptr(const char *tmp){
    string res;
    res+=*tmp;
    if((*tmp=='&')||(*tmp=='|')){
        res+=*(tmp+1);
    }
    return res;
}

static int getAllOffset(const char *path,const char *indexName,vector<uint64_t> &allOffset){
    //int decompressLen=tGetOffsetArr_range(path,indexName,NULL,NULL,0,0);
    int decompressLen=getOffsetArr_range(path,indexName,NULL,NULL,0,0);
    if(decompressLen==-1) return -1;
    allOffset.resize(decompressLen);
    for(int i=0;i<decompressLen;i++){
        allOffset[i] = invertedListBuf[i];
        //allOffset[i] = matchInvertList[i];
    }
    //if(decompressLen > BUF_ELE_NUM){
    if(dynMallocFlag){
        //free(matchInvertList);
        free(invertedListBuf);
        dynMallocFlag = 0;
    }
    return decompressLen;
}


int computeQuery(const char *queryLang,pQueryFile pqf,uint64_t resIndexBuf[]){
    int resLen=0;
    stack<string> optrStack;
    stack<vector<uint64_t> > opndStack;
    vector<uint64_t> allOffset;
    optrStack.push(string("#"));
    char query[QUERYLEN];
    strcpy(query,queryLang);
    int qLen=strlen(queryLang);
    query[qLen]='#';
    query[qLen+1]=0;
    char *tmp=query;
    while((*tmp)!='#'||(optrStack.top()).compare("#")!=0){
        if(isalpha(*tmp)){
            string indexName=getIndexName(tmp);
            char indexPath[MAX_PATH_LEN];
            if(indexName.compare(index_category_str[SIP])==0){
                getPath(indexPath,pqf->srcIpIndexFileName,INDEX);
            }else if(indexName.compare(index_category_str[SIP6])==0){
                getPath(indexPath,pqf->srcIp6IndexFileName,INDEX);
            }else if(indexName.compare(index_category_str[SPORT])==0){
                getPath(indexPath,pqf->srcPortIndexFileName,INDEX);
            }else if(indexName.compare(index_category_str[DIP])==0){
                getPath(indexPath,pqf->dstIpIndexFileName,INDEX);
            }else if(indexName.compare(index_category_str[DIP6])==0){
                getPath(indexPath,pqf->dstIp6IndexFileName,INDEX);
            }else if(indexName.compare(index_category_str[DPORT])==0){
                getPath(indexPath,pqf->dstPortIndexFileName,INDEX);
            }else if(indexName.compare(index_category_str[PROTO])==0){
                getPath(indexPath,pqf->protoIndexFileName,INDEX);
            }else if(indexName.compare(index_category_str[TS])==0){
                getPath(indexPath,pqf->tsIndexFileName,INDEX);
            }else{
                log_to_buf(err_msg+strlen(err_msg),ERROR,"not supported index: %s",indexName.c_str());
                log(ERROR,"not supported index: %s",indexName.c_str());
                clearStack(optrStack,opndStack);
                return -1;
            }
            tmp+=indexName.length();
            int optr;
            int fowardSteps = checkRelationOptr(tmp,&optr);
            if(fowardSteps==-1) {
                clearStack(optrStack,opndStack);
                return -1;               
            }
            tmp+=fowardSteps;
            char indexValue[VAL_LEN];
            int valueLen=getIndexValue(tmp,indexValue,indexName);
            if(valueLen == -1) {
                clearStack(optrStack,opndStack);
                return -1;               
            }else indexValue[valueLen] = 0;
            tmp+=valueLen;
            while(*tmp==' ') tmp++;
            int decompressLen;
            if(optr == EQ){
                decompressLen=getOffsetArr(indexPath,indexName.c_str(),indexValue);
                //decompressLen=tGetOffsetArr(indexPath,indexName.c_str(),indexValue);
            }else if(optr == GE){
                decompressLen=getOffsetArr_range(indexPath,indexName.c_str(),indexValue,NULL,0,0);
                //decompressLen=tGetOffsetArr_range(indexPath,indexName.c_str(),indexValue,NULL,0,0);
            }else if(optr == LE){
                decompressLen=getOffsetArr_range(indexPath,indexName.c_str(),NULL,indexValue,0,0);
                //decompressLen=tGetOffsetArr_range(indexPath,indexName.c_str(),NULL,indexValue,0,0);
            }else if(optr == GT){
                decompressLen=getOffsetArr_range(indexPath,indexName.c_str(),indexValue,NULL,1,0);
                //decompressLen=tGetOffsetArr_range(indexPath,indexName.c_str(),indexValue,NULL,1,0);
            }else if(optr == LT){
                decompressLen=getOffsetArr_range(indexPath,indexName.c_str(),NULL,indexValue,0,-1);
                //decompressLen=tGetOffsetArr_range(indexPath,indexName.c_str(),NULL,indexValue,0,-1);
            }else{
                log_to_buf(err_msg+strlen(err_msg),ERROR,"only support == >= <= > <");
                log(ERROR,"only support == >= <= > <");
            }
            if(decompressLen==-1){
                clearStack(optrStack,opndStack);
                return -1;
            }
            vector<uint64_t> opnd(decompressLen);
            for(int i=0;i<decompressLen;i++){
                opnd[i] = invertedListBuf[i];
                //opnd[i] = matchInvertList[i];
            }
            //if(decompressLen > BUF_ELE_NUM){
            if(dynMallocFlag){
                free(invertedListBuf);
                //free(matchInvertList);
                dynMallocFlag = 0;
            }
            opndStack.push(opnd);
        }else{
            string optr=getOptr(tmp);
            int pos=getPos(optr);
            if(pos==-1){
                log_to_buf(err_msg+strlen(err_msg),ERROR,"not supported operator: %s,only support && || ! ()",optr.c_str());
                log(ERROR,"not supported operator: %s,only support && || ! ()",optr.c_str());
                clearStack(optrStack,opndStack);
                return -1;
            }
            switch(priority[getPos(optrStack.top())][pos]){
                case '<':
                    optrStack.push(optr);
                    tmp+=optr.length();
                    while(*tmp==' ') tmp++;
                    break;
                case '=':
                    optrStack.pop();
                    tmp+=optr.length();
                    while(*tmp==' ') tmp++;
                    break;
                case '>':
                {
                    string opr=optrStack.top();
                    optrStack.pop();
                    vector<uint64_t> opn1;
                    if(opr.compare("!")==0){
                        if(allOffset.size() == 0){
                            char indexPath[MAX_PATH_LEN];
                            getPath(indexPath,pqf->srcPortIndexFileName,INDEX);
                            int sz = getAllOffset(indexPath,index_category_str[SPORT],allOffset);
                            if(sz == -1){
                                log_to_buf(err_msg+strlen(err_msg),ERROR,"error in ! computation");
                                log(ERROR,"error in ! computation");
                                clearStack(optrStack,opndStack);
                                return -1;
                            }
                        }
                        opn1 = allOffset;
                    }else{
                        opn1=opndStack.top();
                        opndStack.pop();
                    }
                    vector<uint64_t> opn2=opndStack.top();
                    opndStack.pop();
                    opndStack.push(logicCompute(opn1,opn2,opr));
                    break;
                }
                default:
                    log_to_buf(err_msg+strlen(err_msg),ERROR,"syntax error");
                    log(ERROR,"syntax error"); 
                    clearStack(optrStack,opndStack);
                    return -1;
            }
        }
    }
    vector<uint64_t> resVec=opndStack.top();
    opndStack.pop();
    resLen=resVec.size();
    for(int i=0;i<resLen && i<MAX_PKT_NUM_PER_RETR;i++){
        resIndexBuf[i]=resVec[i];
    }
    return resLen;
}
