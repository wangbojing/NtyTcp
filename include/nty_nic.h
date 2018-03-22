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




#ifndef __NTY_NIC_H__
#define __NTY_NIC_H__


#include "nty_tcp.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>



#define NETMAP_WITH_LIBS

#include <net/netmap_user.h> 
#pragma pack(1)




#define MAX_PKT_BURST	64
#define MAX_DEVICES		16

#define EXTRA_BUFS		512

#define ETHERNET_FRAME_SIZE		1514
#define ETHERNET_HEADER_LEN		14

#define IDLE_POLL_COUNT			10
#define IDLE_POLL_WAIT			1

typedef struct _nty_nic_context {
	struct nm_desc *nmr;
	unsigned char snd_pktbuf[ETHERNET_FRAME_SIZE];
	unsigned char *rcv_pktbuf[MAX_PKT_BURST];
	uint16_t rcv_pkt_len[MAX_PKT_BURST];
	uint16_t snd_pkt_size;
	uint8_t dev_poll_flag;
	uint8_t idle_poll_count;
} nty_nic_context;


typedef struct _nty_nic_handler {
	int (*init)(nty_thread_context *ctx, const char *ifname);
	int (*read)(nty_nic_context *ctx, unsigned char **stream);
	int (*write)(nty_nic_context *ctx, const void *stream, int length);
	unsigned char* (*get_wbuffer)(nty_nic_context *ctx, int nif, uint16_t pktsize);
} nty_nic_handler;

unsigned char* nty_nic_get_wbuffer(nty_nic_context *ctx, int nif, uint16_t pktsize);
unsigned char* nty_nic_get_rbuffer(nty_nic_context *ctx, int nif, uint16_t *len);

int nty_nic_send_pkts(nty_nic_context *ctx, int nif);
int nty_nic_recv_pkts(nty_nic_context *ctx, int ifidx);

int nty_nic_read(nty_nic_context *ctx, unsigned char **stream);
int nty_nic_write(nty_nic_context *ctx, const void *stream, int length);



int nty_nic_init(nty_thread_context *tctx, const char *ifname);
int nty_nic_select(nty_nic_context *ctx);


#if 0
extern nty_nic_handler nty_netmap_handler;
static nty_nic_handler *nty_current_handler = &nty_netmap_handler;


#define NTY_NIC_INIT(x, y)				nty_current_handler->init(x, y)
#define NTY_NIC_WRITE(x, y, z)			nty_current_handler->write(x, y, z)
#define NTY_NIC_READ(x, y)				nty_current_handler->read(x, y)
#define NTY_NIC_GET_WBUFFER(x, y, z) 	nty_current_handler->get_wbuffer(x, y, z)
#endif


#endif



