#include <config.h>
#include <pthread.h>
#include <structures.h>
#include <jemalloc.h>
#include <shambles.h>
#include <migration.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <logger.h>

struct ShamblesPluginConfig config;

/*current state*/
static int freeFastChunks;
static pthread_mutex_t lruLock = PTHREAD_MUTEX_INITIALIZER;

enum MemLoc{
	FAST,
	SLOW,
	MEM_LOC_SIZE
};

struct LRUlist{
	struct ShamblesChunk *chunk;
	enum MemLoc loc;
	struct LRUlist *next, *prev;
};

struct LRUlist *mru, *lru;

void dq(struct LRUlist **q, struct LRUlist *node){
	if(*q == node){
		if(node->next == node){
			*q = NULL;
		}else{
			*q = node->next;
			node->prev->next = (*q);
			(*q)->prev = node->prev;
		}
	}else{
		struct LRUlist *next = node->next;
		next->prev = node->prev;
		node->prev->next = next;
	}
}

void push(struct LRUlist **q, struct LRUlist *node){
	if(*q == NULL){
		*q = node;
		node->prev = node;
		node->next = node;
	}else{
		node->next = *q;
		node->prev = (*q)->prev;
		node->prev->next = node;
		(*q)->prev = node;
		*q = node;
	}
}

void enq(struct LRUlist **q, struct LRUlist *node){
	if(*q == NULL){
		*q = node;
		node->prev = node;
		node->next = node;
	}else{
		node->next = *q;
		node->prev = (*q)->prev;
		node->prev->next = node;
		(*q)->prev = node;
	}
}

static void allocCallback(AllocEventType type, void *in, void *out, size_t size){
	if(type == ALLOC_EVENT_ALLOC){
		if(size >= config.sizeThresshold){
			struct ShamblesRegion *region;
			struct LRUlist *nodes;
			int i;
			pthread_mutex_lock(&lruLock);
			region = addRegion(out, size);
			region->privData = plugin_malloc(region->nChunks*sizeof(struct LRUlist));
			nodes = (struct LRUlist *)region->privData;
			if(region->privData == NULL) abort();
			for(i = 0; i < region->nChunks; i++){
				nodes[i].chunk = region->chunks + i;
				region->chunks[i].privData = (void *)(nodes + i);
			}
			i = 0;
			while(freeFastChunks && i < region->nChunks){
				nodes[i].loc = FAST;
				enq(&mru, nodes + i);
				bindFast(&config, region->chunks + i);
				freeFastChunks--;
				i++;
			}
			while(i < region->nChunks){
				nodes[i].loc = SLOW;
				enq(&lru, nodes + i);
				bindSlow(&config, region->chunks + i);
				i++;
			}
			pthread_mutex_unlock(&lruLock);
		}
	}else if(type == ALLOC_EVENT_REALLOC){
		//todo
	}else{
		struct ShamblesRegion *region;
		int i;
		if((region = getRegion(in)) != NULL){
			unbind(region);
			struct LRUlist *nodes = (struct LRUlist *)region->privData;
			pthread_mutex_lock(&lruLock);
			for(i = 0; i < region->nChunks; i++){
				if(nodes[i].loc = SLOW){
					dq(&lru, nodes + i);
				}else{
					dq(&mru, nodes + i);
					if(lru == NULL){
						freeFastChunks++;
					}else{
						struct LRUlist *promoted = lru;
						dq(&lru, lru);
						promoted->loc = FAST;
						/*else it has been unbound anyway*/
						if(promoted->chunk > region->chunks + region->nChunks){
							bindFast(&config, promoted->chunk);
						}
						enq(&mru, promoted);
					}
				}
			}
			deleteRegion(region);
			pthread_mutex_unlock(&lruLock);
			plugin_free(nodes);
		}
	}
	LOG_ALLOC(type, in, out, size);
}

void handleSample(void *addr){
	struct ShamblesChunk *chunk;
	struct LRUlist *node;
	pthread_mutex_lock(&lruLock);
	if((chunk = getChunkInfo(addr)) != NULL){
		node = (struct LRUlist *)(chunk->privData);
		if(node->loc == FAST){
			dq(&mru, node);
			LOG_SAMPLE(addr, NULL, 3);
		}else{
			struct LRUlist *victim;
			dq(&lru, node);
			victim = mru->prev;
			dq(&mru, victim);
			push(&lru, victim);
			swap(&config, chunk, victim->chunk);
			node->loc = FAST;
			victim->loc = SLOW;
			LOG_SAMPLE(addr, NULL, 5);
		}
		push(&mru, node);
	}else{
		LOG_SAMPLE(addr, NULL, 0);
	}
	pthread_mutex_unlock(&lruLock);
}

void *sampler_thread_func(void *unused){
	int fd;
	void *buff[2];
	fd = open("/sys/kernel/debug/sampler/samples", O_RDONLY);
	if(fd < 0) return NULL;
	while(1){
		while(read(fd, buff, 16) != 16);
		handleSample(buff[0]);
	}
}

int lruInit(){
	int err;
	pthread_mutex_lock(&lruLock);
	if(err = ShamblesPluginConfigInit(&config)){
		pthread_mutex_unlock(&lruLock);
		return err;
	}
	shamblesStructsInit(&config);
	INIT_LOGGER();
	freeFastChunks = config.fastChunks;
	mru = NULL;
	lru = NULL;
	pthread_mutex_unlock(&lruLock);
}

void shambles_init(void (**reportAllocFunc)(AllocEventType, void *, void *, size_t)){
	pthread_t thread;
	if(lruInit()){
		return;
	}
	*reportAllocFunc = &allocCallback;

	pthread_create(&thread, NULL, &sampler_thread_func, NULL);
	pthread_detach(thread);
}
