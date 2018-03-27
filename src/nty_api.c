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


#include "nty_buffer.h"
#include "nty_header.h"
#include "nty_tcp.h"
#include "nty_api.h"
#include "nty_epoll.h"
#include "nty_socket.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

extern nty_tcp_manager *nty_get_tcp_manager(void);

#if 0
static int nty_peek_for_user(nty_tcp_stream *cur_stream, char *buf, int len) {
	nty_tcp_recv *rcv = cur_stream->rcv;

	int copylen = MIN(rcv->recvbuf->merged_len, len);
	if (copylen <= 0) {
		errno = EAGAIN;
		return -1;
	}
	memcpy(buf, rcv->recvbuf->head, copylen);

	return copylen;
}
#endif

static int nty_copy_to_user(nty_tcp_stream *cur_stream, char *buf, int len) {
	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (tcp == NULL) return -1;
	
	nty_tcp_recv *rcv = cur_stream->rcv;

	int copylen = MIN(rcv->recvbuf->merged_len, len);
	if (copylen < 0) {
		errno = EAGAIN;
		return -1;
	} else if (copylen == 0){
		errno = 0;
		return 0;
	}
	//rcv --> data increase
	//uint32_t prev_rcv_wnd = rcv->rcv_wnd;

	memcpy(buf, rcv->recvbuf->head, copylen);

	RBRemove(tcp->rbm_rcv, rcv->recvbuf, copylen, AT_APP);
	rcv->rcv_wnd = rcv->recvbuf->size - rcv->recvbuf->merged_len;

	//printf("size:%d, merged_len:%d offset %d, %s\n",rcv->recvbuf->size, rcv->recvbuf->merged_len,
	//	rcv->recvbuf->head_offset, rcv->recvbuf->data);

	if (cur_stream->need_wnd_adv) {
		if (rcv->rcv_wnd > cur_stream->snd->eff_mss) {
			if (!cur_stream->snd->on_ackq) {
				cur_stream->snd->on_ackq = 1;
				StreamEnqueue(tcp->ackq, cur_stream);

				cur_stream->need_wnd_adv = 0;
				tcp->wakeup_flag = 0;
			}
		}
	}

	return copylen;
}

static int nty_copy_from_user(nty_tcp_stream *cur_stream, const char *buf, int len) {
	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (tcp == NULL) return -1;
	
	nty_tcp_send *snd = cur_stream->snd;
	
	int sndlen = MIN((int)snd->snd_wnd, len);
	if (sndlen <= 0) {
		errno = EAGAIN;
		return -1;
	}

	if (!snd->sndbuf) {
		snd->sndbuf = SBInit(tcp->rbm_snd, snd->iss + 1);
		if (!snd->sndbuf) {
			cur_stream->close_reason = TCP_NO_MEM;
			errno = ENOMEM;
			return -1;
		}
	}

	int ret = SBPut(tcp->rbm_snd, snd->sndbuf, buf, sndlen);
	assert(ret == sndlen);
	if (ret <= 0) {
		nty_trace_api("SBPut failed. reason: %d (sndlen: %u, len: %u\n", 
				ret, sndlen, snd->sndbuf->len);
		errno = EAGAIN;
		return -1;
	}

	snd->snd_wnd = snd->sndbuf->size - snd->sndbuf->len;
	if (snd->snd_wnd <= 0) {
		nty_trace_api("%u Sending buffer became full!! snd_wnd: %u\n", 
				cur_stream->id, snd->snd_wnd);
	}

	return ret;
}

static int nty_close_stream_socket(int sockid) {
	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	nty_tcp_stream *cur_stream = tcp->smap[sockid].stream;
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
	cur_stream->socket = NULL;

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
		errno = EBADF;
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

static int nty_close_listening_socket(int sockid) {

	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	struct _nty_tcp_listener *listener = tcp->smap[sockid].listener;
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
	tcp->smap[sockid].listener = NULL;

	return 0;
}


int nty_socket(int domain, int type, int protocol) {

	
	if (domain != AF_INET) {
		errno = EAFNOSUPPORT;
		return -1;
	}
	if (type == SOCK_STREAM) {
		type = NTY_TCP_SOCK_STREAM;
	} else {
		errno = EINVAL;
		return -1;
	}

	nty_socket_map *socket = nty_allocate_socket(type, 0);
	if (!socket) {
		errno = ENFILE;
		return -1;
	}

	return socket->id;
}

int nty_bind(int sockid, const struct sockaddr *addr, socklen_t addrlen) {

	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	if (sockid < 0 || sockid >= NTY_MAX_CONCURRENCY) {
		errno = EBADF;
		return -1;
	}

	if (tcp->smap[sockid].socktype == NTY_TCP_SOCK_UNUSED) {
		nty_trace_api("Invalid socket id: %d\n", sockid);
		errno = EBADF;
		return -1;
	}

	if (tcp->smap[sockid].socktype != NTY_TCP_SOCK_STREAM &&
		tcp->smap[sockid].socktype != NTY_TCP_SOCK_LISTENER) {
		nty_trace_api("Not a stream socket id: %d\n", sockid);
		errno = ENOTSOCK;
		return -1;
	}

	if (!addr) {
		nty_trace_api("Socket %d: empty address!\n", sockid);
		errno = EINVAL;
		return -1;
	}

	if (tcp->smap[sockid].opts & NTY_TCP_ADDR_BIND) {
		nty_trace_api("Socket %d: adress already bind for this socket.\n", sockid);
		errno = EINVAL;
		return -1;
	}

	if (addr->sa_family != AF_INET || addrlen < sizeof(struct sockaddr_in)) {
		nty_trace_api("Socket %d: invalid argument!\n", sockid);
		errno = EINVAL;
		return -1;
	}

	struct sockaddr_in *addr_in = (struct sockaddr_in*)addr;
	tcp->smap[sockid].s_addr = *addr_in;
	tcp->smap[sockid].opts |= NTY_TCP_ADDR_BIND;

	return 0;
}

int nty_listen(int sockid, int backlog) {
	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	if (sockid < 0 || sockid >= NTY_MAX_CONCURRENCY) {
		errno = EBADF;
		return -1;
	}
	if (tcp->smap[sockid].socktype == NTY_TCP_SOCK_UNUSED) {
		nty_trace_api("Socket %d: invalid argument!\n", sockid);
		errno = EBADF;
		return -1;
	}
	if (tcp->smap[sockid].socktype == NTY_TCP_SOCK_STREAM) {
		tcp->smap[sockid].socktype = NTY_TCP_SOCK_LISTENER;
	}
	if (tcp->smap[sockid].socktype != NTY_TCP_SOCK_LISTENER) {
		nty_trace_api("Not a listening socket. id: %d\n", sockid);
		errno = ENOTSOCK;
		return -1;
	}

	if (ListenerHTSearch(tcp->listeners, &tcp->smap[sockid].s_addr.sin_port)) {
		errno = EADDRINUSE;
		return -1;
	}
	
	nty_tcp_listener *listener = (nty_tcp_listener*)calloc(1, sizeof(nty_tcp_listener));
	if (!listener) {
		return -1;
	}

	listener->sockid = sockid;
	listener->backlog = backlog;
	listener->socket = &tcp->smap[sockid];

	if (pthread_cond_init(&listener->accept_cond, NULL)) {
		nty_trace_api("pthread_cond_init of ctx->accept_cond\n");
		free(listener);
		return -1;
	}

	if (pthread_mutex_init(&listener->accept_lock, NULL)) {
		nty_trace_api("pthread_mutex_init of ctx->accept_lock\n");
		free(listener);
		return -1;
	}

	listener->acceptq = CreateStreamQueue(backlog);
	if (!listener->acceptq) {
		free(listener);
		errno = ENOMEM;
		return -1;
	}

	tcp->smap[sockid].listener = listener;
	ListenerHTInsert(tcp->listeners, listener);

	return 0;
}

int nty_accept(int sockid, struct sockaddr *addr, socklen_t *addrlen) {

	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	if (sockid < 0 || sockid >= NTY_MAX_CONCURRENCY) {
		errno = EBADF;
		return -1;
	}

	if (tcp->smap[sockid].socktype != NTY_TCP_SOCK_LISTENER) {
		errno = EINVAL;
		return -1;
	}

	nty_tcp_listener *listener = tcp->smap[sockid].listener;
	nty_tcp_stream *accepted = StreamDequeue(listener->acceptq);
	if (!accepted) {
		if (listener->socket->opts & NTY_TCP_NONBLOCK) {
			errno = EAGAIN;
			return -1;
		} else {
			pthread_mutex_lock(&listener->accept_lock);
			while (accepted == NULL && ((accepted = StreamDequeue(listener->acceptq)) == NULL)) {
				pthread_cond_wait(&listener->accept_cond, &listener->accept_lock);

				//printf("");
				if (tcp->ctx->done || tcp->ctx->exit) {
					pthread_mutex_unlock(&listener->accept_lock);
					errno = EINTR;
					return -1;
				}
			}
			pthread_mutex_unlock(&listener->accept_lock);
		}
	}

	nty_socket_map *socket = NULL;
	if (!accepted->socket) {
		socket = nty_allocate_socket(NTY_TCP_SOCK_STREAM, 0);
		if (!socket) {
			nty_trace_api("Failed to create new socket!\n");
			/* TODO: destroy the stream */
			errno = ENFILE;
			return -1;
		}

		socket->stream = accepted;
		accepted->socket = socket;

		socket->s_addr.sin_family = AF_INET;
		socket->s_addr.sin_port = accepted->dport;
		socket->s_addr.sin_addr.s_addr = accepted->daddr;
	}
	if (!(listener->socket->epoll & NTY_EPOLLET) &&
		!StreamQueueIsEmpty(listener->acceptq)) {
		nty_epoll_add_event(tcp->ep, USR_SHADOW_EVENT_QUEUE, listener->socket, NTY_EPOLLIN);		
	}
	nty_trace_api("Stream %d accepted.\n", accepted->id);

	if (addr && addrlen) {
		struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
		addr_in->sin_family = AF_INET;
		addr_in->sin_port = accepted->dport;
		addr_in->sin_addr.s_addr = accepted->daddr;
		*addrlen = sizeof(struct sockaddr_in);
	}

	return accepted->socket->id;
}


ssize_t nty_recv(int sockid, char *buf, size_t len, int flags) {
	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	if (sockid < 0 || sockid >= NTY_MAX_CONCURRENCY) {
		errno = EBADF;
		return -1;
	}

	nty_socket_map *socket = &tcp->smap[sockid];
	if (socket->socktype == NTY_TCP_SOCK_UNUSED) {
		errno = EINVAL;
		return -1;
	}
	
	if (socket->socktype != NTY_TCP_SOCK_STREAM) {
		errno = ENOTSOCK;
		return -1;
	}
	
	/* stream should be in ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2, CLOSE_WAIT */
	nty_tcp_stream *cur_stream = socket->stream;
	if (!cur_stream ||
		!(cur_stream->state == NTY_TCP_ESTABLISHED ||
		  cur_stream->state == NTY_TCP_CLOSE_WAIT ||
		  cur_stream->state == NTY_TCP_FIN_WAIT_1 ||
		  cur_stream->state == NTY_TCP_FIN_WAIT_2)) {
		errno = ENOTCONN;
		return -1;
	}
	
	nty_tcp_recv *rcv = cur_stream->rcv;
	if (cur_stream->state == NTY_TCP_CLOSE_WAIT) {
		if (!rcv->recvbuf) return 0;
		if (rcv->recvbuf->merged_len == 0) return 0;
	}

	if (socket->opts & NTY_TCP_NONBLOCK) {
		if (!rcv->recvbuf || rcv->recvbuf->merged_len == 0) {
			errno = EAGAIN;
			return -1;
		}
	}
	
	//SBUF_LOCK(&rcv->read_lock);
	pthread_mutex_lock(&rcv->read_lock);
#if NTY_ENABLE_BLOCKING

	if (!(socket->opts & NTY_TCP_NONBLOCK)) {
		
		while (!rcv->recvbuf || rcv->recvbuf->merged_len == 0) {
			if (!cur_stream || cur_stream->state != NTY_TCP_ESTABLISHED) {
				pthread_mutex_unlock(&rcv->read_lock);
				
				if (rcv->recvbuf->merged_len == 0) { //disconnect
					errno = 0;
					return 0;
				} else {
					errno = EINTR;
					return -1;
				}
			}
			
			pthread_cond_wait(&rcv->read_cond, &rcv->read_lock);
		}
	}
#endif

	int ret = 0;
	switch (flags) {
		case 0: {
			ret = nty_copy_to_user(cur_stream, buf, len);
			break;
		}
		default: {
			pthread_mutex_unlock(&rcv->read_lock);
			ret = -1;
			errno = EINVAL;
			return ret;
		}		
	}

	int event_remaining = 0;
	if (socket->epoll & NTY_EPOLLIN) {
		if (!(socket->epoll & NTY_EPOLLET) && rcv->recvbuf->merged_len > 0) {
			event_remaining = 1;
		}
	}

	if (cur_stream->state == NTY_TCP_CLOSE_WAIT && 
	    rcv->recvbuf->merged_len == 0 && ret > 0) {
		event_remaining = 1;
	}
	
	pthread_mutex_unlock(&rcv->read_lock);


	if (event_remaining) {
		if (socket->epoll) {
			nty_epoll_add_event(tcp->ep, USR_SHADOW_EVENT_QUEUE, socket, NTY_EPOLLIN);
#if NTY_ENABLE_BLOCKING
		} else if (!(socket->opts & NTY_TCP_NONBLOCK)) {
			if (!cur_stream->on_rcv_br_list) {
				cur_stream->on_rcv_br_list = 1;
				TAILQ_INSERT_TAIL(&tcp->rcv_br_list, cur_stream, rcv->rcv_br_link);
				tcp->rcv_br_list_cnt ++;
			}
		}
#endif
	}

	nty_trace_api("Stream %d: mtcp_recv() returning %d\n", cur_stream->id, ret);
    return ret;
}

ssize_t nty_send(int sockid, const char *buf, size_t len) {
	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	if (sockid < 0 || sockid >= NTY_MAX_CONCURRENCY) {
		errno = EBADF;
		return -1;
	}
	
	nty_socket_map *socket = &tcp->smap[sockid];
	if (socket->socktype == NTY_TCP_SOCK_UNUSED) {
		errno = EINVAL;
		return -1;
	}
	if (socket->socktype != NTY_TCP_SOCK_STREAM) {
		errno = ENOTSOCK;
		return -1;
	}
	/* stream should be in ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2, CLOSE_WAIT */
	nty_tcp_stream *cur_stream = socket->stream;
	if (!cur_stream ||
		!(cur_stream->state == NTY_TCP_ESTABLISHED ||
		  cur_stream->state == NTY_TCP_CLOSE_WAIT)) {
		errno = ENOTCONN;
		return -1;
	}

	if (len <= 0) {
		if (socket->opts & NTY_TCP_NONBLOCK) {
			errno = EAGAIN;
			return -1;
		} else {
			return 0;
		}
	}

	nty_tcp_send *snd = cur_stream->snd;

	pthread_mutex_lock(&snd->write_lock);

#if NTY_ENABLE_BLOCKING
	if (!(socket->opts & NTY_TCP_NONBLOCK)) {
		while (snd->snd_wnd <= 0) {
			if (!cur_stream || cur_stream->state != NTY_TCP_ESTABLISHED) {
				pthread_mutex_unlock(&snd->write_lock);
				errno = EINTR;
				return -1;
			}
			
			pthread_cond_wait(&snd->write_cond, &snd->write_lock);
		}
	}
#endif
	int ret = nty_copy_from_user(cur_stream, buf, len);
	pthread_mutex_unlock(&snd->write_lock);

	nty_trace_api("nty_copy_from_user --> %d, %d\n", 
		snd->on_sendq, snd->on_send_list);
	if (ret > 0 && !(snd->on_sendq || snd->on_send_list)) {
		snd->on_sendq = 1;
		StreamEnqueue(tcp->sendq, cur_stream);
		tcp->wakeup_flag = 1;
	}

	if (ret == 0 && (socket->opts & NTY_TCP_NONBLOCK)) {
		ret = -1;
		errno = EAGAIN;
	}

	if (snd->snd_wnd > 0) {
		if ((socket->epoll & NTY_EPOLLOUT) && !(socket->epoll & NTY_EPOLLET)) {
			nty_epoll_add_event(tcp->ep, USR_SHADOW_EVENT_QUEUE, socket, NTY_EPOLLOUT);
#if NTY_ENABLE_BLOCKING
		} else if (!(socket->opts & NTY_TCP_NONBLOCK)) {
			if (!cur_stream->on_snd_br_list) {
				cur_stream->on_snd_br_list = 1;
				TAILQ_INSERT_TAIL(&tcp->snd_br_list, cur_stream, snd->snd_br_link);
				tcp->snd_br_list_cnt ++;
			}
#endif
		}
	}
	
	nty_trace_api("Stream %d: mtcp_write() returning %d\n", cur_stream->id, ret);
	return ret;
}

int nty_close(int sockid) {
	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	if (sockid < 0 || sockid >= NTY_MAX_CONCURRENCY) {
		errno = EBADF;
		return -1;
	}

	nty_socket_map *socket = &tcp->smap[sockid];
	if (socket->socktype == NTY_TCP_SOCK_UNUSED) {
		errno = EINVAL;
		return -1;
	}
	nty_trace_api("Socket %d: mtcp_close called.\n", sockid);

	int ret = -1;
	switch (tcp->smap[sockid].socktype) {
		case NTY_TCP_SOCK_STREAM: {
			ret = nty_close_stream_socket(sockid);
			break;
		}
		case NTY_TCP_SOCK_LISTENER: {
			ret = nty_close_listening_socket(sockid);
			break;
		}
		case NTY_TCP_SOCK_EPOLL: {
			ret = nty_close_epoll_socket(sockid);
		}
		default: {
			errno = EINVAL;
			ret = -1;
			break;
		}
	}

	nty_free_socket(sockid, 0);
	return ret;
}

#if 0
int nty_connect(int sockid, const struct sockaddr *addr, socklen_t addrlen) {
	nty_tcp_manager *tcp = nty_get_tcp_manager();

	if (!tcp) {
		return -1;
	}

	if (sockid < 0 || sockid >= NTY_MAX_CONCURRENCY) {
		errno = EBADF;
		return -1;
	}

	if (tcp->smap[sockid].socktype != NTY_TCP_SOCK_UNUSED) {
		errno = EINVAL;
		return -1;
	}

	if (tcp->smap[sockid].socktype != NTY_TCP_SOCK_STREAM) {
		errno = ENOTSOCK;
		return -1;
	}

	if (!addr) {
		printf("Socket %d: empty address!\n", sockid);
		errno = EFAULT;
		return -1;
	}

	if (addr->sa_family != AF_INET || addrlen < sizeof(struct sockaddr_in)) {
		printf("Socket %d: invalid argument!\n", sockid);
		errno = EAFNOSUPPORT;
		return -1;
	}

	nty_socket_map *socket = &tcp->smap[sockid];
	if (socket->stream) {
		printf("Socket %d: stream already exist!\n", sockid);
		if (socket->stream->state >= TCP_ST_ESTABLISHED) {
			errno = EISCONN;
		} else {
			errno = EALREADY;
		}
		return -1;
	}

	struct sockaddr_in *addr_in = (struct sockaddr_in*)addr;
	in_addr_t dip = addr_in->sin_addr.s_addr;
	in_port_t dport = addr_in->sin_port;

	if ((socket->opts & NTY_TCP_ADDR_BIND) && 
		socket->s_addr.sin_port != INPORT_ANY &&
		socket->s_addr.sin_addr.s_addr != INADDR_ANY) {

	}
}
#endif


#if NTY_ENABLE_POSIX_API

int socket(int domain, int type, int protocol) {
	
	if (domain != AF_INET) {
		errno = -EAFNOSUPPORT;
		return -1;
	}
	if (type == SOCK_STREAM) {
		type = NTY_TCP_SOCK_STREAM;
	} else {
		errno = -EINVAL;
		return -1;
	}

	struct _nty_socket *socket = nty_socket_allocate(type);
	if (!socket) {
		errno = -ENFILE;
		return -1;
	}

	return socket->id;
}

int bind(int sockid, const struct sockaddr *addr, socklen_t addrlen) {
	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	if (sockid < 0) {
		errno = -EBADF;
		return -1;
	}

	if (tcp->fdtable == NULL) {
		errno = -EBADF;
		return -1;
	}
	struct _nty_socket *s = tcp->fdtable->sockfds[sockid];
	if (s == NULL) {
		errno = -EBADF;
		return -1;
	}

	if (s->socktype == NTY_TCP_SOCK_UNUSED) {
		nty_trace_api("Invalid socket id: %d\n", sockid);
		errno = EBADF;
		return -1;
	}

	if (s->socktype != NTY_TCP_SOCK_STREAM &&
		s->socktype != NTY_TCP_SOCK_LISTENER) {
		nty_trace_api("Not a stream socket id: %d\n", sockid);
		errno = -ENOTSOCK;
		return -1;
	}

	if (!addr) {
		nty_trace_api("Socket %d: empty address!\n", sockid);
		errno = -EINVAL;
		return -1;
	}

	if (s->opts & NTY_TCP_ADDR_BIND) {
		nty_trace_api("Socket %d: adress already bind for this socket.\n", sockid);
		errno = -EINVAL;
		return -1;
	}

	if (addr->sa_family != AF_INET || addrlen < sizeof(struct sockaddr_in)) {
		nty_trace_api("Socket %d: invalid argument!\n", sockid);
		errno = -EINVAL;
		return -1;
	}

	struct sockaddr_in *addr_in = (struct sockaddr_in*)addr;
	s->s_addr= *addr_in;
	s->opts |= NTY_TCP_ADDR_BIND;

	return 0;
}


int listen(int sockid, int backlog) {
	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	if (sockid < 0) {
		errno = -EBADF;
		return -1;
	}

	if (tcp->fdtable == NULL) {
		errno = -EBADF;
		return -1;
	}
	struct _nty_socket *s = tcp->fdtable->sockfds[sockid];
	if (s == NULL) {
		errno = -EBADF;
		return -1;
	}
	
	if (s->socktype == NTY_TCP_SOCK_UNUSED) {
		nty_trace_api("Socket %d: invalid argument!\n", sockid);
		errno = -EBADF;
		return -1;
	}
	if (s->socktype == NTY_TCP_SOCK_STREAM) {
		s->socktype = NTY_TCP_SOCK_LISTENER;
	}
	if (s->socktype != NTY_TCP_SOCK_LISTENER) {
		nty_trace_api("Not a listening socket. id: %d\n", sockid);
		errno = -ENOTSOCK;
		return -1;
	}

	if (ListenerHTSearch(tcp->listeners, &s->s_addr.sin_port)) {
		errno = EADDRINUSE;
		return -1;
	}
	
	nty_tcp_listener *listener = (nty_tcp_listener*)calloc(1, sizeof(nty_tcp_listener));
	if (!listener) {
		return -1;
	}

	listener->sockid = sockid;
	listener->backlog = backlog;
	//listener->socket = &tcp->smap[sockid];
	listener->s = s;

	if (pthread_cond_init(&listener->accept_cond, NULL)) {
		nty_trace_api("pthread_cond_init of ctx->accept_cond\n");
		free(listener);
		return -1;
	}

	if (pthread_mutex_init(&listener->accept_lock, NULL)) {
		nty_trace_api("pthread_mutex_init of ctx->accept_lock\n");
		free(listener);
		return -1;
	}

	listener->acceptq = CreateStreamQueue(backlog);
	if (!listener->acceptq) {
		free(listener);
		errno = -ENOMEM;
		return -1;
	}

	s->listener = listener;
	ListenerHTInsert(tcp->listeners, listener);

	return 0;
}

int accept(int sockid, struct sockaddr *addr, socklen_t *addrlen) {

	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	if (sockid < 0) {
		errno = -EBADF;
		return -1;
	}
	
	if (tcp->fdtable == NULL) {
		errno = -EBADF;
		return -1;
	}
	struct _nty_socket *s = tcp->fdtable->sockfds[sockid];

	if (s == NULL) {
		errno = -EBADF;
		return -1;
	}

	if (s->socktype != NTY_TCP_SOCK_LISTENER) {
		errno = EINVAL;
		return -1;
	}

	nty_tcp_listener *listener = s->listener;
	nty_tcp_stream *accepted = StreamDequeue(listener->acceptq);
	if (!accepted) {
		if (listener->socket->opts & NTY_TCP_NONBLOCK) {
			errno = EAGAIN;
			return -1;
		} else {
			pthread_mutex_lock(&listener->accept_lock);
			while (accepted == NULL && ((accepted = StreamDequeue(listener->acceptq)) == NULL)) {
				pthread_cond_wait(&listener->accept_cond, &listener->accept_lock);

				//printf("");
				if (tcp->ctx->done || tcp->ctx->exit) {
					pthread_mutex_unlock(&listener->accept_lock);
					errno = EINTR;
					return -1;
				}
			}
			pthread_mutex_unlock(&listener->accept_lock);
		}
	}

	struct _nty_socket *socket = NULL;
	if (!accepted->s) {
		socket = nty_socket_allocate(NTY_TCP_SOCK_STREAM);
		if (!socket) {
			nty_trace_api("Failed to create new socket!\n");
			/* TODO: destroy the stream */
			errno = -ENFILE;
			return -1;
		}

		socket->stream = accepted;
		accepted->s = socket;

		socket->s_addr.sin_family = AF_INET;
		socket->s_addr.sin_port = accepted->dport;
		socket->s_addr.sin_addr.s_addr = accepted->daddr;
	}
#if 0
	if (!(listener->socket->epoll & NTY_EPOLLET) &&
		!StreamQueueIsEmpty(listener->acceptq)) {
		nty_epoll_add_event(tcp->ep, USR_SHADOW_EVENT_QUEUE, listener->socket, NTY_EPOLLIN);		
	}
#endif
	nty_trace_api("Stream %d accepted.\n", accepted->id);

	if (addr && addrlen) {
		struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
		addr_in->sin_family = AF_INET;
		addr_in->sin_port = accepted->dport;
		addr_in->sin_addr.s_addr = accepted->daddr;
		*addrlen = sizeof(struct sockaddr_in);
	}

	return accepted->s->id;

}

ssize_t recv(int sockid, void *buf, size_t len, int flags) {
	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	if (sockid < 0) {
		errno = -EBADF;
		return -1;
	}

	if (tcp->fdtable == NULL) {
		errno = -EBADF;
		return -1;
	}
	struct _nty_socket *s = tcp->fdtable->sockfds[sockid];
	if (s == NULL) {
		errno = -EBADF;
		return -1;
	}
	if (s->socktype == NTY_TCP_SOCK_UNUSED) {
		errno = -EINVAL;
		return -1;
	}
	
	if (s->socktype != NTY_TCP_SOCK_STREAM) {
		errno = -ENOTSOCK;
		return -1;
	}
	
	/* stream should be in ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2, CLOSE_WAIT */
	nty_tcp_stream *cur_stream = s->stream;
	if (!cur_stream ||
		!(cur_stream->state == NTY_TCP_ESTABLISHED ||
		  cur_stream->state == NTY_TCP_CLOSE_WAIT ||
		  cur_stream->state == NTY_TCP_FIN_WAIT_1 ||
		  cur_stream->state == NTY_TCP_FIN_WAIT_2)) {
		errno = -ENOTCONN;
		return -1;
	}
	
	nty_tcp_recv *rcv = cur_stream->rcv;
	if (cur_stream->state == NTY_TCP_CLOSE_WAIT) {
		if (!rcv->recvbuf) return 0;
		if (rcv->recvbuf->merged_len == 0) return 0;
	}

	if (s->opts & NTY_TCP_NONBLOCK) {
		if (!rcv->recvbuf || rcv->recvbuf->merged_len == 0) {
			errno = -EAGAIN;
			return -1;
		}
	}
	
	//SBUF_LOCK(&rcv->read_lock);
	pthread_mutex_lock(&rcv->read_lock);
#if NTY_ENABLE_BLOCKING

	if (!(s->opts & NTY_TCP_NONBLOCK)) {
		
		while (!rcv->recvbuf || rcv->recvbuf->merged_len == 0) {
			if (!cur_stream || cur_stream->state != NTY_TCP_ESTABLISHED) {
				pthread_mutex_unlock(&rcv->read_lock);
				
				if (rcv->recvbuf->merged_len == 0) { //disconnect
					errno = 0;
					return 0;
				} else {
					errno = -EINTR;
					return -1;
				}
			}
			
			pthread_cond_wait(&rcv->read_cond, &rcv->read_lock);
		}
	}
#endif

	int ret = 0;
	switch (flags) {
		case 0: {
			ret = nty_copy_to_user(cur_stream, buf, len);
			break;
		}
		default: {
			pthread_mutex_unlock(&rcv->read_lock);
			ret = -1;
			errno = -EINVAL;
			return ret;
		}		
	}
#if 0
	int event_remaining = 0;
	if (s->epoll & NTY_EPOLLIN) {
		if (!(s->epoll & NTY_EPOLLET) && rcv->recvbuf->merged_len > 0) {
			event_remaining = 1;
		}
	}
#endif

	if (cur_stream->state == NTY_TCP_CLOSE_WAIT && 
	    rcv->recvbuf->merged_len == 0 && ret > 0) { //closed 
		//event_remaining = 1;
	}
	
	pthread_mutex_unlock(&rcv->read_lock);

	nty_trace_api("Stream %d: mtcp_recv() returning %d\n", cur_stream->id, ret);
	
    return ret;
}

ssize_t send(int sockid, const void *buf, size_t len, int flags) {
	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	if (sockid < 0) {
		errno = EBADF;
		return -1;
	}
	
	if (tcp->fdtable == NULL) {
		errno = -EBADF;
		return -1;
	}
	struct _nty_socket *s = tcp->fdtable->sockfds[sockid];
	if (s->socktype == NTY_TCP_SOCK_UNUSED) {
		errno = EINVAL;
		return -1;
	}
	if (s->socktype != NTY_TCP_SOCK_STREAM) {
		errno = ENOTSOCK;
		return -1;
	}
	/* stream should be in ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2, CLOSE_WAIT */
	nty_tcp_stream *cur_stream = s->stream;
	if (!cur_stream ||
		!(cur_stream->state == NTY_TCP_ESTABLISHED ||
		  cur_stream->state == NTY_TCP_CLOSE_WAIT)) {
		errno = ENOTCONN;
		return -1;
	}

	if (len <= 0) {
		if (s->opts & NTY_TCP_NONBLOCK) {
			errno = EAGAIN;
			return -1;
		} else {
			return 0;
		}
	}

	nty_tcp_send *snd = cur_stream->snd;

	pthread_mutex_lock(&snd->write_lock);

#if NTY_ENABLE_BLOCKING
	if (!(s->opts & NTY_TCP_NONBLOCK)) {
		while (snd->snd_wnd <= 0) {
			if (!cur_stream || cur_stream->state != NTY_TCP_ESTABLISHED) {
				pthread_mutex_unlock(&snd->write_lock);
				errno = EINTR;
				return -1;
			}
			
			pthread_cond_wait(&snd->write_cond, &snd->write_lock);
		}
	}
#endif
	int ret = nty_copy_from_user(cur_stream, buf, len);
	pthread_mutex_unlock(&snd->write_lock);

	nty_trace_api("nty_copy_from_user --> %d, %d\n", 
		snd->on_sendq, snd->on_send_list);
	if (ret > 0 && !(snd->on_sendq || snd->on_send_list)) {
		snd->on_sendq = 1;
		StreamEnqueue(tcp->sendq, cur_stream);
		tcp->wakeup_flag = 1;
	}

	if (ret == 0 && (s->opts & NTY_TCP_NONBLOCK)) {
		ret = -1;
		errno = -EAGAIN;
	}
#if 0
	if (snd->snd_wnd > 0) {
		if ((s->epoll & NTY_EPOLLOUT) && !(s->epoll & NTY_EPOLLET)) {
			nty_epoll_add_event(tcp->ep, USR_SHADOW_EVENT_QUEUE, socket, NTY_EPOLLOUT);
#if NTY_ENABLE_BLOCKING
		} else if (!(socket->opts & NTY_TCP_NONBLOCK)) {
			if (!cur_stream->on_snd_br_list) {
				cur_stream->on_snd_br_list = 1;
				TAILQ_INSERT_TAIL(&tcp->snd_br_list, cur_stream, snd->snd_br_link);
				tcp->snd_br_list_cnt ++;
			}
#endif
		}
	}
#endif
	
	nty_trace_api("Stream %d: mtcp_write() returning %d\n", cur_stream->id, ret);
	return ret;
}



#endif


