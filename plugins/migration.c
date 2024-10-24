#include <numaif.h>
#include <migration.h>
#include <stdio.h>
#include <tracking.h>

void unbind(struct ShamblesRegion *region){
	mbind(region->start, region->chunks->size, MPOL_DEFAULT, 0, 0, 0);
}

void bindFast(struct ShamblesPluginConfig *config, struct ShamblesChunk *chunk){
	mbind(chunk->start, chunk->size, MPOL_BIND, config->fastmask, config->maxnode, MPOL_MF_MOVE);
	LOG_MIGRATION(SLOW2FAST, chunk->start, chunk->size);
}

void bindSlow(struct ShamblesPluginConfig *config, struct ShamblesChunk *chunk){
	mbind(chunk->start, chunk->size, MPOL_BIND, config->slowmask, config->maxnode, MPOL_MF_MOVE);
	LOG_MIGRATION(FAST2SLOW, chunk->start, chunk->size);
}

void bindFastAlloc(struct ShamblesPluginConfig *config, struct ShamblesChunk *chunk){
	mbind(chunk->start, chunk->size, MPOL_BIND, config->fastmask, config->maxnode, MPOL_MF_MOVE);
	LOG_MIGRATION(INITFAST, chunk->start, chunk->size);
}

void bindSlowAlloc(struct ShamblesPluginConfig *config, struct ShamblesChunk *chunk){
	mbind(chunk->start, chunk->size, MPOL_BIND, config->slowmask, config->maxnode, MPOL_MF_MOVE);
	LOG_MIGRATION(INITSLOW, chunk->start, chunk->size);
}


void swap(struct ShamblesPluginConfig *config, struct ShamblesChunk *fastChunk, struct ShamblesChunk *slowChunk){
	mbind(slowChunk->start, slowChunk->size, MPOL_BIND, config->slowmask, config->maxnode, MPOL_MF_MOVE);
	mbind(fastChunk->start, fastChunk->size, MPOL_BIND, config->fastmask, config->maxnode, MPOL_MF_MOVE);
	LOG_MIGRATION(SLOW2FAST, fastChunk->start, fastChunk->size);
	LOG_MIGRATION(FAST2SLOW, slowChunk->start, slowChunk->size);
}
