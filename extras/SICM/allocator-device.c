#include "sicm_low.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>

static void initAlloc() __attribute__((constructor));
static struct sicm_device_list devices;
static struct sicm_device *small = NULL, *big = NULL;

#define BUFFER_SIZE 0x40960000
static char buffer[BUFFER_SIZE];
static int bind = 0;

#define THRESS (64*1024*1024)
#define VSTHRESS 0x40

static void verbprintf(const char * format, ...){
	#ifdef VERBOSE
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	#endif
}

void initAlloc(){
	int i;
	verbprintf("Using \"sicm_device_alloc\" allocator\n");
	devices = sicm_init();
	verbprintf("Using \"sicm_device_alloc\" allocator\n");
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
}

void *malloc(size_t size){
	if(size < VSTHRESS){
		verbprintf("size(0x%lx) < very small thress(0x%lx)\n", size, THRESS);
		verbprintf("Using initial memory\n");
		if(bind + size > BUFFER_SIZE){
			verbprintf("Out of initial memory\n");
			return NULL;
		}else{
			bind += size;
			return buffer + bind - size;
		}
	}
	if(size > THRESS){
		verbprintf("size(0x%lx) > thress(0x%lx)\n", size, THRESS);
		if(big){
			return sicm_device_alloc(big, size);
		}else{
			verbprintf("Using initial memory\n");
			if(bind + size > BUFFER_SIZE){
				verbprintf("Out of initial memory\n");
				return NULL;
			}else{
				bind += size;
				return buffer + bind - size;
			}
		}
	}else{
		verbprintf("size(0x%lx) < thress(0x%lx)\n", size, THRESS);
		if(small){
			return sicm_device_alloc(small, size);
		}else{
			verbprintf("Using initial memory\n");
			if(bind + size > BUFFER_SIZE){
				verbprintf("Out of initial memory\n");
				return NULL;
			}else{
				bind += size;
				return buffer + bind - size;
			}
		}
	}
}

void free(void *__ptr){
	
}
