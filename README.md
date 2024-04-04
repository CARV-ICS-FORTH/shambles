# SHAMBLES

Memory profiler based on sampling and page faults. Also used for memory migration in heterogeneous memory architectures. Name inspired from One Piece.

# kernel

Patch for the linux kernel. Needed to enable the page fault based sampling. Tested with various Linux kernel versions including 5.16 and 6.6 on x86 and 5.15 on ARM. The current implementation supports only the x86_64 and arm64 architectures.

# jemalloc

Patched jemalloc library that uses the page fault based sampling. The extra functionality is implemented at the plugins

# plugins

Plugins for the patched jemalloc. Can be used for various goals, including profiling and migrations. For profiling only plugins, no special type of memory is required. For migrations, the target machine should support heterogeneous memory (tested with DDR4 and Optane NVDIMM, as well as DDR5 and HBM). Further instructions are located in the plugins directory.

# FUNDING

We thankfully acknowledge the support of the European Commission and the
Greek General Secretariat for Research and Innovation under the EuroHPC
Programme through the DEEP-SEA project (GA 955606). National
contributions from the involved state members (including the Greek
General Secretariat for Research and Innovation) match the EuroHPC
funding.
