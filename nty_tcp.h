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


#ifndef __NTY_TCP_H__
#define __NTY_TCP_H__

#include "nty_buffer.h"
#include "nty_hash.h"
#include "nty_addr.h"
#include "nty_timer.h"

#include "nty_epoll.h"

#define ETH_NUM		4


typedef enum _nty_tcp_state {
	NTY_TCP_CLOSED = 0,
	NTY_TCP_LISTEN = 1,
	NTY_TCP_SYN_SENT = 2,
	NTY_TCP_SYN_RCVD = 3,
	NTY_TCP_ESTABLISHED = 4,
	NTY_TCP_CLOSE_WAIT = 5,
	NTY_TCP_FIN_WAIT_1 = 6,
	NTY_TCP_CLOSING = 7,
	NTY_TCP_LAST_ACK = 8,
	NTY_TCP_FIN_WAIT_2 = 9,
	NTY_TCP_TIME_WAIT = 10,
} nty_tcp_state;

#define NTY_TCPHDR_FIN		0x01
#define NTY_TCPHDR_SYN		0x02
#define NTY_TCPHDR_RST		0x04
#define NTY_TCPHDR_PSH		0x08
#define NTY_TCPHDR_ACK		0x10
#define NTY_TCPHDR_URG		0x20
#define NTY_TCPHDR_ECE		0x40
#define NTY_TCPHDR_CWR		0x80

#define NTY_TCPOPT_MSS_LEN				4	
#define NTY_TCPOPT_WSCALE_LEN			3
#define NTY_TCPOPT_SACK_PERMIT_LEN		2
#define NTY_TCPOPT_SACK_LEN				10
#define NTY_TCPOPT_TIMESTAMP_LEN		10


#define TCP_DEFAULT_MSS		1460
#define TCP_DEFAULT_WSCALE	7
#define TCP_INITIAL_WINDOW	14600
#define TCP_MAX_WINDOW		65535

#define NTY_SEND_BUFFER_SIZE		8192
#define NTY_RECV_BUFFER_SIZE		8192
#define NTY_TCP_TIMEWAIT			0
#define NTY_TCP_TIMEOUT				30

#define TCP_MAX_RTX					16
#define TCP_MAX_SYN_RETRY			7
#define TCP_MAX_BACKOFF				7

#define TCP_SEQ_LT(a,b) 		((int32_t)((a)-(b)) < 0)
#define TCP_SEQ_LEQ(a,b)		((int32_t)((a)-(b)) <= 0)
#define TCP_SEQ_GT(a,b) 		((int32_t)((a)-(b)) > 0)
#define TCP_SEQ_GEQ(a,b)		((int32_t)((a)-(b)) >= 0)
#define TCP_SEQ_BETWEEN(a,b,c)	(TCP_SEQ_GEQ(a,b) && TCP_SEQ_LEQ(a,c))

#define HZ						1000
#define TIME_TICK				(1000000/HZ)		// in us
#define TIMEVAL_TO_TS(t)		(uint32_t)((t)->tv_sec * HZ + ((t)->tv_usec / TIME_TICK))

#define TS_TO_USEC(t)			((t) * TIME_TICK)
#define TS_TO_MSEC(t)			(TS_TO_USEC(t) / 1000)
#define MSEC_TO_USEC(t)			((t) * 1000)
#define USEC_TO_SEC(t)			((t) / 1000000)

#define TCP_INITIAL_RTO 		(MSEC_TO_USEC(500) / TIME_TICK)

#if NTY_ENABLE_BLOCKING

#define SBUF_LOCK_INIT(lock, errmsg, action);		\
	if (pthread_mutex_init(lock, PTHREAD_PROCESS_PRIVATE)) {		\
		perror("pthread_spin_init" errmsg);			\
		action;										\
	}
#define SBUF_LOCK_DESTROY(lock)	pthread_mutex_destroy(lock)
#define SBUF_LOCK(lock)			pthread_mutex_lock(lock)
#define SBUF_UNLOCK(lock)		pthread_mutex_unlock(lock)


#else


#define SBUF_LOCK_INIT(lock, errmsg, action);		\
	if (pthread_spin_init(lock, PTHREAD_PROCESS_PRIVATE)) {		\
		perror("pthread_spin_init" errmsg);			\
		action;										\
	}
#define SBUF_LOCK_DESTROY(lock)	pthread_spin_destroy(lock)
#define SBUF_LOCK(lock)			pthread_spin_lock(lock)
#define SBUF_UNLOCK(lock)		pthread_spin_unlock(lock)


#endif

enum tcp_option {
	TCP_OPT_END		= 0,
	TCP_OPT_NOP		= 1,
	TCP_OPT_MSS		= 2,
	TCP_OPT_WSCALE	= 3,
	TCP_OPT_SACK_PERMIT = 4,
	TCP_OPT_SACK 	= 5,
	TCP_OPT_TIMESTAMP = 8
};

enum tcp_close_reason {
	TCP_NOT_CLOSED		= 0,
	TCP_ACTIVE_CLOSE	= 1,
	TCP_PASSIVE_CLOSE	= 2,
	TCP_CONN_FAIL		= 3,
	TCP_CONN_LOST		= 4,
	TCP_RESET			= 5,
	TCP_NO_MEM			= 6,
	TCP_NOT_ACCEPTED	= 7,
	TCP_TIMEDOUT		= 8
};

enum ack_opt {
	ACK_OPT_NOW,
	ACK_OPT_AGGREGATE,
	ACK_OPT_WACK,
};

enum socket_type {
	NTY_TCP_SOCK_UNUSED,
	NTY_TCP_SOCK_STREAM,
	NTY_TCP_SOCK_PROXY,
	NTY_TCP_SOCK_LISTENER,
	NTY_TCP_SOCK_EPOLL,
	NTY_TCP_SOCK_PIPE,
};

typedef struct _nty_tcp_timestamp {
	uint32_t ts_val;
	uint32_t ts_ref;
} nty_tcp_timestamp;

typedef struct _nty_rtm_stat {
	uint32_t tdp_ack_cnt;
	uint32_t tdp_ack_bytes;
	uint32_t ack_upd_cnt;
	uint32_t ack_upd_bytes;
	uint32_t rto_cnt;
	uint32_t rto_bytes;
} nty_rtm_stat; //__attribute__((packed)) 


typedef struct _nty_tcp_recv {
	uint32_t rcv_wnd;
	uint32_t irs;
	uint32_t snd_wl1;
	uint32_t snd_wl2;

	uint8_t dup_acks;
	uint32_t last_ack_seq;

	uint32_t ts_recent;
	uint32_t ts_lastack_rcvd;
	uint32_t ts_last_ts_upd;
	uint32_t ts_tw_expire;

	uint32_t srtt;
	uint32_t mdev;
	uint32_t mdev_max;
	uint32_t rttvar;
	uint32_t rtt_seq;

	struct _nty_ring_buffer *recvbuf;

	TAILQ_ENTRY(_nty_tcp_stream) he_link;

#if NTY_ENABLE_BLOCKING
	TAILQ_ENTRY(_nty_tcp_stream) rcv_br_link;
	pthread_cond_t read_cond;
	pthread_mutex_t read_lock;
#else
	pthread_spinlock_t read_lock;
#endif

} nty_tcp_recv;

typedef struct _nty_tcp_send {
	uint16_t ip_id;
	uint16_t mss;
	uint16_t eff_mss;
	
	uint8_t wscale_mine;
	uint8_t wscale_peer;
	int8_t nif_out;

	unsigned char *d_haddr;
	uint32_t snd_una;
	uint32_t snd_wnd;

	uint32_t peer_wnd;
	uint32_t iss;
	uint32_t fss;

	uint8_t nrtx;
	uint8_t max_nrtx;
	uint32_t rto;
	uint32_t ts_rto;

	uint32_t cwnd;
	uint32_t ssthresh;
	uint32_t ts_lastack_sent;

	uint8_t is_wack:1,
			ack_cnt:6;

	uint8_t on_control_list;
	uint8_t on_send_list;
	uint8_t on_ack_list;
	uint8_t on_sendq;
	uint8_t on_ackq;
	uint8_t on_closeq;
	uint8_t on_resetq;

	uint8_t on_closeq_int:1,
			on_resetq_int:1,
			is_fin_sent:1,
			is_fin_ackd:1;

	TAILQ_ENTRY(_nty_tcp_stream) control_link;
	TAILQ_ENTRY(_nty_tcp_stream) send_link;
	TAILQ_ENTRY(_nty_tcp_stream) ack_link;
	TAILQ_ENTRY(_nty_tcp_stream) timer_link;
	TAILQ_ENTRY(_nty_tcp_stream) timeout_link;

	struct _nty_send_buffer *sndbuf;

#if NTY_ENABLE_BLOCKING
	TAILQ_ENTRY(_nty_tcp_stream) snd_br_link;
	pthread_cond_t write_cond;
	pthread_mutex_t write_lock;
#else
	pthread_spinlock_t write_lock;
#endif

} nty_tcp_send; //__attribute__((packed))

typedef struct _nty_tcp_stream {
	struct _nty_socket_map *socket;
	uint32_t id:24,
			 stream_type:8;

	uint32_t saddr;
	uint32_t daddr;
	
	uint16_t sport;
	uint16_t dport;

	uint8_t state;
	uint8_t close_reason;
	uint8_t on_hash_table;
	uint8_t on_timewait_list;
	
	uint8_t ht_idx;
	uint8_t closed;
	uint8_t is_bound_addr;
	uint8_t need_wnd_adv;

	int16_t on_rto_idx;
	uint16_t on_timeout_list:1,
			 on_rcv_br_list:1,
			 on_snd_br_list:1,
			 saw_timestamp:1,
			 sack_permit:1,
			 control_list_waiting:1,
			 have_reset:1;

	uint32_t last_active_ts;

	nty_tcp_recv *rcv;
	nty_tcp_send *snd;
	
	uint32_t snd_nxt;
	uint32_t rcv_nxt;

}nty_tcp_stream;

typedef struct _nty_sender {
	int ifidx;
	TAILQ_HEAD(control_head, _nty_tcp_stream) control_list;
	TAILQ_HEAD(send_head, _nty_tcp_stream) send_list;
	TAILQ_HEAD(ack_head, _nty_tcp_stream) ack_list;

	int control_list_cnt;
	int send_list_cnt;
	int ack_list_cnt;
} nty_sender; //__attribute__((packed)) 

typedef struct _nty_thread_context {
	int cpu;
	pthread_t thread;
	uint8_t done:1,
			exit:1,
			interrupt:1;

	struct _nty_tcp_manager *tcp_manager;
	void *io_private_context;

	pthread_mutex_t smap_lock;
	pthread_mutex_t flow_pool_lock;
	pthread_mutex_t socket_pool_lock;
} nty_thread_context; //__attribute__((packed))

typedef struct _nty_tcp_manager {

	struct _nty_mempool *flow;
	struct _nty_mempool *rcv;
	struct _nty_mempool *snd;
	struct _nty_mempool *mv;

	struct _nty_sb_manager *rbm_snd;
	struct _nty_rb_manager *rbm_rcv;
	
	struct _nty_hashtable *tcp_flow_table;

	uint32_t s_index;
	struct _nty_socket_map *smap;
	TAILQ_HEAD(, _nty_socket_map) free_smap;

	struct _nty_addr_pool *ap;
	uint32_t gid;
	uint32_t flow_cnt;

	nty_thread_context *ctx;

	struct _nty_epoll *ep;
	uint32_t ts_last_event;

	struct _nty_hashtable *listeners;

	struct _nty_stream_queue *connectq;
	struct _nty_stream_queue *sendq;
	struct _nty_stream_queue *ackq;

	struct _nty_stream_queue *closeq;
	struct _nty_stream_queue_int *closeq_int;

	struct _nty_stream_queue *resetq;
	struct _nty_stream_queue_int *resetq_int;

	struct _nty_stream_queue *destroyq;

	struct _nty_sender *g_sender;
	struct _nty_sender *n_sender[ETH_NUM];

	struct _nty_rto_hashstore *rto_store;
	TAILQ_HEAD(timewait_head, _nty_tcp_stream) timewait_list;
	TAILQ_HEAD(timeout_head, _nty_tcp_stream) timeout_list;

	int rto_list_cnt;
	int timewait_list_cnt;
	int timeout_list_cnt;

#if NTY_ENABLE_BLOCKING
	TAILQ_HEAD(rcv_br_head, _nty_tcp_stream) rcv_br_list;
	TAILQ_HEAD(snd_br_head, _nty_tcp_stream) snd_br_list;
	int rcv_br_list_cnt;
	int snd_br_list_cnt;
#endif

	uint32_t cur_ts;
	int wakeup_flag;
	int is_sleeping;
} nty_tcp_manager; //__attribute__((packed)) 


#include <arpa/inet.h>

typedef struct _nty_tcp_listener {
	int sockid;
	struct _nty_socket_map *socket;

	int backlog;
	struct _nty_stream_queue *acceptq;

	pthread_mutex_t accept_lock;
	pthread_cond_t accept_cond;

	TAILQ_ENTRY(_nty_tcp_listener) he_link;
} nty_tcp_listener; //__attribute__((packed)) 

typedef struct _nty_socket_map {
	int id;
	int socktype;
	uint32_t opts;

	struct sockaddr_in s_addr;

	union {
		struct _nty_tcp_stream *stream;
		nty_tcp_listener *listener;
		nty_epoll *ep;
		//struct pipe *pp;
	};

	uint32_t epoll;
	uint32_t events;
	nty_epoll_data ep_data;

	TAILQ_ENTRY(_nty_socket_map) free_smap_link;
} nty_socket_map; //__attribute__((packed)) 

uint8_t *EthernetOutput(nty_tcp_manager *tcp, uint16_t h_proto,
	int nif, unsigned char* dst_haddr, uint16_t iplen);
uint8_t *IPOutput(nty_tcp_manager *tcp, nty_tcp_stream *stream, uint16_t tcplen);


nty_tcp_stream *CreateTcpStream(nty_tcp_manager *tcp, nty_socket_map *socket, int type, 
		uint32_t saddr, uint16_t sport, uint32_t daddr, uint16_t dport);

uint8_t *IPOutputStandalone(nty_tcp_manager *tcp, uint8_t protocol, 
		uint16_t ip_id, uint32_t saddr, uint32_t daddr, uint16_t payloadlen);


int nty_tcp_init_manager(nty_thread_context *ctx);
void nty_tcp_init_thread_context(nty_thread_context *ctx);


void RaiseReadEvent(nty_tcp_manager *tcp, nty_tcp_stream *stream);
void RaiseWriteEvent(nty_tcp_manager *tcp, nty_tcp_stream *stream);
void RaiseCloseEvent(nty_tcp_manager *tcp, nty_tcp_stream *stream);
void RaiseErrorEvent(nty_tcp_manager *tcp, nty_tcp_stream *stream);



#endif


