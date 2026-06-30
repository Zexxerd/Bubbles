
#include "high_score.h"

uint8_t newHighScoreTable() {
    /**Creates a new high score table AppVar*/
    uint8_t high_score_appvar; //AppVar
    uint8_t return_code;
    high_score_appvar = ti_Open("BUBBLESH", "r+");
    if (!high_score_appvar) { //does not exist
        high_score_appvar = ti_Open("BUBBLESH", "w+");
        return_code = ti_Write("BUBBLES!DATA\0\0\0\0\0\0\0\0\0", 21, 1, high_score_appvar);
        if (return_code != 1) {
            dbg_printf("new high score table creation failed :(");
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
/*void addHighScore(uint8_t appvar, unsigned int score) {
    uint8_t return_code, index;
    unsigned int current_entry;
    unsigned int shift_temp_size;
    void *shift_temp;
    
    index = HIGH_SCORE_TABLE_LENGTH;
    ti_Seek(-HIGH_SCORE_ENTRY_SIZE, SEEK_END, appvar); //point to last var
    for (uint8_t i = HIGH_SCORE_TABLE_LENGTH - 1; i-- > 0;) {
        return_code = ti_Read(&current_entry, HIGH_SCORE_ENTRY_SIZE, 1, appvar);
        if (return_code != HIGH_SCORE_ENTRY_SIZE) {
            dbg_printf("couldn't read from high score table :(");
            exit(1);
        }
        if (score < current_entry) {
            break;
        }
        index--;
        ti_Seek(-2*HIGH_SCORE_ENTRY_SIZE, SEEK_CUR, appvar);
    }
    if (index == HIGH_SCORE_TABLE_LENGTH) {
        return;
    }
    if (index < HIGH_SCORE_TABLE_LENGTH - 1) {
        shift_temp_size = HIGH_SCORE_ENTRY_SIZE * (HIGH_SCORE_TABLE_LENGTH - index - 1);
        shift_temp = malloc(shift_temp_size);
        if (!shift_temp) {
            dbg_printf("couldn't malloc shift_temp :(");
            exit(1);
        }
        ti_Seek(HEADER_SIZE + index * HIGH_SCORE_ENTRY_SIZE, SEEK_SET, appvar); //find index in table
        ti_Read(shift_temp, HIGH_SCORE_ENTRY_SIZE, HIGH_SCORE_TABLE_LENGTH - index - 1, appvar); //copy entries to shift
        ti_Seek(HIGH_SCORE_ENTRY_SIZE, SEEK_CUR, appvar); //move over one
        ti_Write(shift_temp, shift_temp_size, 1, appvar); //shift entries over
        free(shift_temp);
        ti_Seek(-shift_temp_size, SEEK_CUR, appvar); //go back
    } else {
        ti_Seek(HEADER_SIZE + index * HIGH_SCORE_ENTRY_SIZE, SEEK_SET, appvar);
    }
    ti_Write(&score, HIGH_SCORE_ENTRY_SIZE, 1, appvar); //write score into table
}*/