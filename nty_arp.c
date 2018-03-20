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

#include <pthread.h>

static int nty_arp_output(nty_tcp_manager *tcp, int nif, int opcode,
		uint32_t dst_ip, unsigned char *dst_haddr, unsigned char *target_haddr);



struct arppkt {
	struct ethhdr eh;
	struct arphdr arp;
};

enum arp_opcode {
	arp_op_request = 1,
	arp_op_reply = 2,
};


typedef struct _nty_arp_queue_entry {
	uint32_t ip;
	int nif_out;
	uint32_t ts_out;
	TAILQ_ENTRY(_nty_arp_queue_entry) arp_link;
} nty_arp_queue_entry;

typedef struct _nty_arp_manager {
	TAILQ_HEAD(, _nty_arp_queue_entry) list;
	pthread_mutex_t lock;
} nty_arp_manager;

nty_arp_manager global_arp_manager;
nty_arp_table *global_arp_table = NULL;



int str2mac(char *mac, char *str) {

	char *p = str;
	unsigned char value = 0x0;
	int i = 0;

	while (p != '\0') {
		
		if (*p == ':') {
			mac[i++] = value;
			value = 0x0;
		} else {
			
			unsigned char temp = *p;
			if (temp <= '9' && temp >= '0') {
				temp -= '0';
			} else if (temp <= 'f' && temp >= 'a') {
				temp -= 'a';
				temp += 10;
			} else if (temp <= 'F' && temp >= 'A') {
				temp -= 'A';
				temp += 10;
			} else {	
				break;
			}
			value <<= 4;
			value |= temp;
		}
		p ++;
	}

	mac[i] = value;

	return 0;
}

void print_mac(unsigned char *mac) {
	int i = 0;
	for (i = 0;i < ETH_ALEN-1;i ++) {
		printf("%02x:", mac[i]);
	}
	printf("%02x", mac[i]);
}


void nty_arp_pkt(struct arppkt *arp, struct arppkt *arp_rt, char *hmac) {

	memcpy(arp_rt, arp, sizeof(struct arppkt));

	memcpy(arp_rt->eh.h_dest, arp->eh.h_source, ETH_ALEN);
	str2mac((char*)arp_rt->eh.h_source, hmac);
	arp_rt->eh.h_proto = arp->eh.h_proto;

	arp_rt->arp.h_addrlen = 6;
	arp_rt->arp.protolen = 4;
	arp_rt->arp.oper = htons(2);
	
	str2mac((char*)arp_rt->arp.smac, hmac);
	arp_rt->arp.sip = arp->arp.dip;
	
	memcpy(arp_rt->arp.dmac, arp->arp.smac, ETH_ALEN);
	arp_rt->arp.dip = arp->arp.sip;

}

extern nty_tcp_manager *nty_get_tcp_manager(void);

int nty_arp_process_request(struct arphdr *arph) {

	unsigned char *tmp = GetDestinationHWaddr(arph->sip);
	if (!tmp) {
		nty_arp_register_entry(arph->sip, arph->smac);
	}

	nty_tcp_manager *tcp = nty_get_tcp_manager();
	nty_arp_output(tcp, 0, arp_op_reply, arph->sip, arph->smac, NULL);

	return 0;
}

int nty_arp_process_reply(struct arphdr *arph) {
	unsigned char *tmp = GetDestinationHWaddr(arph->sip);
	if (!tmp) {
		nty_arp_register_entry(arph->sip, arph->smac);
	}

	pthread_mutex_lock(&global_arp_manager.lock);

	nty_arp_queue_entry *ent = NULL;
	TAILQ_FOREACH(ent, &global_arp_manager.list, arp_link) {
		if (ent->ip == arph->sip) {
			TAILQ_REMOVE(&global_arp_manager.list, ent, arp_link);
			free(ent);
			break;
		}
	}
	pthread_mutex_unlock(&global_arp_manager.lock);

	return 0;
}

int nty_arp_init_table(void) {
	global_arp_table = (nty_arp_table*)calloc(1, sizeof(nty_arp_table));
	if (!global_arp_table) return -1;

	global_arp_table->entries = 0;
	global_arp_table->entry = (nty_arp_entry*)calloc(MAX_ARPENTRY, sizeof(nty_arp_entry));
	if (!global_arp_table->entry) return -1;

	TAILQ_INIT(&global_arp_manager.list);
	pthread_mutex_init(&global_arp_manager.lock, NULL);

	return 0;
}

void nty_arp_print_table(void) {
	int i = 0;

	for (i = 0;i < global_arp_table->entries;i ++) {
		uint8_t *da = (uint8_t*)&global_arp_table->entry[i].ip;

		printf("IP addr: %u.%u.%u.%u, "
				"dst_hwaddr: %02X:%02X:%02X:%02X:%02X:%02X\n",
				da[0], da[1], da[2], da[3],
				global_arp_table->entry[i].haddr[0],
				global_arp_table->entry[i].haddr[1],
				global_arp_table->entry[i].haddr[2],
				global_arp_table->entry[i].haddr[3],
				global_arp_table->entry[i].haddr[4],
				global_arp_table->entry[i].haddr[5]);
	}

	if (global_arp_table->entries == 0)
		printf("(blank)\n");

	return ;
}

int nty_arp_register_entry(uint32_t ip, const unsigned char *haddr) {
	assert(global_arp_table != NULL);
	
	int idx = global_arp_table->entries;
	global_arp_table->entry[idx].prefix = 32;
	global_arp_table->entry[idx].ip = ip;

	memcpy(global_arp_table->entry[idx].haddr, haddr, ETH_ALEN);

	global_arp_table->entry[idx].ip_mask = -1;
	global_arp_table->entry[idx].ip_masked = ip;

	global_arp_table->entries = idx + 1;

	printf("Learned new arp entry.\n");
	nty_arp_print_table();

	return 0;
}

static int nty_arp_output(nty_tcp_manager *tcp, int nif, int opcode,
		uint32_t dst_ip, unsigned char *dst_haddr, unsigned char *target_haddr) {

	if (!dst_haddr) return -1;

	struct arphdr *arph = (struct arphdr*)EthernetOutput(tcp, PROTO_ARP, nif, dst_haddr, sizeof(struct arphdr));
	if (!arph) return -1;

	arph->h_type = htons(1);
	arph->h_proto = htons(PROTO_IP);
	arph->h_addrlen = ETH_ALEN;
	arph->protolen = 4;
	arph->oper = htons(opcode);

	arph->sip = NTY_SELF_IP_HEX;
	arph->dip = dst_ip;

	str2mac((char*)arph->smac, NTY_SELF_MAC);
	if (target_haddr) {
		memcpy(arph->dmac, target_haddr, arph->h_addrlen);
	} else {
		memcpy(arph->dmac, dst_haddr, arph->h_addrlen);
	}

	print_mac(arph->smac);
	printf("\n");
	print_mac(arph->dmac);
	printf("\n");
	printf("sip:%x, dip:%x\n", arph->sip, arph->dip);

	return 0;
}

void nty_arp_request(nty_tcp_manager *tcp, uint32_t ip, int nif, uint32_t cur_ts) {

	unsigned char haddr[ETH_ALEN];
	unsigned char taddr[ETH_ALEN];
	nty_arp_queue_entry *ent;

	pthread_mutex_lock(&global_arp_manager.lock);

	TAILQ_FOREACH(ent, &global_arp_manager.list, arp_link) {
		if (ent->ip == ip) {
			pthread_mutex_unlock(&global_arp_manager.lock);
			return ;
		}
	}

	ent = (nty_arp_queue_entry*)calloc(1, sizeof(nty_arp_queue_entry));
	ent->ip = ip;
	ent->nif_out = nif;
	ent->ts_out = cur_ts;

	TAILQ_INSERT_TAIL(&global_arp_manager.list, ent, arp_link);
	
	pthread_mutex_unlock(&global_arp_manager.lock);

	memset(haddr, 0xFF, ETH_ALEN);
	memset(taddr, 0x00, ETH_ALEN);

	nty_arp_output(tcp, nif, arp_op_request, ip, haddr, taddr);
}


int nty_arp_process(nty_nic_context *ctx, unsigned char *stream) {

	if (stream == NULL) return -1;

	struct arppkt *arp = (struct arppkt*)stream;
	
	if (arp->arp.dip == inet_addr(NTY_SELF_IP)) {
#if 0
		nty_arp_pkt(arp, &arp_rt, NTY_SELF_MAC);
		NTY_NIC_WRITE(ctx, &arp_rt, sizeof(struct arppkt));
#else

		switch (ntohs(arp->arp.oper)) {
			case arp_op_request : {
				nty_arp_process_request(&arp->arp);
				break;
			}
			case arp_op_reply : {
				nty_arp_process_reply(&arp->arp);
				break;
			}
		}

#endif
	}

	return 0;
}




