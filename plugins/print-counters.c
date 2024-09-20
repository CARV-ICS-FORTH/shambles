#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "include/counters.h"

static ShamblesCounters counters;

int main(int argc, char **argv){
	int fd;
	ShamblesCounters *data;
	for(int i = 1; i < argc; i++){
		fd = open(argv[i], O_RDONLY);
		data = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_POPULATE, fd, 0);
		counters.allocs += data->allocs;
		counters.frees += data->frees;
		counters.reallocs += data->reallocs;
		counters.hits += data->hits;
		counters.misses += data->misses;
		counters.small += data->small;
		counters.migrations += data->migrations;
	}
	
	printf("Allocatios:\t%ld\n", counters.allocs);
	printf("Deallocatios:\t%ld\n", counters.frees);
	printf("Reallocatios:\t%ld\n", counters.reallocs);
	printf("Samples in fast memory:\t%ld\n", counters.hits);
	printf("Samples in slow memory:\t%ld\n", counters.misses);
	printf("Samples in small allocations:\t%ld\n", counters.small);
	printf("Number of migrations:\t%ld\n", counters.migrations);
	return 0;
}
