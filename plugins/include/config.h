/*
 * Get configuration from env
 */

#ifndef SHAMBLES_PLUGIN_CONFIG_H
#define SHAMBLES_PLUGIN_CONFIG_H

#include <stdlib.h>

#define CONFIG_MAX_MASKS 1

struct ShamblesPluginConfig{
	size_t sizeThresshold;
	size_t chunkSize;
	unsigned long fastChunks;
	unsigned long maxnode;
	unsigned long fastmask[CONFIG_MAX_MASKS], slowmask[CONFIG_MAX_MASKS];
};

int ShamblesPluginConfigInit(struct ShamblesPluginConfig *config);

#endif
