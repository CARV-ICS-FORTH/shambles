# SHAMBLES

Memory profiler based on sampling and page faults. Also used for memory migration in heterogeneous memory architectures. Name inspired from One Piece.

# kernel

Patch for the linux kernel. Needed to enable the page fault based sampling. Tested with the Linux kernel version 5.16.13.

# jemalloc

Patched jemalloc library that uses the page fault based sampling. The extra functionality is implemented at the plugins

# plugins

Plugins for the patched jemalloc. Can be used for various goals, including profiling and migrations.
