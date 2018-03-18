

#ifndef __NTY_ADDR_H__
#define __NTY_ADDR_H__

#include "nty_queue.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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

 

#include <netinet/in.h>


#define NTY_MIN_PORT			1025
#define NTY_MAX_PORT			65535

#ifndef INPORT_ANY
#define INPORT_ANY 	(uint16_t)0
#endif


typedef struct _nty_addr_entry {
	struct sockaddr_in addr;
	TAILQ_ENTRY(_nty_addr_entry) addr_link;
} nty_addr_entry;

typedef struct _nty_addr_map {
	nty_addr_entry *addrmap[NTY_MAX_PORT]; 
} nty_addr_map;

typedef struct _nty_addr_pool {
	nty_addr_entry *pool;
	nty_addr_map *mapper;

	uint32_t addr_base;

	int num_addr;
	int num_entry;
	int num_free;
	int num_used;

	pthread_mutex_t lock;
	TAILQ_HEAD(, _nty_addr_entry) free_list;
	TAILQ_HEAD(, _nty_addr_entry) used_list;
} nty_addr_pool;


nty_addr_pool *CreateAddressPool(in_addr_t addr_base, int num_addr);
nty_addr_pool *CreateAddressPoolPerCore(int core, int num_queues, 
		in_addr_t saddr_base, int num_addr, in_addr_t daddr, in_port_t dport);

void DestroyAddressPool(nty_addr_pool *ap);
int FetchAddress(nty_addr_pool *ap, int core, int num_queues, 
		const struct sockaddr_in *daddr, struct sockaddr_in *saddr);

int FetchAddressPerCore(nty_addr_pool *ap, int core, int num_queues,
		    const struct sockaddr_in *daddr, struct sockaddr_in *saddr);

int FreeAddress(nty_addr_pool *ap, const struct sockaddr_in *addr);


int GetRSSCPUCore(in_addr_t sip, in_addr_t dip, 
	      in_port_t sp, in_port_t dp, int num_queues, uint8_t endian_check);


#endif


