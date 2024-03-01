/*
 * Wrapper for migration system calls
 */

#ifndef SHAMBLES_MIGRATION_H
#define SHAMBLES_MIGRATION_H

#include <structures.h>
#include <config.h>

void unbind(struct ShamblesRegion *region);
void bindFast(struct ShamblesPluginConfig *config, struct ShamblesChunk *chunk);
void bindSlow(struct ShamblesPluginConfig *config, struct ShamblesChunk *chunk);
void swap(struct ShamblesPluginConfig *config, struct ShamblesChunk *fastChunk, struct ShamblesChunk *slowChunk);

#endif
