/* Keep these headers */
#include <tice.h>

/* Standard headers - it's recommended to leave them included */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <graphx.h>
#include <keypadc.h>
#include <fileioc.h>
#include <debug.h>


#define HEADER_SIZE 12
#define HIGH_SCORE_TABLE_LENGTH 3
#define HIGH_SCORE_ENTRY_SIZE 3 //(sizeof(uint24_t))

uint8_t newHighScoreTable();
void getHighScores(uint8_t appvar, void *table);
void setHighScores(uint8_t appvar, void *table);
//void addHighScore(uint8_t appvar, unsigned int score);