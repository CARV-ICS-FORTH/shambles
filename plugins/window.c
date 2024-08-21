#include <config.h>
#include <pthread.h>
#include <structures.h>
#include <jemalloc.h>
#include <shambles.h>
#include <migration.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <logger.h>

struct ShamblesPluginConfig config;

/*current state*/
static int freeFastChunks;
static pthread_mutex_t winLock = PTHREAD_MUTEX_INITIALIZER;

enum MemLoc{
	FAST,
	SLOW,
	MEM_LOC_SIZE
};

struct List{
	struct ShamblesChunk *chunk;
	enum MemLoc loc;
	int count;
	struct List *next, *prev;
};

static struct List *fastList, *slowList;
static struct ShamblesChunk **samples;
static int windowSize, currentLoc = 0;
static struct List **tails;


void move(struct List *node, struct List *after){
	if(node != after){
		node->prev->next = node->next;
		node->next->prev = node->prev;
		node->prev = after;
		node->next = after->next;
		after->next->prev = node;
		after->next = node;
	}
}

void insert(struct List *node, struct List *after){
	node->prev = after;
	node->next = after->next;
	after->next->prev = node;
	after->next = node;
}

void dq(struct List **q, struct List *node){
	if(*q == node){
		if(node->next == node){
			*q = NULL;
		}else{
			*q = node->next;
			node->prev->next = (*q);
			(*q)->prev = node->prev;
		}
	}else{
		struct List *next = node->next;
		next->prev = node->prev;
		node->prev->next = next;
	}
}

void push(struct List **q, struct List *node){
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

void enq(struct List **q, struct List *node){
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
			struct List *nodes;
			int i;
			pthread_mutex_lock(&winLock);
			region = addRegion(out, size);
			region->privData = plugin_malloc(region->nChunks*sizeof(struct List));
			nodes = (struct List *)region->privData;
			if(region->privData == NULL) abort();
			for(i = 0; i < region->nChunks; i++){
				nodes[i].chunk = region->chunks + i;
				region->chunks[i].privData = (void *)(nodes + i);
				nodes[i].count = 0;
			}
			i = 0;
			while(freeFastChunks && i < region->nChunks){
				nodes[i].loc = FAST;
				enq(&fastList, nodes + i);
				bindFast(&config, region->chunks + i);
				freeFastChunks--;
				i++;
			}
			while(i < region->nChunks){
				nodes[i].loc = SLOW;
				enq(&slowList, nodes + i);
				bindSlow(&config, region->chunks + i);
				i++;
			}
			tails[0] = nodes + region->nChunks - 1;
			pthread_mutex_unlock(&winLock);
		}
	}else if(type == ALLOC_EVENT_REALLOC){
		//todo
	}else{
		struct ShamblesRegion *region;
		int i;
		pthread_mutex_lock(&winLock);
		if((region = getRegion(in)) != NULL){
			unbind(region);
			struct List *nodes = (struct List *)region->privData;
			for(i = 0; i < region->nChunks; i++){
				if(nodes[i].loc = SLOW){
					dq(&slowList, nodes + i);
				}else{
					dq(&fastList, nodes + i);
					if(slowList == NULL){
						freeFastChunks++;
					}else{
						struct List *promoted = slowList;
						dq(&slowList, slowList);
						promoted->loc = FAST;
						/*else it has been unbound anyway*/
						if(promoted->chunk > region->chunks + region->nChunks){
							bindFast(&config, promoted->chunk);
						}
						enq(&fastList, promoted);
					}
				}
				if(tails[nodes[i].count] == nodes + i){
					if(nodes[i].prev->count == nodes[i].count){
						tails[nodes[i].count] = nodes[i].prev;
					}else{
						tails[nodes[i].count] = NULL;
					}
				}
			}
			for(i = 0; i < windowSize; i++){
				if(samples[i] && ((samples[i])->start >= region->start) && ((samples[i])->start <= (region->start + (config.chunkSize)*(region->nChunks)))){
					samples[i] = NULL;
				}
			}
			deleteRegion(region);
			plugin_free(nodes);
		}
		pthread_mutex_unlock(&winLock);
	}
	LOG_ALLOC(type, in, out, size);
}

void handleSample(void *addr){
	struct ShamblesChunk *chunk;
	struct List *node;
	pthread_mutex_lock(&winLock);
	uint16_t flags = 0;
	if((chunk = getChunkInfo(addr)) != NULL){
		flags = LOG_TRACKED;
		node = (struct List *)(chunk->privData);
		if(node->loc == FAST){
			flags |= LOG_HIT;
		}
		if(samples[currentLoc]){
			if(chunk == samples[currentLoc]){
				//do nothing
				currentLoc++;
				currentLoc %= windowSize;
				pthread_mutex_unlock(&winLock);
				LOG_SAMPLE(addr, NULL, flags);
				return;
			}
			//demote chunk currently in samples[currentLoc]
			node = (struct List *)(samples[currentLoc]->privData);
			if(node->count <= 0){
				printf("%d\n", *((int *)NULL));
			}
			if((node->loc == SLOW) || (slowList == NULL) || slowList->count < node->count - 1){
				//no migration
				if(!tails[node->count - 1]){
					if(node == tails[node->count]){
						if(node->prev == node){
							//corner case list has 1 element
							if(node->loc == SLOW && fastList->prev->count == node->count){
								tails[node->count] = fastList->prev;
							}else{
								tails[node->count] = NULL;
							}
						}else if(node->prev->count == node->count){
							tails[node->count] = node->prev;
						}else if(node->loc == SLOW && fastList->prev->count == node->count){
							tails[node->count] = fastList->prev;
						}else{
							tails[node->count] = NULL;
						}
					}else{
						if(node == slowList){
							dq(&slowList, node);
							insert(node, tails[node->count]);
						}else if(node == fastList){
							dq(&fastList, node);
							insert(node, tails[node->count]);
						}else{
							move(node, tails[node->count]);
						}
					}
				}else{
					if(tails[node->count] == node){
						if(node == node->prev){
							if((node->loc == FAST) || (fastList->prev->count != node->count)){
								tails[node->count] = NULL;
							}else{
								tails[node->count] = fastList->prev;
							}
						}else if(node->prev->count == node->count){
							tails[node->count] = node->prev;
						}else if(node->loc == SLOW && fastList->prev->count == node->count){
							tails[node->count] = fastList->prev;
						}else{
							tails[node->count] = NULL;
						}
					}
					if(node == slowList){
						dq(&slowList, node);
						insert(node, tails[node->count - 1]);
					}else if(node == fastList){
						dq(&fastList, node);
						insert(node, tails[node->count - 1]);
					}else{
						move(node, tails[node->count - 1]);
					}
				}
				tails[node->count - 1] = node;

			}else{
				//migration
				flags |= LOG_MIGRATION;
				struct List *tmp;
				swap(&config, slowList->chunk, samples[currentLoc]);
				tmp = slowList;
				dq(&slowList, slowList);
				enq(&fastList, tmp);
				tmp->loc = FAST;
				dq(&fastList, node);
				node->loc = SLOW;
				if(tails[node->count] == node){
					if(node->prev->count == node->count){
						tails[node->count] = node->prev;
					}else{
						tails[node->count] = NULL;
					}
				}
				if(tails[node->count - 1] == NULL){
					if(tails[node->count]->loc == SLOW){
						insert(node, tails[node->count]);
					}else{
						push(&slowList, node);
					}
				}else if(tails[node->count - 1]->loc == SLOW){
					insert(node, tails[node->count - 1]);
				}else{
					push(&slowList, node);
				}
				tails[node->count - 1] = node;

			}
			node->count--;
		}
		samples[currentLoc] = chunk;
		{
			node = (struct List *)(chunk->privData);
			if((node->loc == FAST) || fastList->prev->count > node->count){
				//no migration
				if(!tails[node->count + 1]){
					int i;
					if(node == tails[node->count]){
						if(node->prev == node){
							//corner case list has 1 element
							tails[node->count] = NULL;
						}
						else if(node->prev->count == node->count){
							tails[node->count] = node->prev;
						}else{
							tails[node->count] = NULL;
						}
					}

					for(i = node->count + 2; i < windowSize; i++){
						if(tails[i]) break;
					}
					if(tails[i]){
						if(tails[i]->loc == node->loc){
							move(node, tails[i]);
						}else{
							dq(&slowList, node);
							push(&slowList, node);
						}
					}else{
						dq(&fastList, node);
						push(&fastList, node);
					}
				}else{
					if(node == tails[node->count]){
						if(node->prev->count == node->count){
							tails[node->count] = node->prev;
						}else{
							tails[node->count] = NULL;
						}
					}
					if(tails[node->count + 1]->loc == node->loc){
						move(node, tails[node->count + 1]);
					}else{
						dq(&slowList, node);
						push(&slowList, node);
					}
				}
				tails[node->count + 1] = node;
			}else{
				//migration
				flags |= LOG_MIGRATION1;
				int i;
				struct List *tmp;
				swap(&config, chunk, fastList->prev->chunk);
				tmp = fastList->prev;
				dq(&fastList, tmp);
				push(&slowList, tmp);
				tmp->loc = SLOW;
				dq(&slowList, node);
				node->loc = FAST;
				if(tails[node->count] == node){
					if(node->prev->count == node->count){
						tails[node->count] = node->prev;
					}else{
						tails[node->count] = NULL;
					}
				}

				for(i = node->count + 1; i < windowSize; i++){
					if(tails[i]) break;
				}
				if(tails[i]){
					insert(node, tails[i]);
				}else{
					push(&fastList, node);
				}
				tails[node->count + 1] = node;
			}
			node->count++;
		}
		currentLoc++;
		currentLoc %= windowSize;
	}
	pthread_mutex_unlock(&winLock);
	LOG_SAMPLE(addr, NULL, flags);
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

int windowInit(){
	int err;
	char *env;
	pthread_mutex_lock(&winLock);
	if(err = ShamblesPluginConfigInit(&config)){
		pthread_mutex_unlock(&winLock);
		return err;
	}
	shamblesStructsInit(&config);
	INIT_LOGGER();
	freeFastChunks = config.fastChunks;
	fastList = NULL;
	slowList = NULL;
	if((env = getenv("SHAMBLES_WINDOW_SIZE"))==NULL){
		windowSize = 10;
	}else{
		windowSize = atoi(env);
		if(windowSize <= 0){
			windowSize = 10;
		}
	}
	samples = plugin_malloc(windowSize * sizeof(struct ShamblesChunk *));
	tails = plugin_malloc((windowSize + 1) * sizeof(struct List *));
	if((samples == NULL) || (tails == NULL)){
		return -ENOMEM;
	}
	for(int i = 0; i < windowSize; i++){
		samples[i] = NULL;
		tails[i] = NULL;
	}
	tails[windowSize] = NULL;
	pthread_mutex_unlock(&winLock);
}

void shambles_init(void (**reportAllocFunc)(AllocEventType, void *, void *, size_t)){
	pthread_t thread;
	if(windowInit()){
		return;
	}
	
	*reportAllocFunc = &allocCallback;

	pthread_create(&thread, NULL, &sampler_thread_func, NULL);
	pthread_detach(thread);
}
