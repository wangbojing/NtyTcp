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



#ifndef __NTY_MEMPOOL_H__
#define __NTY_MEMPOOL_H__


enum {
	MEM_NORMAL,
	MEM_HUGEPAGE
};


typedef struct _nty_mem_chunk {
	int mc_free_chunks;
	struct _nty_mem_chunk *next;
} nty_mem_chunk;

typedef struct _nty_mempool {
	u_char *mp_startptr;
	nty_mem_chunk *mp_freeptr;
	int mp_free_chunks;
	int mp_total_chunks;
	int mp_chunk_size;
	int mp_type;
} nty_mempool;


nty_mempool *nty_mempool_create(int chunk_size, size_t total_size, int is_hugepage);

void nty_mempool_destory(nty_mempool *mp);

void *nty_mempool_alloc(nty_mempool *mp);

void nty_mempool_free(nty_mempool *mp, void *p);






#endif



