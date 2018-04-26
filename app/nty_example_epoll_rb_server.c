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



#include "nty_api.h"

#include "nty_epoll.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>

#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <sys/socket.h>
#include <sys/types.h>

#if (NTY_ENABLE_EPOLL_RB == 0)
#err "Open NTY_ENABLE_EPOLL_RB"
#endif

#define EPOLL_SIZE		5
#define BUFFER_LENGTH	1024

int main() {

	nty_tcp_setup();

	usleep(1);

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("socket");
		return 1;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(9096);
	addr.sin_addr.s_addr = INADDR_ANY;

	if(bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0) {
		perror("bind");
		return 2;
	}

	if (listen(sockfd, 5) < 0) {
		return 3;
	}

	int epoll_fd = epoll_create(EPOLL_SIZE);
	struct epoll_event ev, events[EPOLL_SIZE];

	ev.events = NTY_EPOLLIN;
	ev.data.fd = sockfd;
	epoll_ctl(epoll_fd, NTY_EPOLL_CTL_ADD, sockfd, &ev);

	while (1) {

		int nready = epoll_wait(epoll_fd, events, EPOLL_SIZE, -1);
		if (nready == -1) {
			printf("epoll_wait\n");
			break;
		} 

		printf("nready : %d\n", nready);

		int i = 0;
		for (i = 0;i < nready;i ++) {

			if (events[i].data.fd == sockfd) {

				struct sockaddr_in client_addr;
				memset(&client_addr, 0, sizeof(struct sockaddr_in));
				socklen_t client_len = sizeof(client_addr);
			
				int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
				if (clientfd <= 0) continue;
	
				char str[INET_ADDRSTRLEN] = {0};
				printf("recvived from %s at port %d, sockfd:%d, clientfd:%d\n", inet_ntop(AF_INET, &client_addr.sin_addr, str, sizeof(str)),
					ntohs(client_addr.sin_port), sockfd, clientfd);

				ev.events = NTY_EPOLLIN | NTY_EPOLLET;
				ev.data.fd = clientfd;
				epoll_ctl(epoll_fd, NTY_EPOLL_CTL_ADD, clientfd, &ev); 
				
			} else {

				int clientfd = events[i].data.fd;

				char buffer[BUFFER_LENGTH] = {0};
				int ret = recv(clientfd, buffer, BUFFER_LENGTH, 0);
				if (ret < 0) {
					if (errno == EAGAIN || errno == EWOULDBLOCK) {
						printf("read all data");
					}
					
					ev.events = NTY_EPOLLIN | NTY_EPOLLET;
					ev.data.fd = clientfd;
					epoll_ctl(epoll_fd, NTY_EPOLL_CTL_DEL, clientfd, &ev);
					
					close(clientfd);
				} else if (ret == 0) {
					printf(" disconnect %d\n", clientfd);
					
					ev.events = NTY_EPOLLIN | NTY_EPOLLET;
					ev.data.fd = clientfd;
					epoll_ctl(epoll_fd, NTY_EPOLL_CTL_DEL, clientfd, &ev);

					close(clientfd);

					printf(" delete clientfd %d\n", clientfd);
					
					break;
				} else {
					printf("Recv: %s, %d Bytes\n", buffer, ret);
					send(clientfd, buffer, ret, 0);
				}

			}
		
		}

	}

	return 0;
}









