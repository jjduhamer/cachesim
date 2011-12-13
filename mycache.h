/*
 * mycache.h: implements functions, structures, and macros required
 *            for each level of cache.
 *
 * Authors: John Duhamel and Mike Travis
 */

//#define DEBUG

#define CLEAN 0
#define NODIRTY 0
#define DIRTY 1

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
ulong_t perf_cycles = 1;
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

    // we will need to add the hit time no matter what
    // because misses need to add for the replay hit
    *cycles += cache->hit_time;

    // search set for correct valid tag
    for (j=0; j<cache->assoc; j++) {
        if (cache->set[index][j].valid && cache->set[index][j].tag == tag) {
#ifdef DEBUG
            printf("HIT\n");
            printf("\tcache hit time added (+%u)\n", cache->hit_time);
#endif
            cache->hit_count++;
            //*cycles += cache->hit_time;
            return 1;
        }
    }
#ifdef DEBUG
    printf("MISS\n");
    printf("\tcache miss time added (+%u)\n", cache->miss_time);
#endif
    cache->miss_count++;
    *cycles += cache->miss_time;
    return 0;
}

/*
 * cache_update: updates the contents of set with a LRU policy.
 *
 * NOTE: the LRU block will always be the first in the set
 */
void cache_update(cache_level cache, uint_t addr, char dirty)
{
    uint_t index, tag;
    uint_t j;
    struct cache_block d;

    // calculate  useful params
    index = (addr / cache->block_size) % cache->sets_in_cache;
    tag = addr >> (32 - cache->bits_in_tag);
    
    // update LRU block params
    //if (cache->set[index][0].valid)
    //    cache->kickouts++;
    cache->set[index]->valid = 1;
    cache->set[index]->dirty = dirty;
    cache->set[index]->tag = tag;
    
#ifdef DEBUG 
    printf("\tset index: %x to tag: %x and dirty: %x\n", index, tag, dirty);
#endif
    
    // update the set priority queue
    d = cache->set[index][0];
    for (j=0; j<cache->assoc-1; j++)
        cache->set[index][j] = cache->set[index][j+1];
    cache->set[index][j] = d; 
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
 * cache_kickout: handles kickouts between caches
 */
void cache_kickout(cache_level l1, cache_level l2, uint_t addr)
{
    uint_t index, l1_addr;

    // reconsturct address of LRU block in l1 cache
    index = (addr / l1->block_size) % l1->sets_in_cache;
    l1_addr = (l1->set[index]->tag << (32 - l1->bits_in_tag)) 
             + (index * l1->block_size);

    if (l1->set[index]->dirty)
        l1->dirty_kickouts++;
    
    // send this address to l2 cache with appropriate dirty bit
    cache_update(l2, l1_addr, l1->set[index]->dirty);
}

/*
 * cache_dirty: returns 1 if the relevant set contains only dirty block.
 * 0 otherwise.
 */
char cache_dirty(cache_level cache, uint_t addr)
{
    uint_t index, j;

    index = (addr / cache->block_size) % cache->sets_in_cache;

#ifdef DEBUG
    printf("\tchecking index: %x for dirty set... ", index); 
#endif
    for (j=0; j<cache->assoc; j++)
        if (!cache->set[index][j].valid || !cache->set[index][j].dirty) {
#ifdef DEBUG
        printf("block %u clean\n", j); 
#endif
            return 0;
        }
#ifdef DEBUG
    printf("all blocks dirty\n");
#endif
    return 1;
}

void cache_transfer(cache_level l1, cache_level l2, uint_t addr)
{
    if (l2->next == NULL) {     // l2 is main memory
        *cycles += l2->sendaddr + l2->ready + (l2->chunktime * l1->block_size / l2->chunksize);
        l1->transfers++;
        cache_read(l1, addr);
    } else {                    // l2 is a cache
        *cycles += l2->transfer_time * (l1->block_size / l2->bus_width);
        l1->transfers++;        
    }
}

/*
 * cache_store: handles all store requests to the cache
 */
void cache_store(cache_level l1, cache_level l2, cache_level mm, 
                uint_t addr, ulong_t * op_cycles)
{ 
#ifdef DEBUG 
    printf("addr = %x\n", addr);
#endif

    cycles = op_cycles;

    // do we need to handle an l1 cache miss?
    if (!cache_hit(l1, addr)) {
        // do we need to handle an l2 cache miss?
        if (!cache_hit(l2, addr)) {
            // mm -> l2 transfer
            /*
            *cycles += mm->sendaddr + mm->ready + (mm->chunktime*l2->block_size/mm->chunksize);
            l2->transfers++;
            cache_read(l2, addr);
            */
            cache_transfer(l2, mm, addr);
#ifdef DEBUG 
            printf("\tmem -> l2 transfer time added (+%u)\n", 
                    mm->sendaddr + mm->ready + (mm->chunktime*l2->block_size/mm->chunksize));
            printf("\tl2 transfers incremented to %Lu\n", l2->transfers);
#endif
        }
        // l2 -> l1 transfer
        /*
        *cycles += l2->transfer_time * (l1->block_size / l2->bus_width);
        l1->transfers++;        
        //cache_read(l1, addr);     // not needed, data filled in later 
        */
        cache_transfer(l1, l2, addr);

#ifdef DEBUG 
        printf("\tl2 hit_time added (+%u)\n", l2->hit_time);
        printf("\tl2 -> l1 transfer time added (+%u)\n", 
                l2->transfer_time * (l1->block_size / l2->bus_width));
        printf("\tl1 transfers incremented to %Lu\n", l1->transfers);
#endif
    }
    // l1 replay
    cache_write(l1, addr);
    //*cycles += l1->hit_time;

#ifdef DEBUG 
    printf("\tl1 hit time added (+%u)\n", l1->hit_time);
#endif
}

/*
 * cache_fetch: takes care of loading cache data in the caches and updates timing 
 * parameters accordingly. 
 *
 * NOTE: I was tempted to implement the cache as a linked list so that I could make 
 * this function recursive.  The reason I did not do this was because the main memory
 * behaves differently than the caches so a level 2 miss would add more complexity to 
 * the function than it would take away.
 */
void cache_fetch(cache_level l1, cache_level l2, cache_level mm, 
                     uint_t addr, ulong_t * op_cycles)
{ 
#ifdef DEBUG 
    printf("addr = %x\n", addr);
#endif

    cycles = op_cycles;

    // do we need to handle an l1 cache miss?
    if (cache_hit(l1, addr) == 0) {
        // do we need to kick out the LRU block of l1?
        if (cache_dirty(l1, addr))
            cache_kickout(l1, l2, addr);
            
        // do we need to handle an l2 cache miss?
        if (cache_hit(l2, addr) == 0) {
            // mm -> l2 transfer 
            *cycles += mm->sendaddr + mm->ready + (mm->chunktime*l2->block_size/mm->chunksize);
            l2->transfers++;
            cache_read(l2, addr);

#ifdef DEBUG 
            printf("\tmem -> l2 transfer time added (+%u)\n", 
                    mm->sendaddr + mm->ready + (mm->chunktime*l2->block_size/mm->chunksize));
            printf("\tl2 transfers incremented to %Lu\n", l2->transfers);
#endif
        }
        // l2 -> l1 transfer
        *cycles += l2->transfer_time * (l1->block_size / l2->bus_width);
        l1->transfers++;        
        cache_read(l1, addr);

#ifdef DEBUG 
        printf("\tl2 hit_time added (+%u)\n", l2->hit_time);
        printf("\tl2 -> l1 transfer time added (+%u)\n", 
                l2->transfer_time * (l1->block_size / l2->bus_width));
        printf("\tl1 transfers incremented to %Lu\n", l1->transfers);
#endif
    }
#ifdef DEBUG 
    printf("\tl1 hit time added (+%u)\n", l1->hit_time);
#endif
}



