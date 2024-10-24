#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "include/logger.h"
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>

char *dirs[] = {"initialised at fast", "initialised at slow", "migrated slow -> fast", "migrated fast -> slow"};
struct timespec start = {LONG_MAX, LONG_MAX};
int allocIndx = 0, sampleIndx = 0, migIndx = 0;
int allocSize = 0, sampleSize = 0, migSize = 0;
AllocEvent *allocs;
Sample *samples;
MigrationEvent* migs;

void printTime(struct timespec *timestamp){
	if(timestamp->tv_nsec < start.tv_nsec){
		timestamp->tv_nsec += 1000000000;
		timestamp->tv_sec--;
	}
	printf("%ld.%09ld: ", timestamp->tv_sec - start.tv_sec, timestamp->tv_nsec - start.tv_nsec);
}

void onAlloc(AllocEvent *event){
	printTime(&event->timestamp);
	if(event->type == ALLOC_EVENT_ALLOC){
		printf("0x%lx bytes allocated at %p\n", event->size, event->out);
	}else if(event->type == ALLOC_EVENT_REALLOC){
		if(event->out == event->in){
			printf("Allocation at %p was realloced to 0x%lx bytes\n", event->out, event->size);
		}else{
			printf("Allocation at %p was realloced to 0x%lx bytes and moved to %p\n", event->in, event->size, event->out);
		}
	}else{
		printf("Allocation at %p was freed\n", event->in);
	}
}

void onSample(Sample *event){
	printTime(&event->timestamp);
	printf("Sampled address %p\n", event->addr);
}

void onMigration(MigrationEvent *event){
	printTime(&event->timestamp);
	printf("(%p, 0x%lx) %s\n", event->addr, event->size, dirs[event->dir]);
}

int main(int argc, char **argv){
	int fd;
	struct stat buf;

	//initialisation
	if(argc != 4){
		fprintf(stderr, "Usage: %s <Allocations file> <Samples file> <Migration file>\n", argv[0]);
		return EINVAL;
	}

	if((fd = open(argv[1], O_RDONLY)) < 0){
		perror("Could not open allocations file");
	}else{
		if(fstat(fd, &buf) < 0){
			perror("Could not stat allocations file");
		}else{
			if((allocs = mmap(NULL, buf.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0)) == MAP_FAILED){
				perror("Could not mmap allocations file");
			}else{
				allocSize = *((int *)allocs);
				allocs++;
			}
		}
		while(close(fd));
	}

	if((fd = open(argv[2], O_RDONLY)) < 0){
		perror("Could not open samples file");
	}else{
		if(fstat(fd, &buf) < 0){
			perror("Could not stat samples file");
		}else{
			if((samples = mmap(NULL, buf.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0)) == MAP_FAILED){
				perror("Could not mmap samples file");
			}else{
				sampleSize = *((int *)samples);
				samples++;
			}
		}
		while(close(fd));
	}

	if((fd = open(argv[3], O_RDONLY)) < 0){
		perror("Could not open migrations file");
	}else{
		if(fstat(fd, &buf) < 0){
			perror("Could not stat migrations file");
		}else{
			if((migs = mmap(NULL, buf.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0)) == MAP_FAILED){
				perror("Could not mmap migrations file");
			}else{
				migSize = *((int *)migs);
				migs++;
			}
		}
		while(close(fd));
	}
	if(allocSize && ((allocs[0].timestamp.tv_sec < start.tv_sec) || ((allocs[0].timestamp.tv_sec == start.tv_sec) && (allocs[0].timestamp.tv_nsec < start.tv_nsec)))){
		start = allocs[0].timestamp;
	}

	if(sampleSize && ((samples[0].timestamp.tv_sec < start.tv_sec) || ((samples[0].timestamp.tv_sec == start.tv_sec) && (samples[0].timestamp.tv_nsec < start.tv_nsec)))){
		start = samples[0].timestamp;
	}

	if(migSize && ((migs[0].timestamp.tv_sec < start.tv_sec) || ((migs[0].timestamp.tv_sec == start.tv_sec) && (migs[0].timestamp.tv_nsec < start.tv_nsec)))){
		start = migs[0].timestamp;
	}

	//main loop
	while(1){
		if(allocIndx < allocSize){
			if((sampleIndx < sampleSize) && ((samples[sampleIndx].timestamp.tv_sec < allocs[allocIndx].timestamp.tv_sec) || ((samples[sampleIndx].timestamp.tv_sec == allocs[allocIndx].timestamp.tv_sec) && (samples[sampleIndx].timestamp.tv_nsec < allocs[allocIndx].timestamp.tv_nsec)))){
				if((migIndx < migSize) && ((migs[migIndx].timestamp.tv_sec < samples[sampleIndx].timestamp.tv_sec) || ((migs[migIndx].timestamp.tv_sec == samples[sampleIndx].timestamp.tv_sec) && (migs[migIndx].timestamp.tv_nsec < samples[sampleIndx].timestamp.tv_nsec)))){
					onMigration(migs + migIndx);
					migIndx++;
				}else{
					onSample(samples + sampleIndx);
					sampleIndx++;
				}
			}else if((migIndx < migSize) && ((migs[migIndx].timestamp.tv_sec < allocs[allocIndx].timestamp.tv_sec) || ((migs[migIndx].timestamp.tv_sec == allocs[allocIndx].timestamp.tv_sec) && (migs[migIndx].timestamp.tv_nsec < allocs[allocIndx].timestamp.tv_nsec)))){
				onMigration(migs + migIndx);
				migIndx++;
			}else{
				onAlloc(allocs + allocIndx);
				allocIndx++;
			}
		}else if(sampleIndx < sampleSize){
			if((migIndx < migSize) && ((migs[migIndx].timestamp.tv_sec < samples[sampleIndx].timestamp.tv_sec) || ((migs[migIndx].timestamp.tv_sec == samples[sampleIndx].timestamp.tv_sec) && (migs[migIndx].timestamp.tv_nsec < samples[sampleIndx].timestamp.tv_nsec)))){
				onMigration(migs + migIndx);
				migIndx++;
			}else{
				onSample(samples + sampleIndx);
				sampleIndx++;
			}
		}else if(migIndx < migSize){
			onMigration(migs + migIndx);
			migIndx++;
		}else{
			return 0;
		}
	}
}
