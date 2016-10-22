/*******************************************************
 * Copyright (C) 2014-2016 robberpen robberpen@hotmail.com
 * 
 * You can not be copied and/or distributed without the express
 * permission of robberpen
 *******************************************************/
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "inetsocket.h"
#define __DBG(fmt, a...)	fprintf(stderr, "%s %d: "fmt, __FUNCTION__, __LINE__, ##a)
#define DOC_ROOT	"/www"

int http_recv(int s, char *buf, int len)
{
	return recv(s, buf, len, 0);
}
int http_send(int s, const char *buf, int len)
{
	return send(s, buf, len, 0);
}
int http_puts(int s, const char *buf)
{
	return http_send(s, buf, strlen(buf));
}
/*
 * parser GET url from @buf eg: "GET /test.html HTTP/1.1"
 * */
static char *parser_http_get_url(char *buf, char *path)
{
	char *p, *e;
	p = buf + 4;		// @buf = "GET /test.html HTTP/1.1"
	e = strchr(p, ' ');
	*e = '\0';
	strcpy(path, p);	// @path = "/test.html"
	*e = ' ';
	return path;
}
static void http_get(int s, char *path)
{
	int fd, len;
	char buf[4096];

	if (strcmp(path, "/") == 0)
		strcpy(path, "index.html");
	if (*path == '/')
		path++;
	if ((fd = open(path, O_RDONLY)) == -1)
		goto not_found;
	
	http_puts(s, "HTTP/1.1 200 OK\r\n");
	http_puts(s, "Connection: close\r\n");
	http_puts(s, "Content-Type: text/html\r\n\r\n");
	//sprintf(buf, "Content-Length: %d\r\n\r\n", 17);
	//http_puts(s, buf);
	while ((len = read(fd, buf, sizeof(buf))) > 0) {
		http_send(s, buf, len);
	}
	close(fd);
	return;
not_found:
	http_puts(s, "HTTP/1.1 404 Not Found\r\n\r\n");
}
static void process_http(int s)
{
	char buf[1024];	//should check socket buffer by getsockopt(2)
	char *p, *line;
	char path[256];
	int method_get, len;

	bzero(buf, sizeof(buf));
	/* read GET or POST */
	len = http_recv(s, buf, sizeof(buf));
	
	__DBG("recv: %d\n", len);
	if (strncmp(buf, "GET", 3) == 0) {
		method_get = 1;
		parser_http_get_url(buf, path);
	} else if (strncmp(buf, "POST", 4) == 0)
		method_get = 0;
	else
		goto method_unsupport;
	//__DBG("get: [%s], PATH: %s\n", buf, path);
	for (p = buf; p && (line = strsep(&p, "\n")); ) {
		__DBG("Header: %s\n", line);
		
	}
	http_get(s, path);
	return;
method_unsupport:
	http_puts(s, "405 Method Not Allowed\r\n\r\n");
}

static void main_loop(int server_socket_fd)
{
	int client_socket_fd;
	struct sockaddr_in client_sock_addr_in;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	char buf[24];

	for (;;) {
		bzero(&client_sock_addr_in, sizeof(client_sock_addr_in));
		printf("%s: %d: waitting connect\n", __FUNCTION__, __LINE__);
		client_socket_fd = accept(server_socket_fd, (struct  sockaddr*)&client_sock_addr_in, &addrlen);
		
		printf("%s: %d: connected from: %s\n", __FUNCTION__, __LINE__,  inet_ntop(AF_INET, &client_sock_addr_in.sin_addr, buf, 24));
#ifdef HTTP_USE_FORK
		if (fork() == 0) {
			close(server_socket_fd);
			process_http(client_socket_fd);
			shutdown(client_socket_fd, SHUT_RDWR);
			exit(0);
		} else {
			/* XXX: don't do that! use close(2) instead of shutdown(2) */
			//shutdown(client_socket_fd, SHUT_RDWR);
			close(client_socket_fd);
		}
#else
		process_http(client_socket_fd);
		/* XXX: don't do that! use close(2) instead of shutdown(2) */
		//shutdown(client_socket_fd, SHUT_RDWR);
		close(client_socket_fd);
#endif
	}
}

int main(int argc, char *argv[])
{
	struct sockaddr_in saddrin;
	struct in_addr inp;
	int rev = -1, src;
	int optval = 1;
	unsigned short int port = 8080;

	if (chdir(DOC_ROOT) == -1) {
		fprintf(stderr, "can not chdir(%s) error: %s", DOC_ROOT, strerror(errno));
		return 0;
	}
	bzero(&saddrin, sizeof(struct sockaddr_in));
        if ((src = InetSockSrvInit("0.0.0.0", port, SOCK_STREAM ,&saddrin)) < 0)
                return EXIT_FAILURE;
	
	listen(src, 5);
	main_loop(src);
	close(src);
	return 0;
}
