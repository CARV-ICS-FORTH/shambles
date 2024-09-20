#include <config.h>
#include <stdlib.h>
#include <numa.h>
#include <errno.h>

/*
 * FIXME: work with more than 64 numa nodes
 */
int ShamblesPluginConfigInit(struct ShamblesPluginConfig *config){
	char *env;
	config->maxnode = numa_max_node() + 2;
	if(config->maxnode >= CONFIG_MAX_MASKS*8*sizeof(unsigned long)){
		return -EOVERFLOW;
	}
	if((env = getenv("SHAMBLES_FAST_NODEMASK"))==NULL){
		config->fastmask[0] = (1LU << (config->maxnode + 1)) - 1;
	}else{
		config->fastmask[0] = strtol(env, NULL, 0);
		if(!config->fastmask[0]){
			config->fastmask[0] = (1LU << (config->maxnode + 1)) - 1;
		}else{
			config->fastmask[0] &= (1LU << (config->maxnode + 1)) - 1;
		}
	}
	if((env = getenv("SHAMBLES_SLOW_NODEMASK"))==NULL){
		config->slowmask[0] = 0;
	}else{
		config->slowmask[0] = strtol(env, NULL, 0);
		config->slowmask[0] &= ((1LU << (config->maxnode + 1)) - 1) & ~config->fastmask[0];
	}
	if((env = getenv("SHAMBLES_SIZE_THRESHOLD"))==NULL){
		config->sizeThresshold = 0x10000;
	}else{
		config->sizeThresshold = strtol(env, NULL, 0);
		if(!config->sizeThresshold){
			config->sizeThresshold = 0x10000;
		}
	}
	if((env = getenv("SHAMBLES_CHUNK_SIZE"))==NULL){
		//not defined, try to get it form SHAMBLES_FAST_MEM_SIZE/SHAMBLES_FAST_MEM_CHUNKS
		if((env = getenv("SHAMBLES_FAST_MEM_SIZE"))==NULL){
			config->chunkSize = (size_t)(-1);
		}else{
			config->chunkSize = strtol(env, NULL, 0);
			if(!config->chunkSize){
				config->chunkSize = (size_t)(-1);
			}else{
				if((env = getenv("SHAMBLES_FAST_MEM_CHUNKS"))==NULL){
					config->chunkSize = (size_t)(-1);
				}else if(!(config->fastChunks = strtol(env, NULL, 0))){
					config->chunkSize = (size_t)(-1);
				}else{
					config->chunkSize /= config->fastChunks;
				}
			}
		}
	}else{
		config->chunkSize = strtol(env, NULL, 0);
		if(!config->chunkSize){
			config->chunkSize = (size_t)(-1);
		}
	}
	if((env = getenv("SHAMBLES_FAST_MEM_CHUNKS"))==NULL){
		//not defined, try to get it form SHAMBLES_FAST_MEM_SIZE/SHAMBLES_CHUNK_SIZE
		if((env = getenv("SHAMBLES_FAST_MEM_SIZE"))==NULL){
			config->fastChunks = (unsigned long)(-1);
		}else{
			config->fastChunks = strtol(env, NULL, 0);
			if(!config->fastChunks){
				config->fastChunks = (unsigned long)(-1);
			}else{
				config->fastChunks /= config->chunkSize;
			}
		}
	}else{
		config->fastChunks = strtol(env, NULL, 0);
		if(!config->fastChunks){
			config->fastChunks = (unsigned long)(-1);
		}
	}
	return 0;
}
