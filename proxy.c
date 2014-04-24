/*
*  Version 1: Sequential web proxy
*  -------------------------------
*  Based on tiny web server in CSAPP
*/


#include <stdio.h>
#include <stdlib.h>
#include "csapp.h"

void parse_url(char *url, char *hostname, char *port_string, char *uri);
void send_msg(int fd, char *msg_title, char* msg_detail);
void change_http_version(char *version);
void show_proxy_target(char *hostname, int port, char *proxy_request);
void launch_request(int fd, char *hostname, int port, char *proxy_request);
void doit(int fd);
void read_requesthdrs(rio_t *rp, char *host_header, char *additional_header);
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

    int listenfd,connfd,port,clientlen;
    struct sockaddr_in clientaddr;

    /* Check command line args */
    if (argc != 2) {
    	fprintf(stderr, "usage: %s <port>\n", argv[0]);
    	exit(1);
    }
    port = atoi(argv[1]);

    listenfd = Open_listenfd(port);

    /* Proxy is running */
    printf("Proxy is up and running...\n");

    /* Accept connection from client, one at a time */
    while(1) {
    	clientlen = sizeof(clientaddr);
    	connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);
    	doit(connfd);
    	Close(connfd);
    }
    return 0;
}

/*
 * doit - handle one HTTP request/response transaction
 */
void doit(int fd) 
{
    char buf[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE];
    char uri[MAXLINE], hostname[MAXLINE];
    char host_header[MAXLINE], additional_header[MAXLINE];
    rio_t rio;
    char proxy_request[MAXLINE], request_header[MAXLINE];
    char port_string[MAXLINE];
  
    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, url, version);
    if (strcasecmp(method, "GET")) { 
       clienterror(fd, method, "501", "Not Implemented",
                "Proxy does not implement this method");
        return;
    }

    /* Reset proxy request */
    memset(proxy_request, 0, sizeof(proxy_request));
    /* Reset request header */
    memset(request_header, 0, sizeof(request_header));
    /* Reset host header */
    memset(host_header, 0, sizeof(host_header));
    /* Reset additional header */
    memset(additional_header, 0, sizeof(additional_header));
    /* Reset hostname */
    memset(hostname, 0, sizeof(hostname));

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
    read_requesthdrs(&rio, host_header, additional_header); 

    /* Set up HTTP request header */
    if (strstr(host_header,"Host")) {
        strcat(request_header, host_header);
    }
    else {
        strcat(request_header, "Host: ");
        strcat(request_header, hostname);
        strcat(request_header, "\r\n");
    }
    // strcat(request_header, user_agent_hdr);
    // strcat(request_header, accept_hdr);
    // strcat(request_header, accept_encoding_hdr);
    strcat(request_header, "Connection: close\r\n");
    strcat(request_header, "Proxy-Connection: close\r\n");
    strcat(request_header, additional_header);
    strcat(proxy_request, request_header);
    strcat(proxy_request, "\r\n");

    /* Open connection to target server */
    int port;
    if(*port_string != NULL){
        port = atoi(port_string);
    } else {
        port = atoi("80"); /* set default HTTP port */
    }
    
    /* Displays proxy's target */
    show_proxy_target(hostname, port, proxy_request);

    /* Proxy launch request on behalf of user */
    launch_request(fd, hostname, port, proxy_request);

}

/*
 * proxy launches request on behalf of user
 */
 void launch_request(int fd, char *hostname, int port, char *proxy_request) {

    int proxyfd = open_clientfd(hostname, port);
    rio_t rio_proxy;
    char buf2[MAXLINE];

    /* error handling */
    if (proxyfd == -2) {
        clienterror(fd, hostname, "001", "DNS error",
                "Server not found");
        return;
    }
    if (proxyfd == -1) {
        clienterror(fd, hostname, "002", "Unix error",
            "Cannot connect to server at this port");
        return;
    }
    /* deliver server's response to client */
    Rio_readinitb(&rio_proxy, proxyfd);
    Rio_writen(proxyfd, proxy_request, strlen(proxy_request));
    while (Rio_readlineb(&rio_proxy, buf2, MAXLINE)) {
        Rio_writen(fd, buf2, strlen(buf2));
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
    printf(proxy_request);
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
 	Rio_writen(fd, buf, strlen(buf));
 }

/*
 * read_requesthdrs - read and parse HTTP request headers
 */

void read_requesthdrs(rio_t *rp, char *host_header, char *additional_header) 
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while(strcmp(buf, "\r\n")) {
		/* Ignore certain header values */
		if (/*(!strstr(buf, "User-Agent:")) &&
            (!strstr(buf, "Accept:")) && 
			(!strstr(buf, "Accept-Encoding:")) &&*/			
			(!strstr(buf, "Connection:")) &&
			(!strstr(buf, "Proxy-Connection:"))) {
            if (strstr(buf, "Host:")) {
                strcpy(host_header, buf);
            }
            else {
                strcat(additional_header, buf); /* Additional headers */
            }
		}
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
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
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}


