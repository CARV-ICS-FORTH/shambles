/*
 * Necessary interface to communicate with jemalloc
 */

#ifndef SHAMBLES_H
#define SHAMBLES_H

typedef enum{
	ALLOC_EVENT_ALLOC,
	ALLOC_EVENT_REALLOC,
	ALLOC_EVENT_FREE,
	ALLOC_EVENT_SIZE
}AllocEventType;

#endif
