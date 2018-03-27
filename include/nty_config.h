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

#ifndef __NTY_CONFIG_H__
#define __NTY_CONFIG_H__



#define NTY_SELF_IP		"192.168.1.147"//"192.168.1.108" //"192.168.1.132" //"192.168.1.131"  //
#define NTY_SELF_IP_HEX	0x9301A8C0 //0x8301A8C0 //
#define NTY_SELF_MAC	"00:0c:29:58:6f:f4"

#define NTY_MAX_CONCURRENCY		1024
#define NTY_SNDBUF_SIZE			8192
#define NTY_RCVBUF_SIZE			8192
#define NTY_MAX_NUM_BUFFERS		1024
#define NTY_BACKLOG_SIZE		1024

#define NTY_ENABLE_MULTI_NIC	0
#define NTY_ENABLE_BLOCKING		1

#define NTY_ENABLE_EPOLL_RB		1
#define NTY_ENABLE_SOCKET_C10M	1
#define NTY_ENABLE_POSIX_API	1


#define NTY_SOCKFD_NR			(1024*1024)
#define NTY_BITS_PER_BYTE		8

//#define NTY_DEBUG 1
#ifdef NTY_DEBUG
#define ntydbg(format, ...) 		fprintf(stdout, format, ##__VA_ARGS__)
#define nty_trace_api(format, ...) 		fprintf(stdout, format, ##__VA_ARGS__)
#define nty_trace_tcp(format, ...) 		fprintf(stdout, format, ##__VA_ARGS__)
#define nty_trace_buffer(format, ...) 		fprintf(stdout, format, ##__VA_ARGS__)
#define nty_trace_eth(format, ...) 		fprintf(stdout, format, ##__VA_ARGS__)
#define nty_trace_ip(format, ...) 		fprintf(stdout, format, ##__VA_ARGS__)
#define nty_trace_timer(format, ...) 		fprintf(stdout, format, ##__VA_ARGS__)
#define nty_trace_epoll(format, ...)	fprintf(stdout, format, ##__VA_ARGS__)
#define nty_trace_socket(format, ...)	fprintf(stdout, format, ##__VA_ARGS__)

#else
#define ntydbg(format, ...) 
#define nty_trace_api(format, ...)
#define nty_trace_tcp(format, ...) 
#define nty_trace_buffer(format, ...)
#define nty_trace_eth(format, ...)
#define nty_trace_ip(format, ...)
#define nty_trace_timer(format, ...)
#define nty_trace_epoll(format, ...)
#define nty_trace_socket(format, ...)


#endif




#endif



