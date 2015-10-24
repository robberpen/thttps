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
#include <sys/types.h>
#include <sys/wait.h>


#ifdef HTTPS_SUPPORT
#include <openssl/bio.h> 
#include <openssl/ssl.h> 
#include <openssl/err.h> 
#endif

#include "inetsocket.h"
#define __DBG(fmt, a...)	fprintf(stderr, "%s %d: "fmt, __FUNCTION__, __LINE__, ##a)
#define DOC_ROOT	"/www"

#ifdef HTTPS_SUPPORT
char *public_key_path = "./ssl/server.cert";
char *private_key_path = "./ssl/server.key";

typedef struct {
	int sock_fd;
	SSL *ssl_fd;
	int use_ssl;
} req_t;

void InitializeSSL()
{
	SSL_load_error_strings();
	SSL_library_init();
	OpenSSL_add_all_algorithms();
}
void DestroySSL()
{
	ERR_free_strings();
	EVP_cleanup();
}
void ShutdownSSL(SSL *ssl)
{
	SSL_shutdown(ssl);
	SSL_free(ssl);
}
SSL *accept_ssl(int fd)
{
	SSL_CTX *sslctx;
	SSL *cSSL;

	if ((sslctx = SSL_CTX_new( SSLv23_server_method())) == NULL)
		goto err;
	SSL_CTX_set_options(sslctx, SSL_OP_SINGLE_DH_USE);
	if (SSL_CTX_use_certificate_file(sslctx, public_key_path , SSL_FILETYPE_PEM) != 1)
		goto err;
	if (SSL_CTX_use_PrivateKey_file(sslctx, private_key_path, SSL_FILETYPE_PEM) != 1)
		goto err;
	if ((cSSL = SSL_new(sslctx)) == NULL)
		goto err;
	if (SSL_set_fd(cSSL, fd) != 1)
		goto err;
	if (SSL_accept(cSSL) != 1)
		goto err;
	return cSSL;
err:
	fprintf(stderr, "ssl error: %s\n", ERR_error_string(ERR_get_error(), NULL));
	return NULL;
}
#endif

int http_recv(req_t *req, char *buf, int len)
{
	return req->use_ssl ? SSL_read(req->ssl_fd, buf, len) : recv(req->sock_fd, buf, len, 0);
}
int http_send(req_t *req, const void *buf, int len)
{
	return req->use_ssl ? SSL_write(req->ssl_fd, buf, len): send(req->sock_fd, buf, len, 0);
}
int http_puts(req_t *req, const char *buf)
{
	return http_send(req, buf, strlen(buf));
}
/*
 * fgets(3) like, but return number characters read from @s
 *   and strip '\r'
 **/
static int http_gets(req_t *req, char *buf, size_t len)
{
	unsigned char c;
	int n = 0;
	while (n < len && http_recv(req, (char *)&c, 1)  == 1) {
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
static int http_get_header(req_t *req, char *key, char *value)
{
	char buf[4096], *p, *k, *v;

	if (http_gets(req, buf, sizeof(buf) -1) <= 0)
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
static int do_cgi(req_t *req, const char *path, int is_method_get)
{
#if 0	//not implement
	int status;
	pid_t pid;
	setenv("METHOD", is_method_get ? "GET":"POST", 1);
	//while (http_get_header(s, key, val) != -1)
	//	__DBG("k:[%s], v:[%s]\n", key, val);

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
		execlp("/www/cgi-bin/cgi", "cgi", NULL);
		exit(EXIT_FAILURE);
	}
	waitpid(pid, &status, 0);
#endif
	return 0;
}
/*
 * the @s is in HEAD content now
 **/
static void http_get_post(req_t *req, char *path, int is_method_get)
{
	int fd, len;
	char buf[4096];

	if (strcmp(path, "/") == 0)
		strcpy(path, "index.html");
	if (*path == '/')
		path++;
	if ((fd = open(path, O_RDONLY)) == -1)
		goto not_found;
//#define UHTTPD_CGI 1
#if UHTTPD_CGI
	if (do_cgi(req, path, is_method_get) == 0)
		goto out;	//success
#endif //UHTTPD_CGI
	http_puts(req, "HTTP/1.1 200 OK\r\n");
	http_puts(req, "Connection: close\r\n");
	http_puts(req, "Content-Type: text/html\r\n\r\n");
	//sprintf(buf, "Content-Length: %d\r\n\r\n", 17);
	//http_puts(s, buf);
	__DBG("ssl send start\n");
	while ((len = read(fd, buf, sizeof(buf))) > 0) {
		http_send(req, buf, len);
	}
out:
	close(req->sock_fd);
	return;
not_found:
	http_puts(req, "HTTP/1.1 404 Not Found\r\n\r\n");
}
static void process_http(req_t *req)
{
	char buf[1024];	//should check socket buffer by getsockopt(2)
	char *p, *line;
	char path[256];
	int method_get, len;

	bzero(buf, sizeof(buf));
	/* read GET or POST */
	//len = http_recv(s, buf, sizeof(buf));
	len = http_gets(req, buf, sizeof(buf));
	
	__DBG("[%d]recv: %d [%s]\n", getpid(), len, buf);
	if (strncmp(buf, "GET", 3) == 0) {
		method_get = 1;
		parser_http_get_url(buf, path);
	} else if (strncmp(buf, "POST", 4) == 0)
		method_get = 0;
	else
		goto method_unsupport;
	__DBG("get: [%s], PATH: %s\n", buf, path);
	for (p = buf; p && (line = strsep(&p, "\n")); ) {
		__DBG("Header: %s\n", line);
		
	}
	http_get_post(req, path, method_get);
	return;
method_unsupport:
	http_puts(req, "405 Method Not Allowed\r\n\r\n");
}

static void main_loop(int server_socket_fd)
{
	int client_socket_fd;
	struct sockaddr_in client_sock_addr_in;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	char buf[24];
	req_t req;
	int ret;

	for (;;) {
		bzero(&client_sock_addr_in, sizeof(client_sock_addr_in));
		printf("%s: %d: waitting connect\n", __FUNCTION__, __LINE__);
		client_socket_fd = accept(server_socket_fd, (struct  sockaddr*)&client_sock_addr_in, &addrlen);
		printf("%s %d new ssl\n", __FUNCTION__, __LINE__);
		if ((req.ssl_fd = accept_ssl(client_socket_fd)) == NULL)
			return;
		req.sock_fd = client_socket_fd;
		req.use_ssl = 1;
		//ret = SSL_read(req.ssl_fd, buf, sizeof(buf));
		//fprintf(stderr, "ssl error: %s\n", ERR_error_string(ERR_get_error(), NULL));
		//printf("%s: %d: SSL success connected from: %s read: %d\n", __FUNCTION__, __LINE__,  inet_ntop(AF_INET, &client_sock_addr_in.sin_addr, buf, 24), ret);
#ifdef HTTP_USE_FORK
		if (fork() == 0) {
			close(server_socket_fd);
			process_http(&req);
			shutdown(client_socket_fd, SHUT_RDWR);
			exit(0);
		} else {
			/* XXX: don't do that! use close(2) instead of shutdown(2) */
			//shutdown(client_socket_fd, SHUT_RDWR);
			close(client_socket_fd);
		}
#else
		process_http(&req);
		//shutdown(client_socket_fd, SHUT_RDWR);
		close(client_socket_fd);
#endif
	}
}

int main(int argc, char *argv[])
{
	struct sockaddr_in saddrin;
	int  src;
	int opt;
	char *doc_root = DOC_ROOT;
	unsigned short port = 8080;
#ifdef HTTPS_SUPPORT
	InitializeSSL();
#endif
	while ((opt = getopt(argc, argv, "d:p:k:c:")) != -1) {
	       switch (opt) {
	       case 'd':
			doc_root = optarg;
		   break;
	       case 'p':
		 	port = (unsigned short)atoi(optarg);
		   break;
	       case 'c':
		 	public_key_path = optarg;
		   break;
	       case 'k':
		 	private_key_path = optarg;
		   break;
	       default: /* '?' */
		   fprintf(stderr, "Usage: %s [-p port] [-d doc_root]\n"
			"\t-p: port number, 8080 is default port\n"
			"\t-d: document root path, /www is default path\n",
			"\t-k: private key file, default: ssl/server.key\n",
			"\t-c: public key file, default: ssl/server.cert\n",
			   argv[0]);
		   exit(EXIT_FAILURE);
	       }
	}

	if (chdir(doc_root) == -1) {
		fprintf(stderr, "can not chdir(%s) error: %s\n", doc_root, strerror(errno));
		return EXIT_FAILURE;
	}
	printf("port %d\n", port);
	bzero(&saddrin, sizeof(struct sockaddr_in));
	if ((src = InetSockSrvInit("0.0.0.0", port, SOCK_STREAM ,&saddrin)) < 0)
		return EXIT_FAILURE;
	listen(src, 5);
	main_loop(src);
	close(src);
	return 0;
}
