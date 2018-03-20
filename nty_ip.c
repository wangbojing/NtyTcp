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




#include "nty_header.h"
#include "nty_tcp.h"
#include "nty_nic.h"
#include "nty_arp.h"

#include <stdio.h>
#include <stdlib.h>


#define IP_RF 0x8000 
#define IP_DF 0x4000 
#define IP_MF 0x2000 


int GetOutputInterface(uint32_t daddr) {
	return 0;
}

extern nty_arp_table *global_arp_table;
extern void nty_arp_request(nty_tcp_manager *tcp, uint32_t ip, int nif, uint32_t cur_ts);
extern int nty_udp_process(nty_nic_context *ctx, unsigned char *stream);
extern int nty_tcp_process(nty_nic_context *ctx, unsigned char *stream);
extern int nty_icmp_process(nty_nic_context *ctx, unsigned char *stream);

unsigned char *GetDestinationHWaddr(uint32_t dip) {
	unsigned char *d_haddr = NULL;
	int prefix = 0;
	int i = 0;

	for (i = 0;i < global_arp_table->entries;i ++) {
		if (global_arp_table->entry[i].prefix == 1) {
			if (global_arp_table->entry[i].ip == dip) {
				d_haddr = global_arp_table->entry[i].haddr;
				break;
			}
		} else {
			if ((dip & global_arp_table->entry[i].ip_mask) == global_arp_table->entry[i].ip_masked) {
				if (global_arp_table->entry[i].prefix > prefix) {
					d_haddr = global_arp_table->entry[i].haddr;
					prefix = global_arp_table->entry[i].prefix;
				}
			}
		}
	}
	return d_haddr;
}

static inline unsigned short ip_fast_csum(const void *iph, unsigned int ihl)
{
	unsigned int sum;

	__asm__ volatile(	"  movl (%1), %0\n"
		"  subl $4, %2\n"
		"  jbe 2f\n"
		"  addl 4(%1), %0\n"
		"  adcl 8(%1), %0\n"
		"  adcl 12(%1), %0\n"
		"1: adcl 16(%1), %0\n"
		"  lea 4(%1), %1\n"
		"  decl %2\n"
		"  jne	1b\n"
		"  adcl $0, %0\n"
		"  movl %0, %2\n"
		"  shrl $16, %0\n"
		"  addw %w2, %w0\n"
		"  adcl $0, %0\n"
		"  notl %0\n"
		"2:"
	/* Since the input registers which are loaded with iph and ipl
	   are modified, we must also specify them as outputs, or gcc
	   will assume they contain their original values. */
	: "=r" (sum), "=r" (iph), "=r" (ihl)
	: "1" (iph), "2" (ihl)
	: "memory");
	return (unsigned short)sum;
}



uint8_t *IPOutputStandalone(nty_tcp_manager *tcp, uint8_t protocol, 
		uint16_t ip_id, uint32_t saddr, uint32_t daddr, uint16_t payloadlen) {

	int nif = GetOutputInterface(daddr);
	if (nif < 0) {
		return NULL;
	}

	unsigned char *haddr = GetDestinationHWaddr(daddr);
	if (!haddr) {

	}

	struct iphdr *iph = (struct iphdr *)EthernetOutput(tcp, PROTO_IP, 0, haddr, payloadlen + IP_HEADER_LEN);
	if (iph == NULL) return NULL;
	
	iph->ihl = IP_HEADER_LEN >> 2;
	iph->version = 4;
	iph->tos = 0;
	iph->tot_len = htons(IP_HEADER_LEN + payloadlen);
	iph->id = htons(ip_id);
	iph->flag_off = htons(IP_DF);
	iph->ttl = 64;
	iph->protocol = protocol;
	iph->saddr = saddr;
	iph->daddr = daddr;
	iph->check = ip_fast_csum(iph, iph->ihl);

	return (uint8_t*)(iph + 1);
}


uint8_t *IPOutput(nty_tcp_manager *tcp, nty_tcp_stream *stream, uint16_t tcplen) {
	struct iphdr *iph;
	int nif = 0;

	if (stream->snd->nif_out >= 0) {
		nif = stream->snd->nif_out;
	} else {
		nif = GetOutputInterface(stream->daddr);
		stream->snd->nif_out = nif;
	}

	unsigned char *haddr = GetDestinationHWaddr(stream->daddr);
	if (!haddr) {
		nty_arp_request(tcp, stream->daddr, stream->snd->nif_out, tcp->cur_ts);
		return NULL;
	}


	iph = (struct iphdr*)EthernetOutput(tcp, PROTO_IP, stream->snd->nif_out, haddr, tcplen + IP_HEADER_LEN);
	if (!iph) return NULL;

	iph->ihl = IP_HEADER_LEN >> 2;
	iph->version = 4;
	iph->tos = 0;
	iph->tot_len = htons(IP_HEADER_LEN + tcplen);
	iph->id = htons(stream->snd->ip_id ++);
	iph->flag_off = htons(0x4000);
	iph->ttl = 64;
	iph->protocol = PROTO_TCP;
	iph->saddr = stream->saddr;
	iph->daddr = stream->daddr;
	iph->check = 0;

	iph->check = ip_fast_csum(iph, iph->ihl);

	return (uint8_t*)(iph+1);

}

int nty_ipv4_process(nty_nic_context *ctx, unsigned char *stream) {

	struct iphdr *iph = (struct iphdr*)(stream + sizeof(struct ethhdr));
	if (ip_fast_csum(iph, iph->ihl)) return -1;

	if (iph->protocol == PROTO_UDP) {
		nty_udp_process(ctx, stream);
	} else if (iph->protocol == PROTO_TCP) {
		nty_tcp_process(ctx, stream);
	} else if (iph->protocol == PROTO_ICMP) {
		nty_icmp_process(ctx, stream);
	}
	return 0;
}


