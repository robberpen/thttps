/*******************************************************
 * Copyright (C) 2014-2016 robberpen robberpen@hotmail.com
 * 
 * You can not be copied and/or distributed without the express
 * permission of robberpen
 *
 * This is a HTTP server demo code to support CGI
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
#define HTTP_USE_FORK 1
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
 * fgets(3) like, but return number characters read from @s
 *   and strip '\r'
 **/
static int http_gets(int s, char *buf, size_t len)
{
	unsigned char c;
	int n = 0;
	while (n < len && http_recv(s, &c, 1)  == 1) {
		switch (c) {
		case '\r':
			//buf[n++] = c;
			break;		//strip '\r'
		case '\n':
			buf[n++] = c;
			buf[n++] = '\0';
			return n;	//success
		default:
			buf[n++] = c;
		}
	}
	return -1; //read error or EOF or the string > @len
}

/*
 * read line for header
 * TODO: some header more then one line
 * TODO: not stip space eg: if "Connection: Close", the @value is " Close"
 **/
static int http_get_header(int s, char *key, char *value)
{
	char buf[4096], *p, *k, *v;

	if (http_gets(s, buf, sizeof(buf) -1) <= 0)
		return -1;
	p = buf;
	k = strsep(&p, ":");
	if (k == NULL)
		return -1;
	v = strsep(&p, "\r\n");
	strcpy(key, k);
	if (v)
		strcpy(value, v);
	return 0;
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
/*
 * CGI implement
 * return: 0 as success, -1 as failure
 **/
static int do_cgi(int s, const char *path, int is_method_get)
{
	char key[1024], val[1024];
	int fd[2], status;
	pid_t pid;
	
	setenv("METHOD", is_method_get ? "GET":"POST", 1);
	
	if ((pid = fork()) == -1) {
		perror("fork()");
		return -1;
	}
	if (pid == 0) {
		if (dup2(s, 0) == -1) {
			perror("dup2(s, 0)");
			exit(EXIT_FAILURE);
		}
		if (dup2(s, 1) == -1) {
			perror("dup2(s, 1)");
			exit(EXIT_FAILURE);
		}
		fprintf(stderr, "%s %d: exec /www/cgi-bin/cgi\n", __FUNCTION__, __LINE__);
		execlp("/www/cgi-bin/cgi", "cgi", NULL);
		exit(EXIT_FAILURE);
	}
	waitpid(pid, &status, 0);
	return 0;
}
/*
 * the @s is in HEAD content now
 **/
static void http_get_post(int s, char *path, int is_method_get)
{
	int fd, len;
	char buf[4096];

	if (strcmp(path, "/") == 0)
		strcpy(path, "index.html");
	if (*path == '/')
		path++;
	__DBG("PATH:%s\n", path);
	if ((fd = open(path, O_RDONLY)) == -1)
		goto not_found;
#define UHTTPD_CGI 1
#if UHTTPD_CGI
	if (do_cgi(s, path, is_method_get) == 0)
		goto out;	//success
#endif //UHTTPD_CGI
	http_puts(s, "HTTP/1.1 200 OK\r\n");
	http_puts(s, "Connection: close\r\n");
	http_puts(s, "Content-Type: text/html\r\n\r\n");
	//sprintf(buf, "Content-Length: %d\r\n\r\n", 17);
	//http_puts(s, buf);
	while ((len = read(fd, buf, sizeof(buf))) > 0) {
		http_send(s, buf, len);
	}
out:
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
	//len = http_recv(s, buf, sizeof(buf));
	len = http_gets(s, buf, sizeof(buf));
	
	__DBG("[%d]recv: %d\n", getpid(), len);
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
	http_get_post(s, path, method_get);
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
		shutdown(client_socket_fd, SHUT_RDWR);
#endif
	}
}

int main(int argc, char *argv[])
{
	struct sockaddr_in saddrin;
	struct in_addr inp;
	int rev = -1, src;
	int optval = 1;
	int opt;
	char *doc_root = DOC_ROOT;
	unsigned short port = 8080;
	while ((opt = getopt(argc, argv, "d:p:")) != -1) {
	       switch (opt) {
	       case 'd':
			doc_root = optarg;
		   break;
	       case 'p':
		 	port = (unsigned short)atoi(optarg);
		   break;
	       default: /* '?' */
		   fprintf(stderr, "Usage: %s [-p port] [-d doc_root]\n"
			"\t-p: port number, 8080 is default port\n"
			"\t-d: document root path, /www is default path\n",
			   argv[0]);
		   exit(EXIT_FAILURE);
	       }
	}

	if (chdir(doc_root) == -1) {
		fprintf(stderr, "can not chdir(%s) error: %s\n", doc_root, strerror(errno));
		return EXIT_FAILURE;
	}
	bzero(&saddrin, sizeof(struct sockaddr_in));
	src = InetSockSrvInit("0.0.0.0", port, SOCK_STREAM ,&saddrin);
	
	listen(src, 5);
	main_loop(src);
	close(src);
	return 0;
}
