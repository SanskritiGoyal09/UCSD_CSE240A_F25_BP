//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <math.h>
#include "predictor.h"

//
// TODO:Student Information
//

const char *studentName = "TODO";
const char *studentID = "TODO";
const char *email = "TODO";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = {"Static", "Gshare",
                         "Tournament", "Custom"};

// define number of bits required for indexing the BHT here.
int ghistoryBits = 15; // Number of bits used for Global History
int bpType;            // Branch Prediction Type
int verbose;

int ghistoryBits_tournament = 15;
int lhistoryBits = 12;
int pcIndexBits = 12;

int base_entries = 2048;
const int num_tag_tables = 4;
uint64_t UGR_PERIOD = 262144ULL;// 256K branches
int HIST_LENGTHS[num_tag_tables + 1] = {0, 14, 15, 44, 128};

int ghr_bits = 128;

//#define TAG_ENTRY_BITS    (tagBits + tag_ctr_bits + tag_u_bits + tag_valid_bits)

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
// TODO: Add your own Branch Predictor data structures here
//
// gshare
uint8_t *bht_gshare;
uint64_t ghistory;
//
// tournament
uint16_t ghr;
uint16_t *localHistoryTable;
uint8_t *bht_local;
uint8_t *bht_global;
uint8_t *chooserTable;
//
// custom
uint64_t ghr_custom_1;
uint64_t ghr_custom_2;
uint8_t last_pred;
int last_provider;
uint64_t branch_count;

struct BaseEntry {
    uint8_t ctr;   //2-bit ctr
};

struct TaggedEntry {
    uint16_t tag;  
    uint8_t ctr : 3;   //3-bit ctr
    uint8_t u : 2;     
    uint8_t valid : 1;
};

struct tage_table {
    int tableSize;
    int historyBits;
    int numTagBits;
};

// Geometric history lengths and tag bits
tage_table tageTables[num_tag_tables] = {
    {.tableSize = 1024, .historyBits = HIST_LENGTHS[1], .numTagBits = 9},   // T1 short-history
    {.tableSize = 1024, .historyBits = HIST_LENGTHS[2], .numTagBits = 9},   // T2 medium-short
    {.tableSize = 1024,  .historyBits = HIST_LENGTHS[3], .numTagBits = 10},  // T3 medium-long
    {.tableSize = 1024,  .historyBits = HIST_LENGTHS[4], .numTagBits = 10}   // T4 long-history
};

BaseEntry* base_bht_table;
TaggedEntry** tag_tables;

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//

// gshare functions
void init_gshare()
{
  int bht_entries = 1 << ghistoryBits;
  bht_gshare = (uint8_t *)malloc(bht_entries * sizeof(uint8_t));
  int i = 0;
  for (i = 0; i < bht_entries; i++)
  {
    bht_gshare[i] = WN;
  }
  ghistory = 0;
}

uint8_t gshare_predict(uint32_t pc)
{
  // get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries - 1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
  switch (bht_gshare[index])
  {
  case WN:
    return NOTTAKEN;
  case SN:
    return NOTTAKEN;
  case WT:
    return TAKEN;
  case ST:
    return TAKEN;
  default:
    printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    return NOTTAKEN;
  }
}

void train_gshare(uint32_t pc, uint8_t outcome)
{
  // get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries - 1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;

  // Update state of entry in bht based on outcome
  switch (bht_gshare[index])
  {
  case WN:
    bht_gshare[index] = (outcome == TAKEN) ? WT : SN;
    break;
  case SN:
    bht_gshare[index] = (outcome == TAKEN) ? WN : SN;
    break;
  case WT:
    bht_gshare[index] = (outcome == TAKEN) ? ST : WN;
    break;
  case ST:
    bht_gshare[index] = (outcome == TAKEN) ? ST : WT;
    break;
  default:
    printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    break;
  }

  // Update history register
  ghistory = ((ghistory << 1) | outcome);
}

void cleanup_gshare()
{
  free(bht_gshare);
}

// tournament functions
void init_tournament()
{
  int lht_entries = 1 << pcIndexBits;
  localHistoryTable = (uint16_t *)malloc(lht_entries * sizeof(uint16_t));

  int bht_local_entries = 1 << lhistoryBits;
  bht_local = (uint8_t *)malloc(bht_local_entries * sizeof(uint8_t));

  int bht_global_entries = 1 << ghistoryBits_tournament;
  bht_global = (uint8_t *)malloc(bht_global_entries * sizeof(uint8_t));

  int chooser_entries = 1 << ghistoryBits_tournament;
  chooserTable = (uint8_t *)malloc(chooser_entries * sizeof(uint8_t));

  for (int i = 0; i < lht_entries; i++)
  {
    localHistoryTable[i] = 0;
  }

  for (int i = 0; i < bht_local_entries; i++)
  {
    bht_local[i] = SNK; //slightly not taken
  }

  for (int i = 0; i < bht_global_entries; i++)
  {
    bht_global[i] = WN;
  }

  for (int i = 0; i < chooser_entries; i++)
  {
    chooserTable[i] = WEAK_LOCAL;
  }
  
  ghr = 0;
}

uint8_t get_local_prediction(uint32_t bht_local_index)
{
  return (bht_local[bht_local_index] >= 4) ? TAKEN : NOTTAKEN;
}

uint8_t get_global_prediction(uint32_t bht_global_index)
{
  return (bht_global[bht_global_index] >= 2) ? TAKEN : NOTTAKEN;
}

uint8_t tournament_predict(uint32_t pc)
{
  // get lower historyBits of pc, lht and ghr
  int lht_entries = 1 << pcIndexBits;
  uint32_t lht_index = pc & (lht_entries - 1);
    
  // Get local history for this PC
  uint32_t local_history = localHistoryTable[lht_index] & ((1u << lhistoryBits) - 1);
  
  // Index into bht_local (3-bit counter)
  uint32_t bht_local_index = local_history;
  
  // Index into bht_global(2-bit) and Chooser tables(2-bit) using GHR
  uint32_t bht_global_index = ghr & ((1u << ghistoryBits_tournament) - 1);

  switch (chooserTable[bht_global_index])
  {
      case STRONG_LOCAL:   // 0
      case WEAK_LOCAL:     // 1
          return get_local_prediction(bht_local_index);

      case WEAK_GLOBAL:    // 2
      case STRONG_GLOBAL:  // 3
          return get_global_prediction(bht_global_index);

      default:
          printf("Warning: Undefined state in chooser table!\n");
          return NOTTAKEN;
  }
}

void train_tournament(uint32_t pc, uint8_t outcome)
{
  // get lower historyBits of pc, lht and ghr
  int lht_entries = 1 << pcIndexBits;
  uint32_t lht_index = pc & (lht_entries - 1);
    
  // Get local history for this PC
  uint32_t local_history = localHistoryTable[lht_index] & ((1u << lhistoryBits) - 1);
  
  // Index into bht_local (3-bit counter)
  uint32_t bht_local_index = local_history;
  
  // Index into bht_global(2-bit) and Chooser tables(2-bit) using GHR
  uint32_t bht_global_index = ghr & ((1u << ghistoryBits_tournament) - 1);

  uint8_t local_pred = get_local_prediction(bht_local_index);
  uint8_t global_pred = get_global_prediction(bht_global_index);

  if (local_pred != global_pred)
  {
    if (local_pred == outcome && chooserTable[bht_global_index] > STRONG_LOCAL)
        chooserTable[bht_global_index]--; // favor local
    else if (global_pred == outcome && chooserTable[bht_global_index] < STRONG_GLOBAL)
        chooserTable[bht_global_index]++; // favor global
  }

  if (outcome == TAKEN)
  {
      if (bht_local[bht_local_index] < STKN) 
        bht_local[bht_local_index]++;
  }
  else
  {
      if (bht_local[bht_local_index] > SNTKN) 
        bht_local[bht_local_index]--;
  }

  if (outcome == TAKEN)
  {
      if (bht_global[bht_global_index] < ST) 
        bht_global[bht_global_index]++;
  }
  else
  {
      if (bht_global[bht_global_index] > SN) 
        bht_global[bht_global_index]--;
  }

  localHistoryTable[lht_index] = ((localHistoryTable[lht_index] << 1) | (outcome & 1)) & ((1u << lhistoryBits) - 1);
  ghr = ((ghr << 1) | (outcome & 1)) & ((1u << ghistoryBits_tournament) - 1);

}

void cleanup_tournament()
{
  free(localHistoryTable);
  free(bht_local);
  free(bht_global);
  free(chooserTable);
}

//custom functions
void init_tage() {
  base_bht_table = (BaseEntry*)malloc(base_entries * sizeof(BaseEntry));
  tag_tables = (TaggedEntry**)malloc(num_tag_tables * sizeof(TaggedEntry*));

  for (int t = 0; t < num_tag_tables; t++)
      tag_tables[t] = (TaggedEntry*)malloc(tageTables[t].tableSize * sizeof(TaggedEntry));

  for (int i = 0; i < base_entries; i++)
      base_bht_table[i].ctr = 1; // weakly not-taken

  for (int t = 0; t < num_tag_tables; t++)
  {  
    for (int i = 0; i < tageTables[t].tableSize; i++) 
    {
      tag_tables[t][i].valid = 0;
      tag_tables[t][i].ctr = 4; // weakly not-taken
      tag_tables[t][i].u = 0;
      tag_tables[t][i].tag = 0;
    }
  }

  ghr_custom_1 = 0;
  ghr_custom_2 = 0;
  branch_count = 0;
  last_pred = 0;
  last_provider = -1;
}

static inline uint32_t fold_history_xor(uint64_t ghr, int hist_len, int out_bits) {
    // Fold 'hist_len' low bits of ghr into an out_bits-sized value via XOR folding.
    // out_bits is assumed to be power-of-two width mask size (i.e., tableSize mask bits).
    uint32_t mask = (1u << out_bits) - 1;
    uint32_t folded = 0;
    int i = 0;
    int pos = 0;
    while (i < hist_len) {
        // take up to out_bits bits chunk
        uint32_t chunk = (uint32_t)((ghr >> i) & mask);
        folded ^= chunk << pos;
        // reduce folded to mask
        folded = (folded & mask) ^ (folded >> out_bits);
        i += out_bits;
    }
    return folded & mask;
}

// Compute index into a table (table->tableSize is power of two)
uint32_t compute_index(uint32_t pc, const tage_table *table) {
    uint32_t mask = table->tableSize - 1;
    uint32_t folded_history = 0;

    int remaining_bits = table->historyBits;
    int i = 0;

    // Fold newer 64 bits
    while (remaining_bits > 0 && i < 64) {
        int bits = (remaining_bits < 32) ? remaining_bits : 32;
        folded_history ^= (uint32_t)((ghr_custom_2 >> i) & ((1ULL << bits) - 1));
        i += bits;
        remaining_bits -= bits;
    }

    // Fold older 64 bits if needed
    i = 0;
    while (remaining_bits > 0 && i < 64) {
        int bits = (remaining_bits < 32) ? remaining_bits : 32;
        folded_history ^= (uint32_t)((ghr_custom_1 >> i) & ((1ULL << bits) - 1));
        i += bits;
        remaining_bits -= bits;
    }

    return (pc ^ folded_history) & mask;
  }

uint16_t compute_tag(uint32_t pc, const tage_table *table) {
    uint32_t mask = (1 << table->numTagBits) - 1;
    uint32_t folded_history = 0;

    int remaining_bits = table->historyBits;
    int i = 0;

    while (remaining_bits > 0 && i < 64) {
        int bits = (remaining_bits < table->numTagBits) ? remaining_bits : table->numTagBits;
        folded_history ^= (uint32_t)((ghr_custom_2 >> i) & ((1ULL << bits) - 1));
        i += bits;
        remaining_bits -= bits;
    }

    i = 0;
    while (remaining_bits > 0 && i < 64) {
        int bits = (remaining_bits < table->numTagBits) ? remaining_bits : table->numTagBits;
        folded_history ^= (uint32_t)((ghr_custom_1 >> i) & ((1ULL << bits) - 1));
        i += bits;
        remaining_bits -= bits;
    }

    return (pc ^ folded_history) & mask;
}


uint8_t tage_predict(uint32_t pc) {
    last_provider = -1;
    int alt_provider = -1;

    // base prediction from base table
    uint8_t base_taken = (base_bht_table[pc % base_entries].ctr >= 2) ? 1 : 0;
    uint8_t pred = base_taken;
    uint8_t altpred = base_taken;

    // scan tag tables from longest history (highest index) to shortest (0)
    for (int t = num_tag_tables - 1; t >= 0; t--) {
        const struct tage_table *tb = &tageTables[t];
        uint32_t idx = compute_index(pc, tb);
        uint16_t tag = compute_tag(pc, tb);
        TaggedEntry *e = &tag_tables[t][idx];
        if (e->valid && e->tag == tag) {
            if (last_provider == -1) {
                last_provider = t;
            } else if (alt_provider == -1) {
                alt_provider = t;
                // don't break - we may want additional bookkeeping, but two is fine
            }
        }
    }

    // alt_pred: from alt_provider if present, else base
    if (alt_provider != -1) {
        const struct tage_table *alt_tb = &tageTables[alt_provider];
        uint32_t idx_alt = compute_index(pc, alt_tb);
        TaggedEntry *e_alt = &tag_tables[alt_provider][idx_alt];
        altpred = (e_alt->ctr >= 4) ? 1 : 0;
    } else {
        altpred = base_taken;
    }

    // provider prediction: if provider exists use its ctr (with usefulness check)
    if (last_provider != -1) {
        const struct tage_table *prov_tb = &tageTables[last_provider];
        uint32_t idx = compute_index(pc, prov_tb);
        TaggedEntry *prov = &tag_tables[last_provider][idx];
        uint8_t prov_pred = (prov->ctr >= 4) ? 1 : 0;

        // if not useful and weak, use altpred
        uint8_t weak = (prov->ctr == 3 || prov->ctr == 4);
        if ((prov->u == 0) && weak) {
            pred = altpred;
        } else {
            pred = prov_pred;
        }
    } else {
        pred = base_taken;
    }

    last_pred = pred;
    return pred;
}

void allocate_on_mispredict(uint32_t pc, int provider, uint8_t outcome) {
    // Scan from provider-1 downwards to find an entry to allocate (prefer shorter histories)
    for (int t = provider - 1; t >= 0; t--) {
        const struct tage_table *tb = &tageTables[t];
        uint32_t idx = compute_index(pc, tb);
        TaggedEntry *e = &tag_tables[t][idx];
        if (!e->valid) {
            // allocate new entry
            e->valid = 1;
            e->tag = compute_tag(pc, tb);
            // initialize counter toward the outcome but weakly
            e->ctr = (outcome == TAKEN) ? 5 : 2; // e.g. weakly taken vs weakly not
            e->u = 0;
            return;
        } else if (e->u == 0) {
            // steal an entry with u==0
            e->valid = 1;
            e->tag = compute_tag(pc, tb);
            e->ctr = (outcome == TAKEN) ? 5 : 2;
            e->u = 0;
            return;
        }
    }
    // no allocation possible
}

void maybe_graceful_u_reset() {
    // Every UGR_PERIOD branches, halve all u counters (right-shift) to decay usefulness.
    if ((branch_count != 0) && ((branch_count % UGR_PERIOD) == 0)) {
        for (int t = 0; t < num_tag_tables; t++) {
            for (int i = 0; i < tageTables[t].tableSize; i++) {
                tag_tables[t][i].u >>= 1;
            }
        }
    }
}

void train_tage(uint32_t pc, uint8_t outcome) {
    branch_count++;
    bool base_is_provider = (last_provider == -1);

    // Base predictor update
    if (base_is_provider) {
        uint8_t &base_ctr = base_bht_table[pc % base_entries].ctr;
        if (outcome == TAKEN) {
            if(base_ctr < 3) base_ctr++;
        } else {
            if(base_ctr > 0) base_ctr--;
        }
    }

    // Tagged table update
    if (!base_is_provider) {
        uint8_t altpred;
        uint32_t idx = compute_index(pc, &tageTables[last_provider]);
        TaggedEntry &prov = tag_tables[last_provider][idx];

        // Compute alt prediction from lower tables or base
        altpred = (base_bht_table[pc % base_entries].ctr >= 2) ? 1 : 0;
        for (int t = last_provider - 1; t >= 0; t--) {
            uint32_t idx_alt = compute_index(pc, &tageTables[t]);
            TaggedEntry &e = tag_tables[t][idx_alt];
            if (e.valid && e.tag == compute_tag(pc, &tageTables[t])) {
                altpred = (e.ctr >= 4) ? 1 : 0;
                break;
            }
        }

        // Update useful counter
        if (altpred != last_pred) {
            if (last_pred == outcome && prov.u < U_MAX) prov.u++;
            else if (last_pred != outcome && prov.u > U_MIN) prov.u--;
        }

        // Update prediction counter
        if (outcome == TAKEN && prov.ctr < 7) prov.ctr++;
        else if (outcome == NOTTAKEN && prov.ctr > 0) prov.ctr--;
    }

    // Allocate on misprediction
    if (!base_is_provider && outcome != last_pred && last_provider < num_tag_tables - 1)
        allocate_on_mispredict(pc, last_provider, outcome);

    // Update 128-bit GHR
    uint64_t new_bit = (uint64_t)outcome & 1;
    ghr_custom_1 = (ghr_custom_1 << 1) | (ghr_custom_2 >> 63); // older 64 bits shift in top bit of newer
    ghr_custom_2 = (ghr_custom_2 << 1) | new_bit;               // newer 64 bits shift in new outcome
}


void init_predictor()
{
  switch (bpType)
  {
  case STATIC:
    break;
  case GSHARE:
    init_gshare();
    break;
  case TOURNAMENT:
    init_tournament();
    break;
  case CUSTOM:
    init_tage();
    break;
  default:
    break;
  }
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint32_t make_prediction(uint32_t pc, uint32_t target, uint32_t direct)
{

  // Make a prediction based on the bpType
  switch (bpType)
  {
  case STATIC:
    return TAKEN;
  case GSHARE:
    return gshare_predict(pc);
  case TOURNAMENT:
    return tournament_predict(pc);
  case CUSTOM:
    return tage_predict(pc);
  default:
    break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//

void train_predictor(uint32_t pc, uint32_t target, uint32_t outcome, uint32_t condition, uint32_t call, uint32_t ret, uint32_t direct)
{
  if (condition)
  {
    switch (bpType)
    {
    case STATIC:
      return;
    case GSHARE:
      return train_gshare(pc, outcome);
    case TOURNAMENT:
      return train_tournament(pc, outcome);;
    case CUSTOM:
      return train_tage(pc, outcome);
    default:
      break;
    }
  }
}
