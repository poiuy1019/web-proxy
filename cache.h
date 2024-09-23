#include <stdio.h>
#include "csapp.h"
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

// Cache block structure
typedef struct {
    char uri[MAXLINE];          // Key: URI of the request
    char response[MAX_OBJECT_SIZE];  // Value: Server's response
    size_t size;                // Size of the stored response
    int lru_count;              // Least Recently Used count for eviction
} cache_block;

// Cache structure
typedef struct {
    cache_block blocks[MAX_CACHE_SIZE];  // Cache entries
    int cache_count;               // Number of cache entries
    int lru_tracker;               // Track the least recently used entries
} Cache;