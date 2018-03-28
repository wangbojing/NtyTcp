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




#include "nty_hash.h"
#include "nty_tcp.h"

unsigned int HashFlow(const void *f) {
	nty_tcp_stream *flow = (nty_tcp_stream*)f;

	unsigned int hash, i;
	char *key = (char *)&flow->saddr;

	for (hash = i = 0;i < 12;i ++) {
		hash += key[i];
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return hash & (NUM_BINS_FLOWS-1);
}

int EqualFlow(const void *f1, const void *f2) {
	nty_tcp_stream *flow1 = (nty_tcp_stream*)f1;
	nty_tcp_stream *flow2 = (nty_tcp_stream*)f2;

	return (flow1->saddr == flow2->saddr &&
			flow1->sport == flow2->sport &&
		    flow1->daddr == flow2->daddr &&
		    flow1->dport == flow2->dport);
}

unsigned int HashListener(const void *l) {
	nty_tcp_listener *listener = (nty_tcp_listener*)l;

	return listener->s->s_addr.sin_port & (NUM_BINS_LISTENERS - 1);
}

int EqualListener(const void *l1, const void *l2) {
	nty_tcp_listener *listener1 = (nty_tcp_listener*)l1;
	nty_tcp_listener *listener2 = (nty_tcp_listener*)l2;

	return (listener1->s->s_addr.sin_port == listener2->s->s_addr.sin_port);
}


#define IS_FLOW_TABLE(x) 	(x == HashFlow)
#define IS_LISTEN_TABLE(x)	(x == HashListener)


nty_hashtable *CreateHashtable(unsigned int (*hashfn) (const void *), // key function
		int (*eqfn) (const void*, const void *),            // equality
		int bins) // no of bins
{
	int i;
	nty_hashtable* ht = calloc(1, sizeof(nty_hashtable));
	if (!ht){
		printf("calloc: CreateHashtable");
		return 0;
	}

	ht->hashfn = hashfn;
	ht->eqfn = eqfn;
	ht->bins = bins;

	/* creating bins */
	if (IS_FLOW_TABLE(hashfn)) {
		ht->ht_stream = calloc(bins, sizeof(hash_bucket_head));
		if (!ht->ht_stream) {
			printf("calloc: CreateHashtable bins!\n");
			free(ht);
			return 0;
		}
		/* init the tables */
		for (i = 0; i < bins; i++)
			TAILQ_INIT(&ht->ht_stream[i]);
	} else if (IS_LISTEN_TABLE(hashfn)) {
		ht->ht_listener = calloc(bins, sizeof(list_bucket_head));
		if (!ht->ht_listener) {
			printf("calloc: CreateHashtable bins!\n");
			free(ht);
			return 0;
		}
		/* init the tables */
		for (i = 0; i < bins; i++)
			TAILQ_INIT(&ht->ht_listener[i]);
	}

	return ht;
}


void DestroyHashtable(nty_hashtable *ht) {
	if (IS_FLOW_TABLE(ht->hashfn)) {
		free(ht->ht_stream);
	} else {
		free(ht->ht_listener);
	}
	free(ht);
}

/*----------------------------------------------------------------------------*/
int StreamHTInsert(nty_hashtable *ht, void *it)
{
	/* create an entry*/ 
	int idx;
	nty_tcp_stream *item = (nty_tcp_stream*)it;

	assert(ht);
	
	idx = ht->hashfn(item);
	assert(idx >=0 && idx < NUM_BINS_FLOWS);

	TAILQ_INSERT_TAIL(&ht->ht_stream[idx], item, rcv->he_link);

	item->ht_idx = TCP_AR_CNT;
	ht->ht_count++;
	
	return 0;
}
/*----------------------------------------------------------------------------*/
void *StreamHTRemove(nty_hashtable *ht, void *it)
{
	hash_bucket_head *head;
	struct _nty_tcp_stream *item = (struct _nty_tcp_stream *)it;
	int idx = ht->hashfn(item);

	head = &ht->ht_stream[idx];
	TAILQ_REMOVE(head, item, rcv->he_link);	

	ht->ht_count--;
	return (item);
}	
/*----------------------------------------------------------------------------*/
void *StreamHTSearch(nty_hashtable *ht, const void *it)
{
	int idx;
	const nty_tcp_stream *item = (const nty_tcp_stream *)it;
	nty_tcp_stream *walk;
	hash_bucket_head *head;

	idx = ht->hashfn(item);

	head = &ht->ht_stream[idx];
	TAILQ_FOREACH(walk, head, rcv->he_link) {
		if (ht->eqfn(walk, item)) 
			return walk;
	}

	return NULL;
}

int ListenerHTInsert(nty_hashtable *ht, void *it)
{
	/* create an entry*/ 
	int idx;
	struct _nty_tcp_listener *item = (struct _nty_tcp_listener *)it;

	assert(ht);
	
	idx = ht->hashfn(item);
	assert(idx >=0 && idx < NUM_BINS_LISTENERS);

	TAILQ_INSERT_TAIL(&ht->ht_listener[idx], item, he_link);
	ht->ht_count++;
	
	return 0;
}
/*----------------------------------------------------------------------------*/
void * 
ListenerHTRemove(nty_hashtable *ht, void *it)
{
	list_bucket_head *head;
	struct _nty_tcp_listener *item = (struct _nty_tcp_listener *)it;
	int idx = ht->hashfn(item);

	head = &ht->ht_listener[idx];
	TAILQ_REMOVE(head, item, he_link);	

	ht->ht_count--;
	return (item);
}	
/*----------------------------------------------------------------------------*/
void * 
ListenerHTSearch(nty_hashtable *ht, const void *it)
{
	int idx;
	nty_tcp_listener item;
	uint16_t port = *((uint16_t *)it);
	nty_tcp_listener *walk;
	list_bucket_head *head;

	struct _nty_socket s;

	s.s_addr.sin_port = port;
	item.s = &s;

	idx = ht->hashfn(&item);

	head = &ht->ht_listener[idx];
	TAILQ_FOREACH(walk, head, he_link) {
		if (ht->eqfn(walk, &item)) 
			return walk;
	}

	return NULL;
}





