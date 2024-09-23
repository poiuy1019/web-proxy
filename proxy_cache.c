#include "csapp.h"

// Define cache-related constants
#define MAX_CACHE_SIZE 1049000  // Maximum cache size (in bytes)
#define MAX_OBJECT_SIZE 102400  // Maximum size for an individual object (in bytes)

// Cache block structure
typedef struct {
    char uri[MAXLINE];           // Key: URI of the request
    char response[MAX_OBJECT_SIZE];  // Value: Server's response (binary data)
    size_t size;                 // Size of the stored response
    int lru_count;               // Least Recently Used count for eviction
} cache_block;

// Cache structure
typedef struct {
    cache_block *blocks;  // Pointer to dynamically allocated cache blocks
    int cache_count;      // Number of cache entries currently in use
    int lru_tracker;      // Track the least recently used entries
    size_t current_cache_size;  // Total size of cached objects (in bytes)
} Cache;

// Global cache variable
Cache cache;

/* Function Prototypes */
void cache_init(void);
int cache_find(char *uri, char *response, size_t *response_size);
void cache_store(char *uri, char *response, size_t size);
void cache_evict(void);
void doit(int clientfd);
void parse_uri(char *uri, char *hostname, char *port, char *path);
void makeHTTPheader(char *http_header, char *hostname, char *path, char *port, rio_t *client_rio);
void *thread(void *connfdp);

/* Initialize the cache */
void cache_init(void) {
    cache.cache_count = 0;  
    cache.lru_tracker = 0;  
    cache.current_cache_size = 0;  // Initialize the total cache size to 0

    // Dynamically allocate memory for cache blocks based on the number of entries we need.
    int num_cache_blocks = MAX_CACHE_SIZE / MAX_OBJECT_SIZE;  // Calculate the number of cache blocks we can have
    cache.blocks = (cache_block *)Malloc(sizeof(cache_block) * num_cache_blocks);
    
    if (cache.blocks == NULL) {
        fprintf(stderr, "Cache initialization failed: insufficient memory\n");
        exit(1);
    }

    // Initialize each cache block's size and LRU count
    for (int i = 0; i < num_cache_blocks; i++) {
        cache.blocks[i].size = 0;
        cache.blocks[i].lru_count = 0;
    }
}
/* Clean up cache memory */
void cache_cleanup(void) {
    if (cache.blocks) {
        free(cache.blocks);
    }
}

/* Search for a URI in the cache */
int cache_find(char *uri, char *response, size_t *response_size) {
    for (int i = 0; i < cache.cache_count; i++) {
        if (strcmp(cache.blocks[i].uri, uri) == 0) {  // URI match
            // Copy the cached binary response data into the output buffer
            memcpy(response, cache.blocks[i].response, cache.blocks[i].size);
            *response_size = cache.blocks[i].size;  // Return the size of the cached response
            cache.blocks[i].lru_count = ++cache.lru_tracker;  // Update LRU count
            return 1;  // Cache hit
        }
    }
    return 0;  // Cache miss
}

/* Store a new response in the cache */
void cache_store(char *uri, char *response, size_t size) {
    if (size > MAX_OBJECT_SIZE) {
        printf("Object too large to cache\n");
        return;
    }

    // Ensure the total cache size doesn't exceed MAX_CACHE_SIZE
    while (cache.current_cache_size + size > MAX_CACHE_SIZE) {
        cache_evict();  // Evict the least recently used cache block
    }

    // Store the new entry in the next available cache block
    strcpy(cache.blocks[cache.cache_count].uri, uri);  // Store the URI
    memcpy(cache.blocks[cache.cache_count].response, response, size);  // Store the response
    cache.blocks[cache.cache_count].size = size;  // Store the size
    cache.blocks[cache.cache_count].lru_count = ++cache.lru_tracker;  // Update LRU count

    // Update the total cache size
    cache.current_cache_size += size;
    
    // Increment the cache count
    cache.cache_count++;
}

/* Evict the least recently used cache block */
void cache_evict(void) {
    if (cache.cache_count == 0) return;  // No need to evict if the cache is empty

    int lru_index = 0;
    int min_lru = cache.blocks[0].lru_count;

    // Find the block with the lowest LRU count (least recently used)
    for (int i = 1; i < cache.cache_count; i++) {
        if (cache.blocks[i].lru_count < min_lru) {
            lru_index = i;
            min_lru = cache.blocks[i].lru_count;
        }
    }

    // Evict the block with the lowest LRU count
    printf("Evicting cache entry: %s\n", cache.blocks[lru_index].uri);

    // Update the total cache size
    cache.current_cache_size -= cache.blocks[lru_index].size;

    // Shift remaining cache blocks to remove the least used one
    for (int i = lru_index; i < cache.cache_count - 1; i++) {
        cache.blocks[i] = cache.blocks[i + 1];
    }

    // Decrease the cache count
    cache.cache_count--;
}

/* Proxy server main request handler (doit function) */
void doit(int clientfd) {
    int serverfd;
    char request_buf[MAXLINE], response_buf[MAXLINE], HTTPheader[MAXLINE];
    char method[MAXLINE], uri[MAXLINE], path[MAXLINE], hostname[MAXLINE], port[MAXLINE];
    char cache_buf[MAX_OBJECT_SIZE], cached_response[MAX_OBJECT_SIZE];
    rio_t request_rio, response_rio;
    size_t bytes, total_bytes = 0, cached_response_size;

    // Initialize the request buffer
    Rio_readinitb(&request_rio, clientfd);
    Rio_readlineb(&request_rio, request_buf, MAXLINE);
    printf("Request headers:\n %s", request_buf);
    sscanf(request_buf, "%s %s", method, uri);

    /*Only handle GET and HEAD methods*/
    if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {
        printf("Proxy does not implement this method\n");
        return;
    }

    // Check if the URI response is cached
    if (cache_find(uri, cached_response, &cached_response_size)) {
        printf("Serving from cache: %s\n", uri);
        Rio_writen(clientfd, cached_response, cached_response_size);  // Send cached response
        return;
    }

    // Parse the URI, prepare headers, and connect to the server
    parse_uri(uri, hostname, port, path);
    makeHTTPheader(HTTPheader, hostname, path, port, &request_rio);
    serverfd = Open_clientfd(hostname, port);

    if (serverfd < 0) {
        printf("Failed to connect to the end server\n");
        return;
    }

    // Forward the request to the server
    Rio_readinitb(&response_rio, serverfd);
    Rio_writen(serverfd, HTTPheader, strlen(HTTPheader));

    // Read the server's response and simultaneously cache and forward it
    while ((bytes = Rio_readnb(&response_rio, response_buf, MAXLINE)) > 0) {
        // Ensure that we do not exceed the cache buffer size
        if (total_bytes + bytes <= MAX_OBJECT_SIZE) {
            memcpy(cache_buf + total_bytes, response_buf, bytes);  // Append to cache buffer
        }
        total_bytes += bytes;
        Rio_writen(clientfd, response_buf, bytes);  // Send response to client
    }

    // Cache the response if the size is within the limit
    if (total_bytes <= MAX_OBJECT_SIZE) {
        cache_store(uri, cache_buf, total_bytes);
    }

    Close(serverfd);
}
/*URI parsing*/
void parse_uri(char *uri, char *hostname, char *port, char *path)
{
    char *hostname_ptr = strstr(uri, "//") ? strstr(uri, "//") + 2 : uri;
    char *path_ptr = strchr(hostname_ptr, '/');
    char *port_ptr = strchr(hostname_ptr, ':');
    
    if (path_ptr)
        strcpy(path, path_ptr);
    else
        strcpy(path, "/");

    if (port_ptr) {
        strncpy(port, port_ptr + 1, path_ptr - port_ptr - 1);
        strncpy(hostname, hostname_ptr, port_ptr - hostname_ptr);
    } else {
        strcpy(port, "80");
        strncpy(hostname, hostname_ptr, path_ptr - hostname_ptr);
    }
}

/*HTTP header generation*/
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

/*Thread routine*/
void *thread(void *connfdp) {
    int connfd = *((int *)connfdp);
    Pthread_detach(pthread_self());
    Free(connfdp);
    doit(connfd);
    Close(connfd);
    return NULL;
}

/*Main function*/
int main(int argc, char **argv) {
    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char hostname[MAXLINE], port[MAXLINE];
    pthread_t tid;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);}
    // Initialize the cache
    cache_init();
    // Open the listening socket
    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from %s:%s\n", hostname, port);
        Pthread_create(&tid, NULL, thread, connfdp);}
    
    // Free cache memory when program exits
    // cache_cleanup();
    return 0;
}
