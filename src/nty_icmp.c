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


extern unsigned short in_cksum(unsigned short *addr, int len);

void nty_icmp_pkt(struct icmppkt *icmp, struct icmppkt *icmp_rt) {

	memcpy(icmp_rt, icmp, sizeof(struct icmppkt));

	icmp_rt->icmp.type = 0x0; //
	icmp_rt->icmp.code = 0x0; //
	icmp_rt->icmp.check = 0x0;

	icmp_rt->ip.saddr = icmp->ip.daddr;
	icmp_rt->ip.daddr = icmp->ip.saddr;

	memcpy(icmp_rt->eh.h_dest, icmp->eh.h_source, ETH_ALEN);
	memcpy(icmp_rt->eh.h_source, icmp->eh.h_dest, ETH_ALEN);

	icmp_rt->icmp.check = in_cksum((unsigned short*)&icmp_rt->icmp, sizeof(struct icmphdr));
	
}

int nty_icmp_process(nty_nic_context *ctx, unsigned char *stream) {

	struct icmppkt *icmph = (struct icmppkt*)stream;

	if (icmph->icmp.type == 0x08) {
		struct icmppkt icmp_rt;
		memset(&icmp_rt, 0, sizeof(struct icmppkt));
		
		nty_icmp_pkt(icmph, &icmp_rt);
		nty_nic_write(ctx, &icmp_rt, sizeof(struct icmppkt));
	}
	return 0;
}



