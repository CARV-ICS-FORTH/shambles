#include "shambles.h"
#include <stdlib.h>
#include <stdio.h>
#include <execinfo.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

typedef struct{
	AllocEventType type;
	void *in, *out;
	size_t size;
	void *callsite[10];
	struct timespec timestamp;
}AllocEvent __attribute__ ((aligned (128)));

typedef struct{
	void *addr;
	char code[16];
	struct timespec timestamp;
}Sample __attribute__ ((aligned (64)));

static int allocCnt = 0, sampleCnt = 0;
static volatile int *allocCntFile, *sampleCntFile;
static  AllocEvent *AllocEventPtr;
static Sample *samplePtr;
static int allocfd, samplefd;
static off_t allocOffset = 0, sampleOffset = 0;
static pid_t pid;
static pthread_mutex_t logLock = PTHREAD_MUTEX_INITIALIZER;

static void logAlloc(AllocEventType type, void *in, void *out, size_t size){
	int n;
	pthread_mutex_lock(&logLock);
	if((((uint64_t)AllocEventPtr) & 0xffffff) == 0){
		if(allocOffset){
			munmap(AllocEventPtr, 0x1000000);
		}
		allocOffset += 0x1000000;
		if(posix_fallocate(allocfd, allocOffset, 0x1000000)){
			perror("fallocate failed");
			abort();
		}
		AllocEventPtr = mmap(NULL, 0x1000000, PROT_READ | PROT_WRITE, MAP_SHARED, allocfd, allocOffset);
		if(AllocEventPtr == MAP_FAILED){
			perror("mmap failed: ");
			abort();
		}
	}

	allocCnt++;
	*allocCntFile = allocCnt;
	AllocEventPtr->type = type;
	AllocEventPtr->in = in;
	AllocEventPtr->out = out;
	AllocEventPtr->size = size;
	n = backtrace(AllocEventPtr->callsite, 10);
	for(int i = n; i < 10; i++){
		AllocEventPtr->callsite[i] = NULL;
	}
	clock_gettime(CLOCK_MONOTONIC, &AllocEventPtr->timestamp);
	AllocEventPtr++;
	pthread_mutex_unlock(&logLock);
}

static void logAllocInit(){
	char fname[128];
	sprintf(fname, "allocs.%d.log", pid);
	allocfd = open(fname, O_RDWR | O_TRUNC | O_CREAT, 0777);
	if(allocfd < 0){
		perror("open failed: ");
		abort();
	}
	if(posix_fallocate(allocfd, allocOffset, 0x1000000)){
		perror("fallocate failed wtf1: ");
		abort();
	}
	AllocEventPtr = mmap(NULL, 0x1000000, PROT_READ | PROT_WRITE, MAP_SHARED, allocfd, allocOffset);
	if(AllocEventPtr == MAP_FAILED){
		perror("mmap failed: ");
		abort();
	}
	allocCntFile = (volatile int *)AllocEventPtr;
	AllocEventPtr++;
	*allocCntFile = 0;
}

void logSampleInit(){
	char fname[128];
	sprintf(fname, "samples.%d.log", pid);
	samplefd = open(fname, O_RDWR | O_TRUNC | O_CREAT, 0777);
	if(allocfd < 0){
		perror("open failed: ");
		abort();
	}
	if(posix_fallocate(samplefd, 0, 0x200000)){
		perror("fallocate 0 failed: ");
		abort();
	}
	samplePtr = mmap(NULL, 0x200000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, samplefd, 0);
	if(samplePtr == MAP_FAILED){
		perror("mmap 0 failed: ");
		abort();
	}
	sampleCntFile = (volatile int *)samplePtr;
	samplePtr++;
	*sampleCntFile = 0;
	{
		FILE *in;
		FILE *out;
		int c;
		//suboptimal but works...
		char fname[128];
		sprintf(fname, "maps.%d.log", pid);
		in = fopen("/proc/self/maps", "r");

		out = fopen(fname, "w");
		if(in == NULL){
			perror("fopen");
			abort();
		}
		if(out == NULL){
			perror("fopen");
			abort();
		}
		do {
			c = fgetc(in);
			if( feof(in) ) {
				break ;
			}
			fputc(c, out);
		} while(1);
		fclose(in);
		fclose(out);
	}
}

void logSample(void *addr, void *code){
	if((((uint64_t)samplePtr) & 0x1fffff) == 0){
		if(sampleOffset){
			munmap(samplePtr, 0x200000);
		}
		sampleOffset += 0x200000;
		if(posix_fallocate(samplefd, sampleOffset, 0x200000)){
			perror("fallocate failed: ");
			abort();
		}
		samplePtr = mmap(NULL, 0x200000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, samplefd, sampleOffset);
		if(samplePtr == MAP_FAILED){
			perror("mmap failed: ");
			abort();
		}
	}

	sampleCnt++;
	*sampleCntFile = sampleCnt;
	samplePtr->addr = addr;
	memcpy(samplePtr->code, code, 15);
	clock_gettime(CLOCK_MONOTONIC, &samplePtr->timestamp);
	samplePtr++;
}

void *sampler_thread_func(void * useless){
	int fd;
	void *buff[2];
	fd = open("/sys/kernel/debug/sampler/samples", O_RDONLY);
	if(fd < 0) return NULL;
	logSampleInit();
	while(1){
		while(read(fd, buff, 16) != 16);
		logSample(buff[0], buff[1]);
	}
}

void shambles_init(void (**reportAllocFunc)(AllocEventType, void *, void *, size_t)){
	void *hack[1];
	pthread_t thread;

	pid = getpid();
	logAllocInit();
	backtrace(hack, 1);
	*reportAllocFunc = &logAlloc;

	pthread_create(&thread, NULL, &sampler_thread_func, NULL);
	pthread_detach(thread);
}
