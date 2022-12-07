#include "sicm_low.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

//static void initAlloc() __attribute__((constructor));
static struct sicm_device_list devices, smallList, bigList;
static struct sicm_device *small = NULL, *big = NULL;
static sicm_arena smallArena = NULL, bigArena = NULL;

static void my_init_hook(void);
static void *my_malloc_hook(size_t, const void *);
static void *my_realloc_hook(void *__ptr, size_t __size, const void *);
static void *my_memalign_hook(size_t alignment, size_t size, const void *);
static void my_free_hook(void *__ptr, const void *);

void (*__malloc_initialize_hook)(void) = my_init_hook;

#define ARENA_SIZE 0x1000000000

#define THRESS (64*1024*1024)

void my_init_hook(){
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
	__malloc_hook = my_malloc_hook;
	__realloc_hook = my_realloc_hook;
	__memalign_hook = my_memalign_hook;
	__free_hook = my_free_hook;
}

void *my_malloc_hook(size_t size, const void *caller){
	if(size > THRESS){
		return sicm_arena_alloc(bigArena, size);
	}else{
		return sicm_arena_alloc(smallArena, size);
	}
}

void *my_realloc_hook(void *__ptr, size_t __size, const void *caller){
	return sicm_realloc(__ptr, __size);
}

void my_free_hook(void *__ptr, const void *caller){
	sicm_free(__ptr);
}

void *calloc(size_t number, size_t size){
	void *ptr;
	ptr = malloc(number*size);
	memset(ptr, 0, number*size);
	
	return ptr;
}

void *my_memalign_hook(size_t alignment, size_t size, const void *caller){
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

