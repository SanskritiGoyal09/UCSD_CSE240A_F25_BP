//========================================================//
//  predictor.h                                           //
//  Header file for the Branch Predictor                  //
//                                                        //
//  Includes function prototypes and global predictor     //
//  variables and defines                                 //
//========================================================//

#ifndef PREDICTOR_H
#define PREDICTOR_H

#include <stdint.h>
#include <stdlib.h>

//
// Student Information
//
extern const char *studentName;
extern const char *studentID;
extern const char *email;

//------------------------------------//
//      Global Predictor Defines      //
//------------------------------------//
#define NOTTAKEN 0
#define TAKEN 1

// The Different Predictor Types
#define STATIC 0
#define GSHARE 1
#define TOURNAMENT 2
#define CUSTOM 3
extern const char *bpName[];

// Definitions for 2-bit counters
#define SN 0 // predict NT, strong not taken
#define WN 1 // predict NT, weak not taken
#define WT 2 // predict T, weak taken
#define ST 3 // predict T, strong taken

// Definitions for 3-bit counters
#define SNTKN 0  // Strongly Not Taken
#define MNTKN 1  // Moderately Not Taken
#define WNTKN 2  // Weakly Not Taken
#define SNK 3 // Slightly Not Taken
#define STK 4  // Slightly Taken
#define WTKN 5   // Weakly Taken
#define MTKN 6   // Moderately Taken
#define STKN 7   // Strongly Taken

// 2-bit chooser states
#define STRONG_LOCAL   0  // Strongly prefer local predictor
#define WEAK_LOCAL     1  // Weakly prefer local predictor
#define WEAK_GLOBAL    2  // Weakly prefer global predictor
#define STRONG_GLOBAL  3  // Strongly prefer global predictor

//usefulness (2-bit) values
#define U_MIN 0
#define U_MAX 3

//valid bit values
#define VALID 1
#define INVALID 0

//sizes for TAGE
#define BASE_ENTRIES      (1 << 11)   // 2048
#define NUM_TAG_TABLES    4
#define TAG_ENTRIES       (1 << 10)   // 1024 per tagged table

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//
extern int ghistoryBits; // Number of bits used for Global History
extern int lhistoryBits; // Number of bits used for Local History
extern int pcIndexBits;  // Number of bits used for PC index
extern int bpType;       // Branch Prediction Type
extern int verbose;
extern int ghistoryBits_tournament;
extern int lhistoryBits;
extern int pcIndexBits;

//------------------------------------//
//    Predictor Function Prototypes   //
//------------------------------------//

// Initialize the predictor
//
void init_predictor();

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint32_t make_prediction(uint32_t pc, uint32_t target, uint32_t direct);

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//
void train_predictor(uint32_t pc, uint32_t target, uint32_t outcome, uint32_t condition, uint32_t call, uint32_t ret, uint32_t direct);

// Please add your code below, and DO NOT MODIFY ANY OF THE CODE ABOVE
// 



#endif
