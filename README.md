# SHAMBLES

Memory profiler based on sampling and page faults. Also used for memory migration in heterogeneous memory architectures. Name inspired from One Piece.

# kernel

Patch for the linux kernel. Needed to enable the page fault based sampling. Tested with the Linux kernel version 5.16.13. The current implementation supports only the x86 architecture.

# jemalloc

Patched jemalloc library that uses the page fault based sampling. The extra functionality is implemented at the plugins

# plugins

Plugins for the patched jemalloc. Can be used for various goals, including profiling and migrations. For profiling only plugins, no special type of memory is rqquired. For migrations, the target machin should support heterogeneous memory (tested with DDR and Optane NVDIMM).

# FUNDING

We thankfully acknowledge the support of the European Commission and the
Greek General Secretariat for Research and Innovation under the EuroHPC
Programme through the DEEP-SEA project (GA 955606). National
contributions from the involved state members (including the Greek
General Secretariat for Research and Innovation) match the EuroHPC
funding.
