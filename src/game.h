#if !defined(GAME_H)
#define GAME_H

#define centerX(box_1_Width, box_2_Width) ((box_2_Width >> 1) - (box_1_Width >> 1))

extern unsigned int player_score;

enum game_mode {
    SURVIVAL = 0,
    LEVELS = 1,
    COMPETITIVE = 2,
    CAMPAIGN = 3
};

enum game_result {
    STOPPED = 0,
    RUNNING = 1,
    LOSE = 2,
    WIN = 4,
    NEXT_LEVEL = 8,
    PAUSE = 16,
    QUIT = 32
};
void game();
#endif