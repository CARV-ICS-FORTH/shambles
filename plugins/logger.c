#include <stdio.h>
#include <execinfo.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include "include/tracking.h"
#include "include/logger.h"

static int pid;

static int allocCnt = 0, sampleCnt = 0, migCnt = 0;
static volatile int *allocCntFile, *sampleCntFile, *migCntFile;
static AllocEvent *allocEventPtr;
static Sample *samplePtr;
static MigrationEvent *migPtr;
static int allocfd, samplefd, migfd;
static pthread_mutex_t logLock = PTHREAD_MUTEX_INITIALIZER, migLock = PTHREAD_MUTEX_INITIALIZER;
static off_t allocOffset = 0, sampleOffset = 0, migOffset = 0;

void dumpMap(){
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

static void logAllocInit(){
	//void *hack[1];
	//backtrace(hack, 1);
	char fname[128];
	sprintf(fname, "allocs.%d.log", pid);
	allocfd = open(fname, O_RDWR | O_TRUNC | O_CREAT, 0777);
	if(allocfd < 0){
		perror("open failed: ");
		abort();
	}
	if(posix_fallocate(allocfd, allocOffset, 0x200000)){
		perror("fallocate failed wtf1: ");
		abort();
	}
	allocEventPtr = mmap(NULL, 0x200000, PROT_READ | PROT_WRITE, MAP_SHARED, allocfd, allocOffset);
	if(allocEventPtr == MAP_FAILED){
		perror("mmap failed: ");
		abort();
	}
	allocCntFile = (volatile int *)allocEventPtr;
	allocEventPtr++;
	*allocCntFile = 0;
}

static void logSampleInit(){
	char fname[128];
	sprintf(fname, "samples.%d.log", pid);
	samplefd = open(fname, O_RDWR | O_TRUNC | O_CREAT, 0777);
	if(samplefd < 0){
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
}

static void logMigInit(){
	char fname[128];
	sprintf(fname, "migrations.%d.log", pid);
	migfd = open(fname, O_RDWR | O_TRUNC | O_CREAT, 0777);
	if(migfd < 0){
		perror("open failed: ");
		abort();
	}
	if(posix_fallocate(migfd, 0, 0x200000)){
		perror("fallocate 0 failed: ");
		abort();
	}
	migPtr = mmap(NULL, 0x200000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, migfd, 0);
	if(migPtr == MAP_FAILED){
		perror("mmap 0 failed: ");
		abort();
	}
	migCntFile = (volatile int *)migPtr;
	migPtr++;
	*migCntFile = 0;
}

void loggerInit(){
	pid = getpid();
	logAllocInit();
	logSampleInit();
	logMigInit();
}

void logSample(void *addr, void *code, uint16_t flags){
	if((((uint64_t)samplePtr) & 0x1fffff) == 0){
		if(sampleOffset){
			munmap((void *)samplePtr - 0x200000, 0x200000);
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
	if(code){
		memcpy(samplePtr->code, code, 15);
	}
	clock_gettime(CLOCK_MONOTONIC, &samplePtr->timestamp);
	samplePtr++;
}

void logAlloc(AllocEventType type, void *in, void *out, size_t size){
	int n;
	pthread_mutex_lock(&logLock);
	if((((uint64_t)allocEventPtr) & 0x1fffff) == 0){
		if(allocOffset){
			munmap((void *)allocEventPtr - 0x200000, 0x200000);
		}
		allocOffset += 0x200000;
		if(posix_fallocate(allocfd, allocOffset, 0x200000)){
			perror("fallocate failed");
			abort();
		}
		allocEventPtr = mmap(NULL, 0x200000, PROT_READ | PROT_WRITE, MAP_SHARED, allocfd, allocOffset);
		if(allocEventPtr == MAP_FAILED){
			perror("mmap failed: ");
			abort();
		}
	}

	allocCnt++;
	*allocCntFile = allocCnt;
	allocEventPtr->type = type;
	allocEventPtr->in = in;
	allocEventPtr->out = out;
	allocEventPtr->size = size;
	//n = backtrace(allocEventPtr->callsite, 10);
	//for(int i = n; i < 10; i++){
		//allocEventPtr->callsite[i] = NULL;
	//}
	clock_gettime(CLOCK_MONOTONIC, &allocEventPtr->timestamp);
	allocEventPtr++;
	pthread_mutex_unlock(&logLock);
}

void logMigration(Direction dir, void *addr, size_t size){
	pthread_mutex_lock(&migLock);
	if((((uint64_t)migPtr) & 0x1fffff) == 0){
		if(migOffset){
			munmap((void *)migPtr - 0x200000, 0x200000);
		}
		migOffset += 0x200000;
		if(posix_fallocate(migfd, migOffset, 0x200000)){
			perror("fallocate failed");
			abort();
		}
		migPtr = mmap(NULL, 0x200000, PROT_READ | PROT_WRITE, MAP_SHARED, migfd, migOffset);
		if(migPtr == MAP_FAILED){
			perror("mmap failed: ");
			abort();
		}
	}

	migCnt++;
	*migCntFile = migCnt;
	migPtr->dir = dir;
	migPtr->addr = addr;
	migPtr->size = size;
	clock_gettime(CLOCK_MONOTONIC, &migPtr->timestamp);
	migPtr++;
	pthread_mutex_unlock(&migLock);
}
