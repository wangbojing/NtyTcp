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



