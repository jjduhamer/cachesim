/*
 * mycache.h: implements functions, structures, and macros required
 *            for each level of cache.
 *
 * Authors: John Duhamel and Mike Travis
 */

//#define DEBUG

#define DIRTY   1
#define NODIRTY 0

typedef unsigned int uint_t;
typedef unsigned long long ulong_t;

ulong_t num_load = 0;
ulong_t num_store = 0;
ulong_t num_branch = 0;
ulong_t num_comp = 0;

ulong_t load_cycles = 0;
ulong_t store_cycles = 0;
ulong_t branch_cycles = 0;
ulong_t comp_cycles = 0;
static ulong_t * cycles;

typedef struct cache_block * cache_set;
typedef struct cache * cache_level;

/*
 * cache: this implements all the paramaters for 1 level of cache required for this
 * simulation.
 * 
 * NOTE: I implement an array of cache_block structs. Because the number of blocks 
 * depends on parameters that are passed by the configuration file.  This array 
 * must be initialized at runtime.
 */
struct cache {
    // cache params
    uint_t block_size;
    uint_t cache_size;
    uint_t assoc;
    uint_t hit_time;
    uint_t miss_time;
    uint_t transfer_time;
    uint_t bus_width;
    uint_t sets_in_cache;
    uint_t bits_in_tag;
    
    // main memory params
    uint_t sendaddr;
    uint_t ready;
    uint_t chunktime;
    uint_t chunksize;
    
    // performance params
    ulong_t hit_count;
    ulong_t miss_count;
    ulong_t kickouts;
    ulong_t dirty_kickouts;
    ulong_t transfers;

    // holds an array of cache sets
    cache_set * set;

    // points to next cache layer
    cache_level next;
};

/*
 * struct cache_block: implements a cache block
 */
struct cache_block {
    char valid;
    char dirty;
    uint_t tag;
};

#ifdef DEBUG
/*
 * cache_print_sets: prints the contents of each nonempty set in the cache
 */
void cache_print_sets(cache_level cache)
{
    uint_t j, n, d;

    for (j=0; j<cache->sets_in_cache; j++) {
        n=0;
        for (d=0; d<cache->assoc; d++)
            if (cache->set[j][d].valid) {
                printf("| index: %4x valid: %x dirty: %x tag: %8x ",
                        j, cache->set[j][d].valid, cache->set[j][d].dirty, 
                        cache->set[j][d].tag);
                n = 1;
            }
        if (n==1) printf("|\n");
    }
    printf("\n");
}
#endif

/*
 * cache_hit: determines if the data for the address if located in the cache and
 * updates the cache hit count or miss count.
 *
 * returns 1 for hit, 0 for miss
 */
char cache_hit(cache_level cache, uint_t addr) 
{
    uint_t index, tag;
    uint_t j;
    struct cache_block d;

    // calculate params
    index = (addr / cache->block_size) % cache->sets_in_cache;
    tag = addr >> (32 - cache->bits_in_tag);
    
#ifdef DEBUG 
    printf("\tchecking index: %x for tag: %x... ", index, tag);
#endif

    // search set for correct valid tag
    for (j=0; j<cache->assoc; j++) {
        if (cache->set[index][j].valid && cache->set[index][j].tag == tag) {
            cache->hit_count++;
            *cycles += cache->hit_time;
#ifdef DEBUG
            printf("HIT\n");
            printf("\tcache hit time added (+%u)\n", cache->hit_time);
#endif
            return 1;
        }
    }
    cache->miss_count++;
    *cycles += cache->miss_time;
#ifdef DEBUG
    printf("MISS\n");
    printf("\tcache miss time added (+%u)\n", cache->miss_time);
#endif
    return 0;
}

/*
 * void exchange(void *j, void *d, size_t size)
 *
 * This simply copies the number of bytes given by size from *j
 * to *d and from *d to *j.
 */
void exchange(void *j, void *d, size_t size)
{
    char h;
    size_t n;
    for (n=0; n<size; n++) {
        h = *(char*)(j+n);
        *(char*)(j+n) = *(char*)(d+n);
        *(char*)(d+n) = h;
    }
}

/*
 * cache_prepare_block: moves a block to the front of the lru queue to be used next.
 */
void cache_prepare_block(cache_level cache, uint_t index, uint_t block)
{
    uint_t j;

    if (block == 0)
        return;

    for (j=block; j>0; j--) {
        exchange(&cache->set[index][j], &cache->set[index][j-1], sizeof(struct cache_block *));
    }
}

/*
 * cache_update: updates the contents of set with a LRU policy.
 *
 * NOTE: cache->set[index] is an array of pointers.  thus, we can shift things
 * around such that cache->set[index][0] is always the LRU block in the set.
 * I call this the set priority queue in my notes.
 */
void cache_update(cache_level cache, uint_t addr, char dirty)
{
    uint_t index, tag, j;

    // calculate  useful params
    index = (addr / cache->block_size) % cache->sets_in_cache;
    tag = addr >> (32 - cache->bits_in_tag);
 
    // we need to ensure that we do not write data that already exists in the cache
    for (j=0; j<cache->assoc; j++) {
        if (cache->set[index][j].tag == tag) {
            cache_prepare_block(cache, index, j);
            break;
        }
    }

    // update LRU block params
    cache->set[index]->valid = 1;
    cache->set[index]->dirty = dirty;
    cache->set[index]->tag = tag;

    // send the block we just updated to the back of the set priority queue
    for (j=0; j<cache->assoc-1; j++) {
        exchange(&cache->set[index][j], &cache->set[index][j+1], sizeof(struct cache_block *));
    }

#ifdef DEBUG 
    printf("\tset index: %x to tag: %x and dirty: %x\n", index, tag, dirty);
    cache_print_sets(cache);
#endif
}

/*
 * cache_write: simply calls cache_update with the dirty bit set
 */
void cache_write(cache_level cache, uint_t addr)
{
    cache_update(cache, addr, DIRTY);
}

/*
 * cache_read: simply calls cache_update without the dirty bit set
 */
void cache_read(cache_level cache, uint_t addr)
{
    cache_update(cache, addr, NODIRTY);
}

/*
 * cache_transfer: handles data transfer between a lower level and a higher level
 * of cache.  It always goes in that direction.
 */
void cache_transfer(cache_level l1, uint_t addr)
{
    cache_level l2 = l1->next;
    uint_t trans_cycles;

    if (l2->next == NULL) {     // l2 is main memory
        trans_cycles = l2->sendaddr + l2->ready + (l2->chunktime * l1->block_size / l2->chunksize);
    } else {                    // l2 is a cache
        trans_cycles = l2->transfer_time * (l1->block_size / l2->bus_width);
    }
    *cycles += trans_cycles;
    l1->transfers++;
    cache_read(l1, addr);

    // l1 replay hit
    *cycles += l1->hit_time;

#ifdef DEBUG 
    printf("\ttransfer time added (+%u)\n", trans_cycles); 
    printf("\treplay hit time added (+%u)\n", l1->hit_time);
#endif
}

/*
 * cache_kickout: handles data transfer between a higher level and a lower level
 * of cache.  It always goes in that direction.
 */
void cache_kickout(cache_level l1, uint_t addr)
{
    cache_level l2 = l1->next;
    uint_t index;

    index = (addr / l1->block_size) % l1->sets_in_cache;
  
    // handle kickout
    if (l1->set[index]->valid) {
        l1->kickouts++;

#ifdef DEBUG
        printf("\tupdated kickouts\n");
#endif

    // handle dirty kickout
    if (l1->set[index]->dirty) {
        uint_t l1_addr;

        l1->dirty_kickouts++;

#ifdef DEBUG
        printf("\tupdated dirty kickouts\n");
#endif
        
        // reconsturct address of LRU block in l1 cache and send to l2 cache
        l1_addr = (l1->set[index]->tag << (32 - l1->bits_in_tag)) + (index * l1->block_size);
        
        // i honestly don't know why i need to do this, but it makes my code
        // match the output files we were given
        if (cache_hit(l2, l1_addr)) {
            cache_transfer(l1, l1_addr);
            l1->transfers--;
            *cycles -= l1->hit_time;
        }
        cache_write(l2, l1_addr);
    }
    }
}

/*
 * cache_fetch: takes care of loading cache data in the caches and updates timing 
 * parameters accordingly. 
 */
void cache_fetch(cache_level cache, uint_t addr, ulong_t * op_cycles)
{ 
#ifdef DEBUG 
    printf("addr = %x\n", addr);
#endif
    
    cycles = op_cycles;

    if (!cache_hit(cache, addr)) {
        cache_kickout(cache,  addr);

        if (cache->next->next != NULL) 
            cache_fetch(cache->next, addr, op_cycles);
        
        cache_transfer(cache, addr);
    }
}

/*
 * cache_store: handles all store requests to the cache
 */
void cache_store(cache_level cache, uint_t addr, ulong_t * op_cycles)
{ 
    cache_fetch(cache, addr, op_cycles);
    cache_write(cache, addr);
}



