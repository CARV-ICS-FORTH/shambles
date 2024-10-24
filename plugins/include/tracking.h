#ifndef SHAMBLES_TRACKING_H

#define SHAMBLES_TRACKING_H

#define LOG_TRACKED 1
#define LOG_HIT 2

typedef enum{
	INITFAST = 0,
	INITSLOW = 1,
	SLOW2FAST = 2,
	FAST2SLOW = 3
}Direction;

#ifdef SHAMBLES_LOG
#define SHAMBLES_LOGGER_ENABLED
#endif


#ifdef SHAMBLES_LOGGER_ENABLED

#include <shambles.h>
#include <stdint.h>

void dumpMap();
void loggerInit();
void logSample(void *addr, void *code, uint16_t flags);
void logAlloc(AllocEventType type, void *in, void *out, size_t size);
void logMigration(Direction dir, void *addr, size_t size);


#define DUMP_MAP() dumpMap()
#define INIT_LOGGER() loggerInit()

#define LOG_SAMPLE(addr, code, flags) logSample(addr, code, flags)
#define LOG_ALLOC(type, in, out, size) logAlloc(type, in, out, size)
#define LOG_MIGRATION(dir, addr, size) logMigration(dir, addr, size);
#else

#define DUMP_MAP()
#define INIT_LOGGER()

#define LOG_SAMPLE(addr, code, flags)
#define LOG_ALLOC(type, in, out, size)
#define LOG_MIGRATION(dir, addr, size)

#endif

#endif
