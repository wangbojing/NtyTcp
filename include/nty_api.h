


#ifndef __NTY_API_H__
#define __NTY_API_H__


#include <sys/types.h>
#include <sys/socket.h>

int nty_socket(int domain, int type, int protocol);
int nty_bind(int sockid, const struct sockaddr *addr, socklen_t addrlen);
int nty_listen(int sockid, int backlog);
int nty_accept(int sockid, struct sockaddr *addr, socklen_t *addrlen);
ssize_t nty_recv(int sockid, char *buf, size_t len, int flags);
ssize_t nty_send(int sockid, const char *buf, size_t len);
int nty_close(int sockid);

void nty_tcp_setup(void);


#endif




