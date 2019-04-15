#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_HOST_LEN 100

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void doit(int connfd);
void parse_uri(char* uri, char* hostname, char* path, int* port);

int main()
{
    int listenfd,connfd;
    socklen_t clientlen;
    char hostname[MAXLINE], port[MAXLINE];

    struct sockaddr_storage clientaddr;

    if (argc != 2) {
    	fprintf(stderr, "usage:%s<port>\n", argv[0]);
    	exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    while(1) {
    	clientlen = sizeof(clientaddr);
    	connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);


    	Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    	printf("Accepted connection from (%s %s).\n", hostname, port);

    	doit(connfd);

    	Close(connfd);
    }
    return 0;
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
	sscanf(rio, "%s %s %s", method, uri, version);
	if (strcasecmp(method, "GET")) {
		printf("Does not implement this method\n");
		return;
	}

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
		pos2 = strstr(pos "/");
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