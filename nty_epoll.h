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




#ifndef __NTY_EPOLL_H__
#define __NTY_EPOLL_H__

//#include "nty_tcp.h"
#include "nty_buffer.h"
#include "nty_header.h"

typedef enum {
	NTY_EPOLL_CTL_ADD = 1,
	NTY_EPOLL_CTL_DEL = 2,
	NTY_EPOLL_CTL_MOD = 3,
} nty_epoll_op; 

typedef enum {

	NTY_EPOLLNONE 	= 0x0000,
	NTY_EPOLLIN 	= 0x0001,
	NTY_EPOLLPRI	= 0x0002,
	NTY_EPOLLOUT	= 0x0004,
	NTY_EPOLLRDNORM = 0x0040,
	NTY_EPOLLRDBAND = 0x0080,
	NTY_EPOLLWRNORM = 0x0100,
	NTY_EPOLLWRBAND = 0x0200,
	NTY_EPOLLMSG	= 0x0400,
	NTY_EPOLLERR	= 0x0008,
	NTY_EPOLLHUP 	= 0x0010,
	NTY_EPOLLRDHUP 	= 0x2000,
	NTY_EPOLLONESHOT = (1 << 30),
	NTY_EPOLLET 	= (1 << 31)

} nty_epoll_type;

typedef union _nty_epoll_data {
	void *ptr;
	int sockid;
	uint32_t u32;
	uint64_t u64;
} nty_epoll_data;

typedef struct {
	uint32_t events;
	nty_epoll_data data;
} nty_epoll_event;

typedef struct _nty_epoll_stat {
	uint64_t calls;
	uint64_t waits;
	uint64_t wakes;

	uint64_t issued;
	uint64_t registered;
	uint64_t invalidated;
	uint64_t handled;
} nty_epoll_stat;

typedef struct _nty_epoll_event_int {
	nty_epoll_event ev;
	int sockid;
} nty_epoll_event_int;

typedef enum {
	USR_EVENT_QUEUE = 0,
	USR_SHADOW_EVENT_QUEUE = 1,
	NTY_EVENT_QUEUE = 2
} nty_event_queue_type;


typedef struct _nty_event_queue {
	nty_epoll_event_int *events;
	int start;
	int end;
	int size;
	int num_events;
} nty_event_queue;

typedef struct _nty_epoll {
	nty_event_queue *usr_queue;
	nty_event_queue *usr_shadow_queue;
	nty_event_queue *queue;

	uint8_t waiting;
	nty_epoll_stat stat;

	pthread_cond_t epoll_cond;
	pthread_mutex_t epoll_lock;
} nty_epoll;

#endif



