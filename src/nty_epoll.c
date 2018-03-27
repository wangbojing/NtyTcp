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
#include "nty_socket.h"

#include <unistd.h>
#include <time.h>
#include <signal.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

extern nty_tcp_manager *nty_get_tcp_manager(void);

char *event_str[] = {"NONE", "IN", "PRI", "OUT", "ERR", "HUP", "RDHUP"};

char *EventToString(uint32_t event) {
	
	switch (event) {
		case NTY_EPOLLNONE : {
			return event_str[0];
		}
		case NTY_EPOLLIN : {
			return event_str[1];
		}
		case NTY_EPOLLPRI : {
			return event_str[2];
		}
		case NTY_EPOLLOUT : {
			return event_str[3];
		}
		case NTY_EPOLLERR : {
			return event_str[4];
		}
		case NTY_EPOLLHUP : {
			return event_str[5];
		}
		case NTY_EPOLLRDHUP : {
			return event_str[6];
		}
		default :
			assert(0);
	}

	return NULL;
}


nty_event_queue *nty_create_event_queue(int size) {
	nty_event_queue *eq = (nty_event_queue*)calloc(1, sizeof(nty_event_queue));
	if (!eq) return NULL;

	eq->start = 0;
	eq->end = 0;
	eq->size = size;
	eq->events = (nty_epoll_event_int*)calloc(size, sizeof(nty_epoll_event_int));
	if (!eq->events) {
		free(eq);
		return NULL;
	}
	eq->num_events = 0;

	return eq;
}

void nty_destory_event_queue(nty_event_queue *eq) {
	if (eq->events) {
		free(eq->events);
	}
	free(eq);
}


int nty_close_epoll_socket(int epid) {

	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	nty_epoll *ep = tcp->smap[epid].ep;
	if (!ep) {
		errno = EINVAL;
		return -1;
	}

	nty_destory_event_queue(ep->usr_queue);
	nty_destory_event_queue(ep->usr_shadow_queue);
	nty_destory_event_queue(ep->queue);

	pthread_mutex_lock(&ep->epoll_lock);
	tcp->ep = NULL;
	tcp->smap[epid].ep = NULL;
	pthread_cond_signal(&ep->epoll_cond);
	pthread_mutex_unlock(&ep->epoll_lock);

	pthread_cond_destroy(&ep->epoll_cond);
	pthread_mutex_destroy(&ep->epoll_lock);

	free(ep);

	return 0;
}

//ep   queue copy to usr_queue
int nty_epoll_flush_events(uint32_t cur_ts) {
	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	nty_epoll *ep = tcp->ep;
	nty_event_queue *usrq = ep->usr_queue;
	nty_event_queue *tcpq = ep->queue;
	
	pthread_mutex_lock(&ep->epoll_lock);

	if (ep->queue->num_events > 0) {
		while (tcpq->num_events > 0 && usrq->num_events < usrq->size) {
			usrq->events[usrq->end++] = tcpq->events[tcpq->start ++];

			if (usrq->end >= usrq->size) usrq->end = 0;
			usrq->num_events ++;

			if (tcpq->start >= tcpq->size) tcpq->start = 0;
			tcpq->num_events --;
		}
	}

	if (ep->waiting && 
		(ep->usr_queue->num_events > 0 || ep->usr_shadow_queue->num_events > 0)) {

		nty_trace_epoll("Broadcasting events. num: %d, cur_ts: %u, prev_ts: %u\n", 
				ep->usr_queue->num_events, cur_ts, tcp->ts_last_event);
		
		tcp->ts_last_event = cur_ts;
		ep->stat.wakes ++;
		pthread_cond_signal(&ep->epoll_cond);
	}

	pthread_mutex_unlock(&ep->epoll_lock);

	return 0;
}


int nty_epoll_add_event(nty_epoll *ep, int queue_type, struct _nty_socket_map *socket, uint32_t event) {
	nty_event_queue *eq = NULL;

	if (!ep || !socket || !event) return -1;

	ep->stat.issued ++;

	if (socket->events & event) return 0;

	if (queue_type == NTY_EVENT_QUEUE) {
		eq = ep->queue;
	} else if (queue_type == USR_EVENT_QUEUE) {
		eq = ep->usr_queue;
	} else if (queue_type == USR_SHADOW_EVENT_QUEUE) {
		eq = ep->usr_shadow_queue;
	} else {
		nty_trace_epoll("Non-existing event queue type!\n");
		return -1;
	}

	if (eq->num_events >= eq->size) {
		nty_trace_epoll("Exceeded epoll event queue! num_events: %d, size: %d\n", 
				eq->num_events, eq->size);
		if (queue_type == USR_EVENT_QUEUE)
			pthread_mutex_unlock(&ep->epoll_lock);
		return -1;
	}

	int idx = eq->end ++;

	socket->events |= event;
	eq->events[idx].sockid = socket->id;
	eq->events[idx].ev.events = event;
	eq->events[idx].ev.data = socket->ep_data;

	if (eq->end >= eq->size) {
		eq->end = 0;
	}
	eq->num_events ++;
	nty_trace_epoll("nty_epoll_add_event --> num_events:%d\n", eq->num_events);

	if (queue_type == USR_EVENT_QUEUE) {
		pthread_mutex_unlock(&ep->epoll_lock);
	}

	ep->stat.registered ++;

	return 0;
}

int nty_raise_pending_stream_events(nty_epoll *ep, nty_socket_map *socket) {

	nty_tcp_stream *stream = socket->stream;
	if (!stream) {
		return -1;
	}
	nty_trace_epoll("Stream %d at state %d\n", stream->id, stream->state);
	if (stream->state < NTY_TCP_ESTABLISHED) {
		return -1;
	}

	if (socket->epoll & NTY_EPOLLIN) {
		nty_tcp_recv *rcv = stream->rcv;
		if (rcv->recvbuf && rcv->recvbuf->merged_len > 0) {
			nty_epoll_add_event(ep, USR_SHADOW_EVENT_QUEUE, socket, NTY_EPOLLIN);
		} else if (stream->state == NTY_TCP_CLOSE_WAIT) {
			nty_epoll_add_event(ep, USR_SHADOW_EVENT_QUEUE, socket, NTY_EPOLLIN);
		}
	}

	if (socket->epoll & NTY_EPOLLOUT) {
		nty_tcp_send *snd = stream->snd;
		if (!snd->sndbuf || (snd->sndbuf && snd->sndbuf->len < snd->snd_wnd)) {
			if (!(socket->events & NTY_EPOLLOUT)) {
				nty_trace_epoll("socket %d: adding write event\n", socket->id);
				nty_epoll_add_event(ep, USR_SHADOW_EVENT_QUEUE, socket, NTY_EPOLLOUT);
			}
		}
	}

	return 0;
}

int nty_epoll_create(int size) {
	nty_tcp_manager *tcp = nty_get_tcp_manager();

	if (size <= 0) {
		errno = EINVAL;
		return -1;
	}

	nty_socket_map *epsocket = nty_allocate_socket(NTY_TCP_SOCK_EPOLL, 0);
	if (!epsocket) {
		errno = ENFILE;
		return -1;
	}

	nty_epoll *ep = (nty_epoll*)calloc(1, sizeof(nty_epoll));
	if (!ep) {
		nty_free_socket(epsocket->id, 0);
		return -1;
	}
	ep->usr_queue = nty_create_event_queue(size);
	if (!ep->usr_queue) {
		nty_free_socket(epsocket->id, 0);
		free(ep);
		return -1;
	}

	ep->usr_shadow_queue = nty_create_event_queue(size);
	if (!ep->usr_shadow_queue) {
		nty_destory_event_queue(ep->usr_queue);
		nty_free_socket(epsocket->id, 0);
		free(ep);
		return -1;
	}

	ep->queue = nty_create_event_queue(size);
	if (!ep->queue) {
		nty_destory_event_queue(ep->usr_shadow_queue);
		nty_destory_event_queue(ep->usr_queue);
		nty_free_socket(epsocket->id, 0);
		free(ep);
		return -1;
	}

	nty_trace_epoll("epoll structure of size %d created.\n", size);

	tcp->ep = ep;
	epsocket->ep = ep;

	if (pthread_mutex_init(&ep->epoll_lock, NULL)) {
		nty_destory_event_queue(ep->queue);
		nty_destory_event_queue(ep->usr_shadow_queue);
		nty_destory_event_queue(ep->usr_queue);
		nty_free_socket(epsocket->id, 0);
		free(ep);
		return -1;
	}

	if (pthread_cond_init(&ep->epoll_cond, NULL)) {
		nty_destory_event_queue(ep->queue);
		nty_destory_event_queue(ep->usr_shadow_queue);
		nty_destory_event_queue(ep->usr_queue);
		nty_free_socket(epsocket->id, 0);
		free(ep);
		return -1;
	}

	return epsocket->id;
}


int nty_epoll_ctl(int epid, int op, int sockid, nty_epoll_event *event) {
	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (tcp == NULL) return -1;

	if (epid < 0 || epid >= NTY_MAX_CONCURRENCY) {
		errno = EBADF;
		return -1;
	}

	if (sockid < 0 || sockid >= NTY_MAX_CONCURRENCY) {
		errno = EBADF;
		return -1;
	}

	if (tcp->smap[epid].socktype == NTY_TCP_SOCK_UNUSED) {
		errno = EBADF;
		return -1;
	}

	if (tcp->smap[epid].socktype != NTY_TCP_SOCK_EPOLL) {
		errno = EINVAL;
		return -1;
	}

	nty_epoll *ep = tcp->smap[epid].ep;
	if (!ep || (!event && op != NTY_EPOLL_CTL_DEL)) {
		errno = EINVAL;
		return -1;
	}

	uint32_t events;
	nty_socket_map *socket = &tcp->smap[sockid];
	if (op == NTY_EPOLL_CTL_ADD) {
		if (socket->epoll) {
			errno = EEXIST;
			return -1;
		}

		events = event->events;
		events |= (NTY_EPOLLERR | NTY_EPOLLHUP);
		socket->ep_data = event->data;
		socket->epoll = events;
		
		nty_trace_epoll("Adding epoll socket %d(type %d) ET: %u, IN: %u, OUT: %u\n", 
				socket->id, socket->socktype, socket->epoll & NTY_EPOLLET, 
				socket->epoll & NTY_EPOLLIN, socket->epoll & NTY_EPOLLOUT);

		if (socket->socktype == NTY_TCP_SOCK_STREAM) {
			nty_raise_pending_stream_events(ep, socket);
		} else if (socket->socktype == NTY_TCP_SOCK_PIPE) {
			//nty_raise_pending_stream_events(struct nty_epoll * ep,nty_socket_map * socket)
		}
	} else if (op == NTY_EPOLL_CTL_MOD) {

		if (!socket->epoll) {
			pthread_mutex_lock(&ep->epoll_lock);
			errno = ENOENT;
			return -1;
		}
		events = event->events;
		events |= (NTY_EPOLLERR | NTY_EPOLLHUP);
		socket->ep_data = event->data;
		socket->epoll = events;

		if (socket->socktype == NTY_TCP_SOCK_STREAM) {
			nty_raise_pending_stream_events(ep, socket);
		} else if (socket->socktype == NTY_TCP_SOCK_PIPE) {
			//
		}
	} else if (op == NTY_EPOLL_CTL_DEL) {
		if (!socket->epoll) {
			errno = ENOENT;
			return -1;
		}
		socket->epoll = NTY_EPOLLNONE;
	}

	return 0;
}

int nty_epoll_wait(int epid, nty_epoll_event *events, int maxevents, int timeout) {

	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	if (epid < 0 || epid >= NTY_MAX_CONCURRENCY) {
		nty_trace_epoll("Epoll id %d out of range.\n", epid);
		errno = EBADF;
		return -1;
	}
	if (tcp->smap[epid].socktype == NTY_TCP_SOCK_UNUSED) {
		errno = EBADF;
		return -1;
	}

	if (tcp->smap[epid].socktype != NTY_TCP_SOCK_EPOLL) {
		errno = EINVAL;
		return -1;
	}

	nty_epoll *ep = tcp->smap[epid].ep;
	if (!ep || !events || maxevents <= 0) {
		errno = EINVAL;
		return -1;
	}
	ep->stat.calls ++;

	if (pthread_mutex_lock(&ep->epoll_lock)) {
		if (errno == EDEADLK) {
			nty_trace_epoll("nty_epoll_wait: epoll_lock blocked\n");
		}
		assert(0);
	}

	int cnt = 0;
	do {
		nty_event_queue *eq = ep->usr_queue;
		nty_event_queue *eq_shadow = ep->usr_shadow_queue;

		while (eq->num_events == 0 && eq_shadow->num_events == 0 && timeout != 0) {
			ep->stat.waits ++;
			ep->waiting = 1;

			if (timeout > 0) {
				struct timespec deadline;
				clock_gettime(CLOCK_REALTIME, &deadline);

				if (timeout >= 1000) {
					int sec = timeout / 1000;
					deadline.tv_sec += sec;
					timeout -= sec * 1000;
				}
				deadline.tv_nsec += timeout * 1000000;

				if (deadline.tv_nsec >= 1000000000) {
					deadline.tv_sec ++;
					deadline.tv_nsec -= 1000000000;
				}

				int ret = pthread_cond_timedwait(&ep->epoll_cond, &ep->epoll_lock, &deadline);
				if (ret && ret != ETIMEDOUT) {
					pthread_mutex_unlock(&ep->epoll_lock);
					nty_trace_epoll("pthread_cond_timedwait failed. ret: %d, error: %s\n", 
							ret, strerror(errno));
					return -1;
				}
				timeout = 0;
			} else if (timeout < 0) {
				nty_trace_epoll("[%s:%s:%d]: pthread_cond_wait\n", __FILE__, __func__, __LINE__);
				int ret = pthread_cond_wait(&ep->epoll_cond, &ep->epoll_lock);
				if (ret) {
					pthread_mutex_unlock(&ep->epoll_lock);
					nty_trace_epoll("pthread_cond_wait failed. ret: %d, error: %s\n", 
							ret, strerror(errno));
					return -1;
				}
			}

			ep->waiting = 0;
			if (tcp->ctx->done || tcp->ctx->exit || tcp->ctx->interrupt) {
				tcp->ctx->interrupt = 0;
				pthread_mutex_unlock(&ep->epoll_lock);
				errno = EINTR;
				return -1;
			}
		}

		int i, validity;
		int num_events = eq->num_events;
		for (i = 0;i < num_events && cnt < maxevents;i ++) {
			nty_socket_map *event_socket = &tcp->smap[eq->events[eq->start].sockid];
			validity = 1;
			if (event_socket->socktype == NTY_TCP_SOCK_UNUSED) 
				validity = 0;
			if (!(event_socket->epoll & eq->events[eq->start].ev.events)) 
				validity = 0;
			if (!(event_socket->events & eq->events[eq->start].ev.events)) 
				validity = 0;

			if (validity) {
				events[cnt++] = eq->events[eq->start].ev;
				assert(eq->events[eq->start].sockid >= 0);

				nty_trace_epoll("Socket %d: Handled event. event: %s, "
						"start: %u, end: %u, num: %u\n", 
						event_socket->id, 
						EventToString(eq->events[eq->start].ev.events), 
						eq->start, eq->end, eq->num_events);

				ep->stat.handled ++;
			} else {
				nty_trace_epoll("Socket %d: event %s invalidated.\n", 
						eq->events[eq->start].sockid, 
						EventToString(eq->events[eq->start].ev.events));
				ep->stat.invalidated ++;
			}
			event_socket->events &= (~eq->events[eq->start].ev.events);

			eq->start ++;
			eq->num_events --;
			if (eq->start >= eq->size) eq->start = 0;
		}

		
		eq = ep->usr_shadow_queue;
		num_events = eq->num_events;
		for (i = 0; i < num_events && cnt < maxevents; i++) {
			nty_socket_map *event_socket = &tcp->smap[eq->events[eq->start].sockid];
			validity = 1;
			if (event_socket->socktype == NTY_TCP_SOCK_UNUSED)
				validity = 0;
			if (!(event_socket->epoll & eq->events[eq->start].ev.events))
				validity = 0;
			if (!(event_socket->events & eq->events[eq->start].ev.events))
				validity = 0;

			if (validity) {
				events[cnt++] = eq->events[eq->start].ev;
				assert(eq->events[eq->start].sockid >= 0);

				nty_trace_epoll("Socket %d: Handled event. event: %s, "
						"start: %u, end: %u, num: %u\n", 
						event_socket->id, 
						EventToString(eq->events[eq->start].ev.events), 
						eq->start, eq->end, eq->num_events);
				ep->stat.handled++;
			} else {
				nty_trace_epoll("Socket %d: event %s invalidated.\n", 
						eq->events[eq->start].sockid, 
						EventToString(eq->events[eq->start].ev.events));
				ep->stat.invalidated++;
			}
			event_socket->events &= (~eq->events[eq->start].ev.events);

			eq->start++;
			eq->num_events--;
			if (eq->start >= eq->size) {
				eq->start = 0;
			}
		}

	}while (cnt == 0 && timeout != 0);

	pthread_mutex_unlock(&ep->epoll_lock);

	return cnt;
}


