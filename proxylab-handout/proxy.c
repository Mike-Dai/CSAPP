#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define CACHE_OBJS_COUNT 10
#define LRU_MAGIC_NUM 9999;
#define MAX_HOST_LEN 100

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";
static const char *host_hdr_format = "Host %s\r\n";
static const char *requestlint_hdr_format = "GET %s HTTP/1.0\r\n";
static const char *endof_hdr = "\r\n";

static const char *connection_key = "Connection";
static const char *user_agent_key = "User-Agent";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";

void doit(int connfd);
void parse_uri(char* uri, char* hostname, char* path, int* port);
void build_http_header(char* http_header, char* hostname, char* path, int port, rio_t* client_rio);
int connect_endServer(char *hostname, int port, char *http_header);
void *thread(void *vargp);
void readerPre(int i);
void readerAfter(int i);
void writerPre(int i);
void writerAfter(int i);
void cache_init();
int cache_find();
int cache_eviction();
void cache_lru(int i);
void cache_uri(char *uri, char *buf);


typedef struct {
	char cache_obj[MAX_OBJECT_SIZE];
	char cache_url[MAXLINE];
	int LRU;
	int isEmpty;

	int readCnt;
	sem_t wmutex;
	sem_t rdcntmutex;

	int writeCnt;
	sem_t wtcntmutex;
	sem_t queue;

}cache_block;

typedef struct {
	cache_block cacheobjs[CACHE_OBJS_COUNT];
	int cache_num;
}Cache;

Cache cache;

int main(int argc, char** argv)
{
    int listenfd,connfd;
    socklen_t clientlen;
    char hostname[MAXLINE], port[MAXLINE];
    pthread_t tid;
    struct sockaddr_storage clientaddr;

    if (argc != 2) {
    	fprintf(stderr, "usage:%s<port>\n", argv[0]);
    	exit(1);
    }

    cache_init();

    listenfd = Open_listenfd(argv[1]);
    while(1) {
    	clientlen = sizeof(clientaddr);
    	connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    	Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    	printf("Accepted connection from (%s %s).\n", hostname, port);
    	Pthread_create(&tid, NULL, thread, (void *)connfd);
    }
    return 0;
}

void *thread(void *vargp) {
	int connfd = (int)vargp;
	Pthread_detach(pthread_self());
	doit(connfd);
    Close(connfd);
}

void doit(int connfd) {
	int end_serverfd;
	char uri[MAXLINE], method[MAXLINE], version[MAXLINE];
	int port;
	rio_t rio, server_rio;
	char buf[MAXLINE];
	char endserver_http_header[MAXLINE];
	char hostname[MAXLINE], path[MAXLINE];

	Rio_readinitb(&rio, connfd);
	Rio_readlineb(&rio, buf, MAXLINE);
	sscanf(buf, "%s %s %s", method, uri, version);

	char uri_store[100];
	strcpy(uri_store, uri);

	if (strcasecmp(method, "GET")) {
		printf("Does not implement this method\n");
		return;
	}

	int index;
	if ((index = cache_find()) != -1) {
		readerPre(index);
		Rio_writen(connfd, cache.cacheobjs[index].cache_obj, strlen(cache.cacheobjs[index].cache_obj));
		readerAfter(index);
		cache_lru(index);
		return;
	}

	parse_uri(uri, hostname, path, &port);

	
	
	build_http_header(endserver_http_header, hostname, path, port, &rio);
	
	end_serverfd = connect_endServer(hostname, port, endserver_http_header);
	if (end_serverfd < 0) {
		printf("Connection failed\n");
		return;
	}

	Rio_readinitb(&server_rio, end_serverfd);
	Rio_writen(end_serverfd, endserver_http_header, strlen(endserver_http_header));

	int size = 0;
	char cachebuf[MAX_OBJECT_SIZE];
	size_t n;
	while ((n = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0) {
		size += n;
		if (size < MAX_OBJECT_SIZE) {
			strcat(cachebuf, buf);
		}
		printf("receive %d bytes from server", n);
		Rio_writen(connfd, buf, n);
	}
	if (size < MAX_OBJECT_SIZE) {
		cache_uri(uri_store, cachebuf);
	}
	Close(end_serverfd);
}

inline int connect_endServer(char *hostname, int port, char *http_header) {
	char portStr[100];
	sprintf(portStr, "%d", port);
	return Open_clientfd(hostname, portStr);
}

void parse_uri(char* uri, char* hostname, char* path, int* port) {
	*port = 80;
	char* pos = strstr(uri, "//");
	if (pos != NULL) {
		pos = pos + 2;
	}
	else {
		pos = uri;
	}
	char* pos2 = strstr(pos, ":");
	if (pos2 != NULL) {
		*pos2 = '\0';
		sscanf(pos, "%s", hostname);
		sscanf(pos2+1, "%d%s", port, path);
	}
	else {
		pos2 = strstr(pos, "/");
		if (pos2 != NULL) {
			*pos2 = '\0';
			sscanf(pos, "%s", hostname);
			*pos2 = '/';
			sscanf(pos2, "%s", path);
		}
		else {
			sscanf(pos, "%s", hostname);
		}
	}
	return;
}

void build_http_header(char* http_header, char* hostname, char* path, int port, rio_t *client_rio) {
	char request_hdr[MAXLINE], buf[MAXLINE], host_hdr[MAXLINE], other_hdr[MAXLINE];
	sprintf(request_hdr, requestlint_hdr_format, path);

	while(Rio_readlineb(client_rio, buf, MAXLINE) > 0) {
		if (strcmp(buf, endof_hdr) == 0) {
			break;
		}
		if (!strncasecmp(buf, host_key, strlen(host_key))) {
			strcpy(host_hdr, buf);
		}
		if (strncasecmp(buf, connection_key, strlen(connection_key)) 
			&& strncasecmp(buf, proxy_connection_key, strlen(proxy_connection_key)) 
			&& strncasecmp(buf, user_agent_key, strlen(user_agent_key))) {
			strcat(other_hdr, buf);
		}
	}
	if (strlen(host_hdr) == 0) {
		sprintf(host_hdr, host_hdr_format, hostname);
	}
	sprintf(http_header, "%s%s%s%s%s%s%s",
		request_hdr, host_hdr, conn_hdr, prox_hdr,
		user_agent_hdr, other_hdr, endof_hdr);
	return;
}

void readerPre(int i) {
	cache_block cacheobj = cache.cacheobjs[i];
	P(&cacheobj.queue);
	P(&cacheobj.rdcntmutex);
	cacheobj.readCnt++;
	if (cacheobj.readCnt == 1) {
		P(&cacheobj.wmutex);
	}
	V(&cacheobj.rdcntmutex);
	V(&cacheobj.queue);
}

void readerAfter(int i) {
	cache_block cacheobj = cache.cacheobjs[i];
	P(&cacheobj.rdcntmutex);
	cacheobj.readCnt--;
	if (cacheobj.readCnt == 0) {
		V(&cacheobj.wmutex);
	}
	V(&cacheobj.rdcntmutex);
}

void writerPre(int i) {
	cache_block cacheobj = cache.cacheobjs[i];
	P(&cacheobj.wtcntmutex);
	cacheobj.writeCnt++;
	if (cacheobj.writeCnt == 1) {
		P(&cacheobj.queue);
	}
	V(&cacheobj.wtcntmutex);
	P(&cacheobj.wmutex);
}

void writerAfter(int i) {
	cache_block cacheobj = cache.cacheobjs[i];
	V(&cacheobj.wmutex);
	P(&cacheobj.wtcntmutex);
	cacheobj.writeCnt--;
	if (cacheobj.writeCnt == 0) {
		V(&cacheobj.queue);
	}
	V(&cacheobj.wtcntmutex);
}

void cache_init() {
	int i;
	for (i = 0; i < MAX_CACHE_SIZE; i++) {
		cache_block cacheobj = cache.cacheobjs[i];
		cacheobj.LRU = 0;
		cacheobj.isEmpty = 1;
		Sem_init(&cacheobj.wmutex, 0, 1);
		Sem_init(&cacheobj.wtcntmutex, 0, 1);
		Sem_init(&cacheobj.rdcntmutex, 0, 1);
		Sem_init(&cacheobj.queue, 0, 1);
		cacheobj.readCnt = 0;
		cacheobj.writeCnt = 0;
	}
	cache.cache_num = 0;
}

int cache_find(char *uri) {
	int i;
	for (i = 0; i < MAX_CACHE_SIZE; i++) {
		cache_block cacheobj = cache.cacheobjs[i];
		readerPre(i);
		if ((cacheobj.isEmpty == 0) && (strcmp(cacheobj.cache_url, uri) == 0)) {
			readerAfter(i);
			return i;
		}
		readerAfter(i);
	}
	return -1;
}

int cache_eviction() {
	int i;
	int min = LRU_MAGIC_NUM;
	int minindex = 0;
	for (i = 0; i < MAX_CACHE_SIZE; i++) {
		cache_block cacheobj = cache.cacheobjs[i];
		readerPre(i);
		if (cacheobj.isEmpty == 1) {
			readerAfter(i);
			return i;
		}
		if (cacheobj.LRU < min) {
			minindex = i;
			readerAfter(i);
			continue;
		}
		readerAfter(i);
	}
	return minindex;
}

void cache_lru(int index) {
	int i;
	for (i = 0; i < MAX_CACHE_SIZE; i++) {
		cache_block cacheobj = cache.cacheobjs[i];
		writerPre(i);
		if (i == index) {
			cacheobj.LRU = LRU_MAGIC_NUM;
			writerAfter(i);
		}
		else if (cacheobj.isEmpty == 0) {
			cacheobj.LRU--;
			writerAfter(i);
		}
	}
}

void cache_uri(char *uri, char *buf) {
	int i = cache_eviction();
	cache_block cacheobj = cache.cacheobjs[i];
	writerPre(i);
	strcpy(cacheobj.cache_url, uri);
	strcpy(cacheobj.cache_obj, buf);
	cacheobj.isEmpty = 0;
	writerAfter(i);
	cache_lru(i);
}