#include <stdint.h>
#include <structures.h>
#include <jemalloc.h>

#define STRUCTURE_CHUNK_BITS	14
#define LAST_LEVEL_BITS			10
#define LOW_LEVEL_BITS			6

#define NODE_MEMORDER 16
static uint64_t ADDR_RELEVANT_BITS;
static uint64_t PAGE_SHIFT;
static uint64_t REGION_SHIFT;
static uint64_t REGION_SIZE;
#ifdef P2CHSIZE
static uint64_t CHUNK_SHIFT;
#endif /* P2CHSIZE */
static uint64_t CHUNK_SIZE;
static uint64_t REGION_NODE_MASK;
static uint64_t NON_LEAF_NODES_DEPTH;
static uint64_t LEAF_NODES_ARRAY_SIZE;
static uint64_t NON_LEAF_NODES_ARRAY_SIZE;
static uint64_t NON_LEAF_NODE_MASK_0;
static uint64_t NON_LEAF_SHIFT;
static uint64_t NON_LEAF_SHIFT_0;

struct RegionNode{
	void *lowAddr, *highAddr;
	struct ShamblesRegion *lowReg, *highReg;
};

struct NonLeafNode{
	union{
		struct NonLeafNode *nln;
		struct RegionNode *regions;
	};
	uint64_t refcount;
};

static struct NonLeafNode *root;

void shamblesStructsInit(struct ShamblesPluginConfig *config){
	PAGE_SHIFT = 12;
	ADDR_RELEVANT_BITS = 39;
#ifdef P2CHSIZE
	CHUNK_SHIFT = 63 - __builtin_clzl(config->chunkSize);
	CHUNK_SIZE = 1 << CHUNK_SHIFT;
#else
	CHUNK_SIZE = config->chunkSize;
#endif /* P2CHSIZE */
	REGION_SHIFT = 63 - __builtin_clzl(config->sizeThresshold);
	REGION_SIZE = 1 << REGION_SHIFT;
	REGION_NODE_MASK = ((1LLU << NODE_MEMORDER)/sizeof(struct RegionNode) - 1) << REGION_SHIFT;
	NON_LEAF_NODES_ARRAY_SIZE = (1LLU << NODE_MEMORDER)/sizeof(struct NonLeafNode);
	LEAF_NODES_ARRAY_SIZE = (1LLU << NODE_MEMORDER)/sizeof(struct RegionNode);
	NON_LEAF_NODE_MASK_0 = ((1LLU << NODE_MEMORDER)/sizeof(struct NonLeafNode) - 1) << (REGION_SHIFT + __builtin_ctzl(LEAF_NODES_ARRAY_SIZE));
	NON_LEAF_SHIFT = __builtin_ctzl(NON_LEAF_NODES_ARRAY_SIZE);
	NON_LEAF_NODES_DEPTH = 0;
	while(NON_LEAF_NODE_MASK_0 < (1LLU << ADDR_RELEVANT_BITS)){
		NON_LEAF_NODES_DEPTH++;
		NON_LEAF_NODE_MASK_0 <<= NON_LEAF_SHIFT;
	}
	NON_LEAF_SHIFT_0 = __builtin_ctzl(NON_LEAF_NODE_MASK_0);
	root = plugin_calloc(__builtin_popcountl(NON_LEAF_NODE_MASK_0 & ((1 << ADDR_RELEVANT_BITS) - 1)), sizeof(struct NonLeafNode));
}

struct ShamblesRegion *getRegion(void *addr){
	struct NonLeafNode *nl;
	struct RegionNode *rn;
	uint64_t mask, shift;
	nl = root + (((uint64_t)addr) & NON_LEAF_NODE_MASK_0 & ADDR_RELEVANT_BITS);
	mask = NON_LEAF_NODE_MASK_0 >> NON_LEAF_SHIFT;
	shift = NON_LEAF_SHIFT_0 - NON_LEAF_SHIFT;
	for(int i = 0; i < NON_LEAF_NODES_DEPTH; i++){
		if(nl->nln == NULL){
			return NULL;
		}
		nl = nl->nln + ((((uint64_t)addr) & mask) >> shift);
		shift -= NON_LEAF_SHIFT;
		mask >>= NON_LEAF_SHIFT;
	}
	if(nl->regions == NULL){
		return NULL;
	}
	rn = nl->regions + ((((uint64_t)addr) & REGION_NODE_MASK) >> REGION_SHIFT);
	if(addr >= rn->highAddr){
		return rn->highReg;
	}
	if(addr < rn->lowAddr){
		return rn->lowReg;
	}
	return NULL;
}

struct ShamblesChunk *getChunkInfo(void *addr){
	struct ShamblesRegion *region;
	if((region = getRegion(addr)) == NULL){
		return NULL;
	}
#ifdef P2CHSIZE
	return region->chunks + (((uint64_t)(addr - region->start)) >> CHUNK_SHIFT);
#else
	return region->chunks + (((uint64_t)(addr - region->start))/CHUNK_SIZE);
#endif /* P2CHSIZE */
}

struct ShamblesChunk *getChunkInfoNU(void *addr){
	struct ShamblesRegion *region;
	int i;
	if((region = getRegion(addr)) == NULL){
		return NULL;
	}
	for(i = 0; i < region->nChunks - 1; i++){
		if(addr < region->chunks[i+1].start){
			break;
		}
	}
	return region->chunks + i;
}

/*
 * This is suboptimal af, but it will not be optimised anytime soon, we might as well consider it as not performance critical (or maybe not)
 */
struct ShamblesRegion *addRegion(void *addr, size_t size){
	size_t nChunks, mod;
	void *end = addr + size;
	struct NonLeafNode *nl;
	struct RegionNode *rn;
	uint64_t mask, shift;
	uint64_t addr0 = (uint64_t)addr;
	struct ShamblesRegion *output;
	output = plugin_malloc(sizeof(struct ShamblesRegion));
	output->start = addr;
#ifdef P2CHSIZE
	nChunks = size >> CHUNK_SHIFT;
	mod = size & (CHUNK_SIZE - 1);
#else
	nChunks = size/CHUNK_SIZE;
	mod = size % CHUNK_SIZE;
#endif /* P2CHSIZE */

	output->nChunks = nChunks + (mod != 0);
	output->chunks = plugin_malloc(output->nChunks*sizeof(struct ShamblesChunk));
	for(int i = 0; i < nChunks; i++){
		output->chunks[i].size = CHUNK_SIZE;
		output->chunks[i].start = addr + (i*CHUNK_SIZE);
	}
	if(mod){
		output->chunks[nChunks].size = mod;
		output->chunks[nChunks].start = addr + (nChunks*CHUNK_SIZE);
	}

	if(addr0 & ((1LLU << REGION_SHIFT) - 1)){
		nl = root + (addr0 & NON_LEAF_NODE_MASK_0 & ADDR_RELEVANT_BITS);
		mask = NON_LEAF_NODE_MASK_0 >> NON_LEAF_SHIFT;
		shift = NON_LEAF_SHIFT_0 - NON_LEAF_SHIFT;
		for(int i = 0; i < NON_LEAF_NODES_DEPTH; i++){
			if(nl->nln == NULL){
				nl->nln = plugin_calloc(NON_LEAF_NODES_ARRAY_SIZE, sizeof(struct NonLeafNode));
			}
			nl->refcount++;
			nl = nl->nln + ((addr0 & mask) >> shift);
			shift -= NON_LEAF_SHIFT;
			mask >>= NON_LEAF_SHIFT;
		}
		if(nl->regions == NULL){
			nl->regions = plugin_calloc(LEAF_NODES_ARRAY_SIZE, sizeof(struct RegionNode));
		}
		nl->refcount++;
		rn = nl->regions + ((addr0 & REGION_NODE_MASK) >> REGION_SHIFT);
		rn->highAddr = addr;
		rn->highReg = output;
	}
	addr0 &= ~((1LLU << REGION_SHIFT) - 1);
	addr0 += 1LLU << REGION_SHIFT;
	while(addr0 < (((uint64_t)end) & ~((1LLU << REGION_SHIFT) - 1))){
		nl = root + (addr0 & NON_LEAF_NODE_MASK_0 & ADDR_RELEVANT_BITS);
		mask = NON_LEAF_NODE_MASK_0 >> NON_LEAF_SHIFT;
		shift = NON_LEAF_SHIFT_0 - NON_LEAF_SHIFT;
		for(int i = 0; i < NON_LEAF_NODES_DEPTH; i++){
			if(nl->nln == NULL){
				nl->nln = plugin_calloc(NON_LEAF_NODES_ARRAY_SIZE, sizeof(struct NonLeafNode));
			}
			nl->refcount++;
			nl = nl->nln + ((addr0 & mask) >> shift);
			shift -= NON_LEAF_SHIFT;
			mask >>= NON_LEAF_SHIFT;
		}
		if(nl->regions == NULL){
			nl->regions = plugin_calloc(LEAF_NODES_ARRAY_SIZE, sizeof(struct RegionNode));
		}
		nl->refcount++;
		rn = nl->regions + ((addr0 & REGION_NODE_MASK) >> REGION_SHIFT);
		rn->highAddr = addr;
		rn->highReg = output;
		rn->lowAddr = end;
		rn->lowReg = output;
		addr0 += 1LLU << REGION_SHIFT;
	}
	if(addr0 != (uint64_t)end){
		nl = root + (addr0 & NON_LEAF_NODE_MASK_0 & ADDR_RELEVANT_BITS);
		mask = NON_LEAF_NODE_MASK_0 >> NON_LEAF_SHIFT;
		shift = NON_LEAF_SHIFT_0 - NON_LEAF_SHIFT;
		for(int i = 0; i < NON_LEAF_NODES_DEPTH; i++){
			if(nl->nln == NULL){
				nl->nln = plugin_calloc(NON_LEAF_NODES_ARRAY_SIZE, sizeof(struct NonLeafNode));
			}
			nl->refcount++;
			nl = nl->nln + ((addr0 & mask) >> shift);
			shift -= NON_LEAF_SHIFT;
			mask >>= NON_LEAF_SHIFT;
		}
		if(nl->regions == NULL){
			nl->regions = plugin_calloc(LEAF_NODES_ARRAY_SIZE, sizeof(struct RegionNode));
		}
		nl->refcount++;
		rn = nl->regions + ((addr0 & REGION_NODE_MASK) >> REGION_SHIFT);
		rn->lowAddr = end;
		rn->lowReg = output;
	}
	return output;
}

struct ShamblesRegion *addRegionChunks(void *addr, size_t size, int nChunks, size_t *chunkSizes){
	void *end = addr + size;
	struct NonLeafNode *nl;
	struct RegionNode *rn;
	uint64_t mask, shift;
	uint64_t addr0 = (uint64_t)addr;
	struct ShamblesRegion *output;
	output = plugin_malloc(sizeof(struct ShamblesRegion));
	output->start = addr;
	output->nChunks = nChunks;
	output->chunks = plugin_malloc(output->nChunks*sizeof(struct ShamblesChunk));
	output->chunks[0].size = chunkSizes[0];
	output->chunks[0].start = addr;
	for(int i = 1; i < nChunks; i++){
		output->chunks[i].size = chunkSizes[i];
		output->chunks[i].start = output->chunks[i-1].start + chunkSizes[i-1];
	}

	if(addr0 & ((1LLU << REGION_SHIFT) - 1)){
		nl = root + (addr0 & NON_LEAF_NODE_MASK_0 & ADDR_RELEVANT_BITS);
		mask = NON_LEAF_NODE_MASK_0 >> NON_LEAF_SHIFT;
		shift = NON_LEAF_SHIFT_0 - NON_LEAF_SHIFT;
		for(int i = 0; i < NON_LEAF_NODES_DEPTH; i++){
			if(nl->nln == NULL){
				nl->nln = plugin_calloc(NON_LEAF_NODES_ARRAY_SIZE, sizeof(struct NonLeafNode));
			}
			nl->refcount++;
			nl = nl->nln + ((addr0 & mask) >> shift);
			shift -= NON_LEAF_SHIFT;
			mask >>= NON_LEAF_SHIFT;
		}
		if(nl->regions == NULL){
			nl->regions = plugin_calloc(LEAF_NODES_ARRAY_SIZE, sizeof(struct RegionNode));
		}
		nl->refcount++;
		rn = nl->regions + ((addr0 & REGION_NODE_MASK) >> REGION_SHIFT);
		rn->highAddr = addr;
		rn->highReg = output;
	}
	addr0 &= ~((1LLU << REGION_SHIFT) - 1);
	addr0 += 1LLU << REGION_SHIFT;
	while(addr0 < (((uint64_t)end) & ~((1LLU << REGION_SHIFT) - 1))){
		nl = root + (addr0 & NON_LEAF_NODE_MASK_0 & ADDR_RELEVANT_BITS);
		mask = NON_LEAF_NODE_MASK_0 >> NON_LEAF_SHIFT;
		shift = NON_LEAF_SHIFT_0 - NON_LEAF_SHIFT;
		for(int i = 0; i < NON_LEAF_NODES_DEPTH; i++){
			if(nl->nln == NULL){
				nl->nln = plugin_calloc(NON_LEAF_NODES_ARRAY_SIZE, sizeof(struct NonLeafNode));
			}
			nl->refcount++;
			nl = nl->nln + ((addr0 & mask) >> shift);
			shift -= NON_LEAF_SHIFT;
			mask >>= NON_LEAF_SHIFT;
		}
		if(nl->regions == NULL){
			nl->regions = plugin_calloc(LEAF_NODES_ARRAY_SIZE, sizeof(struct RegionNode));
		}
		nl->refcount++;
		rn = nl->regions + ((addr0 & REGION_NODE_MASK) >> REGION_SHIFT);
		rn->highAddr = addr;
		rn->highReg = output;
		rn->lowAddr = end;
		rn->lowReg = output;
		addr0 += 1LLU << REGION_SHIFT;
	}
	if(addr0 != (uint64_t)end){
		nl = root + (addr0 & NON_LEAF_NODE_MASK_0 & ADDR_RELEVANT_BITS);
		mask = NON_LEAF_NODE_MASK_0 >> NON_LEAF_SHIFT;
		shift = NON_LEAF_SHIFT_0 - NON_LEAF_SHIFT;
		for(int i = 0; i < NON_LEAF_NODES_DEPTH; i++){
			if(nl->nln == NULL){
				nl->nln = plugin_calloc(NON_LEAF_NODES_ARRAY_SIZE, sizeof(struct NonLeafNode));
			}
			nl->refcount++;
			nl = nl->nln + ((addr0 & mask) >> shift);
			shift -= NON_LEAF_SHIFT;
			mask >>= NON_LEAF_SHIFT;
		}
		if(nl->regions == NULL){
			nl->regions = plugin_calloc(LEAF_NODES_ARRAY_SIZE, sizeof(struct RegionNode));
		}
		nl->refcount++;
		rn = nl->regions + ((addr0 & REGION_NODE_MASK) >> REGION_SHIFT);
		rn->lowAddr = end;
		rn->lowReg = output;
	}
	return output;
}

static void recursiveDelete(uint64_t addr, int flag, struct NonLeafNode *nl, uint64_t mask, uint64_t shift, int depth){
	if(!depth){
		struct RegionNode *rn;
		rn = nl->regions + ((addr & REGION_NODE_MASK) >> REGION_SHIFT);
		if(flag & 1){
			rn->highAddr = NULL;
			rn->highReg = NULL;
		}
		if(flag & 2){
			rn->lowAddr = NULL;
			rn->lowReg = NULL;
		}
		if(!(--nl->refcount)){
			plugin_free(nl->regions);
			nl->regions = NULL;
		}
	}else{
		recursiveDelete(addr, flag, nl->nln + ((addr & mask) >> shift), mask >> NON_LEAF_SHIFT, shift - NON_LEAF_SHIFT, depth - 1);
		if(!(--nl->refcount)){
			plugin_free(nl->nln);
			nl->nln = NULL;
		}
	}
}

/*
 * This is suboptimal af, but it will not be optimised anytime soon, we might as well consider it as not performance critical (or maybe not)
 */
void deleteRegion(struct ShamblesRegion *region){
	void *addr, *end;
	struct NonLeafNode *nl, *next;
	struct RegionNode *rn;
	uint64_t mask, shift;
	addr = region->chunks[0].start;
	end = region->chunks[region->nChunks - 1].start + region->chunks[region->nChunks - 1].size;
	uint64_t addr0 = (uint64_t)addr;
	if(addr0 & ((1LLU << REGION_SHIFT) - 1)){
		recursiveDelete(addr0, 1, root + (addr0 & NON_LEAF_NODE_MASK_0 & ADDR_RELEVANT_BITS), NON_LEAF_NODE_MASK_0 >> NON_LEAF_SHIFT, NON_LEAF_SHIFT_0 - NON_LEAF_SHIFT, NON_LEAF_NODES_DEPTH);
	}
	addr0 &= ~((1LLU << REGION_SHIFT) - 1);
	addr0 += 1LLU << REGION_SHIFT;
	while(addr0 < (((uint64_t)end) & ~((1LLU << REGION_SHIFT) - 1))){
		recursiveDelete(addr0, 3, root + (addr0 & NON_LEAF_NODE_MASK_0 & ADDR_RELEVANT_BITS), NON_LEAF_NODE_MASK_0 >> NON_LEAF_SHIFT, NON_LEAF_SHIFT_0 - NON_LEAF_SHIFT, NON_LEAF_NODES_DEPTH);
		addr0 += 1LLU << REGION_SHIFT;
	}
	if(addr0 != (uint64_t)end){
		recursiveDelete(addr0, 2, root + (addr0 & NON_LEAF_NODE_MASK_0 & ADDR_RELEVANT_BITS), NON_LEAF_NODE_MASK_0 >> NON_LEAF_SHIFT, NON_LEAF_SHIFT_0 - NON_LEAF_SHIFT, NON_LEAF_NODES_DEPTH);
	}

	plugin_free(region->chunks);
	plugin_free(region);
}
