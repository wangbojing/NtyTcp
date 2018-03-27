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


#ifndef __NTY_SOCKET_H__
#define __NTY_SOCKET_H__


#include "nty_buffer.h"
#include "nty_tcp.h"
#include "nty_config.h"

#include <pthread.h>


typedef struct _nty_socket_map {
	int id;
	int socktype;
	uint32_t opts;

	struct sockaddr_in s_addr;

	union {
		struct _nty_tcp_stream *stream;
		struct _nty_tcp_listener *listener;
#if NTY_ENABLE_EPOLL_RB
		void *ep;
#else
		struct _nty_epoll *ep;
#endif
		//struct pipe *pp;
	};

	uint32_t epoll;
	uint32_t events;
	uint64_t ep_data;

	TAILQ_ENTRY(_nty_socket_map) free_smap_link;
} nty_socket_map; //__attribute__((packed)) 


enum nty_socket_opts{
	NTY_TCP_NONBLOCK = 0x01,
	NTY_TCP_ADDR_BIND = 0x02,
};

nty_socket_map *nty_allocate_socket(int socktype, int need_lock);
void nty_free_socket(int sockid, int need_lock);
nty_socket_map *nty_get_socket(int sockid);


/*
 * rebuild socket module for support 10M
 */
#if NTY_ENABLE_SOCKET_C10M


struct _nty_socket {
	int id;	
	int socktype;

	uint32_t opts;
	struct sockaddr_in s_addr;

	union {
		struct _nty_tcp_stream *stream;
		struct _nty_tcp_listener *listener;
		void *ep;
	};
	struct _nty_socket_table *socktable;
};


struct _nty_socket_table {
	size_t max_fds;
	int cur_idx;
	struct _nty_socket **sockfds;
	unsigned char *open_fds;
	pthread_spinlock_t lock;
};

struct _nty_socket* nty_socket_allocate(int socktype);

void nty_socket_free(int sockid);

struct _nty_socket* nty_socket_get(int sockid);

struct _nty_socket_table * nty_socket_init_fdtable(void);



#endif

#endif


