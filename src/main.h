#if !defined(MAIN_H)
#define MAIN_H
#define centerx_image(screen_width, image_width) ((screen_width - image_width) >> 1)
#define centery_image(screen_height, image_height) ((screen_height - image_height) >> 1)
enum {
    LOSE = 1,
    WIN = 2,
    NEXT_LEVEL = 4,
    PAUSE = 8
} game_result;
#endif // MAIN_H
