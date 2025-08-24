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
#include "key.h"
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

enum game_mode current_game;
bool lost, won;
bool quit;

char lose_string[] = "You Lose!";
char win_string[] = "You Win!";

char option_strings[4][18] = {
    "SURVIVAL!",
    "LEVELS",
    "VERSUS",
    "BUBBLES: THE GAME"
};
int option_lengths[4];

bool kb_2nd_press, kb_2nd_prev; 
bool kb_clear_press, kb_clear_prev; 


int main(void) {
    //int i,j,k;
    //point_t point;
    uint16_t saved_palette[256];

    option_lengths[0] = gfx_GetStringWidth(option_strings[0]);
    option_lengths[1] = gfx_GetStringWidth(option_strings[1]);
    //option_lengths[2] = gfx_GetStringWidth(option_strings[2]);
    //option_lengths[3] = gfx_GetStringWidth(option_strings[3]);

    bool at_logo = true; // exit logo
    bool booted = false; //
    bool option_selected = false;
    bool gfx_pal_modified = false; //true if gfx_palette != saved_palette
    uint8_t show_levels_not_implemented = false; //true if user selected levels option
    //bool show_versus_not_implemented = false; //true if user selected versus option
    uint8_t option = 0;
    quit = false;
    int generic_timer = 0;
    uint8_t arrow_timer = 0;

    const uint8_t levels_not_implemented_color_index = (sizeof_bubble_pal >> 1) + 7;
    gfx_palette[255] = 0xFFFF;
    gfx_palette[levels_not_implemented_color_index] = 0x0000;

    gfx_Begin();
    gfx_SetDrawBuffer();
    gfx_SetPalette(bubble_pal, sizeof_bubble_pal, 0);
    gfx_SetPalette(bubble_colors, 14, sizeof_bubble_pal >> 1);
    gfx_SetPalette(bubbles_logo_pal, sizeof_bubbles_logo_pal, 49);
    memcpy(saved_palette, gfx_palette, sizeof(saved_palette));
    int logo_left = centerx_image(LCD_WIDTH, bubbles_logo_width);
    int logo_top = centery_image(LCD_HEIGHT, bubbles_logo_height);
    int dark = 0;
    int bright = 0;

    gfx_pal_modified = true;
    while (true) {
        kb_Scan();
        kb_2nd_prev = kb_2nd_press;
        kb_2nd_press = kb_Data[1] & kb_2nd;
        kb_clear_prev = kb_clear_press;
        kb_clear_press = kb_Data[6] & kb_Clear;
        if (logo_top > 20) {
            if (single_press(kb_2nd_press, kb_2nd_prev)) {
                logo_top = 20;
                memcpy(gfx_palette, saved_palette, sizeof(saved_palette));
                gfx_pal_modified = false;
                at_logo = false;
            } else {
                if (dark < 255) {
                    fade_in(saved_palette, &dark, 10);
                } else {
                    if (gfx_pal_modified) {
                        memcpy(gfx_palette, saved_palette, sizeof(saved_palette));
                        gfx_pal_modified = false;
                    }
                    logo_top -= 5;
                    if (logo_top < 20) {
                        logo_top = 20;
                        at_logo = false;
                    }
                }
            }
        } else {
            if (!option_selected) {
                if (single_press(kb_2nd_press, kb_2nd_prev)) {
                    option_selected = true;
                } else if (kb_Data[7] & kb_Up) {
                    option -= option ? 1 : 0;
                } else if (kb_Data[7] & kb_Down) {
                    option += (option < 1) ? 1 : 0;
                } else if (single_press(kb_clear_press, kb_clear_prev)) {
                    quit = true;
                }
                //fade_in(saved_palette, &dark, 5);
                /*if (dark == 255 / 5 * 5) {
                    memcpy(gfx_palette, saved_palette, sizeof(saved_palette));
                    gfx_pal_modified = false;
                    dark = 256;
                } else if (dark < 255) {
                    for (int i = 0; i < 256; i++) {
                        gfx_palette[i] = gfx_Darken(saved_palette[i], dark);
                    }
                    dark += 5;
                }*/
            } else {
                if (option == 0) { // no options rn
                    if (gfx_pal_modified) {
                        memcpy(gfx_palette, saved_palette, sizeof(saved_palette));
                        gfx_pal_modified = false;
                    }
                    if (!quit) {
                        current_game = SURVIVAL;
                        bright = 255;
                        gfx_pal_modified = true;
                        gfx_SetTextFGColor(0);
                        while (bright > 0) {
                            bright -= 5;
                            for (int i = 0; i < 256; i++) {
                                gfx_palette[i] = gfx_Lighten(saved_palette[i], bright);
                            }
                        }
                        gfx_FillScreen(0xFF);
                        memcpy(gfx_palette, saved_palette, sizeof(saved_palette));
                        gfx_BlitBuffer();
                        gfx_pal_modified = false;
                        //while(!os_GetCSC());
                        game();
                        option_selected = false;
                    }
                } else {
                    show_levels_not_implemented = !show_levels_not_implemented;
                    gfx_palette[levels_not_implemented_color_index] = BLACK;
                    generic_timer = 0;
                    option_selected = false;
                }
            }
        }
        gfx_FillScreen(0xFF);
        gfx_SetTransparentColor(0x31);
        gfx_TransparentSprite(bubbles_logo, logo_left, logo_top);
        gfx_SetTransparentColor(0x00);
        if (!at_logo) {
            gfx_SetTextScale(2, 2);
            gfx_SetTextFGColor(0);
            for (unsigned int i = 0; i < (sizeof(option_strings) / sizeof(option_strings[0])) - 2; i++) {
                gfx_PrintStringXY(option_strings[i], 96, 100 + i * 24);
            }
            gfx_PrintStringXY(">", 80, 100 + option * 24);
        }
        if (show_levels_not_implemented) {
            gfx_SetTextScale(1, 1);
            gfx_SetTextFGColor((sizeof_bubble_pal >> 1) + 7);
            gfx_PrintStringXY("Levels mode not yet implemented!", 20, LCD_HEIGHT - 16);
            if (single_press(kb_2nd_press, kb_2nd_prev)) {
                show_levels_not_implemented = false;
            }
            if (generic_timer++ > 30) {
                gfx_SetTextFGColor(levels_not_implemented_color_index);
                bright = 255 - (generic_timer - 30) * 5;
                gfx_palette[levels_not_implemented_color_index] = gfx_Lighten(gfx_palette[levels_not_implemented_color_index], bright);
                if (bright <= 0) {
                    gfx_palette[levels_not_implemented_color_index] = BLACK;
                    show_levels_not_implemented = false;
                    generic_timer = 0;
                }
            }
        }   
        if (quit) {
            break;
        }
        gfx_BlitBuffer();
    }
    gfx_End();
}
