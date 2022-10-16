/*************************************************************************
	> File Name: test.c
	> Author: Tom Won
	> Mail: yuew951115@qq.com
	> Created Time: Sat 09 May 2020 10:06:11 PM CST
 ************************************************************************/

#include "common.h"
#include "log.h"
#include "./tdms.h"
#include "net.h"
#include <fcntl.h>
#include <pthread.h>

#define FRAME_SZ 0x80000000-0x040000

uint64_t filesz = 0;

uint8_t *pcapBuf;

long lens[2];
void* bufs[2];

static long  read_traffic_from_file(BYTE *buf,const char *filePath,uint64_t off){
    struct stat st;
    if(stat(filePath,&st) == -1){
        printf("fail to get file %s size!\n",filePath);
        return -1;
    } 
    unsigned long fsz = st.st_size;
    int rfd = open(filePath,O_RDONLY,0664);
    if(rfd == -1){
        printf("fail to open file!\n");
        return -1;
    }
    if(lseek(rfd,off,SEEK_SET) == -1) return -1;
    long rsz = read(rfd,buf,fsz-off);
    if(rsz != (fsz-off)){
        printf("read file error!\n");
        return -1;
    }
    close(rfd);
    return rsz;
}

void init_init_param(tdmsInitParam *para){
    para->core_num = 1;
    para->db_port = 3307;
    para->warning_thres = 0.7;
    para->rewrite_thres = 0.85;
    para->search_ret_sz = 5;
    para->data_buf_sz = (uint64_t)1024*1024*1024*2;
    para->disk_cap = (uint64_t)1024*1024*1024*8;
    strcpy(para->data_storage_dir,"/home/tdms_b/test/data/");
    strcpy(para->index_storage_dir,"/home/tdms_b/test/ind/");
    strcpy(para->log_storage_dir,"/home/tdms_b/test/log/");
    strcpy(para->result_storage_dir,"/home/tdms_b/test/result/");
    strcpy(para->db_address,"localhost");
    strcpy(para->db_uname,"test");
    strcpy(para->db_pwd,"135246");
    strcpy(para->db_name,"tdms_db");
}

int gen_dig(void *pcapBuf,int pos,tdmsPktDigest *dig){
    pPcapPacketHead pHdr = (void *)pcapBuf+pos;
    dig->ts.tv_sec = pHdr->ts.sec;
    dig->ts.tv_usec = pHdr->ts.usec;
    pos += PCAP_PACKET_HEAD_SIZE;
    //ETH_HEADER *eHdr = (void *)pcapBuf + pos;

    /*
    if(eHdr->ether_type == ETHERTYPE_IP){
        pos += sizeof(ETH_HEADER);
        IP_HEADER *ipHdr = (void *)pcapBuf + pos;
        dig->proto = ipHdr->ip_p;
        memcpy(dig->src_ip.ip4,&(ipHdr->ip_src),4);
        memcpy(dig->dst_ip.ip4,&(ipHdr->ip_dst),4);
        pos += sizeof(IP_HEADER);
        if(ipHdr->ip_p == 6){
            TCP_HEADER *tHdr = (void *)pcapBuf + pos;
            dig->src_port = (tHdr->th_sport >> 8) | (tHdr->th_sport << 8);
            dig->dst_port = (tHdr->th_dport >> 8) | (tHdr->th_dport << 8);
        }else if(ipHdr->ip_p == 17){
            UDP_HEADER *uHdr = (void *)pcapBuf + pos;
            dig->src_port = (uHdr->uh_sport >> 8) | (uHdr->uh_sport << 8);
            dig->dst_port = (uHdr->uh_dport >> 8) | (uHdr->uh_dport << 8);
        }else{
            dig->src_port = 0;
            dig->dst_port = 0;
        }
        dig->af = 4;
    }else if(eHdr->ether_type == ETHERTYPE_IPV6){
        pos += sizeof(ETH_HEADER);
        struct ipv6_hdr *ip6Hdr = (void *)pcapBuf + pos;

        dig->proto = ip6Hdr->proto;

        memcpy(dig->src_ip.ip6,&(ip6Hdr->src_addr),16);
        memcpy(dig->dst_ip.ip6,&(ip6Hdr->dst_addr),16);

        pos += sizeof(struct ipv6_hdr);

        if(ip6Hdr->proto == 6){
            TCP_HEADER *tHdr = (void *)pcapBuf + pos;
            dig->src_port = (tHdr->th_sport >> 8) | (tHdr->th_sport << 8);
            dig->dst_port = (tHdr->th_dport >> 8) | (tHdr->th_dport << 8);
        }else if(ip6Hdr->proto == 17){
            UDP_HEADER *uHdr = (void *)pcapBuf + pos;
            dig->src_port = (uHdr->uh_sport >> 8) | (uHdr->uh_sport << 8);
            dig->dst_port = (uHdr->uh_dport >> 8) | (uHdr->uh_dport << 8);
        }else{
            dig->src_port = 0;
            dig->dst_port = 0;
        }
        dig->af = 6;
    }else{
        dig->proto = 0;
        dig->src_port = 0;
        dig->dst_port = 0;
        memset(dig->src_ip.ip6,0,16);
        memset(dig->dst_ip.ip6,0,16);
        dig->af = 6;
    }
    */

    ETH_HEADER *eHdr = (void *)pcapBuf + pos;
    gettimeofday(&(dig->ts),NULL);
    if(eHdr->ether_type == ETHERTYPE_IP){
        pos += sizeof(ETH_HEADER);
        IP_HEADER *ipHdr = (void *)pcapBuf + pos;
        dig->proto = ipHdr->ip_p;
        memcpy(dig->src_ip.ip4,&(ipHdr->ip_src),4);
        memcpy(dig->dst_ip.ip4,&(ipHdr->ip_dst),4);
        pos += sizeof(IP_HEADER);
        if(ipHdr->ip_p == 6){
            TCP_HEADER *tHdr = (void *)pcapBuf + pos;
            dig->src_port = (tHdr->th_sport >> 8) | (tHdr->th_sport << 8);
            dig->dst_port = (tHdr->th_dport >> 8) | (tHdr->th_dport << 8);
        }else if(ipHdr->ip_p == 17){
            UDP_HEADER *uHdr = (void *)pcapBuf + pos;
            dig->src_port = (uHdr->uh_sport >> 8) | (uHdr->uh_sport << 8);
            dig->dst_port = (uHdr->uh_dport >> 8) | (uHdr->uh_dport << 8);
        }else{
            dig->src_port = 0;
            dig->dst_port = 0;
        }
        dig->af = 4;
    }else{
        dig->proto = 0;
        dig->src_port = 0;
        dig->dst_port = 0;
        memset(dig->src_ip.ip4,0,4);
        memset(dig->dst_ip.ip4,0,4);
        dig->af = 4;
    }

    return pHdr->caplen;
}

int main(int argc,char *argv[]){
    tdmsInitParam init_param;
    init_init_param(&init_param);
    int init_res = tdms_init(&init_param,0);
    if(init_res == -1){
        printf("%s",tdms_init_status);
        return 0;
    }else{
        printf("init success\n");
    }

    pcapBuf = (uint8_t *)malloc(0x80040000);

    long filesz = read_traffic_from_file(pcapBuf,"/home/trace/1.pcap",0);
    if(filesz == -1) return 0;

    long pos = PCAP_FILE_HEAD_SIZE;

    int i = 0;
    while(pos < filesz && pos < FRAME_SZ){
        tdmsPktDigest *dig = (tdmsPktDigest *)malloc(sizeof(tdmsPktDigest));
        int plen = gen_dig(pcapBuf,pos,dig);
        int r = tdms_store_proc(pcapBuf+pos+PCAP_PACKET_HEAD_SIZE,plen,dig,0,1);
        if(r == -1){
            printf("%s",tdms_store_status);
            return 0;
        }
        pos += (PCAP_PACKET_HEAD_SIZE + plen);
        i++;
        free(dig);
    }

    printf("total %d pkts\n",i);

    free(pcapBuf);

    tdms_uninit(0,1);
}
