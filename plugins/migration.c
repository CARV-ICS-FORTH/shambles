#include <numaif.h>
#include <migration.h>
#include <stdio.h>

void unbind(struct ShamblesRegion *region){
	mbind(region->start, region->chunks->size, MPOL_DEFAULT, 0, 0, 0);
	//fprintf(stderr, "unbinding %p-%p\n", region->start, region->start + region->chunks->size);
}

void bindFast(struct ShamblesPluginConfig *config, struct ShamblesChunk *chunk){
	if(mbind(chunk->start, chunk->size, MPOL_BIND, config->fastmask, config->maxnode, MPOL_MF_MOVE)){
		//printf("%p, %ld, %x, %ld\n", chunk->start, chunk->size, config->fastmask[0], config->maxnode);
		//perror("bindFast");
	}else{
		//fprintf(stderr, "binding %p-%p to fast\n", chunk->start, chunk->start + chunk->size);
	}
}

void bindSlow(struct ShamblesPluginConfig *config, struct ShamblesChunk *chunk){
	if(mbind(chunk->start, chunk->size, MPOL_BIND, config->slowmask, config->maxnode, MPOL_MF_MOVE)){
		//printf("%p, %ld, %x, %ld\n", chunk->start, chunk->size, config->slowmask[0], config->maxnode);
		//perror("bindSlow");
	}else{
		//fprintf(stderr, "binding %p-%p to slow\n", chunk->start, chunk->start + chunk->size);
	}
}

void swap(struct ShamblesPluginConfig *config, struct ShamblesChunk *fastChunk, struct ShamblesChunk *slowChunk){
	mbind(slowChunk->start, slowChunk->size, MPOL_BIND, config->slowmask, config->maxnode, MPOL_MF_MOVE);
	mbind(fastChunk->start, fastChunk->size, MPOL_BIND, config->fastmask, config->maxnode, MPOL_MF_MOVE);
	//fprintf(stderr, "binding %p-%p to slow\n", slowChunk->start, slowChunk->start + slowChunk->size);
	//fprintf(stderr, "binding %p-%p to fast\n", fastChunk->start, fastChunk->start + fastChunk->size);
}
