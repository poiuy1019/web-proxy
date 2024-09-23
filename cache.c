#include "csapp.h"

// Define cache-related constants
#define MAX_CACHE_SIZE 1049000  // Maximum cache size (in bytes) 1MB
#define MAX_OBJECT_SIZE 102400  // Maximum size for an individual object (in bytes) 100kb

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
} Cache;

// Global cache variable
Cache cache;

/* Function Prototypes */
void cache_init(Cache *cache);
int cache_find(Cache *cache, char *uri, char *response, size_t *response_size);
void cache_store(Cache *cache, char *uri, char *response, size_t size);
void cache_evict(Cache *cache);

/* Initialize the cache */
void cache_init(Cache *cache) {
    cache->cache_count = 0;  // Initialize the number of entries in the cache
    cache->lru_tracker = 0;  // Initialize LRU tracker
     cache.current_cache_size = 0;  // Initialize the total cache size to 0
     
    int num_entries = MAX_CACHE_SIZE / MAX_OBJECT_SIZE;  // Calculate the number of cache blocks we can have
    cache.blocks = (cache_block *)malloc(sizeof(cache_block) * num_entries);
    
    if (cache.blocks == NULL) {
        fprintf(stderr, "Cache initialization failed: insufficient memory\n");
        exit(1);
    }

    // Initialize each cache block's size and LRU count
    for (int i = 0; i < num_entries; i++) {
        cache.blocks[i].size = 0;
        cache.blocks[i].lru_count = 0;
    }
}

/* Search for a URI in the cache */
int cache_find(Cache *cache, char *uri, char *response, size_t *response_size) {
    for (int i = 0; i < cache->cache_count; i++) {
        if (strcmp(cache->blocks[i].uri, uri) == 0) {  // URI match
            // Copy the cached binary response data into the output buffer
            memcpy(response, cache->blocks[i].response, cache->blocks[i].size);
            *response_size = cache->blocks[i].size;  // Return the size of the cached response
            cache->blocks[i].lru_count = ++cache->lru_tracker;  // Update LRU count
            return 1;  // Cache hit
        }
    }
    return 0;  // Cache miss
}

/* Store a new response in the cache */
void cache_store(Cache *cache, char *uri, char *response, size_t size) {
    if (size > MAX_OBJECT_SIZE) {
        printf("Object too large to cache\n");
        return;
    }

    // Evict the least recently used cache entry if the cache is full
    if (cache->cache_count >= MAX_CACHE_SIZE) {
        cache_evict(cache);
    }

    // Store the new entry in the next available cache block
    strcpy(cache->blocks[cache->cache_count].uri, uri);  // Store the URI
    memcpy(cache->blocks[cache->cache_count].response, response, size);  // Store the response
    cache->blocks[cache->cache_count].size = size;  // Store the size
    cache->blocks[cache->cache_count].lru_count = ++cache->lru_tracker;  // Update LRU count
    cache->cache_count++;  // Increment cache count
}

/* Evict the least recently used cache block */
void cache_evict(Cache *cache) {
    if (cache->cache_count == 0) return;  // No need to evict if the cache is empty

    int lru_index = 0;
    int min_lru = cache->blocks[0].lru_count;

    // Find the block with the lowest LRU count (least recently used)
    for (int i = 1; i < cache->cache_count; i++) {
        if (cache->blocks[i].lru_count < min_lru) {
            lru_index = i;
            min_lru = cache->blocks[i].lru_count;
        }
    }

    // Evict the block with the lowest LRU count
    printf("Evicting cache entry: %s\n", cache->blocks[lru_index].uri);

    // Shift remaining cache blocks to remove the least used one
    for (int i = lru_index; i < cache->cache_count - 1; i++) {
        cache->blocks[i] = cache->blocks[i + 1];
    }

    cache->cache_count--;  // Reduce cache count
}
