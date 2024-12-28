// Program Name: BUBBLES
// Author(s): Zexxerd
// Description: Bubbles!

/* Keep these headers */
#include <tice.h>

/* Standard headers - it's recommended to leave them included */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <graphx.h>
#include <keypadc.h>
#include <debug.h>

#include "main.h"
#include "level.h"
#include "game.h"
#include "gfx/bubble.h"

//matrix(x,y) = matrix((y * matrix_cols) + x)
/**
 Note: Pop bubbles before checking deadzone.
 */

#ifndef TILE_WIDTH
#define TILE_WIDTH 16
#endif
#ifndef TILE_HEIGHT
#define TILE_HEIGHT 16
#endif

extern const uint16_t bubble_colors[7];

bool lost, won;
bool quit;

char lose_string[] = "You Lose!";
char game_mode_strings[4][18] = {
    "SURVIVAL!",
    "LEVELS",
    "VERSUS",
    "BUBBLES: THE GAME"
};

int main(void) {
    //int i,j,k;
    //point_t point;
    uint16_t saved_palette[256];
    bool selected = true;
    bool booted = false;
    quit = false;
    
    gfx_palette[255] = 0xFFFF;
    gfx_Begin();
    gfx_SetDrawBuffer();
    gfx_SetPalette(bubble_pal, sizeof_bubble_pal, 0);
    gfx_SetPalette(bubble_colors, 14, sizeof_bubble_pal >> 1);
    gfx_SetPalette(bubbles_logo_pal, sizeof_bubbles_logo_pal, 49);
    int logo_left = centerx_image(LCD_WIDTH, bubbles_logo_width);
    int logo_top = centery_image(LCD_HEIGHT, bubbles_logo_height);
    int dark = 0;
    while (true) {
        if (selected) {
            gfx_FillScreen(255);
            gfx_SetTransparentColor(0x31);
            gfx_TransparentSprite(bubbles_logo, logo_left, logo_top);
            if (!booted) {
                memcpy(saved_palette, gfx_palette, sizeof(saved_palette));
                memset(gfx_palette, 0, sizeof(uint16_t) * 256);
                booted = true;
            }
            gfx_BlitBuffer();
        }
        gfx_SetTransparentColor(0);
        selected = false;
        while (!selected) {
            kb_Scan();
            if (kb_Data[1] & kb_2nd) {
                selected = true;
            } else if (kb_Data[6] & kb_Clear) {
                quit = true;
                selected = true;
            }
            if (dark == 255 / 5 * 5) {
                memcpy(gfx_palette, saved_palette, sizeof(saved_palette));
                dark = 256;
            } else if (dark < 255) {
                for (int i = 0; i < 256; i++) {
                    gfx_palette[i] = gfx_Darken(saved_palette[i], dark);
                }
                dark += 5;
            }
        }
        memcpy(gfx_palette, saved_palette, sizeof(saved_palette));
        if (!quit) {
            game();
        }
        if (quit) {
            break;
        }
    }
    gfx_End();
}
