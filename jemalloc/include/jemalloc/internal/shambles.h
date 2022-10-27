#ifndef JEMALLOC_INTERNAL_SHAMBLES_H
#define JEMALLOC_INTERNAL_SHAMBLES_H

typedef enum{
	ALLOC_EVENT_ALLOC,
	ALLOC_EVENT_REALLOC,
	ALLOC_EVENT_FREE,
	ALLOC_EVENT_SIZE
}AllocEventType;

void logAlloc(AllocEventType type, void *in, void *out, size_t size);

#endif
