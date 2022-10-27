# SHAMBLES plugins

A plugin should handle allocation and sampling events. It should be be built as a shred library and the filename of the library should be included asigned to the SHAMBLES_PLUGIN environment variable.

# shambles_init()

This is the only mandatory function at the plugin. It takes function pointer reference as an argument, which should be set to the a function (implemented at the plugin) that handles allocation events. The thread that reads and processes the memory access samples should also be spawned by this function.
