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
	 
#include "nty_epoll.h"
#include "nty_header.h"
#include "nty_socket.h"

#include <pthread.h>
#include <errno.h>

extern nty_tcp_manager *nty_get_tcp_manager(void);


nty_socket_map *nty_allocate_socket(int socktype, int need_lock) {
	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (tcp == NULL) {
		assert(0);
		return NULL;
	}

	if (need_lock) {
		pthread_mutex_lock(&tcp->ctx->smap_lock);
	}

	nty_socket_map *socket = NULL;
	while (socket == NULL) {
		socket = TAILQ_FIRST(&tcp->free_smap);
		if (!socket) {
			if (need_lock) {
				pthread_mutex_unlock(&tcp->ctx->smap_lock);
			}
			printf("The concurrent sockets are at maximum.\n");
			return NULL;
		}
		TAILQ_REMOVE(&tcp->free_smap, socket, free_smap_link);

		if (socket->events) {
			printf("There are still not invalidate events remaining.\n");
			TAILQ_INSERT_TAIL(&tcp->free_smap, socket, free_smap_link);
			socket = NULL;
		}
	}

	if (need_lock) {
		pthread_mutex_unlock(&tcp->ctx->smap_lock);
	}
	socket->socktype = socktype;
	socket->opts = 0;
	socket->stream = NULL;
	socket->epoll = 0;
	socket->events = 0;

	memset(&socket->s_addr, 0, sizeof(struct sockaddr_in));
	memset(&socket->ep_data, 0, sizeof(nty_epoll_data));

	return socket;
	
}


void nty_free_socket(int sockid, int need_lock) {

	nty_tcp_manager *tcp = nty_get_tcp_manager();
	nty_socket_map *socket = &tcp->smap[sockid];

	if (socket->socktype == NTY_TCP_SOCK_UNUSED) {
		return ;
	}
	socket->socktype = NTY_TCP_SOCK_UNUSED;
	socket->socktype = NTY_EPOLLNONE;
	socket->events = 0;

	if (need_lock) {
		pthread_mutex_lock(&tcp->ctx->smap_lock);
	}
	tcp->smap[sockid].stream = NULL;
	TAILQ_INSERT_TAIL(&tcp->free_smap, socket, free_smap_link);

	if (need_lock) {
		pthread_mutex_unlock(&tcp->ctx->smap_lock);
	}
}


nty_socket_map *nty_get_socket(int sockid) {
#if 1	
	if (sockid < 0 || sockid >= NTY_MAX_CONCURRENCY) {
		errno = EBADF;
		return NULL;
	}
#endif
	nty_tcp_manager *tcp = nty_get_tcp_manager();
	nty_socket_map *socket = &tcp->smap[sockid];

	return socket;
}                     

