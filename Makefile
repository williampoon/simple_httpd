CC = gcc

all: server cgi

server: app.c simple_httpd.c
	$(CC) app.c simple_httpd.c -o server

cgi:
	(cd cgi-bin; make)

clean:
	rm -f server
	(cd cgi-bin; make clean)
