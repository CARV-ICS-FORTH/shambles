#include "include/logger.h"
#include "include/counters.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <shambles.h>
#include <stdlib.h>
static ShamblesCounters *shamblesCounters;

void dumpMap(){
	
}

void loggerInit(){
	int fd;
	char fname[128];
	sprintf(fname, "counters.%d.log", getpid());
	fd = open(fname, O_RDWR | O_TRUNC | O_CREAT, 0777);
	if(fd < 0){
		perror("shamblesCounters: open failed: ");
		abort();
	}
	if(posix_fallocate(fd, 0, 0x1000)){
		perror("shamblesCounters fallocate failed: ");
		abort();
	}
	shamblesCounters = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, 0);
	if(shamblesCounters == MAP_FAILED){
		perror("shamblesCounters mmap failed: ");
		abort();
	}
}

void logSample(void *addr, void *code, uint16_t flags){
	if(flags & LOG_TRACKED){
		if(flags & LOG_HIT){
			__atomic_fetch_add(&shamblesCounters->hits, 1, __ATOMIC_RELAXED);
		}else{
			__atomic_fetch_add(&shamblesCounters->misses, 1, __ATOMIC_RELAXED);
		}
	}else{
		__atomic_fetch_add(&shamblesCounters->small, 1, __ATOMIC_RELAXED);
	}
	if(flags & LOG_MIGRATION){
		__atomic_fetch_add(&shamblesCounters->migrations, 1, __ATOMIC_RELAXED);
	}
	if(flags & LOG_MIGRATION1){
		__atomic_fetch_add(&shamblesCounters->migrations, 1, __ATOMIC_RELAXED);
	}
}

void logAlloc(AllocEventType type, void *in, void *out, size_t size){
	if(type == ALLOC_EVENT_ALLOC){
		__atomic_fetch_add(&shamblesCounters->allocs, 1, __ATOMIC_RELAXED);
	}else if(type == ALLOC_EVENT_FREE){
		__atomic_fetch_add(&shamblesCounters->frees, 1, __ATOMIC_RELAXED);
	}else{
		__atomic_fetch_add(&shamblesCounters->reallocs, 1, __ATOMIC_RELAXED);
	}
}
