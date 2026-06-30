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
#include <fileioc.h>
#include <debug.h>

#include "main.h"
#include "level.h"
#include "key.h"
#include "game.h"
#include "gfx/bubble.h"
#include "high_score.h"

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
enum game_result game_status;
bool quit;

char lose_string[] = "You Lose!";
char win_string[] = "You Win!";
char best_shooters_string[] = "Best Shooters";

char option_strings[4][18] = {
    "SURVIVAL!",
    "LEVELS",
    "VERSUS",
    "BUBBLES: THE GAME"
};
int option_lengths[4];

bool kb_2nd_press, kb_2nd_prev; 
bool kb_clear_press, kb_clear_prev;
bool kb_up_press, kb_up_prev;
bool kb_down_press, kb_down_prev;

unsigned int player_score;

int main(void) {
    //int i,j,k;
    //point_t point;
    uint16_t saved_palette[256];
    uint8_t high_score_appvar;
    unsigned int high_scores[3];
    unsigned int temp;
    uint8_t placement;
    game_status = STOPPED;

    option_lengths[0] = gfx_GetStringWidth(option_strings[0]);
    option_lengths[1] = gfx_GetStringWidth(option_strings[1]);
    //option_lengths[2] = gfx_GetStringWidth(option_strings[2]);
    //option_lengths[3] = gfx_GetStringWidth(option_strings[3]);
    player_score = 0;

    bool at_logo_animation = true; // 
    bool option_selected = false;
    bool gfx_pal_modified = false; //true if gfx_palette != saved_palette
    bool show_levels_not_implemented = false; //true if user selected levels option
    //bool high_scores_printed = false; //implement partial redraw first
    uint8_t option = 0;
    quit = false;
    int generic_timer = 0;
    uint8_t arrow_timer = 0;

    high_score_appvar = newHighScoreTable();
    getHighScores(high_score_appvar, high_scores);

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
                at_logo_animation = false;
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
                        at_logo_animation = false;
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
                if (option == SURVIVAL) { // no options rn
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
                        if (player_score > high_scores[HIGH_SCORE_TABLE_LENGTH - 1]) { //determine high score placement
                            placement = HIGH_SCORE_TABLE_LENGTH - 1;
                            high_scores[HIGH_SCORE_TABLE_LENGTH - 1] = player_score;
                            for (uint8_t i = HIGH_SCORE_TABLE_LENGTH - 1; i > 0; i--) {
                                if (high_scores[i - 1] > high_scores[i]) { //if in order, break
                                    break;
                                }
                                placement--;
                                temp = high_scores[i - 1];
                                high_scores[i - 1] = high_scores[i];
                                high_scores[i] = temp;
                            }
                        }
            setHighScores(high_score_appvar, high_scores);
                        if (game_status != QUIT) {
                                gfx_pal_modified = true;
                                memcpy(saved_palette, gfx_palette, sizeof(saved_palette));

                            if (game_status == LOSE) {
                                dark = 255;
                                while (dark > 0) {
                                    dark -= 5;
                                    for (int i = 0; i < 256; i++) {
                                        gfx_palette[i] = gfx_Darken(saved_palette[i], dark);
                                    }
                                }
                            } else if (game_status == WIN) {
                                bright = 255;
                                gfx_pal_modified = true;
                                memcpy(saved_palette, gfx_palette, sizeof(saved_palette));
                                while (bright > 0) {
                                    bright -= 5;
                                    for (int i = 0; i < 256; i++) {
                                        gfx_palette[i] = gfx_Lighten(saved_palette[i], bright);
                                    }
                                }
                            }
                        }
                        option_selected = false;
                    }
                } else {
                    if (!show_levels_not_implemented) {
                        gfx_palette[levels_not_implemented_color_index] = BLACK;
                        bright = 255;
                        generic_timer = 0;
                        show_levels_not_implemented = true;
                    }
                    option_selected = false;
                }
            }
        }
        gfx_FillScreen(0xFF);
        gfx_SetTransparentColor(0x31);
        gfx_TransparentSprite(bubbles_logo, logo_left, logo_top);
        gfx_SetTransparentColor(0x00);
        if (!at_logo_animation) {
            gfx_SetTextScale(2, 2);
            gfx_SetTextFGColor(0);
            for (uint8_t i = 0; i < (sizeof(option_strings) / sizeof(option_strings[0])) - 2; i++) { //exclude Versus and Campaign
                gfx_PrintStringXY(option_strings[i], 96, 100 + i * 24);
            }
            gfx_PrintStringXY(">", 80, 100 + option * 24);
            
            gfx_PrintStringXY(best_shooters_string, centerx_image(LCD_WIDTH, gfx_GetStringWidth(best_shooters_string)), 172);
            gfx_SetTextScale(1, 1);
            for (uint8_t i = 0; i < HIGH_SCORE_TABLE_LENGTH; i++) {
                gfx_PrintUIntXY(high_scores[i], 8, (LCD_WIDTH >> 1) - (8*8/2), 196 + i * 10);
            }

            if (show_levels_not_implemented) {
                if (single_press(kb_2nd_press, kb_2nd_prev)) {
                    bright = 0;
                    generic_timer = 40;
                    gfx_palette[levels_not_implemented_color_index] = BLACK;
                    show_levels_not_implemented = false;
                } else {
                    if (generic_timer++ > 30) {
                        gfx_SetTextFGColor(levels_not_implemented_color_index);
                        if (bright > 0) {
                            bright -= 5;
                            gfx_palette[levels_not_implemented_color_index] = gfx_Lighten(gfx_palette[levels_not_implemented_color_index], bright);
                        }
                    }
                    gfx_SetTextScale(1, 1);
                    gfx_SetTextFGColor(levels_not_implemented_color_index);
                    gfx_PrintStringXY("Levels mode not yet implemented!", 20, LCD_HEIGHT - 8);
                    if (!bright) {
                        generic_timer = 0;
                        show_levels_not_implemented = false;
                    }
                }
            }
        }
        gfx_BlitBuffer();
        switch (game_status) {
            case LOSE:
                dark = 0;
                gfx_pal_modified = true;
                gfx_SetTextFGColor(0);
                while (dark < 255) {
                    dark += 10;
                    if (dark > 255) {
                        dark = 255;
                    }
                    for (int i = 0; i < 256; i++) {
                        gfx_palette[i] = gfx_Darken(saved_palette[i], dark);
                    }
                }
                game_status = STOPPED; 
                break;
            case WIN:
                bright = 0;
                gfx_pal_modified = true;
                gfx_SetTextFGColor(0);
                while (bright < 255) {
                    bright += 10;
                    if (bright > 255) {
                        bright = 255;
                    }
                    for (int i = 0; i < 256; i++) {
                        gfx_palette[i] = gfx_Lighten(saved_palette[i], bright);
                    }
                }
                game_status = STOPPED;
                break;
            default:
                break;
        }
        if (quit) {
            break;
        }
    }
    ti_Close(high_score_appvar);
    gfx_End();
}
