.PHONY: default
default: all ;

CFLAGS = -Iinclude -Iplugin-malloc/include/jemalloc/ -O3 -flto -fPIC
LD_FLAGS= -O3 -flto --shared -lpthread -lnuma
CC = gcc

plugin-malloc/lib/libjemalloc_pic.a: plugin-malloc.sh
	./plugin-malloc.sh

%.o: %.c include/migration.h include/config.h include/shambles.h include/structures.h plugin-malloc/lib/libjemalloc_pic.a
	$(CC) $(CFLAGS) -c -o $@ $< -c


static.so: static.o migration.o config.o
	$(CC) $(LD_FLAGS) -o $@  $^

lru.so: lru.o migration.o config.o structures.o plugin-malloc/lib/libjemalloc_pic.a
	$(CC) $(LD_FLAGS) -o $@  $^

window.so: window.o migration.o config.o structures.o plugin-malloc/lib/libjemalloc_pic.a
	$(CC) $(LD_FLAGS) -o $@  $^

log.so: log.o
	$(CC) $(LD_FLAGS) -o $@  $^

clean:
	rm *.o *.so
	rm -rf plugin-malloc

all: plugin-malloc/lib/libjemalloc_pic.a static.so lru.so window.so log.so
