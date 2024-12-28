#if !defined(MAIN_H)
#define MAIN_H
#include <graphx.h>
#define centerx_image(screen_width, image_width) ((screen_width - image_width) >> 1)
#define centery_image(screen_height, image_height) ((screen_height - image_height) >> 1)
enum {
    LOSE = 1,
    WIN = 2,
    NEXT_LEVEL = 4,
    PAUSE = 8
} game_result;

void fade_in(uint16_t * copy_palette, int step, bool * start) {
    static uint8_t value;
    if (*start == true) {
        value = 0;
        *start = false;
        memcpy(copy_palette, gfx_palette, sizeof(copy_palette[0]) * 256);
    }
    if (value <= 255 / step * step) {
        for (int i = 0; i < 256; i++) {
            gfx_palette[i] = gfx_Darken(copy_palette[0], value);
        }
        value += step;
    }
}
#endif // MAIN_H
