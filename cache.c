#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;
int cache_active; //0 if cache is not active, 1 if is
int entry_num = -1;

//Check to make sure the parameters are valid
int validate_cache_parameters(int diskNum, int blockNum, const uint8_t *buf) {
	if (cache_active == 0) {
		return -1;
	}

	if (diskNum < 0 || diskNum > 15) {
		return -1;
	}
	if (blockNum < 0 || blockNum > 255) {
		return -1;
	}
	if (buf == 0) {
		return -1;
	}
	
	return 1;
}

//Create a cache with size num_entries
int cache_create(int num_entries) {
	
	//Check if there's already an active cache
	if (cache_active == 1) {
		return -1;
	}
	
	//Make sure size is in bounds
	if (num_entries < 2 || num_entries > 4096) {
		return -1;
	}
	
	cache_active = 1; //Set active status
	cache_size = num_entries;
	cache = malloc(sizeof(cache_entry_t) * cache_size); //Allocate memory for the entire cache

  return 1;
}

//Destroy cache
int cache_destroy(void) {

	//Make sure there is an active cache to destroy
	if (cache_active == 0) {
		return -1;
	}
	
	cache_active = 0;
	free(cache); //Free memory allocated to the cache
	cache = NULL;
	cache_size = 0;
	
  return -10;
}

//Look for an entry in the cache and copy this entry into buf
int cache_lookup(int disk_num, int block_num, uint8_t *buf) {

	//Validate parameters
	if (validate_cache_parameters(disk_num, block_num, buf) == -1) {
		return -1;	
	}

	num_queries += 1; //Increment queries for efficiency calculation
	if (entry_num == -1) {
		return -1;
	}
	
	//Look for the entry in the cache
	for(int i = 0; i < cache_size; i++) {
		if(cache[i].disk_num == disk_num && cache[i].block_num == block_num) {
			memcpy(buf, cache[i].block, 256);
			num_hits += 1;
			clock += 1;
			cache[i].access_time = clock;
			return 1;
		}
	}
	
	return -1;

}

//Update a specific cache entry
void cache_update(int disk_num, int block_num, const uint8_t *buf) {

	//Look for the entry
	for(int i = 0; i < cache_size; i++) {
		if(cache[i].disk_num == disk_num && cache[i].block_num == block_num) {
			memcpy(cache[i].block, buf, 256);
			cache[i].access_time = clock;
			return;
		}
	}
	
}

//Insert a new entry
int cache_insert(int disk_num, int block_num, const uint8_t *buf) {

	if (validate_cache_parameters(disk_num, block_num, buf) == -1) {
		return -1;
	}
	
	clock += 1;
	//Make sure the entry isn't already in the cache
	for (int i = 0; i < cache_size; i++) {
		if (cache[i].disk_num == disk_num && cache[i].block_num == block_num && cache[i].valid == true) {
			cache_update(disk_num, block_num, buf);
			return -1;
		}
	}

	//Insert entry into cache
	if (entry_num + 1 < cache_size) {
		memcpy(cache[entry_num + 1].block, buf, 256);
		cache[entry_num + 1].valid = true;
		cache[entry_num + 1].disk_num = disk_num;
		cache[entry_num + 1].block_num = block_num;
		cache[entry_num + 1].access_time = clock;
		entry_num += 1;
		return 1;
	}
        
	
	//If cache is full, find the oldest entry and replace it with the new entry
	cache_entry_t* oldestEntry = &cache[0];
	for(int i = 1; i < cache_size; i++) {
		if (cache[i].access_time < oldestEntry->access_time) {
			oldestEntry = &cache[i];
		}
	}
	
	entry_num += 1;
	memcpy(oldestEntry->block, buf, 256);
	oldestEntry->disk_num = disk_num;
	oldestEntry->block_num = block_num;
	oldestEntry->access_time = clock;
	
  return 1;
}

bool cache_enabled(void) {

	if (cache_active == 1) {
		return true;
	}
	
  return false;
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}

