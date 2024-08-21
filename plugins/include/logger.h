#ifndef SHAMBLES_LOGGER_H

#define SHAMBLES_LOGGER_H

#define LOG_TRACKED 1
#define LOG_HIT 2
#define LOG_MIGRATION 4
#define LOG_MIGRATION1 8

#ifdef SHAMBLES_COUNTERS
#define SHAMBLES_LOGGER_ENABLED
#endif

#ifdef SHAMBLES_LOGGER_ENABLED

void dumpMap();
void loggerInit();
void logSample(void *addr, void *code, uint16_t flags);
void logAlloc(AllocEventType type, void *in, void *out, size_t size);

#define DUMP_MAP() dumpMap()
#define INIT_LOGGER() loggerInit()

#define LOG_SAMPLE(addr, code, flags) logSample(addr, code, flags)
#define LOG_ALLOC(type, in, out, size) logAlloc(type, in, out, size)
#else

#define DUMP_MAP()
#define INIT_LOGGER()

#define LOG_SAMPLE(addr, code, flags)
#define LOG_ALLOC(type, in, out, size)


#endif

#endif
