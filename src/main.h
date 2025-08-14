#ifndef MAIN_H
#define MAIN_H

#include <graphx.h>

#define centerx_image(screen_width, image_width) ((screen_width - image_width) >> 1)
#define centery_image(screen_height, image_height) ((screen_height - image_height) >> 1)

extern enum {
    LOSE = 1,
    WIN = 2,
    NEXT_LEVEL = 4,
    PAUSE = 8
} game_result;

void fade_in(uint16_t * copy_palette, int * value, int step) {
    /**
     * Function that fades the screen in from black.
     * Uses the provided palette to 
     * Note that the first frame only sets the screen to black without any fading done.
     * Intended usage:
     * while (value \< 255) {
     *   fade\_in(copy_palette, &value, step);
     * }
     *
     * @param copy_palette stores a copy of gfx_palette. Must not be NULL.
     * @param value pointer to an integer that holds the current fade value
     * @param step amount to increase value by during each pass.
     */
    static bool started = false;
    static int fade_step; // discourage unnecessary passage of step value to this function (i guess)
    //dbg_printf("started: %s, value: %d\n", started ? "true" : "false", *value);
    if (!started) {
        if (*value == 255) {
            return;
        }
        fade_step = step;
        memcpy(copy_palette, gfx_palette, sizeof(copy_palette[0]) * 256);
        started = true;
    }

    if (*value <= 255) {
        for (int i = 0; i < 256; i++) {
            gfx_palette[i] = gfx_Darken(copy_palette[i], *value);
        }
        if (*value == 255) {
            started = false;
        } else {
            *value += fade_step;
            if (*value > 255) {
                *value = 255;
            }
        }
    }
}
#endif // MAIN_H
