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



#include "nty_nic.h"
#include <sys/poll.h>

/*
 * 1. init 
 * 2. read
 * 3. write
 */


int nty_nic_init(nty_thread_context *tctx, const char *ifname) {

	if (tctx == NULL) return -1;

	nty_nic_context *ctx = calloc(1, sizeof(nty_nic_context));
	if (ctx == NULL) {
		return -2;
	}
	tctx->io_private_context = ctx;

	struct nmreq req;
	memset(&req, 0, sizeof(struct nmreq));
	req.nr_arg3 = EXTRA_BUFS;
	
	ctx->nmr = nm_open(ifname, &req, 0, NULL);
	if (ctx->nmr == NULL) return -2;

	return 0;
}


static int nty_nic_read(nty_nic_context *ctx, unsigned char **stream) {

	if (ctx == NULL) return -1;

	struct nm_pkthdr h;
	*stream = nm_nextpkt(ctx->nmr, &h);

	return 0;
}

static int nty_nic_write(nty_nic_context *ctx, const void *stream, int length) {

	if (ctx == NULL) return -1;
	if (stream == NULL) return -2;
	if (length == 0) return 0;

	nm_inject(ctx->nmr, stream, length);

	return 0;
}

int nty_nic_send_pkts(nty_nic_context *ctx, int nif) {

	if (ctx->snd_pkt_size == 0) return -1;

tx_again:
	if (nm_inject(ctx->nmr, ctx->snd_pktbuf, ctx->snd_pkt_size) == 0) {
		printf("Failed to send pkt of size %d on interface: %d\n",
			  ctx->snd_pkt_size, nif);
		ioctl(ctx->nmr->fd, NIOCTXSYNC, NULL);
		goto tx_again;
	}
	ctx->snd_pkt_size = 0;

	return 0;
}

unsigned char *nty_nic_get_wbuffer(nty_nic_context *ctx, int nif, uint16_t pktsize) {
#if 0
	if (ctx->snd_pkt_size != 0) {
		nty_nic_send_pkts(ctx, nif);
	}
#endif
	ctx->snd_pkt_size = pktsize;
	return (uint8_t*)ctx->snd_pktbuf;
}

int nty_nic_recv_pkts(nty_nic_context *ctx, int ifidx) {

	assert(ctx != NULL);
	
	int n = ctx->nmr->last_rx_ring - ctx->nmr->first_rx_ring + 1;
	int i = 0, r = ctx->nmr->cur_rx_ring, got = 0, count = 0;

	for (i = 0;i < n && ctx->dev_poll_flag;i ++) {
		struct netmap_ring *ring;

		r = ctx->nmr->cur_rx_ring + i;
		if (r > ctx->nmr->last_rx_ring) r = ctx->nmr->first_rx_ring;

		ring = NETMAP_RXRING(ctx->nmr->nifp, r);
		
		for ( ;!nm_ring_empty(ring) && i != got; got ++) {
			
			int idx = ring->slot[ring->cur].buf_idx;
			ctx->rcv_pktbuf[count] = NETMAP_BUF(ring, idx);
			
			ctx->rcv_pkt_len[count] = ring->slot[ring->cur].len;
			ring->head = ring->cur = nm_ring_next(ring, ring->cur);

			count ++;
		}
	}

	ctx->nmr->cur_rx_ring = r;
	ctx->dev_poll_flag = 0;

	return count;
}

unsigned char* nty_nic_get_rbuffer(nty_nic_context *ctx, int nif, uint16_t *len) {
	*len = ctx->rcv_pkt_len[nif];
	return ctx->rcv_pktbuf[nif];
}


int nty_nic_select(nty_nic_context *ctx) {

	int rc = 0;

	struct pollfd pfd = {0};
	pfd.fd = ctx->nmr->fd;
	pfd.events = POLLIN;

	if (ctx->idle_poll_count >= IDLE_POLL_COUNT) {
		rc = poll(&pfd, 1, IDLE_POLL_WAIT);
	} else {
		rc = poll(&pfd, 1, 0);
	}

	ctx->idle_poll_count = (rc == 0) ? ctx->idle_poll_count + 1 : 0;

	if (!(pfd.revents & POLLERR)) ctx->dev_poll_flag = 1;

	return rc;

}

nty_nic_handler nty_netmap_handler = {
	.init = nty_nic_init,
	.read = nty_nic_read,
	.write = nty_nic_write,
	.get_wbuffer = nty_nic_get_wbuffer,
};




