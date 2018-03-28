/*
 * MIT License
 *
 * Copyright (c) [2018] [WangBoJing]

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
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
#include "nty_nic.h"
#include "nty_arp.h"

#include <pthread.h>

//gcc -o nty_stack *.o -lpthread -lhugetlbfs

unsigned short in_cksum(unsigned short *addr, int len) {
	register int nleft = len;
	register unsigned short *w = addr;
	register int sum = 0;
	unsigned short answer = 0;

	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	if (nleft == 1) {
		*(u_char *)(&answer) = *(u_char *)w ;
		sum += answer;
	}

	sum = (sum >> 16) + (sum & 0xffff);	
	sum += (sum >> 16);			
	answer = ~sum;
	
	return (answer);

}


uint8_t *EthernetOutput(nty_tcp_manager *tcp, uint16_t h_proto,
	int nif, unsigned char* dst_haddr, uint16_t iplen) {

	nty_thread_context *ctx = tcp->ctx;

	uint8_t *buf = (uint8_t*)nty_nic_get_wbuffer(ctx->io_private_context, 0, iplen+ETHERNET_HEADER_LEN);
	if (buf == NULL) return NULL;

	struct ethhdr *ethh = (struct ethhdr*)buf;
	int i = 0;

	str2mac((char*)ethh->h_source, NTY_SELF_MAC);
	for (i = 0;i < ETH_ALEN;i ++) {
		ethh->h_dest[i] = dst_haddr[i];
	}
	ethh->h_proto = htons(h_proto);

	return (uint8_t*)(ethh+1);
}

extern int nty_ipv4_process(nty_nic_context *ctx, unsigned char *stream);

static int nty_eth_process(nty_nic_context *ctx, unsigned char *stream) {

	struct ethhdr *eh = (struct ethhdr*)stream;

	if (ntohs(eh->h_proto) == PROTO_IP) {
		nty_ipv4_process(ctx, stream);
	} else if (ntohs(eh->h_proto) == PROTO_ARP) {
		nty_arp_process(ctx, stream);
	}

	return 0;
}


extern nty_tcp_manager *nty_get_tcp_manager(void);
extern void CheckRtmTimeout(nty_tcp_manager *tcp, uint32_t cur_ts, int thresh);
extern void CheckTimewaitExpire(nty_tcp_manager *tcp, uint32_t cur_ts, int thresh);
extern void CheckConnectionTimeout(nty_tcp_manager *tcp, uint32_t cur_ts, int thresh);



static void *nty_tcp_run(void *arg) {
	nty_nic_context *ctx = (nty_nic_context *)arg;

	nty_tcp_manager *tcp = nty_get_tcp_manager();

	while (1) {

		struct pollfd pfd = {0};
		pfd.fd = ctx->nmr->fd;
		pfd.events = POLLIN | POLLOUT;
		
		int ret = poll(&pfd, 1, -1);
		if (ret < 0) continue;

		// check send data should 
		struct timeval cur_ts = {0};
		gettimeofday(&cur_ts, NULL);
		uint32_t ts = TIMEVAL_TO_TS(&cur_ts);

		if (tcp->flow_cnt > 0) {
			CheckRtmTimeout(tcp, ts, NTY_MAX_CONCURRENCY);
			CheckTimewaitExpire(tcp, ts, NTY_MAX_CONCURRENCY);
			CheckConnectionTimeout(tcp, ts, NTY_MAX_CONCURRENCY);
			
			nty_tcp_handle_apicall(ts);
		}

		nty_tcp_write_chunks(ts);
		
		if (!(pfd.revents & POLLERR)) ctx->dev_poll_flag = 1;

		if (pfd.revents & POLLIN) {

			unsigned char *stream = NULL;
			nty_nic_read(ctx, &stream);
			nty_eth_process(ctx, stream);

		} else if (pfd.revents & POLLOUT) {

			nty_nic_send_pkts(ctx, 0);
		} 
				
	}

	return NULL;
}

void nty_tcp_setup(void) {
	nty_thread_context *tctx = (nty_thread_context*)calloc(1, sizeof(nty_thread_context));
	assert(tctx != NULL);
	printf("nty_stack start\n");

	//int ret = NTY_NIC_INIT(tctx, "netmap:eth0");
	int ret = nty_nic_init(tctx, "netmap:eth1");
	if (ret != 0) {
		printf("init nic failed\n");
		return ;
	}
	nty_tcp_init_thread_context(tctx);
	nty_nic_context *ctx = (nty_nic_context*)tctx->io_private_context;

	nty_arp_init_table();

	pthread_t thread_id;
	ret = pthread_create(&thread_id, NULL, nty_tcp_run, ctx);
	assert(ret == 0);

}


