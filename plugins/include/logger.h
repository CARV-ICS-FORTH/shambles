#ifndef SHAMBLES_LOGGER_H

#define SHAMBLES_LOGGER_H

#include <tracking.h>
#include <shambles.h>
#include <stdint.h>

typedef struct{
	AllocEventType type;
	void *in, *out;
	size_t size;
	void *callsite[10];
	struct timespec timestamp;
}AllocEvent __attribute__ ((aligned (128)));

typedef struct{
	void *addr;
	char code[16];
	struct timespec timestamp;
}Sample __attribute__ ((aligned (64)));

typedef struct{
	void *addr;
	Direction dir;
	size_t size;
	struct timespec timestamp;
}MigrationEvent __attribute__ ((aligned (64)));

#endif
