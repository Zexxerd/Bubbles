#if !defined(GAME_H)
#define GAME_H

#ifndef ANIM_POP_BEHIND_MAX
#define ANIM_POP_BEHIND_MAX 70 //animate up to 10 rows at once currently
#endif
#ifndef ANIM_FALL_BEHIND_MAX
#define ANIM_FALL_BEHIND_MAX 70 //animate up to 10 rows at once currently
#endif

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

enum anim_behind_img {
    ANIM_BUBBLE,
    ANIM_POP_PARTICLE,
};

typedef struct anim_behind {
    bool valid; //true if sprite is displayed
    point_t pos;
    enum anim_behind_img img;
    gfx_sprite_t * sprite;
} anim_behind_t;

static char * printfloat(float elapsed);
int min(int a, int b);

static void capture_behind_sprite(anim_behind_t *behind, int x, int y);
static void restore_behind_sprite(anim_behind_t *behind);

void game();
#endif //GAME_H