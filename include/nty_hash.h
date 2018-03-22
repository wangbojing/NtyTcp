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



#ifndef __NTY_HASH_H__
#define __NTY_HASH_H__


#include <stdint.h>

#include "nty_queue.h"
//#include "nty_tcp.h"


#define NUM_BINS_FLOWS		131072
#define NUM_BINS_LISTENERS	1024
#define TCP_AR_CNT			3

#if 0
typedef struct hash_bucket_head {
	struct _nty_tcp_stream *tqh_first;
	struct _nty_tcp_stream **tqh_last;
} hash_buchet_head;
#else

#define HASH_BUCKET_ENTRY(type)	\
	struct {					\
		struct type *tqh_first;	\
		struct type **tqh_last;	\
	}

#endif

typedef HASH_BUCKET_ENTRY(_nty_tcp_stream) hash_bucket_head;
typedef HASH_BUCKET_ENTRY(_nty_tcp_listener) list_bucket_head;

typedef struct _nty_hashtable {
	uint8_t ht_count;
	uint32_t bins;
#if 0
	union {
		TAILQ_ENTRY(_nty_tcp_stream) *ht_stream;
		TAILQ_ENTRY(_nty_tcp_listener) *ht_listener;
	};
#else
	union {
		hash_bucket_head *ht_stream;
		list_bucket_head *ht_listener;
	};
#endif
	unsigned int (*hashfn)(const void *);
	int (*eqfn)(const void *, const void *);
} nty_hashtable;


void *ListenerHTSearch(nty_hashtable *ht, const void *it);
void *StreamHTSearch(nty_hashtable *ht, const void *it);
int ListenerHTInsert(nty_hashtable *ht, void *it);
int StreamHTInsert(nty_hashtable *ht, void *it);
void *StreamHTRemove(nty_hashtable *ht, void *it);


unsigned int HashFlow(const void *f);
int EqualFlow(const void *f1, const void *f2);
unsigned int HashListener(const void *l);
int EqualListener(const void *l1, const void *l2);

nty_hashtable *CreateHashtable(unsigned int (*hashfn) (const void *), // key function
		int (*eqfn) (const void*, const void *),            // equality
		int bins);
void DestroyHashtable(nty_hashtable *ht);




#endif


