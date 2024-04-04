# SHAMBLES plugins

A plugin should handle allocation and sampling events. It should be be built as a shared library and the filename of the library should be included assigned to the SHAMBLES_PLUGIN environment variable.

## shambles_init()

This is the only mandatory function at the plugin. It takes function pointer reference as an argument, which should be set to the a function (implemented at the plugin) that handles allocation events. The thread that reads and processes the memory access samples should also be spawned by this function.

# AVAILABLE PLUGINS

There are five available plugins. Each implements different funtionality.

## log

Runs SHAMBLES, in order to dump the memory allocations and accesses in files. Can be used to create heatmaps and scatterplots, or used for other purposes. Requires the custom kernel, but not heterogeneous memory.

## static

Places each allocation object staticaly on the fast or slow memory based on the order of allocation and environment variables. Each array is placed entirely to a specific memory tier. Requires some kind heterogeneous memory, but it can run on a stock kernel.

## static-fractional

Places each allocation object (or part of it) staticaly on the fast or slow memory based on the order of allocation and environment variables. In contrast to the above, we can place parts of an array to different tiers. Requires some kind heterogeneous memory, but it can run on a stock kernel.

## lru

Implements the LRU migration policy. Requires the custom kernel and heterogeneous memory.

## window

Implements the window migration policy. Requires the custom kernel and heterogeneous memory.

# ENVIRONMENT VARIABLES

Several environment variables are used during runtime, in order to control the plugins.

## SHAMBLES_PLUGIN

The path of the shared object to the corresponding plugin. For example lru.so for the lru plugin.

## SHAMBLES_SIZE_THRESHOLD

Allocations of this size (in bytes) and above, are handled by SHAMBLES. Defaults at 16 KB.

## SHAMBLES_FAST_NODEMASK and SHAMBLES_SLOW_NODEMASK

Bitmaps of the NUNA memory nodes that are considered fast and slow respectively. Used by the static, static-fractional, lru and window plugins.

## SHAMBLES_CHUNK_SIZE SHAMBLES_FAST_MEM_CHUNKS SHAMBLES_FAST_MEM_SIZE

Control the available fast memory and the chunk size. A chunk is the granularity of memory  that is handled as a unit. It can be an antire allocation or part of it. Any two of the above three variables are set, and the other can be deduced by those, since SHAMBLES_FAST_MEM_SIZE = SHAMBLES_CHUNK_SIZE * SHAMBLES_FAST_MEM_CHUNKS. Used by the lru and window plugins.

## SHAMBLES_SLOW_BITMASK

A bitmap of the arrays that go to the slow memory. Used by the static plugin.

## SHAMBLES_ALLOC_FAST_FRACTIONS

A comma separated list of the faction fo each array that goes to the fast memory. If there are more allocations (greater than SHAMBLES_SIZE_THRESHOLD) than the values in this variable, the remaining are placed in the slow memory. Each value is between 0 (slow) and 1 (fast). Used by the static-fractional plugin.

## SHAMBLES_WINDOW_SIZE

The size of the window. The last SHAMBLES_WINDOW_SIZE samples are taken into account for the migration decision. Used by the window plugin.
