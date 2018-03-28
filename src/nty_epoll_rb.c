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



#include "nty_tree.h"
#include "nty_queue.h"
#include "nty_epoll_inner.h"
#include "nty_config.h"

#if NTY_ENABLE_EPOLL_RB


#include <pthread.h>
#include <stdint.h>
#include <time.h>


//static pthread_mutex_t epmutex;


extern nty_tcp_manager *nty_get_tcp_manager(void);

int epoll_create(int size) {

	if (size <= 0) return -1;

	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;
	
	struct _nty_socket *epsocket = nty_socket_allocate(NTY_TCP_SOCK_EPOLL);
	if (epsocket == NULL) {
		nty_trace_epoll("malloc failed\n");
		return -1;
	}

	struct eventpoll *ep = (struct eventpoll*)calloc(1, sizeof(struct eventpoll));
	if (!ep) {
		nty_free_socket(epsocket->id, 0);
		return -1;
	}

	ep->rbcnt = 0;
	RB_INIT(&ep->rbr);
	LIST_INIT(&ep->rdlist);

	if (pthread_mutex_init(&ep->mtx, NULL)) {
		free(ep);
		nty_free_socket(epsocket->id, 0);
		return -2;
	}

	if (pthread_mutex_init(&ep->cdmtx, NULL)) {
		pthread_mutex_destroy(&ep->mtx);
		free(ep);
		nty_free_socket(epsocket->id, 0);
		return -2;
	}

	if (pthread_cond_init(&ep->cond, NULL)) {
		pthread_mutex_destroy(&ep->cdmtx);
		pthread_mutex_destroy(&ep->mtx);
		free(ep);
		nty_free_socket(epsocket->id, 0);
		return -2;
	}

	if (pthread_spin_init(&ep->lock, PTHREAD_PROCESS_SHARED)) {
		pthread_cond_destroy(&ep->cond);
		pthread_mutex_destroy(&ep->cdmtx);
		pthread_mutex_destroy(&ep->mtx);
		free(ep);

		nty_free_socket(epsocket->id, 0);
		return -2;
	}

	tcp->ep = (void*)ep;
	epsocket->ep = (void*)ep;

	return epsocket->id;
}

int epoll_ctl(int epid, int op, int sockid, struct epoll_event *event) {

	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	nty_trace_epoll(" epoll_ctl --> 1111111:%d, sockid:%d\n", epid, sockid);
	struct _nty_socket *epsocket = tcp->fdtable->sockfds[epid];
	struct _nty_socket *socket = tcp->fdtable->sockfds[sockid];

	nty_trace_epoll(" epoll_ctl --> 1111111:%d, sockid:%d\n", epsocket->id, socket->id);
	if (epsocket->socktype == NTY_TCP_SOCK_UNUSED) {
		errno = -EBADF;
		return -1;
	}

	if (epsocket->socktype != NTY_TCP_SOCK_EPOLL) {
		errno = -EINVAL;
		return -1;
	}

	nty_trace_epoll(" epoll_ctl --> eventpoll\n");

	struct eventpoll *ep = (struct eventpoll*)epsocket->ep;
	if (!ep || (!event && op != EPOLL_CTL_DEL)) {
		errno = -EINVAL;
		return -1;
	}

	if (op == EPOLL_CTL_ADD) {

		pthread_mutex_lock(&ep->mtx);

		struct epitem tmp;
		tmp.sockfd = sockid;
		struct epitem *epi = RB_FIND(_epoll_rb_socket, &ep->rbr, &tmp);
		if (epi) {
			nty_trace_epoll("rbtree is exist\n");
			pthread_mutex_unlock(&ep->mtx);
			return -1;
		}

		epi = (struct epitem*)calloc(1, sizeof(struct epitem));
		if (!epi) {
			pthread_mutex_unlock(&ep->mtx);
			errno = -ENOMEM;
			return -1;
		}
		
		epi->sockfd = sockid;
		memcpy(&epi->event, event, sizeof(struct epoll_event));

		epi = RB_INSERT(_epoll_rb_socket, &ep->rbr, epi);
		assert(epi == NULL);
		ep->rbcnt ++;
		
		pthread_mutex_unlock(&ep->mtx);

	} else if (op == EPOLL_CTL_DEL) {

		pthread_mutex_lock(&ep->mtx);

		struct epitem tmp;
		tmp.sockfd = sockid;
		struct epitem *epi = RB_FIND(_epoll_rb_socket, &ep->rbr, &tmp);
		if (!epi) {
			nty_trace_epoll("rbtree no exist\n");
			pthread_mutex_unlock(&ep->mtx);
			return -1;
		}
		
		epi = RB_REMOVE(_epoll_rb_socket, &ep->rbr, epi);
		if (!epi) {
			nty_trace_epoll("rbtree is no exist\n");
			pthread_mutex_unlock(&ep->mtx);
			return -1;
		}

		ep->rbcnt --;
		free(epi);
		
		pthread_mutex_unlock(&ep->mtx);

	} else if (op == EPOLL_CTL_MOD) {

		struct epitem tmp;
		tmp.sockfd = sockid;
		struct epitem *epi = RB_FIND(_epoll_rb_socket, &ep->rbr, &tmp);
		if (epi) {
			epi->event.events = event->events;
			epi->event.events |= EPOLLERR | EPOLLHUP;
		} else {
			errno = -ENOENT;
			return -1;
		}

	} else {
		nty_trace_epoll("op is no exist\n");
		assert(0);
	}

	return 0;
}

int epoll_wait(int epid, struct epoll_event *events, int maxevents, int timeout) {

	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	//nty_socket_map *epsocket = &tcp->smap[epid];
	struct _nty_socket *epsocket = tcp->fdtable->sockfds[epid];
	if (epsocket == NULL) return -1;

	if (epsocket->socktype == NTY_TCP_SOCK_UNUSED) {
		errno = -EBADF;
		return -1;
	}

	if (epsocket->socktype != NTY_TCP_SOCK_EPOLL) {
		errno = -EINVAL;
		return -1;
	}

	struct eventpoll *ep = (struct eventpoll*)epsocket->ep;
	if (!ep || !events || maxevents <= 0) {
		errno = -EINVAL;
		return -1;
	}

	if (pthread_mutex_lock(&ep->cdmtx)) {
		if (errno == EDEADLK) {
			nty_trace_epoll("epoll lock blocked\n");
		}
		assert(0);
	}

	
	while (ep->rdnum == 0 && timeout != 0) {

		ep->waiting = 1;
		if (timeout > 0) {

			struct timespec deadline;

			clock_gettime(CLOCK_REALTIME, &deadline);
			if (timeout >= 1000) {
				int sec;
				sec = timeout / 1000;
				deadline.tv_sec += sec;
				timeout -= sec * 1000;
			}

			deadline.tv_nsec += timeout * 1000000;

			if (deadline.tv_nsec >= 1000000000) {
				deadline.tv_sec++;
				deadline.tv_nsec -= 1000000000;
			}

			int ret = pthread_cond_timedwait(&ep->cond, &ep->cdmtx, &deadline);
			if (ret && ret != ETIMEDOUT) {
				nty_trace_epoll("pthread_cond_timewait\n");
				
				pthread_mutex_unlock(&ep->cdmtx);
				
				return -1;
			}
			timeout = 0;
		} else if (timeout < 0) {

			int ret = pthread_cond_wait(&ep->cond, &ep->cdmtx);
			if (ret) {
				nty_trace_epoll("pthread_cond_wait\n");
				pthread_mutex_unlock(&ep->cdmtx);

				return -1;
			}
		}
		ep->waiting = 0; 

	}

	pthread_mutex_unlock(&ep->cdmtx);

	pthread_spin_lock(&ep->lock);

	int cnt = 0;
	int num = (ep->rdnum > maxevents ? maxevents : ep->rdnum);
	int i = 0;
	
	while (num != 0 && !LIST_EMPTY(&ep->rdlist)) { //EPOLLET

		struct epitem *epi = LIST_FIRST(&ep->rdlist);
		LIST_REMOVE(epi, rdlink);
		epi->rdy = 0;

		memcpy(&events[i++], &epi->event, sizeof(struct epoll_event));
		
		num --;
		cnt ++;
		ep->rdnum --;
	}
	
	pthread_spin_unlock(&ep->lock);

	return cnt;
}


/* 
 * insert callback inside to struct tcp_stream
 * 
 */
int epoll_event_callback(struct eventpoll *ep, int sockid, uint32_t event) {

	struct epitem tmp;
	tmp.sockfd = sockid;
	struct epitem *epi = RB_FIND(_epoll_rb_socket, &ep->rbr, &tmp);
	if (!epi) {
		nty_trace_epoll("rbtree not exist\n");
		assert(0);
	}
	if (epi->rdy) {
		epi->event.events |= event;
		return 1;
	} 

	nty_trace_epoll("epoll_event_callback --> %d\n", epi->sockfd);
	
	pthread_spin_lock(&ep->lock);
	epi->rdy = 1;
	LIST_INSERT_HEAD(&ep->rdlist, epi, rdlink);
	ep->rdnum ++;
	pthread_spin_unlock(&ep->lock);

	pthread_mutex_lock(&ep->cdmtx);

	pthread_cond_signal(&ep->cond);
	pthread_mutex_unlock(&ep->cdmtx);
	return 0;
}

static int epoll_destroy(struct eventpoll *ep) {

	//remove rdlist

	while (!LIST_EMPTY(&ep->rdlist)) {
		struct epitem *epi = LIST_FIRST(&ep->rdlist);
		LIST_REMOVE(epi, rdlink);
	}
	
	//remove rbtree
	pthread_mutex_lock(&ep->mtx);
	
	for (;;) {
		struct epitem *epi = RB_MIN(_epoll_rb_socket, &ep->rbr);
		if (epi == NULL) break;
		
		epi = RB_REMOVE(_epoll_rb_socket, &ep->rbr, epi);
		free(epi);
	}
	pthread_mutex_unlock(&ep->mtx);

	return 0;
}

int nty_epoll_close_socket(int epid) {

	nty_tcp_manager *tcp = nty_get_tcp_manager();
	if (!tcp) return -1;

	struct eventpoll *ep = (struct eventpoll *)tcp->fdtable->sockfds[epid]->ep;
	if (!ep) {
		errno = -EINVAL;
		return -1;
	}

	epoll_destroy(ep);

	pthread_mutex_lock(&ep->mtx);
	tcp->ep = NULL;
	tcp->fdtable->sockfds[epid]->ep = NULL;
	pthread_cond_signal(&ep->cond);
	pthread_mutex_unlock(&ep->mtx);

	pthread_cond_destroy(&ep->cond);
	pthread_mutex_destroy(&ep->mtx);

	pthread_spin_destroy(&ep->lock);

	free(ep);

	return 0;
}


#endif


