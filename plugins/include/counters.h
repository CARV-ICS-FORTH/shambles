#ifndef SHAMBLES_COUNTERS_H
#define SHAMBLES_COUNTERS_H
#include <stdint.h>

typedef struct{
	uint64_t allocs;
	uint64_t frees;
	uint64_t reallocs;
	uint64_t hits;
	uint64_t misses;
	uint64_t migrations;
	uint64_t small;
}ShamblesCounters;

#endif
