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
#define ntydbg(format, ...) 		fprintf(stdout, " File:"__FILE__",line:%05d:"format, __LINE__, ##__VA_ARGS__)
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

#define UNUSED(expr)	do {(void)(expr); } while(0)


#endif



