#include <migration.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <config.h>
#include <shambles.h>
#include <jemalloc.h>

#define FRACTION_SHIFT 20
#define PAGE_SHIFT 12
static uint64_t *fractions;

static struct ShamblesPluginConfig config;
static int current = 0, len;

static uint64_t fraction(char *str){
	uint64_t out;
	out = (1 << (FRACTION_SHIFT + 1))*atof(str);
	out = (out & 1)?((out >> 1) + 1):(out >> 1);
	out = (out < (1 << FRACTION_SHIFT))?out:(1 << FRACTION_SHIFT);
	return out;
}

static inline size_t fastSize(size_t size, uint64_t fract){
	size = ((size << PAGE_SHIFT) * fract) >> (FRACTION_SHIFT + PAGE_SHIFT - 1);
	return (size >> 1) + (size & 1);
}

static void bindAlloc(AllocEventType type, void *in, void *out, size_t size){
	int loc;
	if(type == ALLOC_EVENT_ALLOC && size >= config.sizeThresshold){
		struct ShamblesChunk chunk;
		loc = __atomic_fetch_add(&current, 1, __ATOMIC_RELAXED);
		if(loc >= len || (fractions[loc] == 0)){
			chunk.start = out;
			chunk.size = size;
			bindSlow(&config, &chunk);
		}else if((fractions[loc] >= (1 << FRACTION_SHIFT))){
			chunk.start = out;
			chunk.size = size;
			bindFast(&config, &chunk);
		}else{
			chunk.start = out;
			chunk.size = fastSize(size, fractions[loc]);
			bindFast(&config, &chunk);
			chunk.start = out + chunk.size;
			chunk.size = size - chunk.size;
			bindSlow(&config, &chunk);
		}
	}
}

void shambles_init(void (**reportAllocFunc)(AllocEventType, void *, void *, size_t)){
	char *env;
	if(ShamblesPluginConfigInit(&config)){
		abort();
	}
	if((env = getenv("SHAMBLES_ALLOC_FAST_FRACTIONS"))==NULL){
		len = 0;
	}else{
		char *next;
		int i;
		for(i = 0; env[i] != '\0'; i++){
			len += (env[i] == ',');
		}
		len++;
		fractions = plugin_malloc(len * sizeof(uint64_t *));
		for(i = 0; i < len - 1; i++){
			next = strstr(env, ",");
			*next = '\0';
			fractions[i] = fraction(env);
			env = next + 1;
		}
		fractions[len - 1] = fraction(env);
	}
	*reportAllocFunc = &bindAlloc;
}
