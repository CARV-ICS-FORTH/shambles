#include <migration.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <config.h>
#include <shambles.h>
#include <jemalloc.h>
#include <logger.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#define FRACTION_SHIFT 20
#define PAGE_SHIFT 12
static uint64_t *fractions;

static struct ShamblesPluginConfig config;
static pthread_mutex_t structLock = PTHREAD_MUTEX_INITIALIZER;
static int current = 0, len;
static int invert;

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

#ifdef SHAMBLES_LOGGER_ENABLED
static void bindAlloc(AllocEventType type, void *in, void *out, size_t size){
	int loc;
	if(type == ALLOC_EVENT_ALLOC){
		if(size >= config.sizeThresshold){
			struct ShamblesRegion *region;
			struct ShamblesChunk chunk;
			loc = __atomic_fetch_add(&current, 1, __ATOMIC_RELAXED);
			pthread_mutex_lock(&structLock);
			if(loc >= len || (fractions[loc] == 0)){
				region = addRegionChunks(out, size, 1, &size);
				if(invert){
					region->chunks->privData = (void *)(1);
					bindFast(&config, region->chunks);
				}else{
					region->chunks->privData = (void *)(0);
					bindSlow(&config, region->chunks);
				}
			}else if((fractions[loc] >= (1 << FRACTION_SHIFT))){
				region = addRegionChunks(out, size, 1, &size);
				if(invert){
					region->chunks->privData = (void *)(0);
					bindSlow(&config, region->chunks);
				}else{
					region->chunks->privData = (void *)(1);
					bindFast(&config, region->chunks);
				}
			}else{
				size_t sizes[2];
				sizes[0] = fastSize(size, fractions[loc]);
				sizes[1] = size - sizes[0];
				region = addRegionChunks(out, size, 2, sizes);
				if(invert){
					region->chunks->privData = (void *)(0);
					region->chunks[1].privData = (void *)(1);
					bindSlow(&config, region->chunks);
					bindFast(&config, region->chunks + 1);
				}else{
					region->chunks->privData = (void *)(1);
					region->chunks[1].privData = (void *)(0);
					bindFast(&config, region->chunks);
					bindSlow(&config, region->chunks + 1);
				}
			}
			pthread_mutex_unlock(&structLock);
		}
	}else if(type == ALLOC_EVENT_REALLOC){
		//todo
	}else{
		struct ShamblesRegion *region;
		pthread_mutex_lock(&structLock);
		if((region = getRegion(in)) != NULL){
			deleteRegion(region);
		}
		pthread_mutex_unlock(&structLock);
	}
	LOG_ALLOC(type, in, out, size);
}
#else
static void bindAlloc(AllocEventType type, void *in, void *out, size_t size){
	int loc;
	if(type == ALLOC_EVENT_ALLOC && size >= config.sizeThresshold){
		struct ShamblesChunk chunk;
		loc = __atomic_fetch_add(&current, 1, __ATOMIC_RELAXED);
		if(loc >= len || (fractions[loc] == 0)){
			chunk.start = out;
			chunk.size = size;
			if(invert){
				bindFast(&config, &chunk);
			}else{
				bindSlow(&config, &chunk);
			}
		}else if((fractions[loc] >= (1 << FRACTION_SHIFT))){
			chunk.start = out;
			chunk.size = size;
			if(invert){
				bindSlow(&config, &chunk);
			}else{
				bindFast(&config, &chunk);
			}
		}else{
			chunk.start = out;
			chunk.size = fastSize(size, fractions[loc]);
			if(invert){
				bindSlow(&config, &chunk);
			}else{
				bindFast(&config, &chunk);
			}
			chunk.start = out + chunk.size;
			chunk.size = size - chunk.size;
			if(invert){
				bindFast(&config, &chunk);
			}else{
				bindSlow(&config, &chunk);
			}
		}
	}
}
#endif

static void handleSample(void *addr){
	struct ShamblesChunk *chunk;
	pthread_mutex_lock(&structLock);
	if((chunk = getChunkInfoNU(addr)) != NULL){
		if(chunk->privData == (void *)(0)){
			LOG_SAMPLE(addr, NULL, LOG_TRACKED);
		}else{
			LOG_SAMPLE(addr, NULL, LOG_TRACKED | LOG_HIT);
		}
	}else{
		LOG_SAMPLE(addr, NULL, 0);
	}
	pthread_mutex_unlock(&structLock);
}

static void *logThread(void *unused){
	int fd;
	void *buff[2];
	fd = open("/sys/kernel/debug/sampler/samples", O_RDONLY);
	if(fd < 0) return NULL;
	while(1){
		while(read(fd, buff, 16) != 16);
		handleSample(buff[0]);
	}
}

void shambles_init(void (**reportAllocFunc)(AllocEventType, void *, void *, size_t)){
	char *env;
	pthread_t thread;
	if(ShamblesPluginConfigInit(&config)){
		abort();
	}
	if((env = getenv("SHAMBLES_ALLOC_FAST_FRACTIONS"))==NULL && (env = getenv("SHAMBLES_ALLOC_SLOW_FRACTIONS"))==NULL){
		invert = 0;
		len = 0;
	}else{
		if(getenv("SHAMBLES_ALLOC_FAST_FRACTIONS")==NULL){
			invert = 1;
		}else{
			invert = 0;
		}
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
#ifdef SHAMBLES_LOGGER_ENABLED
	shamblesStructsInit(&config);
	INIT_LOGGER();
	pthread_create(&thread, NULL, &logThread, NULL);
	pthread_detach(thread);
#endif
	*reportAllocFunc = &bindAlloc;
}
