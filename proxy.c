/*
 *  Version 2: Concurrent Web Proxy
 *  -------------------------------
 *  April 25th 2014
 */


#include <stdio.h>
#include <stdlib.h>
#include "csapp.h"

void parse_url(char *url, char *hostname, char *port_string, char *uri);
void send_msg(int fd, char *msg_title, char* msg_detail);
void change_http_version(char *version);
void show_proxy_target(char *hostname, int port, char *proxy_request);
void launch_request(char *buf, int fd, char *hostname, int port, char *proxy_request);
void *thread(void *vargp);
void doit(int fd);
void read_requesthdrs(char *buf, rio_t *rp, char *host_header, char *additional_header);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);


/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including these long lines in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";

int main(int argc, char **argv)
{

    printf("%s%s%s", user_agent_hdr, accept_hdr, accept_encoding_hdr);
    printf("\n");

    int listenfd,*connfd,port,clientlen;
    struct sockaddr_in clientaddr;
    pthread_t tid;

    /* Ignore SIGPIPE */
    Signal(SIGPIPE, SIG_IGN);
    /* Block SIGPIPE */
    sigset_t mask;
    Sigemptyset(&mask);
    Sigaddset(&mask, SIGPIPE);
    Sigprocmask(SIG_BLOCK, &mask, NULL);

    /* Check command line args */
    if (argc != 2) {
    	fprintf(stderr, "usage: %s <port>\n", argv[0]);
    	exit(1);
    }
    port = atoi(argv[1]);

    listenfd = Open_listenfd(port);

    /* Proxy is running */
    printf("Proxy is up and running...\n");

    /* Spawn one thread for each client request */
    while(1) {
    	clientlen = sizeof(clientaddr);
        connfd = Malloc(sizeof(int));
    	*connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);
        Pthread_create(&tid, NULL, thread, connfd);
    }
    return 0;
}

/*
 * thread
 */
void *thread(void *vargp) 
{
    /* Retrieve the file descriptor */
    int fd = *((int *) vargp);
    /* Detach the thread */
    Pthread_detach(pthread_self());
    Free(vargp);
    doit(fd);
    Close(fd);
    return NULL;
}

/*
 * our wrapper for rio_writen, does not exit proxy
 */
void Rio_writen_new(int fd, void *usrbuf, size_t n) 
{
    if (rio_writen(fd, usrbuf, n) != n) {
        /* Ignore */
         printf("Oops! Rio_writen error...\n");
         return; 
    }
        
}

/*
 * our wrapper for rio_readn, does not exit proxy
 */
size_t Rio_readn_new(int fd, void *ptr, size_t nbytes)
{
    size_t n;

    if ((n = rio_readn(fd, ptr, nbytes)) < 0) {
        /* Ignore */
        printf("Oops! Rio_readn error...\n");
        return n;
    }

}

/*
 * our wrapper for rio_readnb, does not exit proxy
 */
size_t Rio_readnb_new(rio_t *rp, void *usrbuf, size_t n)
{
    size_t rc;

    if ((rc = rio_readnb(rp, usrbuf, n)) < 0) {
        /* Ignore */
        printf("Oops! Rio_readnb error...\n");
        return rc;
    }

}

/*
 * our wrapper for rio_readlineb, does not exit proxy
 */
size_t Rio_readlineb_new(rio_t *rp, void *usrbuf, size_t maxlen)
{
    size_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0) {
        /* Ignore */
        printf("Oops! Rio_readlineb error");
        return rc;
    }

}

/*
 * doit - handle one HTTP request/response transaction
 */
void doit(int fd) 
{
    int port;
    char buf[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE];
    char uri[MAXLINE], hostname[MAXLINE];
    char host_header[MAXLINE], additional_header[MAXLINE];
    char proxy_request[MAXLINE], request_header[MAXLINE];
    char port_string[MAXLINE];
    rio_t rio;

    /* Reset */
    memset(buf, 0, sizeof(buf));
    memset(proxy_request, 0, sizeof(proxy_request));
    memset(request_header, 0, sizeof(request_header));
    memset(host_header, 0, sizeof(host_header));
    memset(additional_header, 0, sizeof(additional_header));
    memset(hostname, 0, sizeof(hostname));
  
    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    Rio_readlineb_new(&rio, buf, MAXLINE);

    if ((sscanf(buf, "%s %s %s", method, url, version) != 3) &&
        (strstr(url, "http://") == NULL) && 
        (strstr(version, "HTTP/1.") == NULL))  {
        clienterror(fd, "", "Oops", "Malformed HTTP request",
                "Bad!");
        return;
    }
    if (strcasecmp(method, "GET")) { 
       clienterror(fd, method, "501", "Not Implemented",
                "Proxy does not implement this method");
        return;
    }

    /* Parse URL of Web browser to hostname and URI */
    parse_url(url, hostname, port_string, uri);
    change_http_version(version);

    /* Set up proxy's HTTP request string */
    strcat(proxy_request, method);
    strcat(proxy_request, " ");
    strcat(proxy_request, uri);
    strcat(proxy_request, " ");
    strcat(proxy_request, version);
    strcat(proxy_request, "\r\n");

    /* Read in browser's request header,
     * ignore all headers except host header
     * and some additional header other then we
     * will define
     */
    read_requesthdrs(buf, &rio, host_header, additional_header);

    /* Set up HTTP request header */
    if (strstr(host_header,"Host")) {
        strcat(request_header, host_header);
    }
    else {
        strcat(request_header, "Host: ");
        strcat(request_header, hostname);
        strcat(request_header, "\r\n");
    }
    strcat(request_header, user_agent_hdr);
    strcat(request_header, accept_hdr);
    strcat(request_header, accept_encoding_hdr);
    strcat(request_header, "Connection: close\r\n");
    strcat(request_header, "Proxy-Connection: close\r\n");
    strcat(request_header, additional_header);
    strcat(proxy_request, request_header);
    strcat(proxy_request, "\r\n");

    /* Open connection to target server */
    if(*port_string != NULL){
        port = atoi(port_string);
    } else {
        port = atoi("80"); /* set default HTTP port */
    }
    
    /* Displays proxy's target */
    show_proxy_target(hostname, port, proxy_request);

    /* Proxy launch request on behalf of user */
    launch_request(buf, fd, hostname, port, proxy_request);

}

/*
 * proxy launches request on behalf of user
 */
 void launch_request(char *buf, int fd, char *hostname, int port, char *proxy_request) {

    int proxyfd = open_clientfd_r(hostname, port);
    rio_t rio_proxy;
    // char buf2[MAXLINE];
    size_t count;

    /* error handling */
    if (proxyfd == -2) {
        clienterror(fd, hostname, "Oops!", "DNS error",
                "Server not found");
        return;
    }
    if (proxyfd == -1) {
        clienterror(fd, hostname, "Oops!", "Unix error",
            "Cannot connect to server at this port");
        return;
    }
    /* deliver server's response to client */
    Rio_readinitb(&rio_proxy, proxyfd);
    Rio_writen_new(proxyfd, proxy_request, strlen(proxy_request));
    while ((count = Rio_readn_new(proxyfd, buf, MAXLINE)) > 0) {
        Rio_writen_new(fd, buf, count);
    }
    Close(proxyfd);
 }


/*
 * prints proxy's target including hostname, port number and proxy_request
 */
 void show_proxy_target(char *hostname, int port, char *proxy_request) {
    printf("----PROXY TARGET----\n");
    printf("hostname = %s\n", hostname);
    printf("port number = %d\n", port);
    printf("----REQUEST HEADER----\n");
    printf("header length = %d\n",(int)strlen(proxy_request));
    printf("%s", proxy_request);
 }

/*
 * change_http_version - change the http version to HTTP/1.0
 */
 void change_http_version(char *version) {
 	if (version[strlen(version) - 1] == '1')
 		version[strlen(version) - 1] = '0';
 }

/*
 * send_msg - proxy sends a msg to client, for debugging purpose
 */
 void send_msg(int fd, char *msg_title, char* msg_detail) {

 	char buf[MAXLINE];
 	sprintf(buf, "From proxy: %s%s\r\n", msg_title, msg_detail);
 	Rio_writen_new(fd, buf, strlen(buf));
 }

/*
 * read_requesthdrs - read and parse HTTP request headers
 */

void read_requesthdrs(char *buf, rio_t *rp, char *host_header, char *additional_header) 
{

    Rio_readlineb_new(rp, buf, MAXLINE);

    /* Return when buffer is empty */
    if (strlen(buf) == 0) {
        return;
    }
    while(strcmp(buf, "\r\n")) {
		/* Ignore certain header values */
		if ((!strstr(buf, "User-Agent:")) &&
            (!strstr(buf, "Accept:")) && 
			(!strstr(buf, "Accept-Encoding:")) &&			
			(!strstr(buf, "Connection:")) &&
			(!strstr(buf, "Proxy-Connection:"))) {
            if (strstr(buf, "Host:")) {
                strcpy(host_header, buf);
            }
            else {
                strcat(additional_header, buf); /* Additional headers */
            }
		}
        Rio_readlineb_new(rp, buf, MAXLINE);
    }
    return;
}

/*
 * parse_uri - parse complete URL from the browser
 *             into hostname and URI
 */

void parse_url(char *url, char *hostname, char *port_string, char *uri) 
{
   /* for extracting hostname in URL */
   int begin = 0;
   int end = 0;
   char *pos; 
    if (pos = strstr(url, "//")) {
       pos += 2;
       begin = (int) (pos - url);
    }
    char *hostnamePosition = pos;

    if (pos = strstr(pos, ":")) { // has port number
        //printf("inside has port number\n");
        char *colonPosition = pos;
        /* extract port number */
        if(pos = strstr(pos, "/")) { // has port number and has uri
            /* extract uri */
            strcpy(uri, pos);
            end = (int) (colonPosition - url);
            /* extract hostname */
            for (int i = begin; i < end; i++) {
                hostname[i - begin] = url[i];
            }
            /* add null terminating string */
            hostname[strlen(hostname)] = '\0';

            /* extrace port string */
            int portLength = (int) (pos - colonPosition - 1);
            for (int i = 0; i < portLength; i++) {
                port_string[i] = colonPosition[i + 1];
            }
            /* add null terminating string */
            port_string[strlen(port_string)] = '\0';
        }
        else { // has port number, but no uri
            strcpy(uri, "/");
            // no uri, so all char after ":" belongs to port number
            // printf("has port number, but no uri\n");
            end = (int) (colonPosition - url);
            /* extract hostname */
            for (int i = begin; i < end; i++) {
                hostname[i - begin] = url[i];
            }
            hostname[strlen(hostname)] = '\0';
            strcpy(port_string, colonPosition + 1);
        }
    }
    else { // do not have port number
        *port_string = NULL;
        pos = hostnamePosition; // recur pos
        if (pos = strstr(pos, "/")) {
            /* extract uri */
            strcpy(uri, pos);
            end = (int) (pos - url);
            /* extract hostname */
            for (int i = begin; i < end; i++) {
                hostname[i - begin] = url[i];
            }
            /* add null terminating string */
            hostname[strlen(hostname)] = '\0';
        }
        else {
            strcpy(uri, "/");
            strcpy(hostname, hostnamePosition);
        }
    }
        
}

/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Super-Awesome Web Proxy</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen_new(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen_new(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen_new(fd, buf, strlen(buf));
    Rio_writen_new(fd, body, strlen(body));
}


