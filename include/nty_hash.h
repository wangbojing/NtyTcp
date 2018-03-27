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


