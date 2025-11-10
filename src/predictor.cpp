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

int tagBits = 9; // tag bits per entry
int tag_ctr_bits = 3;
int tag_u_bits = 2;
int tag_valid_bits = 1;

// GHR size (we chose 64 bits)
int ghr_bits = 64

#define TAG_ENTRY_BITS    (tagBits + tag_ctr_bits + tag_u_bits + tag_valid_bits)

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
    return NOTTAKEN;
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
      return;
    default:
      break;
    }
  }
}
