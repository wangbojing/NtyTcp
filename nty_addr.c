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

 

#include "nty_addr.h"

#include <pthread.h>

nty_addr_pool *CreateAddressPool(in_addr_t addr_base, int num_addr)
{
	nty_addr_pool *ap;
	int num_entry;
	int i, j, cnt;
	in_addr_t addr;
	uint32_t addr_h;

	ap = (nty_addr_pool *)calloc(1, sizeof(nty_addr_pool));
	if (!ap)
		return NULL;

	/* initialize address pool */
	num_entry = num_addr * (NTY_MAX_PORT - NTY_MIN_PORT);
	ap->pool = (nty_addr_entry *)calloc(num_entry, sizeof(nty_addr_entry));
	if (!ap->pool) {
		free(ap);
		return NULL;
	}

	/* initialize address map */
	ap->mapper = (nty_addr_map *)calloc(num_addr, sizeof(nty_addr_map));
	if (!ap->mapper) {
		free(ap->pool);
		free(ap);
		return NULL;
	}

	TAILQ_INIT(&ap->free_list);
	TAILQ_INIT(&ap->used_list);

	if (pthread_mutex_init(&ap->lock, NULL)) {
		free(ap->pool);
		free(ap);
		return NULL;
	}

	pthread_mutex_lock(&ap->lock);

	ap->addr_base = ntohl(addr_base);
	ap->num_addr = num_addr;

	cnt = 0;
	for (i = 0; i < num_addr; i++) {
		addr_h = ap->addr_base + i;
		addr = htonl(addr_h);
		for (j = NTY_MIN_PORT; j < NTY_MAX_PORT; j++) {
			ap->pool[cnt].addr.sin_addr.s_addr = addr;
			ap->pool[cnt].addr.sin_port = htons(j);
			ap->mapper[i].addrmap[j] = &ap->pool[cnt];
			
			TAILQ_INSERT_TAIL(&ap->free_list, &ap->pool[cnt], addr_link);

			if ((++cnt) >= num_entry)
				break;
		}
	}
	ap->num_entry = cnt;
	ap->num_free = cnt;
	ap->num_used = 0;
	
	pthread_mutex_unlock(&ap->lock);

	return ap;
}


nty_addr_pool *CreateAddressPoolPerCore(int core, int num_queues, 
		in_addr_t saddr_base, int num_addr, in_addr_t daddr, in_port_t dport)
{
	nty_addr_pool *ap;
	int num_entry;
	int i, j, cnt;
	in_addr_t saddr;
	uint32_t saddr_h, daddr_h;
	uint16_t sport_h, dport_h;
	int rss_core;
#if 0
	uint8_t endian_check = (current_iomodule_func == &dpdk_module_func) ?
		0 : 1;
#else
	uint8_t endian_check = 1;
#endif

	ap = (nty_addr_pool *)calloc(1, sizeof(nty_addr_pool));
	if (!ap)
		return NULL;

	/* initialize address pool */
	num_entry = (num_addr * (NTY_MAX_PORT - NTY_MIN_PORT)) / num_queues;
	ap->pool = (nty_addr_entry *)calloc(num_entry, sizeof(nty_addr_entry));
	if (!ap->pool) {
		free(ap);
		return NULL;
	}
	
	/* initialize address map */
	ap->mapper = (nty_addr_map *)calloc(num_addr, sizeof(nty_addr_map));
	if (!ap->mapper) {
		free(ap->pool);
		free(ap);
		return NULL;
	}

	TAILQ_INIT(&ap->free_list);
	TAILQ_INIT(&ap->used_list);

	if (pthread_mutex_init(&ap->lock, NULL)) {
		free(ap->pool);
		free(ap);
		return NULL;
	}

	pthread_mutex_lock(&ap->lock);

	ap->addr_base = ntohl(saddr_base);
	ap->num_addr = num_addr;
	daddr_h = ntohl(daddr);
	dport_h = ntohs(dport);

	/* search address space to get RSS-friendly addresses */
	cnt = 0;
	for (i = 0; i < num_addr; i++) {
		saddr_h = ap->addr_base + i;
		saddr = htonl(saddr_h);
		for (j = NTY_MIN_PORT; j < NTY_MAX_PORT; j++) {
			if (cnt >= num_entry)
				break;

			sport_h = j;
			rss_core = GetRSSCPUCore(daddr_h, saddr_h, dport_h, sport_h, num_queues, endian_check);
			if (rss_core != core)
				continue;

			ap->pool[cnt].addr.sin_addr.s_addr = saddr;
			ap->pool[cnt].addr.sin_port = htons(sport_h);
			ap->mapper[i].addrmap[j] = &ap->pool[cnt];
			TAILQ_INSERT_TAIL(&ap->free_list, &ap->pool[cnt], addr_link);
			cnt++;
		}
	}

	ap->num_entry = cnt;
	ap->num_free = cnt;
	ap->num_used = 0;
	//fprintf(stderr, "CPU %d: Created %d address entries.\n", core, cnt);
#if 0
	if (ap->num_entry < CONFIG.max_concurrency) {
		fprintf(stderr, "[WARINING] Available # addresses (%d) is smaller than"
				" the max concurrency (%d).\n", 
				ap->num_entry, CONFIG.max_concurrency);
	}
#endif

	pthread_mutex_unlock(&ap->lock);

	return ap;
}

void DestroyAddressPool(nty_addr_pool *ap) {
	if (!ap)
		return;

	if (ap->pool) {
		free(ap->pool);
		ap->pool = NULL;
	}

	if (ap->mapper) {
		free(ap->mapper);
		ap->mapper = NULL;
	}

	pthread_mutex_destroy(&ap->lock);

	free(ap);
}

int FetchAddress(nty_addr_pool *ap, int core, int num_queues, 
		const struct sockaddr_in *daddr, struct sockaddr_in *saddr)
{
	nty_addr_entry *walk, *next;
	int rss_core;
	int ret = -1;
#if 0
	uint8_t endian_check = (current_iomodule_func == &dpdk_module_func) ?
		0 : 1;
#else
	uint8_t endian_check = 1;
#endif

	if (!ap || !daddr || !saddr)
		return -1;

	pthread_mutex_lock(&ap->lock);

	walk = TAILQ_FIRST(&ap->free_list);
	while (walk) {
		next = TAILQ_NEXT(walk, addr_link);

		if (saddr->sin_addr.s_addr != INADDR_ANY &&
		    walk->addr.sin_addr.s_addr != saddr->sin_addr.s_addr) {
			walk = next;
			continue;
		}

		if (saddr->sin_port != INPORT_ANY &&
		    walk->addr.sin_port != saddr->sin_port) {
			walk = next;
			continue;
		}

		rss_core = GetRSSCPUCore(ntohl(walk->addr.sin_addr.s_addr), 
					 ntohl(daddr->sin_addr.s_addr), ntohs(walk->addr.sin_port), 
					 ntohs(daddr->sin_port), num_queues, endian_check);

		if (core == rss_core)
			break;

		walk = next;
	}

	if (walk) {
		*saddr = walk->addr;
		TAILQ_REMOVE(&ap->free_list, walk, addr_link);
		TAILQ_INSERT_TAIL(&ap->used_list, walk, addr_link);
		ap->num_free--;
		ap->num_used++;
		ret = 0;
	}
	
	pthread_mutex_unlock(&ap->lock);

	return ret;
}

int FetchAddressPerCore(nty_addr_pool *ap, int core, int num_queues,
		    const struct sockaddr_in *daddr, struct sockaddr_in *saddr)
{
	nty_addr_entry *walk;
	int ret = -1;

	if (!ap || !daddr || !saddr)
		return -1;

	pthread_mutex_lock(&ap->lock);
	
	/* we don't need to calculate RSSCPUCore if mtcp_init_rss is called */
	walk = TAILQ_FIRST(&ap->free_list);
	if (walk) {
		*saddr = walk->addr;
		TAILQ_REMOVE(&ap->free_list, walk, addr_link);
		TAILQ_INSERT_TAIL(&ap->used_list, walk, addr_link);
		ap->num_free--;
		ap->num_used++;
		ret = 0;
	}
	
	pthread_mutex_unlock(&ap->lock);
	
	return ret;
}

int FreeAddress(nty_addr_pool *ap, const struct sockaddr_in *addr)
{
	nty_addr_entry *walk, *next;
	int ret = -1;

	if (!ap || !addr)
		return -1;

	pthread_mutex_lock(&ap->lock);

	if (ap->mapper) {
		uint32_t addr_h = ntohl(addr->sin_addr.s_addr);
		uint16_t port_h = ntohs(addr->sin_port);
		int index = addr_h - ap->addr_base;

		if (index >= 0 && index < ap->num_addr) {
			walk = ap->mapper[addr_h - ap->addr_base].addrmap[port_h];
		} else {
			walk = NULL;
		}

	} else {
		walk = TAILQ_FIRST(&ap->used_list);
		while (walk) {
			next = TAILQ_NEXT(walk, addr_link);
			if (addr->sin_port == walk->addr.sin_port && 
					addr->sin_addr.s_addr == walk->addr.sin_addr.s_addr) {
				break;
			}

			walk = next;
		}

	}

	if (walk) {
		TAILQ_REMOVE(&ap->used_list, walk, addr_link);
		TAILQ_INSERT_TAIL(&ap->free_list, walk, addr_link);
		ap->num_free++;
		ap->num_used--;
		ret = 0;
	}

	pthread_mutex_unlock(&ap->lock);

	return ret;
}






static void 
BuildKeyCache(uint32_t *cache, int cache_len)
{
#define NBBY 8 /* number of bits per byte */

	/* Keys for system testing */
	static const uint8_t key[] = {
		 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
		 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
		 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
		 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
		 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05
	};

	uint32_t result = (((uint32_t)key[0]) << 24) | 
		(((uint32_t)key[1]) << 16) | 
		(((uint32_t)key[2]) << 8)  | 
		((uint32_t)key[3]);

	uint32_t idx = 32;
	int i;

	for (i = 0; i < cache_len; i++, idx++) {
		uint8_t shift = (idx % NBBY);
		uint32_t bit;

		cache[i] = result;
		bit = ((key[idx/NBBY] << shift) & 0x80) ? 1 : 0;
		result = ((result << 1) | bit);
	}
}
/*-------------------------------------------------------------*/ 
static uint32_t 
GetRSSHash(in_addr_t sip, in_addr_t dip, in_port_t sp, in_port_t dp)
{
#define MSB32 0x80000000
#define MSB16 0x8000
#define KEY_CACHE_LEN 96

	uint32_t res = 0;
	int i;
	static int first = 1;
	static uint32_t key_cache[KEY_CACHE_LEN] = {0};
	
	if (first) {
		BuildKeyCache(key_cache, KEY_CACHE_LEN);
		first = 0;
	}

	for (i = 0; i < 32; i++) {
		if (sip & MSB32)
			res ^= key_cache[i];
		sip <<= 1;
	}
	for (i = 0; i < 32; i++) {
		if (dip & MSB32)
			res ^= key_cache[32+i];
		dip <<= 1;
	}
	for (i = 0; i < 16; i++) {
		if (sp & MSB16)
			res ^= key_cache[64+i];
		sp <<= 1;
	}
	for (i = 0; i < 16; i++) {
		if (dp & MSB16)
			res ^= key_cache[80+i];
		dp <<= 1;
	}
	return res;
}
/*-------------------------------------------------------------------*/ 
/* RSS redirection table is in the little endian byte order (intel)  */
/*                                                                   */
/* idx: 0 1 2 3 | 4 5 6 7 | 8 9 10 11 | 12 13 14 15 | 16 17 18 19 ...*/
/* val: 3 2 1 0 | 7 6 5 4 | 11 10 9 8 | 15 14 13 12 | 19 18 17 16 ...*/
/* qid = val % num_queues */
/*-------------------------------------------------------------------*/ 
int
GetRSSCPUCore(in_addr_t sip, in_addr_t dip, 
	      in_port_t sp, in_port_t dp, int num_queues, uint8_t endian_check)
{
	#define RSS_BIT_MASK 0x0000007F
	uint32_t masked = GetRSSHash(sip, dip, sp, dp) & RSS_BIT_MASK;

	if (endian_check) {
		static const uint32_t off[4] = {3, 1, -1, -3};
		masked += off[masked & 0x3];
	}

	return (masked % num_queues);
}





