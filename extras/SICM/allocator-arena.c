#include "sicm_low.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

static void initAlloc() __attribute__((constructor));
static struct sicm_device_list devices, smallList, bigList;
static struct sicm_device *small = NULL, *big = NULL;
static sicm_arena smallArena = NULL, bigArena = NULL;

#define ARENA_SIZE 0x1000000000

#define THRESS (64*1024*1024)

void initAlloc(){
	int i;
	devices = sicm_init();
	for(i = 0; i < devices.count; i++){
		struct sicm_device* device = devices.devices[i];
		sicm_pin(device);
		if(sicm_capacity(device)){
			if(device->tag == SICM_DRAM){
				small = device;
			}
			if(device->tag == SICM_OPTANE){
				big = device;
			}
		}
	}
	if(small == NULL){
		small = big;
	}
	if(big == NULL){
		big = small;
	}
	smallList.devices = &small;
	bigList.devices = &big;
	smallList.count = 1;
	bigList.count = 1;
	if(small != NULL){
		smallArena = sicm_arena_create(ARENA_SIZE, SICM_ALLOC_STRICT, &smallList);
	}
	if(big != NULL){
		bigArena = sicm_arena_create(ARENA_SIZE, SICM_ALLOC_STRICT, &bigList);
	}
}

void *malloc(size_t size){
	if(size > THRESS){
		return sicm_arena_alloc(bigArena, size);
	}else{
		return sicm_arena_alloc(smallArena, size);
	}
}

void *realloc(void *__ptr, size_t __size){
	return sicm_realloc(__ptr, __size);
}

void free(void *__ptr){
	sicm_free(__ptr);
}

void *calloc(size_t number, size_t size){
	void *ptr;
	ptr = malloc(number*size);
	memset(ptr, 0, number*size);
	
	return ptr;
}

void *aligned_alloc(size_t alignment, size_t size){
	if(size > THRESS){
		return sicm_arena_alloc_aligned(bigArena, size, alignment);
	}else{
		return sicm_arena_alloc_aligned(smallArena, size, alignment);
	}
}

int posix_memalign(void **ptr, size_t alignment, size_t size){
	*ptr = aligned_alloc(alignment, size);
	return 0;
}

