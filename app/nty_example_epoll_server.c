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


#define EPOLL_SIZE		1024
#define BUFFER_LENGTH	1024

int main() {

	nty_tcp_setup();

	usleep(1);

	int sockfd = nty_socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("socket");
		return 1;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(9096);
	addr.sin_addr.s_addr = INADDR_ANY;

	if(nty_bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0) {
		perror("bind");
		return 2;
	}

	if (nty_listen(sockfd, 5) < 0) {
		return 3;
	}

	int epoll_fd = nty_epoll_create(EPOLL_SIZE);
	nty_epoll_event ev, events[EPOLL_SIZE] = {0};

	ev.events = sockfd;
	ev.data = (uint64_t)sockfd;
	nty_epoll_ctl(epoll_fd, NTY_EPOLL_CTL_ADD, sockfd, &ev);

	while (1) {

		int nready = nty_epoll_wait(epoll_fd, events, EPOLL_SIZE, -1);
		if (nready == -1) {
			printf("epoll_wait\n");
			break;
		} 

		printf("nready : %d\n", nready);

		int i = 0;
		for (i = 0;i < nready;i ++) {

			if (events[i].data == (uint64_t)sockfd) {

				struct sockaddr_in client_addr;
				memset(&client_addr, 0, sizeof(struct sockaddr_in));
				socklen_t client_len = sizeof(client_addr);
			
				int clientfd = nty_accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
				if (clientfd <= 0) continue;
	
				char str[INET_ADDRSTRLEN] = {0};
				printf("recvived from %s at port %d, sockfd:%d, clientfd:%d\n", inet_ntop(AF_INET, &client_addr.sin_addr, str, sizeof(str)),
					ntohs(client_addr.sin_port), sockfd, clientfd);

				ev.events = NTY_EPOLLIN | NTY_EPOLLET;
				ev.data = clientfd;
				nty_epoll_ctl(epoll_fd, NTY_EPOLL_CTL_ADD, clientfd, &ev); 
				
			} else {

				int clientfd = (int)events[i].data;

				char buffer[BUFFER_LENGTH] = {0};
				int ret = nty_recv(clientfd, buffer, BUFFER_LENGTH, 0);
				if (ret < 0) {
					if (errno == EAGAIN || errno == EWOULDBLOCK) {
						printf("read all data");
					}
					
					nty_close(clientfd);
					
					ev.events = NTY_EPOLLIN | NTY_EPOLLET;
					ev.data = clientfd;
					nty_epoll_ctl(epoll_fd, NTY_EPOLL_CTL_DEL, clientfd, &ev);
				} else if (ret == 0) {
					printf(" disconnect %d\n", clientfd);
					
					close(clientfd);

					ev.events = NTY_EPOLLIN | NTY_EPOLLET;
					ev.data = clientfd;
					nty_epoll_ctl(epoll_fd, NTY_EPOLL_CTL_DEL, clientfd, &ev);
					
					break;
				} else {
					printf("Recv: %s, %d Bytes\n", buffer, ret);
				}

			}
		
		}

	}

	return 0;
}





