// 例子：求两数之和

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLINE 1024

int main() {
    char *buf, *p;
    char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
    int n1 = 0, n2 = 0;
    
    if((buf = getenv("QUERY_STRING")) != NULL) {
        p = strchr(buf, '&');
        *p = '\0';
        strcpy(arg1, buf);
        strcpy(arg2, p + 1);
        n1 = atoi(arg1);
        n2 = atoi(arg2);
    }
    
    sprintf(content, "<p>%d + %d = %d</p>", n1, n2, n1 + n2);
    
    printf("Content-length: %d\r\n", strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    printf("%s", content);
    
    fflush(stdout);
    
    exit(0);
}
