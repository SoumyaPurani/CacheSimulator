//
// Created by Alex Brodsky on 2023-11-13.
//

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "cache.h"
#include "main.h"

typedef struct CacheLine {
    unsigned long tag;
    int timestamp;
    char *data;
} CacheLine;

typedef struct Cache {
    CacheLine **sets;
    unsigned int word_size;
    unsigned int set_count;
    unsigned int line_count;
    unsigned int line_size;
    int clock;
} Cache;

// Helper function to find the least recently used line in a set
int find_lru_line(CacheLine *set, unsigned int line_count) {
    int lru_index = 0;
    int min_timestamp = set[0].timestamp;

    for (int i = 1; i < line_count; i++) {
        if (set[i].timestamp < min_timestamp) {
            lru_index = i;
            min_timestamp = set[i].timestamp;
        }
    }

    return lru_index;
}

// Function to create a new cache
extern cache_t cache_new(unsigned int word_size, unsigned int sets,
                         unsigned int lines, unsigned int line_size) {
    // Allocate memory for the cache
    Cache *cache = malloc(sizeof(Cache));
    // If memory allocation fails, return NULL
    if (!cache){
        return NULL;
    }

    // Initialize the cache parameters
    cache->word_size = word_size;
    cache->set_count = sets;
    cache->line_count = lines;
    cache->line_size = line_size;
    cache->clock = 0;

    // Allocate memory for the cache sets
    cache->sets = malloc(sizeof(CacheLine *) * sets);
    // If memory allocation fails, free the cache and return NULL
    if (!cache->sets) {
        free(cache);
        return NULL;
    }

    // For each set in the cache
    for (unsigned int i = 0; i < sets; i++) {
        // Allocate memory for the cache lines
        cache->sets[i] = malloc(sizeof(CacheLine) * lines);
        // If memory allocation fails
        if (!cache->sets[i]) {
            // Free the previously allocated sets
            for (unsigned int j = 0; j < i; j++) {
                free(cache->sets[j]);
            }
            // Free the sets and the cache, then return NULL
            free(cache->sets);
            free(cache);
            return NULL;
        }

        // For each line in the set
        for (unsigned int j = 0; j < lines; j++) {
            // Allocate memory for the data in the line
            cache->sets[i][j].data = malloc(line_size);
            // If memory allocation fails
            if (!cache->sets[i][j].data) {
                // Free the previously allocated data
                for (unsigned int k = 0; k < j; k++) {
                    free(cache->sets[i][k].data);
                }
                // Free the lines, the sets, and the cache, then return NULL
                for (unsigned int k = 0; k <= i; k++) {
                    free(cache->sets[k]);
                }
                free(cache->sets);
                free(cache);
                return NULL;
            }
            // Initialize the tag and timestamp of the line
            cache->sets[i][j].tag = 0;
            cache->sets[i][j].timestamp = 0;
        }
    }

    // Return the newly created cache
    return (cache_t)cache;
}

extern void cache_load_word(cache_t handle, unsigned long address, void *word) {
    Cache *cache = (Cache *)handle;

    unsigned long offset = address % cache->line_size;
    unsigned long index = (address / cache->line_size) % cache->set_count;
    unsigned long tag = address / (cache->line_size * cache->set_count);

    // Search for the tag in the set
    for (int i = 0; i < cache->line_count; i++) {
        if (cache->sets[index][i].tag == tag && cache->sets[index][i].timestamp != 0) {
            // Cache hit
            memcpy(word, cache->sets[index][i].data + offset, cache->word_size / 8);
            cache->sets[index][i].timestamp = ++cache->clock;
            return;
        }
    }

    // Cache miss
    int lru_index = find_lru_line(cache->sets[index], cache->line_count);

    // Load block from next level
    load_block_from_next_level(address, cache->sets[index][lru_index].data);

    // Update cache line information
    cache->sets[index][lru_index].tag = tag;
    cache->sets[index][lru_index].timestamp = ++cache->clock;

    // Copy the word into the buffer
    memcpy(word, cache->sets[index][lru_index].data + offset, cache->word_size / 8);
}

extern void cache_load_block(cache_t handle, unsigned long address, void *block,
                             unsigned int size) {
    Cache *cache = (Cache *)handle;
    unsigned long index = (address / cache->line_size) % cache->set_count;
    unsigned long tag = address / (cache->line_size * cache->set_count);

    // Search for the tag in the set
    for (int i = 0; i < cache->line_count; i++) {
        if (cache->sets[index][i].tag == tag && cache->sets[index][i].timestamp != 0) {
            // Cache hit
            memcpy(block, cache->sets[index][i].data, size); // Copy the entire block
            cache->sets[index][i].timestamp = ++cache->clock;
            return;
        }
    }

    // Cache miss
    int lru_index = find_lru_line(cache->sets[index], cache->line_count);

    // Load block from next level
    load_block_from_next_level(address, cache->sets[index][lru_index].data);

    // Update cache line information
    cache->sets[index][lru_index].tag = tag;
    cache->sets[index][lru_index].timestamp = ++cache->clock;

    // Copy the entire block into the buffer
    memcpy(block, cache->sets[index][lru_index].data, size);
}

extern void cache_store_word(cache_t handle, unsigned long address, void *word) {
    Cache *cache = (Cache *)handle;

    unsigned long offset = address % cache->line_size;
    unsigned long index = (address / cache->line_size) % cache->set_count;
    unsigned long tag = address / (cache->line_size * cache->set_count);

    // Search for the tag in the set
    for (int i = 0; i < cache->line_count; i++) {
        if (cache->sets[index][i].tag == tag && cache->sets[index][i].timestamp != 0) {
            // Cache hit
            memcpy(cache->sets[index][i].data + offset, word, cache->word_size / 8);
            cache->sets[index][i].timestamp = ++cache->clock;
            return;
        }
    }

    // Cache miss
    int lru_index = find_lru_line(cache->sets[index], cache->line_count);

    // Load block from next level
    load_block_from_next_level(address, cache->sets[index][lru_index].data);

    // Update cache line information
    cache->sets[index][lru_index].tag = tag;
    cache->sets[index][lru_index].timestamp = ++cache->clock;

    // Store the word in the cache
    memcpy(cache->sets[index][lru_index].data + offset, word, cache->word_size / 8);
}