#include "app.h"

/* Unix风格错误处理函数 */
void unix_error(char *msg) {
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}

/************************************
 * Unix进程控制相关的包裹函数
 ***********************************/
pid_t Fork(void) {
    pid_t pid;
    
    if((pid = fork()) < 0)
        unix_error("Fork error");
    
    return pid;
}

void Execve(const char *filename, char *const argv[], char *const envp[]) {
    if(execve(filename, argv, envp) < 0)
        unix_error("Execve error");
}

pid_t Wait(int *status) {
    pid_t pid;
    
    if((pid = wait(status)) < 0)
        unix_error("Wait error");
    
    return pid;
}

/************************************
 * Unix IO相关的包裹函数
 ***********************************/
int Open(const char *pathname, int flags, mode_t mode) {
    int rc;
    
    if((rc = open(pathname, flags, mode)) < 0)
        unix_error("Open error");
    
    return rc;
}

void Close(int fd) {
    int rc;
    
    if((rc = close(fd)) < 0)
        unix_error("Close error");
    
    return ;
}

int Dup2(int fd1, int fd2) {
    int rc;
    
    if((rc = dup2(fd1, fd2)) < 0)
        unix_error("Dup2 error");
    
    return rc;
}

/************************************
 * 内存映射函数的包裹函数
 ***********************************/
void *Mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset) {
    void *ptr;
    
    if((ptr = mmap(addr, len, prot, flags, fd, offset)) == (void *) -1)
        unix_error("mmap error");
    
    return ptr;
}

void Munmap(void *start, size_t length) {
    if(munmap(start, length) < 0)
        unix_error("munmap error");
    
    return ;
}

/************************************
 * Sockets接口相关的包裹函数
 ***********************************/
int Socket(int domain, int type, int protocol) {
    int rc;
    
    if((rc = socket(domain, type, protocol)) < 0)
        unix_error("Socket error");
    
    return rc;
}

void Setsockopt(int s, int level, int optname, const void *optval, int optlen) {
    int rc;
    
    if((rc = setsockopt(s, level, optname, optval, optlen)) < 0)
        unix_error("Setsockopt error");
    
    return ;
}

void Bind(int sockfd, struct sockaddr *my_addr, int addrlen) {
    int rc;
    
    if((rc = bind(sockfd, my_addr, addrlen)) < 0)
        unix_error("Bind error");
    
    return ;
}

void Listen(int s, int backlog) {
    int rc;
    
    if((rc = listen(s, backlog)) < 0)
        unix_error("Listen error");
    
    return ;
}

int Accept(int s, struct sockaddr *addr, socklen_t *addrlen) {
    int rc;
    
    if((rc = accept(s, addr, addrlen)) < 0)
        unix_error("Accept error");
    
    return rc;
}

/************************************
 * Rio包 - 健壮的IO函数
 ***********************************/
/*
 * 健壮地读n个字节
 */
ssize_t rio_writen(int fd, void *usrbuf, size_t n) {
    size_t  nleft = n;
    ssize_t nwritten;
    char    *bufp = usrbuf;
    
    while(nleft > 0) {
        if((nwritten = write(fd, bufp, nleft)) <= 0) {
            if(errno == EINTR)  //被信号处理程序中断
                nwritten = 0;   //再次调用write()
            else
                return -1;      //write()设置errno
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
    
    return n;
}


/*
 * Unix read()函数的包裹函数
 */
ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n) {
    int cnt;
    
    while(rp->rio_cnt <= 0) {
        rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));
        if(rp->rio_cnt < 0) {
            if(errno != EINTR)  //被信号处理程序中断
                return -1;
        }
        else if(rp->rio_cnt == 0)   //EOF
            return 0;
        else
            rp->rio_bufptr = rp->rio_buf;
    }
    
    cnt = n;
    if(rp->rio_cnt < n)
        cnt = rp->rio_cnt;
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    
    return cnt;
}

/*
 * 关联描述符到一个缓冲
 */
void rio_readinitb(rio_t *rp, int fd) {
    rp->rio_fd = fd;
    rp->rio_cnt = 0;
    rp->rio_bufptr = rp->rio_buf;
}

/*
 * 健壮读一行文本
 */
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) {
    int n, rc;
    char c, *bufp = usrbuf;
    
    for(n = 1; n < maxlen; n++) {
        if((rc = rio_read(rp, &c, 1)) == 1) {
            *bufp++ = c;
            if(c == '\n')
                break;
        }
        else if(rc == 0) {
            if(n == 1)
                return 0;   /* EOF, 没读到数据 */
            else
                break;      /* EOF, 读取了部分数据 */
        }
        else {
            return -1;
        }
    }
    *bufp = 0;
    return n;
}

/************************************
 * Rio函数的包裹函数
 ***********************************/
void Rio_writen(int fd, void *usrbuf, size_t n) {
    if(rio_writen(fd, usrbuf, n) != n)
        unix_error("Rio_writen error");
    
    return ;
}

void Rio_readinitb(rio_t *rp, int fd) {
    rio_readinitb(rp, fd);
}

ssize_t Rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) {
    ssize_t rc;
    
    if((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0)
        unix_error("Rio_readlineb error");
    
    return rc;
}

/************************************
 * 辅助函数
 ***********************************/
/*  
 * 在指定端口上打开并返回一个监听socket，出错时返回-1并设置errno 
 */
int open_listenfd(int port) {
    int listenfd, optval = 1;
    struct sockaddr_in serveraddr;
    
    /* 创建一个socket描述符 */
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;
    
    /* 排除在调用bind()函数时返回的"Address already in use"错误 */
    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) < 0)
        return -1;
    
    /* listenfd绑定指定端口 */
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);
    if(bind(listenfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;
    
    /* 作为监听socket等待连接请求 */
    if(listen(listenfd, LISTENQ) < 0)
        return -1;
    
    return listenfd;
}

/*  
 * open_listenfd的包裹函数
 */
int Open_listenfd(int port) {
    int rc;
    
    if((rc = open_listenfd(port)) < 0)
        unix_error("Open_listenfd error");
    
    return rc;
}
