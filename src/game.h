#if !defined(GAME_H)
#define GAME_H

#define centerX(box_1_Width, box_2_Width) ((box_2_Width >> 1) - (box_1_Width >> 1))
enum game_mode {
    SURVIVAL = 0,
    LEVELS = 1,
    COMPETITIVE = 2,
    CAMPAIGN = 3
};

void game();
#endif
