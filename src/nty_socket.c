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

	 
#include "nty_epoll_inner.h"
#include "nty_header.h"
#include "nty_socket.h"

#include <hugetlbfs.h>
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


/*
 * socket fd need to support 10M, so rebuild socket module.
 * 
 */

#if NTY_ENABLE_SOCKET_C10M

struct _nty_socket_table * nty_socket_allocate_fdtable(void) {
	
	//nty_tcp_manager *tcp = nty_get_tcp_manager();
	//if (tcp == NULL) return NULL;

	struct _nty_socket_table *sock_table = (struct _nty_socket_table*)calloc(1, sizeof(struct _nty_socket_table));
	if (sock_table == NULL) {
		errno = -ENOMEM;
		return NULL;
	}

	size_t total_size = NTY_SOCKFD_NR * sizeof(struct _nty_socket *);
#if 0 //(NTY_SOCKFD_NR > 1024)
	sock_table->sockfds = (struct _nty_socket **)get_huge_pages(total_size, GHP_DEFAULT);
	if (sock_table->sockfds == NULL) {
		errno = -ENOMEM;
		free(sock_table);
		return NULL;
	}
#else
	int res = posix_memalign((void **)&sock_table->sockfds, getpagesize(), total_size);
	if (res != 0) {
		errno = -ENOMEM;
		free(sock_table);
		return NULL;
	}
#endif

	sock_table->max_fds = (NTY_SOCKFD_NR % NTY_BITS_PER_BYTE ? NTY_SOCKFD_NR / NTY_BITS_PER_BYTE + 1 : NTY_SOCKFD_NR / NTY_BITS_PER_BYTE);

	sock_table->open_fds = (unsigned char*)calloc(sock_table->max_fds, sizeof(unsigned char));
	if (sock_table->open_fds == NULL) {
		errno = -ENOMEM;
#if 0 //(NTY_SOCKFD_NR > 1024)
		free_huge_pages(sock_table->sockfds);
#else
		free(sock_table->sockfds);
#endif
		free(sock_table);
		return NULL;
	}

	if (pthread_spin_init(&sock_table->lock, PTHREAD_PROCESS_SHARED)) {
		errno = -EINVAL;
		free(sock_table->open_fds);
#if 0 //(NTY_SOCKFD_NR > 1024)
		free_huge_pages(sock_table->sockfds);
#else
		free(sock_table->sockfds);
#endif
		free(sock_table);

		return NULL;
	}

	//tcp->fdtable = sock_table;
	
	return sock_table;
}


void nty_socket_free_fdtable(struct _nty_socket_table *fdtable) {

	pthread_spin_destroy(&fdtable->lock);
	free(fdtable->open_fds);
	
#if (NTY_SOCKFD_NR > 1024)
	free_huge_pages(fdtable->sockfds);
#else
	free(fdtable->sockfds);
#endif

	free(fdtable);
}


/*
 * singleton should use CAS
 */
struct _nty_socket_table *nty_socket_get_fdtable(void) {
#if 0
	if (fdtable == NULL) {
		fdtable = nty_socket_allocate_table();
	}
	return fdtable;
#else
	nty_tcp_manager *tcp = nty_get_tcp_manager();

	return tcp->fdtable;
#endif
}

struct _nty_socket_table * nty_socket_init_fdtable(void) {
	return nty_socket_allocate_fdtable();
}


int nty_socket_find_id(unsigned char *fds, int start, size_t max_fds) {

	size_t i = 0;
	for (i = start;i < max_fds;i ++) {
		if (fds[i] != 0xFF) {
			break;
		}
	}
	if (i == max_fds) return -1;

	int j = 0;
	char byte = fds[i];
	while (byte % 2) {
		byte /= 2;
		j ++;
	}

	return i * NTY_BITS_PER_BYTE + j;
}

char nty_socket_unuse_id(unsigned char *fds, size_t idx) {

	int i = idx / NTY_BITS_PER_BYTE;
	int j = idx % NTY_BITS_PER_BYTE;

	char byte = 0x01 << j;
	fds[i] &= ~byte;

	return fds[i];
}

int nty_socket_set_start(size_t idx) {
	return idx / NTY_BITS_PER_BYTE;
}

char nty_socket_use_id(unsigned char *fds, size_t idx) {

	int i = idx / NTY_BITS_PER_BYTE;
	int j = idx % NTY_BITS_PER_BYTE;

	char byte = 0x01 << j;

	fds[i] |= byte;

	return fds[i];
}


struct _nty_socket* nty_socket_allocate(int socktype) {

	struct _nty_socket *s = (struct _nty_socket*)calloc(1, sizeof(struct _nty_socket));
	if (s == NULL) {
		errno = -ENOMEM;
		return NULL;
	}

	struct _nty_socket_table *sock_table = nty_socket_get_fdtable();
	

	pthread_spin_lock(&sock_table->lock);

	s->id = nty_socket_find_id(sock_table->open_fds, sock_table->cur_idx, sock_table->max_fds);
	if (s->id == -1) {
		pthread_spin_unlock(&sock_table->lock);
		errno = -ENFILE;
		return NULL;
	}
	
	sock_table->cur_idx = nty_socket_set_start(s->id);
	char byte = nty_socket_use_id(sock_table->open_fds, s->id);
	
	sock_table->sockfds[s->id] = s;

	nty_trace_socket("nty_socket_allocate --> nty_socket_use_id : %x\n", byte);
	
	pthread_spin_unlock(&sock_table->lock);

	s->socktype = socktype;
	s->opts = 0;
	s->socktable = sock_table;
	s->stream = NULL;

	memset(&s->s_addr, 0, sizeof(struct sockaddr_in));

	UNUSED(byte);

	return s;
}

void nty_socket_free(int sockid) {

	struct _nty_socket_table *sock_table = nty_socket_get_fdtable();

	struct _nty_socket *s = sock_table->sockfds[sockid];
	sock_table->sockfds[sockid] = NULL;

	pthread_spin_lock(&sock_table->lock);

	char byte = nty_socket_unuse_id(sock_table->open_fds, sockid);

	sock_table->cur_idx = nty_socket_set_start(sockid);
	nty_trace_socket("nty_socket_free --> nty_socket_unuse_id : %x, %d\n",
		byte, sock_table->cur_idx);
	
	pthread_spin_unlock(&sock_table->lock);

	free(s);

	UNUSED(byte);
	nty_trace_socket("nty_socket_free --> Exit\n");

	return ;
}

struct _nty_socket* nty_socket_get(int sockid) {

	struct _nty_socket_table *sock_table = nty_socket_get_fdtable();
	if(sock_table == NULL) return NULL;

	return sock_table->sockfds[sockid];
}

int nty_socket_close_stream(int sockid) {
	
	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	struct _nty_socket *s = nty_socket_get(sockid);
	if (s == NULL) return -1;

	nty_tcp_stream *cur_stream = s->stream;
	if (!cur_stream) {
		nty_trace_api("Socket %d: stream does not exist.\n", sockid);
		errno = ENOTCONN;
		return -1;
	}

	if (cur_stream->closed) {
		nty_trace_api("Socket %d (Stream %u): already closed stream\n", 
				sockid, cur_stream->id);
		return 0;
	}
	cur_stream->closed = 1;
	
	nty_trace_api("Stream %d: closing the stream.\n", cur_stream->id);
	cur_stream->s = NULL;

	if (cur_stream->state == NTY_TCP_CLOSED) {
		printf("Stream %d at TCP_ST_CLOSED. destroying the stream.\n", 
				cur_stream->id);
		
		StreamEnqueue(tcp->destroyq, cur_stream);
		tcp->wakeup_flag = 1;
		
		return 0;
	} else if (cur_stream->state == NTY_TCP_SYN_SENT) {
		
		StreamEnqueue(tcp->destroyq, cur_stream);		
		tcp->wakeup_flag = 1;
		
		return -1;
	} else if (cur_stream->state != NTY_TCP_ESTABLISHED &&
			   cur_stream->state != NTY_TCP_CLOSE_WAIT) {
		nty_trace_api("Stream %d at state %d\n", 
				cur_stream->id, cur_stream->state);
		errno = -EBADF;
		return -1;
	}

	cur_stream->snd->on_closeq = 1;
	int ret = StreamEnqueue(tcp->closeq, cur_stream);
	tcp->wakeup_flag = 1;

	if (ret < 0) {
		nty_trace_api("(NEVER HAPPEN) Failed to enqueue the stream to close.\n");
		errno = EAGAIN;
		return -1;
	}

	return 0;
}

int nty_socket_close_listening(int sockid) {

	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	struct _nty_socket *s = nty_socket_get(sockid);
	if (s == NULL) return -1;
	
	struct _nty_tcp_listener *listener = s->listener;
	if (!listener) {
		errno = EINVAL;
		return -1;
	}

	if (listener->acceptq) {
		DestroyStreamQueue(listener->acceptq);
		listener->acceptq = NULL;
	}

	pthread_mutex_lock(&listener->accept_lock);
	pthread_cond_signal(&listener->accept_cond);
	pthread_mutex_unlock(&listener->accept_lock);

	pthread_cond_destroy(&listener->accept_cond);
	pthread_mutex_destroy(&listener->accept_lock);

	free(listener);
	s->listener = NULL;

	return 0;
}



#endif

