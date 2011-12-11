/*
 * mycache.h: implements functions, structures, and macros required
 *            for each level of cache.
 *
 * Authors: John Duhamel and Mike Travis
 */

/*
 * global variables
 */

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

typedef struct cache_block * cache_set;

/*
 * cache: this implements all the paramaters for 1 level of cache required for this
 * simulation.
 * 
 * NOTE: I implement an array of cache_block structs. Because the number of blocks 
 * depends on parameters that are passed by the configuration file.  This array 
 * must be initialized at runtime.
 */
struct cache {
    // configuration params
    uint_t block_size;
    uint_t cache_size;
    uint_t assoc;
    uint_t hit_time;
    uint_t miss_time;
    uint_t transfer_time;
    uint_t bus_width;
    uint_t sets_in_cache;
    uint_t bits_in_tag;

    // performance params
    ulong_t hit_count;
    ulong_t miss_count;
    ulong_t kickouts;
    ulong_t dirty_kickouts;
    ulong_t transfers;

    // holds an array of cache sets
    cache_set * set;
};

/*
 * struct cache_block: implements a cache block
 */
struct cache_block {
    char valid;
    uint_t tag;
};

/*
 * main_mem: this struct holds all of the parameters for the main memory required for 
 * this simulation.
 */
struct main_mem {
    // configuration params
    uint_t sendaddr;
    uint_t ready;
    uint_t chunktime;
    uint_t chunksize;

    // performace params
    ulong_t hit_count;
    ulong_t miss_count;
    ulong_t kickouts;
    ulong_t dirty_kickouts;
    ulong_t transfers; 
};

/*
 * fetch: attempts to fetch the byte address and updates the cache hit count or miss count.
 *
 * returns 1 for hit, 0 for miss
 */
char fetch(struct cache * cache, uint_t addr) 
{
    uint_t index, tag;
    uint_t j;
    struct cache_block d;

    // calculate params
    index = (addr / cache->block_size) % cache->sets_in_cache;
    tag = addr >> (32 - cache->bits_in_tag);

    // search set for correct valid tag
    for (j=0; j<cache->assoc; j++) {
        if (cache->set[index][j].valid && cache->set[index][j].tag == tag) {
            cache->hit_count++;
            /*        
            // update the set priority queue
            d = cache->set[index][j];
            while (j<cache->assoc-1)
                cache->set[index][j] = cache->set[index][++j];
            cache->set[index][j] = d; 
            */
            return 1;
        }
    }
    cache->miss_count++;
    return 0;
}

/*
 * update_set: updates the contents of set with a LRU policy.
 *
 * NOTE: the LRU block will always be the first in the set ;D
 */
void update_set(struct cache * cache, uint_t addr)
{
    uint_t index, tag;
    uint_t j;
    struct cache_block d;

    // calculate  useful params
    index = (addr / cache->block_size) % cache->sets_in_cache;
    tag = addr >> (32 - cache->bits_in_tag);
#ifdef DEBUG 
    printf("\tindex: %u\t\ttag: %u\n", index, tag);
#endif
    // update LRU block params
    cache->set[index]->valid = 1;
    cache->set[index]->tag = tag;
    
    // update the set priority queue
    d = cache->set[index][0];
    for (j=0; j<cache->assoc-1; j++)
        cache->set[index][j] = cache->set[index][j+1];
    cache->set[index][j] = d; 
}

/*
 * cache_loadstore: takes care of loading and storing data in the caches and 
 * updates timing parameters accordingly.  Because the load and store instructions 
 * are so similar, it made sense to combine them into one function.
 *
 * NOTE: I was tempted to implement the cache as a linked list so that I could make 
 * this function recursive.  The reason I did not do this was because the main memory
 * behaves differently than the caches so a level 2 miss would add more complexity to 
 * the function than it would take away.
 */
void cache_loadstore(struct cache * l1, struct cache * l2, struct main_mem * mm, 
                     uint_t addr, char op_type)
{ 
    ulong_t * cycles;

#ifdef DEBUG 
    printf("type = %c, addr = %x\n", op_type, addr);
#endif
    // decide which cycle count to update
    switch (op_type) {
        case 'L':   // load
            cycles = &load_cycles;
            break;
        case 'S':   // store
            cycles = &store_cycles;
            break;
        case 'B':   // branch
            cycles = &branch_cycles;
            break;
        case 'C':   // computation
            cycles = &comp_cycles;
            break;
    }

    // attempt to fetch addr from l1 cache
    if (fetch(l1, addr) == 0) {       // l1 cache miss
        // update cycles for l1 cache miss
        *cycles += l1->miss_time;
#ifdef DEBUG 
        printf("\tl1 cache miss on %c\n", op_type);
        printf("\tl1 miss time added (+%u)\n", l1->miss_time);
#endif
        // attempt to fetch addr from l2 cache
        if (fetch(l2, addr) == 0) {    // l2 cache miss
            // update cycles for l2 cache miss
            *cycles += l2->miss_time;
            
            // update the set
            update_set(l2, addr);
            
            // update cycles for mem -> l2 transfer
            *cycles += mm->sendaddr + mm->ready + (mm->chunktime*l2->block_size/mm->chunksize);
            l2->transfers++;
#ifdef DEBUG 
            printf("\tl2 cache miss on %c\n", op_type);
            printf("\tl2 miss time added (+%u)\n", l2->miss_time);
            printf("\tmem -> l2 transfer time added (+%u)\n", 
                    mm->sendaddr + mm->ready + (mm->chunktime*l2->block_size/mm->chunksize));
            printf("\tl2 transfers incremented to %Lu\n", l2->transfers);
        } else {
            printf("\tl2 cache hit on %c\n", op_type);
#endif
        }

        // update cycles for l2 hit
        *cycles += l2->hit_time;
        
        // update the set 
        update_set(l1, addr);
        
        // update cycles for l2 -> l1 transfer
        *cycles += l2->transfer_time * (l1->block_size / l2->bus_width);
        l1->transfers++;        
#ifdef DEBUG 
        printf("\tl2 hit_time added (+%u)\n", l2->hit_time);
        printf("\tl2 -> l1 transfer time added (+%u)\n", 
                l2->transfer_time * (l1->block_size / l2->bus_width));
        printf("\tl1 transfers incremented to %Lu\n", l1->transfers);
    } else {
        printf("\tl1 cache hit on %c\n", op_type);
#endif
    }

    // update cycles for l1 cache hit
    *cycles += l1->hit_time;
#ifdef DEBUG 
    printf("\tl1 hit time added (+%u)\n", l1->hit_time);
#endif
}


