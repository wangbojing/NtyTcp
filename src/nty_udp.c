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



void nty_udp_pkt(struct udppkt *udp, struct udppkt *udp_rt) {
	memcpy(udp_rt, udp, sizeof(struct udppkt));

	memcpy(udp_rt->eh.h_dest, udp->eh.h_source, ETH_ALEN);
	memcpy(udp_rt->eh.h_source, udp->eh.h_dest, ETH_ALEN);

	memcpy(&udp_rt->ip.saddr, &udp->ip.daddr, sizeof(udp->ip.saddr));
	memcpy(&udp_rt->ip.daddr, &udp->ip.saddr, sizeof(udp->ip.saddr));

	memcpy(&udp_rt->udp.source, &udp->udp.dest, sizeof(udp->udp.source));
	memcpy(&udp_rt->udp.dest, &udp->udp.source, sizeof(udp->udp.dest));
}

int nty_udp_process(nty_nic_context *ctx, unsigned char *stream) {

	struct udppkt *udph = (struct udppkt *)stream;

	//struct in_addr addr;
	//addr.s_addr = udph->ip.saddr;

	int udp_length = ntohs(udph->udp.len);
	udph->body[udp_length-8] = '\0';
	//printf("%s:%d:length:%d, ip_len:%d --> %s \n", inet_ntoa(addr), udph->udp.source, 
	//	udp_length, ntohs(udph->ip.tot_len), udph->body);


	struct udppkt udph_rt;
	nty_udp_pkt(udph, &udph_rt);
	nty_nic_write(ctx, &udph_rt, sizeof(struct udppkt));

	return 0;
}

