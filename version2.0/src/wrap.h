#ifndef __WRAP_H__
#define __WRAP_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

void perr_exit(const char *s);

int Socket(int family, int type, int protocol);

int Bind(int fd, const struct sockaddr *sa, socklen_t salen);

int Accept(int fd, struct sockaddr *sa, socklen_t *salen);

int Listen(int fd, int backlog);

int Connect(int fd, const struct sockaddr *sa, socklen_t salen);

ssize_t Read(int fd, void *ptr, size_t nbytes);

ssize_t Write(int fd, const void *ptr, size_t nbytes);

int Close(int fd);

//实现fread函数的功能
//读特定长度字节
ssize_t Readn(int fd, void *vptr, size_t n);

//实现fwrite函数的功能
//写特定长度字节
ssize_t Writen(int fd, const void *vptr, size_t n);

//设置信号处理函数
void addsig(int sig, void(handler)(int), bool restart = true);

//将文件描述符设置成非阻塞
int setSocketNonBlocking(int fd);

//
static ssize_t my_read(int fd, char *ptr);

//fget()函数的功能
ssize_t Readline(int fd, void *vptr, size_t maxlen);


#endif