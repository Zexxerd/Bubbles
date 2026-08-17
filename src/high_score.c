
#include "high_score.h"

uint8_t newHighScoreTable(void) {
    /**Creates a new high score table AppVar, or returns a pointer to the existing one.
     *
     * @return an AppVar handle
    */
    uint8_t high_score_appvar; //AppVar
    uint8_t return_code;
    high_score_appvar = ti_Open("BUBBLESH", "r+");
    if (!high_score_appvar) { //does not exist
        high_score_appvar = ti_Open("BUBBLESH", "w+");
        return_code = ti_Write("BUBBLES!DATA\0\0\0\0\0\0\0\0\0", 21, 1, high_score_appvar);
        if (return_code != 1) {
            #ifdef DEBUG
            dbg_printf("new high score table creation failed :(");
            #endif
            exit(1);
        }
    }
    return high_score_appvar;
}

void getHighScores(uint8_t appvar, void *table) {
    /**Gets high scores from AppVar.*/
    ti_Seek(HEADER_SIZE, SEEK_SET, appvar);
    if (ti_Read((void *) table, HIGH_SCORE_ENTRY_SIZE, HIGH_SCORE_TABLE_LENGTH, appvar) != HIGH_SCORE_TABLE_LENGTH) {
        exit(1);
    }
}
void setHighScores(uint8_t appvar, void *table) {
    /**Gets high scores from AppVar.*/
    ti_Seek(HEADER_SIZE, SEEK_SET, appvar);
    if (ti_Write((void *) table, HIGH_SCORE_ENTRY_SIZE, HIGH_SCORE_TABLE_LENGTH, appvar) != HIGH_SCORE_TABLE_LENGTH) {
        exit(1);
    }
}