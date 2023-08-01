#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "arch.h"

int main(int argc, char **argv){
	uint64_t witers, iters, size;
	DEFINE_COUNT(count);
	DEFINE_ADDR(ptr);
	DEFINE_COUNTER(i);
	DEFINE_INC(inc);
	DEFINE_OFFT(offset);
	DEFINE_MASK(mask);
	DEFINE_TMP(temp);

	struct timespec start0, start1, end0, end1;


	if(argc != 6){
		fprintf(stderr, "Usage: %s <count> <warm up itterations> <itterations> <memory size order> <memory increment 0>\n", argv[0]);
		return 2;
	}
	count = strtol(argv[1], NULL, 0);
	witers = strtol(argv[2], NULL, 0);
	iters = strtol(argv[3], NULL, 0);
	size = strtol(argv[4], NULL, 0);
	inc = strtol(argv[5], NULL, 0);

	if(count <= 0 || iters <= 0 || witers < 0 || iters <= 0 || size < 0 || inc < 0){
		fprintf(stderr, "Usage: %s <count> <warm up itterations> <itterations> <memory size order> <memory increment 0>\n", argv[0]);
		return 1;
	}
	size = 1 << size;


	ptr = malloc(size);
	if(ptr == NULL){
		return 3;
	}

	mask = size - 1;
	//printf("%p %p\n", mask, size);


	//no memory operation
	i = witers;
	offset = 0;
	for(; i > 0; i--){
		BURN_CYCLES(count, temp);
		NMOP(ptr, offset, temp);
		offset = (offset + inc) & mask;
	}
	i = iters;
	clock_gettime(CLOCK_MONOTONIC_RAW, &start0);
	offset = 0;
	for(; i > 0; i--){
		BURN_CYCLES(count, temp);
		NMOP(ptr, offset, temp);
		offset = (offset + inc) & mask;
	}
	clock_gettime(CLOCK_MONOTONIC_RAW, &end0);
	//load
	i = witers;
	offset = 0;
	for(; i > 0; i--){
		BURN_CYCLES(count, temp);
		LOAD_64(ptr, offset, temp);
		offset = (offset + inc) & mask;
	}
	i = iters;
	clock_gettime(CLOCK_MONOTONIC_RAW, &start1);
	offset = 0;
	for(; i > 0; i--){
		BURN_CYCLES(count, temp);
		LOAD_64(ptr, offset, temp);
		offset = (offset + inc) & mask;
	}
	clock_gettime(CLOCK_MONOTONIC_RAW, &end1);

	//printf("No memory opperation time/itteration:\t%ldps\n", ((end0.tv_sec - start0.tv_sec)*1000000000 + (end0.tv_nsec - start0.tv_nsec))*1000/iters);
	//printf("Load 64 opperation time/itteration:\t%ldps\n", ((end1.tv_sec - start1.tv_sec)*1000000000 + (end1.tv_nsec - start1.tv_nsec))*1000/iters);
	printf("%ld,%ld,%ld,%ld,%ld,", count, size, inc, ((end0.tv_sec - start0.tv_sec)*1000000000 + (end0.tv_nsec - start0.tv_nsec))*1000/iters, ((end1.tv_sec - start1.tv_sec)*1000000000 + (end1.tv_nsec - start1.tv_nsec))*1000/iters);
	return 0;
}
