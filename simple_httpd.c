#include "app.h"

void    doit(int fd);
void    read_requesthdrs(rio_t *rp);
int     parse_uri(char *uri, char *filename, char *cgiargs);
void    get_filetype(char *filename, char *filetype);
void    serve_static(int fd, char *filename, int filesize);
void    serve_dynamic(int fd, char *filename, char *cgiargs);
void    clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv) {
    int listenfd, connfd, port;
    struct sockaddr_in clientaddr;
    socklen_t clientlen = sizeof(clientaddr);
    
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    port = atoi(argv[1]);
    
    listenfd = Open_listenfd(port);
    while(1) {
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        if (Fork() == 0) {      // 子进程
            Close(listenfd);    // 关闭自己的监听socket
            doit(connfd);       // 处理请求
            Close(connfd);      // 关闭客户端连接
            exit(0);
        }
        Close(connfd);  // 父进程关闭连接socket
    }
}

/*
 * 处理一个HTTP请求
 */
void doit(int fd) {
    int         is_static;
    struct stat sbuf;
    char        buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char        filename[MAXLINE], cgiargs[MAXLINE];
    rio_t       rio;
    
    /* 读取请求行和请求头部 */
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);
    if(strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not Implemented", "Simple does not implment this method");
        return ;
    }
    read_requesthdrs(&rio);
    
    /* 从GET请求中解析URI */
    is_static = parse_uri(uri, filename, cgiargs);
    if(stat(filename, &sbuf) < 0) {
        clienterror(fd, filename, "404", "Not found", "Simple couldn't find this file");
        return ;
    }
    
    if(is_static) { /* 处理静态内容 */
        if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR && sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden", "Simple couldn't read the file");
            return ;
        }
        serve_static(fd, filename, sbuf.st_size);
    }
    else { /* 处理动态内容 */
        if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR && sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden", "Simple couldn't run the CGI program");
            return ;
        }
        serve_dynamic(fd, filename, cgiargs);
    }
}

/*
 * 读取并解析HTTP请求头部
 */
void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];
    
    do {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);   // 忽略请求头并打印
    } while(strcmp(buf, "\r\n"));
    
    return ;
}

/*
 * 解析URI，静态请求返回1，动态请求返回0
 */
int parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;
    
    if(!strstr(uri, "cgi-bin")) {   /* 静态内容 */
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        if(uri[strlen(uri) - 1] == '/')
            strcat(filename, "index.html");
        return 1;
    }
    else { /*动态内容*/
        ptr = index(uri, '?');
        if(ptr) {
            strcpy(cgiargs, ptr + 1);
            *ptr = '\0';
        }
        else {
            strcpy(cgiargs, "");
        }
        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }
}

/*
 * 复制一个文件返回给客户端
 */
void serve_static(int fd, char *filename, int filesize) {
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];
    
    /* 发送响应头部给客户端 */
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Simple Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));
    
    /* 发送响应体给客户端 */
    srcfd = Open(filename, O_RDONLY, 0);
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
}

/*
 * 通过文件后缀获取文件类型
 */
void get_filetype(char *filename, char *filetype) {
    if(strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if(strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if(strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}

/*
 * 代替客户端运行一个CGI程序
 */
void serve_dynamic(int fd, char *filename, char *cgiargs) {
    char buf[MAXLINE], *emptylist[] = {NULL};
    
     /* 发送HTTP响应头部 */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Simple Web Server\r\n", buf);
    Rio_writen(fd, buf, strlen(buf));
    
    if(Fork() == 0) {   //子进程
        //可以添加其他CGI变量
        setenv("QUERY_STRING", cgiargs, 1);
        Dup2(fd, STDOUT_FILENO);    //重定向标准输出到客户端
        Execve(filename, emptylist, environ);   //执行CGI程序
    }
    Wait(NULL); //父进程等待并回收子进程，避免子进程变僵尸进程
}

/*
 * 返回错误信息给客户端
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];
    
    /* 发送HTTP响应体 */
    sprintf(body, "<html><title>Simple Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Simple Web server</em>\r\n", body);

    /* 发送HTTP响应头部 */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    sprintf(buf, "%sContent-type: text/html\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n\r\n", buf, strlen(body));

    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
