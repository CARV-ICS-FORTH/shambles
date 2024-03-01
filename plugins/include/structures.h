/*
 * Reusable structures for memory allocations
 */

#ifndef SHAMBLES_STRUCTURES_H
#define SHAMBLES_STRUCTURES_H

#include <stdlib.h>
#include <config.h>

struct ShamblesChunk{
	void *start;
	size_t size;
	void *privData;
};

struct ShamblesRegion{
	void *start;
	int nChunks;
	struct ShamblesChunk *chunks;
	void *privData;
};


void shamblesStructsInit(struct ShamblesPluginConfig *config);
struct ShamblesChunk *getChunkInfo(void *addr);
struct ShamblesRegion *getRegion(void *addr);
struct ShamblesRegion *addRegion(void *addr, size_t size);
void deleteRegion(struct ShamblesRegion *region);

#endif
