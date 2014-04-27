/*
 *  213 Proxy Lab - Concurrent Web Proxy with Caching
 *  --------------------------------------------
 *  Team member: Wenting Shi
 *               Yijie Ma
 *  Date: April 27th 2014
 *  --------------------------------------------
 *  Description:
 *  (1) This program is a web proxy that serves as an 
 *      intermediary between client and server. 
 *  (2) It is a multi-threaded proxy that can serve many
 *      request simultaneously with different threads.
 *      Readers/Writers lock has been used for protecting
 *      critical resources (accessing of cache) and supporting
 *      multiple readers.
 *  (3) The proxy implements a caching functionality
 *      with linked list and a LRU replacement policy. 
 *      After receiving http request from the client, 
 *      it checks whether the web object exists in the cache. 
 *      If it finds the web object, it delivers directly from cache, 
 *      otherwise it launches request to the server 
 *      on behalf of the client and updates the cache. 
 */

#include "csapp.h"
#include "cache.h"

/* Function declarations */
void parse_url(char *url, char *hostname, char *port_string, char *uri);
void change_http_version(char *version);
void show_proxy_target(char *hostname, int port, char *proxy_request);
int launch_request(char *buf, int fd, char *hostname, 
                    int port, char *proxy_request, 
                    char *content, size_t *content_length);
void *thread(void *vargp);
void doit(int fd);
void read_requesthdrs(char *buf, rio_t *rp, char *host_header, 
                        char *additional_header);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);


/* Max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* Required headers */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";

/* Reader/Writer Lock for Cache Access */
pthread_rwlock_t lock;

int main(int argc, char **argv)
{
    int listenfd, *connfd, port, clientlen;
    struct sockaddr_in clientaddr;
    pthread_t tid;
    sigset_t mask;

    /* Proxy introduces itself */
    printf("%s%s%s", user_agent_hdr, accept_hdr, accept_encoding_hdr);
    printf("\n");

    /* cache initialization */
    init_cache();

    /* reader/writer lock initialization */
    pthread_rwlock_init(&lock, NULL);

    /* Block SIGPIPE */
    Sigemptyset(&mask);
    Sigaddset(&mask, SIGPIPE);
    Sigprocmask(SIG_BLOCK, &mask, NULL);

    /* Check command line args */
    if (argc != 2) {
    	fprintf(stderr, "usage: %s <port>\n", argv[0]);
    	exit(1);
    }
    port = atoi(argv[1]);

    listenfd = open_listenfd(port);
    if (listenfd < 0) {
        printf("Unable to use port: %d\n", port);
    }
    else {
        /* Proxy is running */
        printf("Proxy is up and running...\n");
    }

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
 * thread that handles one request
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
         return; 
    }
        
}

/*
 * our wrapper for rio_readnb, does not exit proxy
 */
ssize_t Rio_readnb_new(rio_t *rp, void *usrbuf, size_t n)
{
    size_t rc;

    if ((rc = rio_readnb(rp, usrbuf, n)) < 0) {
        /* Ignore */
        return rc;
    }

}

/*
 * our wrapper for rio_readlineb, does not exit proxy
 */
ssize_t Rio_readlineb_new(rio_t *rp, void *usrbuf, size_t maxlen)
{
    size_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0) {
        /* Ignore */
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
    char content[MAX_OBJECT_SIZE];
    rio_t rio;
    char *wo_content;
    size_t content_length = 0;
    web_object *wo;
    web_object *new_wo;
    int can_cache;

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

    if ((sscanf(buf, "%s %s %s", method, url, version) != 3) ||
        (strstr(url, "http://") == NULL) || 
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

    /* Set up first line of proxy's HTTP request string */
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
    if((*port_string) != '\0'){
        port = atoi(port_string);
    } else {
        port = atoi("80"); /* set default HTTP port */
    }
    
    /* Displays proxy's target */
    //show_proxy_target(hostname, port, proxy_request); 

    /* Reader access cache */
    pthread_rwlock_rdlock(&lock);
    wo = find(url);
    pthread_rwlock_unlock(&lock); 

    if (wo != NULL) {
        /* Deliver the web object to client from cache */
        wo_content = wo->content;
        content_length = wo->size;
        Rio_writen_new(fd, wo_content, content_length);
    }
    else {
        /* Proxy launch request on behalf of user */
        can_cache = launch_request(buf, fd, hostname, port,
                         proxy_request, content, &content_length);
        if (!can_cache) {
            //printf("Unable to cache...\n");
        }
        else {
            /* Cache the web object */
            //printf("Caching web object...\n");
            new_wo = create_web_object(content, url, content_length);
            /* Writer access cache */
            pthread_rwlock_wrlock(&lock);
            add_to_cache(new_wo);
            pthread_rwlock_unlock(&lock);
            
        }
    }
}

/*
 * proxy launches request on behalf of user
 * returns 1 when the web-object can be cached, 
 * returns 0 when the size of the web-object exceeds the limit
 *           or proxy failed to connect to requested server
 */
 int launch_request(char *buf, int fd, char *hostname, 
                    int port, char *proxy_request, 
                    char *content, size_t *content_length) {
    int proxyfd = open_clientfd_r(hostname, port);
    rio_t rio_proxy;
    size_t count;
    int can_cache = 1;

    /* error handling */
    if (proxyfd == -2) {
        clienterror(fd, hostname, "Oops!", "DNS error",
                "Server not found");
        return 0;
    }
    if (proxyfd == -1) {
        clienterror(fd, hostname, "Oops!", "Unix error",
            "Cannot connect to server at this port");
        return 0;
    }
    /* deliver request to server */
    Rio_readinitb(&rio_proxy, proxyfd);
    Rio_writen_new(proxyfd, proxy_request, strlen(proxy_request));

    /* read server response and write to client */
    while ((count = Rio_readnb_new(&rio_proxy, buf, MAXLINE)) > 0) {       
        Rio_writen_new(fd, buf, count);
        /* save content for caching */
        if ((*content_length) + count > MAX_OBJECT_SIZE) {
            can_cache = 0;  /* Exceeds maximum object size limit */
        }
        if (can_cache) {
            memcpy(content + (*content_length), buf, count);
            (*content_length) += count;
        }                    
    }
    Close(proxyfd);
    return can_cache;
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
 * read_requesthdrs - read and parse browser's HTTP request headers
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
 * parse_url - parse complete URL from the browser
 *             into hostname, port, URI
 */

void parse_url(char *url, char *hostname, char *port_string, char *uri) 
{
   int begin = 0;
   int end = 0;
   int i;
   char *pos;
   char *colonPosition;
   char *hostnamePosition; 
    if (pos = strstr(url, "//")) {
       pos += 2;
       begin = (int) (pos - url);
    }
    hostnamePosition = pos;

    if (pos = strstr(pos, ":")) {
        /* has port number */ 
        colonPosition = pos;
        /* extract port number */
        if(pos = strstr(pos, "/")) { 
            /* extract uri */
            strcpy(uri, pos);
            end = (int) (colonPosition - url);
            /* extract hostname */
            for (i = begin; i < end; i++) {
                hostname[i - begin] = url[i];
            }
            /* add null terminating string */
            hostname[strlen(hostname)] = '\0';

            /* extrace port string */
            int portLength = (int) (pos - colonPosition - 1);
            for (i = 0; i < portLength; i++) {
                port_string[i] = colonPosition[i + 1];
            }
            /* add null terminating string */
            port_string[strlen(port_string)] = '\0';
        }
        else { 
            /* no uri, only port number */
            strcpy(uri, "/");
            end = (int) (colonPosition - url);
            /* extract hostname */
            for (i = begin; i < end; i++) {
                hostname[i - begin] = url[i];
            }
            hostname[strlen(hostname)] = '\0';
            strcpy(port_string, colonPosition + 1);
        }
    }
    else { 
        /* no port number detected */
        *port_string = '\0';
        pos = hostnamePosition; 
        if (pos = strstr(pos, "/")) {
            /* extract uri */
            strcpy(uri, pos);
            end = (int) (pos - url);
            /* extract hostname */
            for (i = begin; i < end; i++) {
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


