#ifndef __SIMPLE_H__
#define __SIMPLE_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* 简化bind(), connect(), accept()函数的调用 */
typedef struct sockaddr SA;

/* 内部缓冲结构 */
#define RIO_BUFSIZE 4096
typedef struct {
    int     rio_fd;                 /* 内部缓冲描述符 */
    int     rio_cnt;                /* 内部缓冲里的未读字节数 */
    char    *rio_bufptr;            /* 内部缓冲里的下一个未读字节指针 */
    char    rio_buf[RIO_BUFSIZE];   /* 内部缓冲 */
}rio_t;

extern char **environ;  /* 定义于libc */

#define MAXLINE     4096    /* 每个文本行的最大长度 */
#define MAXBUF      4096    /* IO缓冲的最大值 */
#define LISTENQ     1024    /* listen()函数的第二个参数 */

/* 错误处理函数 */
void unix_error(char *msg);

/* 进程控制相关的包裹函数 */
pid_t           Fork(void);
void            Execve(const char *filename, char *const argv[], char *const envp[]);
pid_t           Wait(int *status);

/* Unix I/O wrappers */
int     Open(const char *pathname, int flags, mode_t mode);
void    Close(int fd);
int     Dup2(int fd1, int fd2);

/* Memory mapping wrappers */
void *Mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
void Munmap(void *start, size_t length);

/* Sockets接口相关的包裹函数*/
int     Socket(int domain, int type, int protocol);
void    Setsockopt(int s, int level, int optname, const void *optval, int optlen);
void    Bind(int sockfd, struct sockaddr *my_addr, int addrlen);
void    Listen(int s, int backlog);
int     Accept(int s, struct sockaddr *addr, socklen_t *addrlen);

/* 健壮的IO函数 */
void    Rio_writen(int fd, void *usrbuf, size_t n);
void    Rio_readinitb(rio_t *rp, int fd);
ssize_t Rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);

/* Server相关的包裹函数 */
int open_listenfd(int port);
int Open_listenfd(int port);

#endif
