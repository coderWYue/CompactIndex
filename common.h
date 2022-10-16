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
#include <sys/wait.h>
#include <sys/time.h>
#include <stdint.h>
#include <limits.h>
#include <dirent.h>
#include <arpa/inet.h>
#ifndef __cplusplus
#include <stdatomic.h>
#endif

#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

#define SUCCESS 0
#define FAILED -1
#define RESTART 1
#define QUEUE_NUM 1

#define PCAP_PACKET_HEAD_SIZE 16
#define PCAP_FILE_HEAD_SIZE 24

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

#endif
