/*
 *  Author : WangBoJing , email : 1989wangbojing@gmail.com
 * 
 *  Copyright Statement:
 *  --------------------
 *  This software is protected by Copyright and the information contained
 *  herein is confidential. The software may not be copied and the information
 *  contained herein may not be used or disclosed except with the written
 *  permission of Author. (C) 2017
 * 
 *

****       *****                                  ************
  ***        *                                    **   **    *
  ***        *         *                          *    **    **
  * **       *         *                         *     **     *
  * **       *         *                         *     **     *
  *  **      *        **                               **
  *  **      *       ***                               **
  *   **     *    ***********    *****    *****        **             *****         *  ****
  *   **     *        **           **      **          **           ***    *     **** *   **
  *    **    *        **           **      *           **           **     **      ***     **
  *    **    *        **            *      *           **          **      **      **       *
  *     **   *        **            **     *           **         **       **      **       **
  *     **   *        **             *    *            **         **               **       **
  *      **  *        **             **   *            **         **               **       **
  *      **  *        **             **   *            **         **               **       **
  *       ** *        **              *  *             **         **               **       **
  *       ** *        **              ** *             **         **               **       **
  *        ***        **               * *             **         **         *     **       **
  *        ***        **     *         **              **          **        *     **      **
  *         **        **     *         **              **          **       *      ***     **
  *         **         **   *          *               **           **     *       ****   **
*****        *          ****           *             ******           *****        **  ****
                                       *                                           **
                                      *                                            **
                                  *****                                            **
                                  ****                                           ******

 *
 */




#ifndef __NTY_HEADER_H__
#define __NTY_HEADER_H__


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <sys/poll.h>



#define NTY_SELF_IP		"192.168.0.109"//"192.168.1.108" //"192.168.1.132" //"192.168.1.131"  //
#define NTY_SELF_IP_HEX	0x6D00A8C0 //0x8301A8C0 //
#define NTY_SELF_MAC	"00:0c:29:58:6f:f4"

#define NTY_MAX_CONCURRENCY		1024
#define NTY_SNDBUF_SIZE			8192
#define NTY_RCVBUF_SIZE			8192
#define NTY_MAX_NUM_BUFFERS		1024
#define NTY_BACKLOG_SIZE		1024

#define NTY_ENABLE_MULTI_NIC	0
#define NTY_ENABLE_BLOCKING		1

#define ETH_ALEN		6

#define IP_HEADER_LEN		20
#define TCP_HEADER_LEN		20

#define PROTO_IP	0x0800
#define PROTO_ARP	0x0806

#define PROTO_UDP	17
#define PROTO_TCP	6
#define PROTO_ICMP	1
#define PROTO_IGMP	2


struct ethhdr {
	unsigned char h_dest[ETH_ALEN];
	unsigned char h_source[ETH_ALEN];
	unsigned short h_proto;
} __attribute__ ((packed));

struct iphdr {
	unsigned char ihl:4,
				version:4;
	unsigned char tos;
	unsigned short tot_len;
	unsigned short id;
	unsigned short flag_off;
	unsigned char ttl;
	unsigned char protocol;
	unsigned short check;
	unsigned int saddr;
	unsigned int daddr;
} __attribute__ ((packed));

struct udphdr {
	unsigned short source;
	unsigned short dest;
	unsigned short len;
	unsigned short check;
} __attribute__ ((packed));

struct udppkt {
	struct ethhdr eh;
	struct iphdr ip;
	struct udphdr udp;
	unsigned char body[128];
} __attribute__ ((packed));


struct tcphdr {
	unsigned short source;
	unsigned short dest;
	unsigned int seq;
	unsigned int ack_seq;

	unsigned short res1:4, 
		doff:4, 
		fin:1,
		syn:1,
		rst:1,
		psh:1,
		ack:1,
		urg:1,
		ece:1,
		cwr:1;
	unsigned short window;
	unsigned short check;
	unsigned short urg_ptr;
} __attribute__ ((packed));

struct arphdr {
	unsigned short h_type;
	unsigned short h_proto;
	unsigned char h_addrlen;
	unsigned char protolen;
	unsigned short oper;
	unsigned char smac[ETH_ALEN];
	unsigned int sip;
	unsigned char dmac[ETH_ALEN];
	unsigned int dip;
} __attribute__ ((packed));


struct icmphdr {
	unsigned char type;
	unsigned char code;
	unsigned short check;
	unsigned short identifier;
	unsigned short seq;
	unsigned char data[32];
} __attribute__ ((packed));


struct icmppkt {
	struct ethhdr eh;
	struct iphdr ip;
	struct icmphdr icmp;
} __attribute__ ((packed));

//unsigned short in_cksum(unsigned short *addr, int len);

//#define NTY_DEBUG 1
#ifdef NTY_DEBUG
#define ntydbg(format, ...) 		fprintf(stdout, format, ##__VA_ARGS__)
#define nty_trace_api(format, ...) 		fprintf(stdout, format, ##__VA_ARGS__)
#define nty_trace_tcp(format, ...) 		fprintf(stdout, format, ##__VA_ARGS__)
#define nty_trace_buffer(format, ...) 		fprintf(stdout, format, ##__VA_ARGS__)
#define nty_trace_eth(format, ...) 		fprintf(stdout, format, ##__VA_ARGS__)

#else
#define ntydbg(format, ...) 
#define nty_trace_api(format, ...)
#define nty_trace_tcp(format, ...) 
#define nty_trace_buffer(format, ...)
#define nty_trace_eth(format, ...)
#endif


#endif


