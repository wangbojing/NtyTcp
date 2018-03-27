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

	nty_socket_map *epsocket = nty_allocate_socket(NTY_TCP_SOCK_EPOLL, 0);
	if (epsocket == NULL) {
		nty_trace_epoll("malloc failed\n");
		return -1;
	}

	struct eventpoll *ep = (struct eventpoll*)calloc(1, sizeof(struct eventpoll));
	if (!ep) {
		nty_free_socket(epsocket->id, 0);
		return -1;
	}

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

	nty_socket_map *epsocket = &tcp->smap[epid];
	nty_socket_map *socket = &tcp->smap[sockid];
	
	if (epsocket->socktype == NTY_TCP_SOCK_UNUSED || 
		socket->socktype == NTY_TCP_SOCK_UNUSED) {
		errno = -EBADF;
		return -1;
	}

	if (epsocket->socktype != NTY_TCP_SOCK_EPOLL) {
		errno = -EINVAL;
		return -1;
	}

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
		
		pthread_mutex_unlock(&ep->mtx);

	} else if (op == EPOLL_CTL_DEL) {

		pthread_mutex_lock(&ep->mtx);

		struct epitem tmp;
		tmp.sockfd = sockid;
		struct epitem *epi = RB_REMOVE(_epoll_rb_socket, &ep->rbr, &tmp);
		if (!epi) {
			nty_trace_epoll("rbtree is no exist\n");
			pthread_mutex_unlock(&ep->mtx);
			return -1;
		}

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

	nty_socket_map *epsocket = &tcp->smap[epid];

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

	printf("epoll_event_callback --> %d\n", epi->sockfd);
	
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


#endif


