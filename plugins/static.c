#include <migration.h>
#include <stdint.h>
#include <stdlib.h>
#include <config.h>
#include <shambles.h>

static uint64_t bitmap;
struct ShamblesPluginConfig config;
int cnt = 0;

static void bindAlloc(AllocEventType type, void *in, void *out, size_t size){
	int loc;
	if(type == ALLOC_EVENT_ALLOC && size >= config.sizeThresshold){
		struct ShamblesChunk chunk;
		chunk.start = out;
		chunk.size = size;
		loc = __atomic_fetch_add(&cnt, 1, __ATOMIC_RELAXED);
		if(bitmap & (1 << loc)){
			bindSlow(&config, &chunk);
		}else{
			bindFast(&config, &chunk);
		}
	}
}

void shambles_init(void (**reportAllocFunc)(AllocEventType, void *, void *, size_t)){
	char *env;
	if(ShamblesPluginConfigInit(&config)){
		abort();
	}
	if((env = getenv("SHMBLES_SLOW_BITMASK"))==NULL){
		bitmap = 0;
	}else{
		bitmap = strtol(env, NULL, 0);
	}
	*reportAllocFunc = &bindAlloc;
}
