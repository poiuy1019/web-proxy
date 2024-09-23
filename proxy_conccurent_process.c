#include "csapp.h"

void doit(int clientfd);
void parse_uri(char *uri, char *hostname, char *port, char *path);
void makeHTTPheader(char *http_header, char *hostname, char *path, char *port, rio_t *client_rio);
void parse_uri(char *uri, char *hostname, char *port, char *path);
void sigchld_handler(int sig);

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    //Set up signal handler to reap zombie child processes
    Signal(SIGCHLD, sigchld_handler);

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]); // Open the listening socket
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen); // Accept client connection

        // Fork to create a child process for each client
        if (Fork() == 0) {  // Child process
            Close(listenfd);  // Child closes listening socket
            doit(connfd);     // Handle the request
            Close(connfd);    // Close connection with client
            exit(0);          // Child exits
        }
        }
        Close(connfd);  // Parent closes connected socket (important)

    }


// Function to handle the SIGCHLD signal and reap child processes
void sigchld_handler(int sig)
{
    while (waitpid(-1, 0, WNOHANG) > 0);  // Reap all terminated child processes
    return;
}

void doit(int clientfd){
    int serverfd;
    char request_buf[MAXLINE], response_buf[MAXLINE], HTTPheader[MAXLINE];
    char method[MAXLINE], uri[MAXLINE], path[MAXLINE], hostname[MAXLINE], port[MAXLINE];

    rio_t request_rio, response_rio;
    size_t bytes;
    /* Read the request line */
    Rio_readinitb(&request_rio, clientfd);
    Rio_readlineb(&request_rio, request_buf, MAXLINE);
    printf("Request headers:\n %s", request_buf);
    sscanf(request_buf, "%s %s", method, uri);

    /*Only handle GET and HEAD methods*/
    if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {
        printf("Proxy does not implement this method");
        return;
        }
    
    parse_uri(uri, hostname, port, path);
    makeHTTPheader(HTTPheader, hostname, path, port, &request_rio);

    serverfd = Open_clientfd(hostname, port);
    if (serverfd < 0) {
        printf("Failed to connect to the end server");
        return;
    }
    Rio_readinitb(&response_rio, serverfd);
    Rio_writen(serverfd, HTTPheader, strlen(HTTPheader));
    while((bytes = Rio_readlineb(&response_rio, response_buf, MAXLINE)) != 0)
    {
        printf("proxy received %ld bytes, then send\n", bytes);
        Rio_writen(clientfd, response_buf, bytes);
    }
    Close(serverfd);
}

void parse_uri(char *uri, char *hostname, char *port, char *path)
{
    char *hostname_ptr = strstr(uri, "//") ? strstr(uri, "//") + 2 : uri;
    char *path_ptr = strchr(hostname_ptr, '/');
    char *port_ptr = strchr(hostname_ptr, ':');
    
    if (path_ptr)strcpy(path, path_ptr);
    else strcpy(path, "/");
    if (port_ptr) {
        strncpy(port, port_ptr + 1, path_ptr - port_ptr - 1);
        strncpy(hostname, hostname_ptr, port_ptr - hostname_ptr);
    } else {
        strcpy(port, "80");
        strncpy(hostname, hostname_ptr, path_ptr - hostname_ptr);}
}

void makeHTTPheader(char *http_header, char *hostname, char *path, char *port, rio_t *client_rio) {
    char buf[MAXLINE], request_header[MAXLINE], other_header[MAXLINE], host_header[MAXLINE];
    sprintf(request_header, "GET %s HTTP/1.0\r\n", path);
    while (Rio_readlineb(client_rio, buf, MAXLINE) > 0) {
        if (strcmp(buf, "\r\n") == 0)
            break;
        if (!strncasecmp(buf, "Host:", strlen("Host:"))) {
            strcpy(host_header, buf);
            continue;}
        if (!strncasecmp(buf, "Connection:", strlen("Connection")) || 
            !strncasecmp(buf, "Proxy-Connection:", strlen("Proxy-Connection")) || 
            !strncasecmp(buf, "User-Agent:", strlen("User-Agent"))) {
            strcat(other_header, buf);}}
    if (strlen(host_header) == 0) {
        sprintf(host_header, "Host: %s\r\n", hostname);}
    sprintf(http_header, "%s%sConnection: close\r\nProxy-Connection: close\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n\r\n", request_header, host_header);
}