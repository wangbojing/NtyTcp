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

