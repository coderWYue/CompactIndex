/*************************************************************************
	> File Name: common.h
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Wed 18 Sep 2019 01:19:22 AM PDT
 ************************************************************************/

#ifndef _COMMON_H
#define _COMMON_H
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <memory.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <stdint.h>
#include <limits.h>
#include <dirent.h>
#include <arpa/inet.h>


//#define UINT64_MAX  0xffffffffffffffff


#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

#define SUCCESS 0
#define FAILED -1
#define RESTART 1
#define QUEUE_NUM 1

#define PCAP_PACKET_HEAD_SIZE 16
#define PCAP_FILE_HEAD_SIZE 24

#define IPV6_ADDR_LEN 16
#define IPV4_ADDR_LEN 4
#define IPV6_BIT_LEN 128
#define IPV4_BIT_LEN 32

#define  I_R_YES    1    //作为出参或返回值
#define  I_R_NO     2    //作为出参或返回值

#define  I_R_OK        0  //作为函数的返回值
#define  I_R_ERROR    -1  //作为函数的返回值

#define  I_SET_FLAG    1  //设置flag值
#define  I_RESET_FLAG  0  //复位flag值
#define  LIBTDMS_VERSION  "1.1.0"


typedef struct pcapTimeStamp{
    uint32_t sec;
    uint32_t usec;
}__attribute__((__packed__)) pcapTimeStamp,*pPcapTimeStamp;

typedef struct pcapPacketHead{
    pcapTimeStamp ts;
    uint32_t caplen;
    uint32_t len;
}__attribute__((__packed__)) pcapPacketHead,*pPcapPacketHead;

typedef struct pcapFileHead{
    uint8_t magic[4];
    uint16_t major;
    uint16_t minor;
    int32_t thiszone;
    uint32_t sigfigs;
    uint32_t snaplen;
    uint32_t linktype;
}__attribute__((__packed__)) pcapFileHead,*pPcapFileHead;

typedef int8_t STATUS;

typedef uint8_t BYTE;

typedef unsigned __int128 uint128_t;


#endif
