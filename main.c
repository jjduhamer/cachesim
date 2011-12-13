/*
 * cachesim.c: Implements a cache simulator
 *
 * Authors: John Duhamel and Mike Travis
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <libconfig.h>
#include "mycache.h"

void parse_config(char *);
void report();

static struct cache l1d, l1i, l2, mm;

#define lg(x) ((uint_t) (log(x) / log(2)))

/*
 * ec_malloc: performs malloc with error checking and sets memory to 0 (for thoroughness).
 */
void * ec_malloc(ulong_t size) 
{
    void *j;
    if ((j = malloc(size)) == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    memset(j, 0, size);
    return j;
}

int main(int argc, char **argv)
{
    char op;    // holds the op code (L, S, B, C) 
    uint_t op_addr, byte_addr;
    uint_t j, d;
    
    // parse configuration file
    parse_config(".cacherc");
    for (j=1; j<argc; j++)
        parse_config(argv[j]);
    
    // finish initialization from data gathered in config file
    if (l1i.assoc == 0)     // fully associative
        l1i.assoc = l1i.cache_size / l1i.block_size;
    l1i.sets_in_cache = l1i.cache_size / (l1i.assoc * l1i.block_size);
    l1i.bits_in_tag = 32 - lg(l1i.sets_in_cache) - lg(l1i.block_size);
    l1i.next = &l2;

    if (l1d.assoc == 0)     // fully associative
        l1d.assoc = l1d.cache_size / l1d.block_size;
    l1d.sets_in_cache = l1d.cache_size / (l1d.assoc * l1d.block_size);
    l1d.bits_in_tag = 32 - lg(l1d.sets_in_cache) - lg(l1d.block_size);
    l1d.next = &l2;

    if (l2.assoc == 0)     // fully associative
        l2.assoc = l2.cache_size / l2.block_size;
    l2.sets_in_cache = l2.cache_size / (l2.assoc * l2.block_size);
    l2.bits_in_tag = 32 - lg(l2.sets_in_cache) - lg(l2.block_size);
    l2.next = &mm;

    mm.next = NULL;

    // allocate space for blocks in each cache 
    l1i.set = (struct cache_block **) ec_malloc(l1i.sets_in_cache * sizeof(struct cache_block *));
    for (j=0; j<l1i.sets_in_cache; j++)
        l1i.set[j] = (struct cache_block *) ec_malloc(l1i.assoc 
                                                        * sizeof(struct cache_block));
    
    l1d.set = (struct cache_block **) ec_malloc(l1d.sets_in_cache * sizeof(struct cache_block *));
    for (j=0; j<l1d.sets_in_cache; j++)
        l1d.set[j] = (struct cache_block *) ec_malloc(l1d.assoc 
                                                        * sizeof(struct cache_block));
    
    l2.set = (struct cache_block **) ec_malloc(l2.sets_in_cache * sizeof(struct cache_block *));
    for (j=0; j<l2.sets_in_cache; j++)
        l2.set[j] = (struct cache_block *) ec_malloc(l2.assoc 
                                                        * sizeof(struct cache_block));
    
    // run cache simulation 
    j = 0;
    while (scanf("%c %x %x\n", &op, &op_addr, &byte_addr) == 3) {
        
#ifdef DEBUG
        printf("inst %u, type = %c\n", j++, op);
#endif

       
        switch (op) {
            case 'L':   // load word
                num_load++;
                cache_fetch(&l1i, op_addr, &load_cycles);
                //cache_fetch(&l1i, &l2, &mm, op_addr, &load_cycles);
                cache_fetch(&l1d, byte_addr, &load_cycles);
                //cache_fetch(&l1d, &l2, &mm, byte_addr, &load_cycles);
                break;
            case 'S':   // store word
                num_store++;
                cache_fetch(&l1i, op_addr, &store_cycles);
                //cache_fetch(&l1i, &l2, &mm, op_addr, &store_cycles);
                cache_store(&l1d, byte_addr, &store_cycles);
                //cache_store(&l1d, &l2, &mm, byte_addr, &store_cycles);
                break;
            case 'B':   // branch
                num_branch++;
                cache_fetch(&l1i, op_addr, &branch_cycles);
                //cache_fetch(&l1i, &l2, &mm, op_addr, &branch_cycles);
                branch_cycles += 1;
#ifdef DEBUG
                printf("\tbranch time added (+1)\n");
#endif
                break;
            case 'C':   // compute
                num_comp++;
                cache_fetch(&l1i, op_addr, &comp_cycles);
                //cache_fetch(&l1i, &l2, &mm, op_addr, &comp_cycles);
                comp_cycles += byte_addr;
#ifdef DEBUG
                printf("\tcomputation time added (+%d)\n", byte_addr);
#endif
                break;
        }
#ifdef DEBUG
        printf("execution time: %Lu\n\n", load_cycles+store_cycles+branch_cycles+comp_cycles);
#endif
    }
    
    report();
  
#ifdef DEBUG
    printf("l1i:\n");
    cache_print_sets(&l1i);
    printf("l1d:\n");
    cache_print_sets(&l1d);
    printf("l2:\n");
    cache_print_sets(&l2);
#endif

    for (j=0; j<l1i.sets_in_cache; j++)
        free(l1i.set[j]);
    free(l1i.set);
    
    for (j=0; j<l1d.sets_in_cache; j++)
        free(l1d.set[j]);
    free(l1d.set);
    
    for (j=0; j<l2.sets_in_cache; j++)
        free(l2.set[j]);
    free(l2.set);
    
    exit(EXIT_SUCCESS);
}

/*
 * report: Generates a report at the end of a simulation.
 */
void report()
{
    ulong_t l1i_total_req = l1i.hit_count + l1i.miss_count;
    float l1i_hit_rate = (double) l1i.hit_count / l1i_total_req * 100;
    float l1i_miss_rate = (double) l1i.miss_count / l1i_total_req * 100;
    
    ulong_t l1d_total_req = l1d.hit_count + l1d.miss_count;
    float l1d_hit_rate = (double) l1d.hit_count / l1d_total_req * 100;
    float l1d_miss_rate = (double) l1d.miss_count / l1d_total_req * 100;
    
    ulong_t l2_total_req = l2.hit_count + l2.miss_count;
    float l2_hit_rate = (double) l2.hit_count / l2_total_req * 100;
    float l2_miss_rate = (double) l2.miss_count / l2_total_req * 100;
    
    ulong_t inst_refs = l1i.hit_count + l1i.miss_count;
    ulong_t data_refs = l1d.hit_count + l1d.miss_count;
    ulong_t total_refs = inst_refs + data_refs;
    
    ulong_t num_inst = num_load + num_store + num_branch + num_comp;
    float perc_load = (float) num_load / num_inst * 100;
    float perc_store = (float) num_store / num_inst * 100;
    float perc_branch = (float) num_branch / num_inst * 100;
    float perc_comp = (float) num_comp / num_inst * 100;
    
    ulong_t total_cycles = load_cycles + store_cycles + branch_cycles + comp_cycles;
    float perc_load_cycles = (float) load_cycles / total_cycles * 100;
    float perc_store_cycles = (float) store_cycles / total_cycles * 100;
    float perc_branch_cycles = (float) branch_cycles / total_cycles * 100;
    float perc_comp_cycles = (float) comp_cycles / total_cycles * 100;
    
    float load_cpi = (float) load_cycles / num_load;
    float store_cpi = (float) store_cycles / num_store;
    float branch_cpi = (float) branch_cycles / num_branch;
    float comp_cpi = (float) comp_cycles / num_comp;
    float overall_cpi = (float) total_cycles / num_inst;

    ulong_t perf_cycles = 2 * num_inst;

    uint_t l1i_cost = (100 * l1i.cache_size / 4096) * (lg(l1i.assoc) + 1);
    uint_t l1d_cost = (100 * l1d.cache_size / 4096) * (lg(l1d.assoc) + 1);
    uint_t l2_cost = (50 * l2.cache_size / 65536) + (50 * lg(l2.assoc));
    uint_t mm_cost = 50 + (200 * ((100 / mm.ready) - 1)) + 25 + (100 * ((mm.chunksize / 16) - 1));

    // report statistics passed in from config file
    printf("\
Memory System:\n\
\tDcache size = %u : ways = %u : block size = %u\n\
\tIcache size = %u : ways = %u : block size = %u\n\
\tL2-cache size = %u : ways = %u : block size = %u\n\
\tMemory ready time = %u : chunksize = %u : chunktime = %u\n\n",
        l1d.cache_size, l1d.assoc, l1d.block_size,
        l1i.cache_size, l1i.assoc, l1i.block_size,
        l2.cache_size, l2.assoc, l2.block_size,
        mm.ready, mm.chunksize, mm.chunktime);
    // report statistics for execution time
    printf("\
Execute time = %Lu : Total refs = %Lu\n\
Inst refs = %Lu : Data refs = %Lu\n\n",
        total_cycles, total_refs,
        inst_refs, data_refs);
    // report number of instructions
    printf("\
Number of Instructions: [Percentage]\n\
\tLoads  (L) = %Lu [%.1f%%] : Stores (S) = %Lu [%.1f%%]\n\
\tBranch (B) = %Lu [%.1f%%] : Comp. (C) = %Lu [%.1f%%]\n\
\tTotal  (T) = %Lu\n\n",
        num_load, perc_load, num_store, perc_store,
        num_branch, perc_branch, num_comp, perc_comp,
        num_inst);
    printf("\
Cycles for Instructions: [Percentage]\n\
\tLoads  (L) = %Lu [%.1f%%] : Stores (S) = %Lu [%.1f%%]\n\
\tBranch (B) = %Lu [%.1f%%] : Comp. (C) = %Lu [%.1f%%]\n\
\tTotal  (T) = %Lu\n\n",
        load_cycles, perc_load_cycles, store_cycles, perc_store_cycles,
        branch_cycles, perc_branch_cycles, comp_cycles, perc_comp_cycles,
        total_cycles);
    printf("\
Cycles per Instruction (CPI):\n\
\tLoads  (L) = %.1f : Stores (S) = %.1f\n\
\tBranch (B) = %.1f : Comp. (C) = %.1f\n\
\tOverall (CPI) = %.1f\n\n",
        load_cpi, store_cpi, branch_cpi, comp_cpi, overall_cpi);
    printf("\
Cycles for processor w/ perfect memory system = %Lu\n\
Cycles for processor w/ simulated memory system = %Lu\n\
Ratio of simulated to perfect performance = %.1f\n\n",
        perf_cycles, total_cycles, (float) (total_cycles / perf_cycles));
    // report for l1 instruction cache
    printf("\
Memory Level: L1i\n\
\tHit Count = %Lu\tMiss Count = %Lu\tTotal Requests = %Lu\n\
\tHit Rate = %.1f%%\tMiss Rate = %.1f%%\n \
\tKickouts : %Lu Dirty Kickouts : %Lu Transfers : %Lu\n\n",
        l1i.hit_count, l1i.miss_count, l1i_total_req,
        l1i_hit_rate, l1i_miss_rate,
        l1i.kickouts, l1i.dirty_kickouts, l1i.transfers);
    // report for l1 data cache
    printf("\
Memory Level: L1d\n\
\tHit Count = %Lu\tMiss Count = %Lu\tTotal Requests = %Lu\n\
\tHit Rate = %.1f%%\tMiss Rate = %.1f%%\n \
\tKickouts : %Lu Dirty Kickouts : %Lu Transfers : %Lu\n\n",
        l1d.hit_count, l1d.miss_count, l1d_total_req,
        l1d_hit_rate, l1d_miss_rate,
        l1d.kickouts, l1d.dirty_kickouts, l1d.transfers);
    // report for l2 cache
    printf("\
Memory Level: L2\n\
\tHit Count = %Lu\tMiss Count = %Lu\tTotal Requests = %Lu\n\
\tHit Rate = %.1f%%\tMiss Rate = %.1f%%\n \
\tKickouts : %Lu Dirty Kickouts : %Lu Transfers : %Lu\n\n",
        l2.hit_count, l2.miss_count, l2_total_req,
        l2_hit_rate, l2_miss_rate,
        l2.kickouts, l2.dirty_kickouts, l2.transfers);
    // report cost statistics
    printf("\
L1 cache cost (Icache $%u) + (Dcache $%u) = $%u\n\
L2 cache cost = $%u\n\
Memory Cost = $%u\n\
Total Cost = $%u\n\n",
    l1i_cost, l1d_cost, l1i_cost+l1d_cost,
    l2_cost, mm_cost, l1i_cost+l1d_cost+l2_cost+mm_cost);
}

/*
 * parse_config: Parses a cofniguration file and updates the specified parameters.
 */
void parse_config(char *cfile)
{
    config_t cf, *cfg;
    config_setting_t *setting;
    const char *str;
    
    cfg = &cf;  // this is for pure convienece ;-D
    config_init(cfg);
    
    /* check for errors in the configuration file */
    if (!config_read_file(cfg, cfile)) {
        fprintf(stderr, "ERROR: %s:%d - %s\n", config_error_file(cfg),
                config_error_line(cfg), config_error_text(cfg));
        config_destroy(cfg);
        return;
    }
    
    /* set the various parameters */
    if ((setting = config_lookup(cfg, "L1_cache")) != NULL) {
        int block_size, cache_size, assoc, hit_time, miss_time;
        if (config_setting_lookup_int(setting, "block_size", &block_size)) {
            l1i.block_size = block_size;
            l1d.block_size = block_size;
        }
        if (config_setting_lookup_int(setting, "cache_size", &cache_size)) {
            l1i.cache_size = cache_size;
            l1d.cache_size = cache_size;
        }
        if (config_setting_lookup_int(setting, "assoc", &assoc)) {
            l1i.assoc = assoc;
            l1d.assoc = assoc;
        }
        if (config_setting_lookup_int(setting, "hit_time", &hit_time)) {
            l1i.hit_time = hit_time;
            l1d.hit_time = hit_time;
        }
        if (config_setting_lookup_int(setting, "miss_time", &miss_time)) {
            l1i.miss_time = miss_time;
            l1d.miss_time = miss_time;
        }
    }
    
    if ((setting = config_lookup(cfg, "L2_cache")) != NULL) {
        int block_size, cache_size, assoc, hit_time, miss_time, transfer_time, bus_width;
        if (config_setting_lookup_int(setting, "block_size", &block_size))
            l2.block_size = block_size;
        if (config_setting_lookup_int(setting, "cache_size", &cache_size))
            l2.cache_size = cache_size;
        if (config_setting_lookup_int(setting, "assoc", &assoc))
            l2.assoc = assoc;
        if (config_setting_lookup_int(setting, "hit_time", &hit_time))
            l2.hit_time = hit_time;
        if (config_setting_lookup_int(setting, "miss_time", &miss_time))
            l2.miss_time = miss_time;
        if (config_setting_lookup_int(setting, "transfer_time", &transfer_time))
            l2.transfer_time = transfer_time;
        if (config_setting_lookup_int(setting, "bus_width", &bus_width))
            l2.bus_width = bus_width;
    }
    
    if ((setting = config_lookup(cfg, "Main_Mem")) != NULL) {
        int sendaddr, ready, chunktime, chunksize;
        if (config_setting_lookup_int(setting, "sendaddr", &sendaddr))
            mm.sendaddr = sendaddr;
        if (config_setting_lookup_int(setting, "ready", &ready))
            mm.ready = ready;
        if (config_setting_lookup_int(setting, "chunktime", &chunktime))
            mm.chunktime = chunktime;
        if (config_setting_lookup_int(setting, "chunksize", &chunksize))
            mm.chunksize = chunksize;
    }
    
    config_destroy(cfg);
}


