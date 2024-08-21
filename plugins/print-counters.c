#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "include/counters.h"

int main(int argc, char **argv){
	int fd;
	static ShamblesCounters *counters;
	fd = open(argv[1], O_RDONLY);
	counters = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_POPULATE, fd, 0);
	printf("Allocatios:\t%ld\n", counters->allocs);
	printf("Deallocatios:\t%ld\n", counters->frees);
	printf("Reallocatios:\t%ld\n", counters->reallocs);
	printf("Samples in fast memory:\t%ld\n", counters->hits);
	printf("Samples in slow memory:\t%ld\n", counters->misses);
	printf("Samples in small allocations:\t%ld\n", counters->small);
	printf("Number of migrations:\t%ld\n", counters->migrations);
	return 0;
}
