#include <tice.h>

/* Standard headers - it's recommended to leave them included */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <graphx.h>
#include <keypadc.h>
#include <debug.h>

#include "key.h"
#include "pixelate.h"
#include "bubble.h"
#include "game.h"
#include "gfx/bubble.h"

//matrix(x,y) = matrix((y * matrix_cols) + x)
/**
 Note: Pop bubbles before checking deadzone.
 */

/**
 TODO: Add animation bubble list instead of pop sprite/fall sprite int arrays
 Mark this mode as survival;
 */
#ifndef TILE_WIDTH
#define TILE_WIDTH 16
#endif
#ifndef TILE_HEIGHT
#define TILE_HEIGHT 16
#endif

static char * printfloat(float elapsed) {
    real_t elapsed_real;
    static char str[10];
    elapsed_real = os_FloatToReal(elapsed <= 0.001f ? 0.0f : elapsed);
    os_RealToStr(str, &elapsed_real, 8, 1, 2);
    return str;
}

extern uint8_t row_offset; // 0: even row shifted; 1: odd row shifted
extern uint8_t max_color;
extern uint8_t * available_colors;

extern uint8_t game_flags; //global because our grid no longer holds it
extern uint8_t shift_rate;
extern unsigned int player_score;
extern unsigned int turn_counter;
extern unsigned int push_down_time;

#ifdef DEBUG
extern bool debug_flag;
#endif

extern gfx_sprite_t * bubble_sprites[7];
extern gfx_sprite_t * bubble_pop_sprites[7];
extern const uint16_t bubble_colors[7];

extern bool pop_started;
extern point_t * pop_locations;
extern bubble_list_t pop_cluster;
extern uint8_t pop_counter; // timer for pop animation

extern bool fall_started;
extern falling_bubble_list_t fall_data;
extern int fall_total;
extern uint8_t fall_counter;

extern bool quit;
extern char lose_string[];
extern char option_strings[4][18];

extern bool kb_2nd_press, kb_2nd_prev;
extern bool kb_clear_press, kb_clear_prev;

void game(void) {
    int i,j,k; //universal counter
    point_t point;
    kb_key_t key;
    kb_key_t prevkey;
    
    int level_start_y_pos;
    uint8_t fps_counter;
    float fps_ratio;
    float fps, last_fps, ticks;
    char * fps_string;
    
#ifdef DEBUG
    uint8_t x, y;
    uint8_t highlight_timer;
    int debug_fall_total;
    point_t debug_point;
    bubble_t debug_bubble;
    bubble_list_t neighbors;
    bubble_list_t foundcluster;
    //debugTestOutput();
    //debugTestRead();
    //exit(1);
#endif //DEBUG
    
    enum game_mode current_game;
    //level
    uint8_t level_number;
    uint8_t level_number_len;
    bool level_start_finished;
    char * level_type_text;
    bool lost, won;

    //grid, shooter
    shooter_t shooter;
    grid_t grid;
    gfx_sprite_t * grid_buffer;
    gfx_sprite_t * behind_proj_sprite;
    
    //key presses

    //pop_sprite
    gfx_sprite_t * pop_sprite;
    gfx_sprite_t * pop_sprite_rotations[3];
    //gfx_sprite_t * behind_pop_sprites;
    
    bubble_list_t animation_list;
    gfx_sprite_t * lose_animation_behind;
    
    bool draw_behind_proj_sprite;
    //grid settings
    max_color = 6;
    shift_rate = 7;
    push_down_time = 6;
    turn_counter = 0;
    player_score = 0;
    fps = last_fps = 30;
    fps_string = malloc(15 * sizeof(char));

    //level
    level_number = 0;
    level_number_len = 1;
    level_start_finished = false;
    level_start_y_pos = 0;
    level_type_text = "";
    current_game = SURVIVAL;

    //shooter
    shooter.x = 160 - (TILE_WIDTH >> 1);
    shooter.y = 220 - (TILE_HEIGHT >> 1);
    shooter.angle = 0;
    shooter.pal_index  = (sizeof_bubble_pal >> 1) + 7;
    shooter.projectile.x = 0;
    shooter.projectile.y = 0;
    shooter.projectile.speed = 5;
    for (i = 0; i < 3; i++)
        shooter.next_bubbles[i] = randInt(0, max_color);
    shooter.flags = DEACTIVATED;
    shooter.vectors = generateVectors(-64, 64, 4);
    shooter.projectile.x = 0;
    shooter.projectile.y = 0;
    shooter.projectile.color = shooter.next_bubbles[0];
    shooter.projectile.visible = false;
    shooter.projectile.angle = 0;
    shooter.counter = 0;
    //grid
    grid.cols = 7;
    grid.rows = 17; //16 + deadzone
    grid.x = centerX(((TILE_WIDTH * grid.cols) + (TILE_WIDTH >> 1)), LCD_WIDTH);
    grid.y = 0;
    grid.ball_diameter = TILE_WIDTH;
    game_flags = RENDER | NEW_LEVEL;
    grid.width = (TILE_WIDTH * grid.cols) + (TILE_WIDTH>>1);
    grid.height = (ROW_HEIGHT * grid.rows) + (TILE_WIDTH>>2);
    grid.bubbles = (bubble_t *) malloc((grid.cols*grid.rows) * sizeof(bubble_t));
    if (grid.bubbles == NULL) exit(1);
    grid.possible_collisions.bubbles = NULL;
    //declare an image buffer for the grid
    grid_buffer = gfx_MallocSprite(grid.cols * TILE_WIDTH + (TILE_WIDTH>>1),ROW_HEIGHT * grid.rows + (TILE_WIDTH>>2));
    if (grid_buffer == NULL) exit(1);
    
    srand(rtc_Time());
    available_colors = (uint8_t *) malloc(8 * sizeof(uint8_t));
    setAvailableColors(available_colors, 0x7F);
    initGrid(grid,grid.rows,grid.cols, 10, NULL);
    grid.possible_collisions = getPossibleCollisions(grid);
    
    //popping animation sprites
    pop_counter = 0;
    pop_started = false;
    for (i = 0; i < 3; i++) {
        pop_sprite_rotations[i] = gfx_MallocSprite(bubble_red_pop_width, bubble_red_pop_height);
        if (pop_sprite_rotations[i] == NULL)
            exit(1);
    }
    //falling animation
    fall_counter = 0;
    fall_started = false;
    fall_data.bubbles = (falling_bubble_t *) malloc(grid.cols * grid.rows * sizeof(falling_bubble_t));
    
    //redraw
    behind_proj_sprite = gfx_MallocSprite(16, 16);
    draw_behind_proj_sprite = false;
    
    lost = false;
    won = false;
    
    strcpy(fps_string,"FPS: ");
    fps_counter = 0;
    row_offset = 0;
    
#ifdef DEBUG
    x = y = 0;
    highlight_timer = 0;
#endif
    gfx_FillScreen(255);
    /*Initialize timer*/
    timer_Control = TIMER1_DISABLE;
    timer_1_Counter = 0;
    timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_UP;
    
    /*Main game*/
    //Starting level
    /*
     Note: Peform graphics buffer-using logic before clearing screen
     */
    while (!single_press(kb_clear_press, kb_clear_prev)) {
        kb_Scan();
        prevkey = key;
        key = kb_AnyKey();
        kb_2nd_prev = kb_2nd_press;
        kb_2nd_press = kb_Data[1] & kb_2nd;
        kb_clear_prev = kb_clear_press;
        kb_clear_press = kb_Data[6] & kb_Clear;
        if (game_flags & RENDER) {
            renderGrid(grid, grid_buffer);
            game_flags &= ~RENDER;
        }
        gfx_FillScreen(255);  //Goal: change render method to partial
        if (game_flags & NEW_LEVEL) {
            if (!level_start_finished) {
                gfx_SetTextScale(4, 4);
                if (level_start_y_pos == 0) {
                    gfx_palette[0] = WHITE;
                }
                if (current_game == SURVIVAL) {
                    level_type_text = option_strings[SURVIVAL];
                } else if (current_game == LEVELS) {
                    level_type_text = "Level";
                }
                if (level_start_y_pos < 64) {
                    gfx_palette[0] = ((32 - (level_start_y_pos >> 1)) << 11) | ((64 - level_start_y_pos) << 5) | (32 - (level_start_y_pos >> 1));
                    level_start_y_pos += 2;
                }
                if (current_game == SURVIVAL) {
                    gfx_PrintStringXY(level_type_text, 0, level_start_y_pos);
                } else if (current_game == LEVELS) {
                    gfx_PrintStringXY(level_type_text, 0, level_start_y_pos);
                    gfx_PrintUIntXY(level_number + 1, level_number_len, gfx_GetStringWidth(level_type_text), level_start_y_pos);
                }
                if (single_release(key, prevkey) && (level_start_y_pos >= 40)) {
                    gfx_palette[0] = BLACK;
                    gfx_SetTextScale(1, 1);
                    shooter.flags &= ~DEACTIVATED;
                    level_start_finished = true;
                    game_flags &= ~NEW_LEVEL;
                    kb_Reset();
                }
                gfx_BlitBuffer();
                continue;
            } else {
                if (grid.rows < 10) {
                    grid.rows++;
                } else {
                    shooter.flags &= ~DEACTIVATED;
                    game_flags &= ~NEW_LEVEL;
                }
            }
        }
        if (kb_Data[7] & kb_Left) {
#ifdef DEBUG
            x -= (x > 0);
#endif
            if (shooter.angle > LBOUND) {
                shooter.angle -= SHOOTER_STEP;
                //shooter.flags |= REDRAW_SHOOTER;
            }
            
        }
        if (kb_Data[7] & kb_Right) {
#ifdef DEBUG
            x += (x < grid.cols-1);
#endif
            if (shooter.angle < RBOUND) {
                shooter.angle += SHOOTER_STEP;
                //shooter.flags |= REDRAW_SHOOTER;
            }
        }
#ifdef DEBUG
        if (kb_Data[7] & kb_Up)
            y -= (y > 0);
        if (kb_Data[7] & kb_Down)
            y += (y < grid.rows - 1);
#endif
        /*Shoot bubbles*/
        if (kb_Data[1] & kb_2nd) {
            if (!(shooter.flags & DEACTIVATED)) { // implement shaking animation?
                if (!(shooter.flags & ACTIVE_PROJ)) {
                    if (grid.possible_collisions.size) {
                        shooter.projectile.x = shooter.x;
                        shooter.projectile.y = shooter.y;
                        shooter.projectile.speed = 5;
                        shooter.projectile.angle = shooter.angle;
                        shooter.projectile.color = shooter.next_bubbles[0];
                        shooter.next_bubbles[0] = shooter.next_bubbles[1];
                        shooter.next_bubbles[1] = shooter.next_bubbles[2];
                        available_colors = getAvailableColors(grid);
                        if (!available_colors[0]) {
                            available_colors[0] = 1;
                            available_colors[1] = 0;
                        }
                        shooter.next_bubbles[2] = available_colors[randInt(1, available_colors[0])];
                        shooter.projectile.visible = true;
                        shooter.flags |= ACTIVE_PROJ;
                    }
                }
            } else {
                dbg_printf("shooter is deactivated, cannot shoot\n");
                shooter.flags |= SHAKE; //start shaking animation
                shooter.counter = (uint8_t) fps / 3; //shake for 1/3 second
            }
        }
        
#ifdef DEBUG
        /*Debug: Show neighbors*/
        if (kb_Data[2] & kb_Alpha) {
            while (kb_Data[2] & kb_Alpha) kb_Scan();
            debug_bubble = grid.bubbles[(y * grid.cols) + x];
            neighbors = getNeighbors(grid,debug_bubble.x,debug_bubble.y,false);
            gfx_SetColor(255);
            debug_point.x = gfx_GetStringWidth("Current:");
            debug_point.y = 0;
            gfx_FillRectangle(0, 0, 100, (debug_point.y + neighbors.size) << 3);
            gfx_PrintStringXY("Current:", 0, 0);
            gfx_PrintStringXY("(", debug_point.x, 0);
            gfx_PrintUIntXY(debug_bubble.x, 2, debug_point.x + 8, 0);
            gfx_PrintStringXY(",", debug_point.x + 24, debug_point.y);
            gfx_PrintUIntXY(debug_bubble.y, 2, debug_point.x + 40, 0);
            gfx_PrintStringXY(")", debug_point.x + 56, debug_point.y);
            gfx_PrintUIntXY(debug_bubble.color, 2, debug_point.x + 64, 0);

            debug_point.x = gfx_GetStringWidth("Neighbors:");
            debug_point.y = 32;
            gfx_PrintStringXY("Neighbors:",0,debug_point.y);
            for (i = 0; i < neighbors.size; i++) {
                j = debug_point.y + (i << 3);
                gfx_PrintStringXY("(",debug_point.x,j);
                gfx_PrintUIntXY(neighbors.bubbles[i].x, 2,debug_point.x + 8, j);
                gfx_PrintStringXY(",", debug_point.x + 24, debug_point.y);
                gfx_PrintUIntXY(neighbors.bubbles[i].y, 2, debug_point.x + 40, j);
                gfx_PrintStringXY(")", debug_point.x + 56, debug_point.y);
                gfx_PrintUIntXY(neighbors.bubbles[i].color, 2, debug_point.x + 64, j);
            }
            gfx_BlitBuffer();
            while(!os_GetCSC());
        }
        /*Debug: Foundcluster*/
        if (kb_Data[1] & kb_Mode) {
            while (kb_Data[1] & kb_Mode) kb_Scan();
            foundcluster = findCluster(grid, x, y, true, true, false);
            gfx_SetColor(255);
            gfx_FillRectangle(220, 0, 100, foundcluster.size<<3);
            gfx_PrintStringXY("Foundcluster:", 220, 0);
            for (i = 0; i < foundcluster.size; i++){
                debug_point.x = 220;
                debug_point.y = 16 + (i<<4);
                gfx_PrintUIntXY(foundcluster.bubbles[i].x, 2, debug_point.x, debug_point.y);
                gfx_PrintUIntXY(foundcluster.bubbles[i].y, 2, debug_point.x + 24, debug_point.y);
                gfx_PrintUIntXY(foundcluster.bubbles[i].color, 2, debug_point.x + 56, debug_point.y);
                gfx_TransparentSprite(bubble_sprites[foundcluster.bubbles[i].color], LCD_WIDTH - TILE_WIDTH, 16 + (i<<4));
            }
            gfx_BlitBuffer();
            while(!os_GetCSC());
            free(foundcluster.bubbles);
        }
        /*Debug: Change shooter color*/
        if (kb_Data[2] & kb_Math) {
            if (!shooter.next_bubbles[0]--)
                shooter.next_bubbles[0] = max_color;
            while (kb_Data[2] & kb_Math) kb_Scan();
            game_flags |= RENDER;
        }
        if (kb_Data[3] & kb_Apps) {
            if (shooter.next_bubbles[0]++ == max_color)
                shooter.next_bubbles[0] = 0;
            while (kb_Data[3] & kb_Apps) kb_Scan();
            game_flags |= RENDER;
        }
        /*Debug: Change tile color*/
        if (kb_Data[3] & kb_GraphVar) {
            if (!(grid.bubbles[y * grid.cols + x].flags & EMPTY)) {
                grid.bubbles[y * grid.cols + x].color = shooter.next_bubbles[0];
            }
            game_flags |= RENDER;
        }
        /*Debug: Enable/disable a tile*/
        if (kb_Data[1] & kb_Del) {
            grid.bubbles[y * grid.cols + x].flags ^= EMPTY;
            while (kb_Data[1] & kb_Del) kb_Scan();
            game_flags |= RENDER;
        }
        /*Debug: Falling bubbles*/
        if (kb_Data[4] & kb_Stat) {
            gfx_FillScreen(255);
            debug_fall_total = findFloatingClusters(grid);
            gfx_PrintUIntXY(debug_fall_total,8,60,120);
            for (i = 0;i < grid.cols*grid.rows;i++) {
                if (grid.bubbles[i].flags & FALLING) {
                    drawTile(grid.bubbles[i].color,grid.x+(grid.bubbles[i].x*TILE_WIDTH + (row_offset?TILE_WIDTH>>1:0)),grid.y+(grid.bubbles[i].y*ROW_HEIGHT));
                }
            }
            gfx_BlitBuffer();
            while(!os_GetCSC());
        }
        /*Debug: Available colors*/
        if (kb_Data[4] & kb_Prgm) {
            gfx_FillScreen(255);
            gfx_PrintStringXY("Size: ",0,0);
            gfx_PrintUIntXY(available_colors[0],1,48,0);
            for (j = 0;j < available_colors[0];j++) {
                gfx_PrintUIntXY(available_colors[j+1],8,120,16+j*8);
            }
            gfx_BlitBuffer();
            while(!os_GetCSC());
        }
        /*Debug: Possible collisions*/
        if (kb_Data[5] & kb_Vars) {
            gfx_FillScreen(255);
            grid.possible_collisions = getPossibleCollisions(grid);
            for (i = 0;i < grid.possible_collisions.size;i++) {
                point = getTileCoordinate(grid.possible_collisions.bubbles[i].x,grid.possible_collisions.bubbles[i].y);
                point.x += grid.x;
                point.y += grid.y;
                drawTile(grid.possible_collisions.bubbles[i].color,point.x,point.y);
            }
            gfx_BlitBuffer();
            while (!os_GetCSC());
        }
        /*Debug: Pushdown*/
        if (kb_Data[2] & kb_Recip) {
            pushDown(&grid);
            while(!os_GetCSC());
        }
        /*Debug: Shift Rate*/
        if (kb_Data[3] & kb_Sin) {
            shift_rate--;
        }
        if (kb_Data[4] & kb_Cos) {
            shift_rate++;
        }
        if (kb_Data[5] & kb_Tan) {
            push_down_time--;
        }
        if (kb_Data[6] & kb_Power) {
            push_down_time++;
        }
        /*Debug: Set deactivated*/
        if (kb_Data[2] & kb_Square) {
            shooter.flags ^= DEACTIVATED;
        }
#endif //DEBUG
        if (game_flags & POP) {
            if (!pop_started) {
                pop_locations = (point_t *) malloc(sizeof(point_t) * pop_cluster.size);
                if (pop_locations == NULL) {
                    debug_message("pop_locations null!!!! :(");
                    exit(1);
                }

                pop_sprite = bubble_pop_sprites[pop_cluster.bubbles[0].color];
                pop_sprite_rotations[0] = gfx_FlipSpriteY(pop_sprite,pop_sprite_rotations[0]);
                pop_sprite_rotations[1] = gfx_FlipSpriteX(pop_sprite,pop_sprite_rotations[1]);
                pop_sprite_rotations[2] =  gfx_FlipSpriteX(pop_sprite_rotations[0],pop_sprite_rotations[2]);
                for (i = 0;i < pop_cluster.size;i++) {
                    point = getTileCoordinate(pop_cluster.bubbles[i].x,pop_cluster.bubbles[i].y);
                    pop_locations[i].x = point.x + grid.x;
                    pop_locations[i].y = point.y + grid.y;
                    
                }
                pop_started = true;
            }
            pop_counter++;
            if (pop_counter == 20) {
                pop_counter = 0;
                pop_started = false;
                game_flags &= ~POP;
                free(pop_locations);
                free(pop_cluster.bubbles);
            }
        }
        if (game_flags & FALL) {
            if (!fall_started) {
                fall_counter = 0;
                fall_started = true;
            }
            //Animate
            for (i = 0; i < fall_total; i++) {
                fall_data.bubbles[i].x += (int8_t) fall_data.bubbles[i].velocity;
                fall_data.bubbles[i].y += (fall_counter * 5) >> 2;
            }
            fall_counter++;
            if (fall_counter == 25) {
                fall_counter = 0;
                fall_started = false;
                game_flags &= ~FALL;
                memset(fall_data.bubbles, 0, sizeof(falling_bubble_t) * fall_data.size);
                /*for(i = 0;i < fall_data.size;i++) {
                 fall_data.bubbles[i].x = 0;
                 fall_data.bubbles[i].y = 0;
                 fall_data.bubbles[i].color = 0;
                 fall_data.bubbles[i].velocity = 0;
                 }*/
            }
        }
        if (game_flags & CHECK) {
            for (i = 0;i < grid.cols; i++) {
                if (!(grid.bubbles[grid.cols * (grid.rows - 1) + i].flags & EMPTY)) {
                    lost = true;
                }
            }
            j = 0;
            for (i = 0; i < grid.cols * grid.rows; i++) {
                if (!(grid.bubbles[grid.cols * (grid.rows - 1) + i].flags & EMPTY)) {
                    j = 1;
                    break;
                }
            }
            if (!j) {
                game_flags |= NEW_LEVEL;
                if (current_game == LEVELS)
                    won = true;
            }
        }
        if (game_flags & PUSHDOWN) {
            pushDown(&grid);
            game_flags &= ~PUSHDOWN;
        }
        if (game_flags & CHECK) {
            grid.possible_collisions = getPossibleCollisions(grid);
            game_flags &= ~CHECK;
        }
        //Move the projectile
        
        if (draw_behind_proj_sprite) {
            gfx_Sprite(behind_proj_sprite, shooter.projectile.x, shooter.projectile.y);
            draw_behind_proj_sprite = false;
        }
        if (shooter.flags & ACTIVE_PROJ) {
            fps_ratio = fps / last_fps;
            moveProj(grid, &shooter, fps_ratio * 2);
        }
        
        //Display
        if (!(game_flags & NEW_LEVEL) && (current_game == SURVIVAL)) {
            if (shooter.flags & SHAKE) {
                dbg_printf("shaker: (%d, %d) %d\n",(shooter.shake_values & 0xF0) >> 4, shooter.shake_values & 0x0F, shooter.counter);
                if (!shooter.counter) {
                    shooter.flags &= ~SHAKE;
                } else {
                    shooter.shake_values = (randInt(-4, 4) << 4) | randInt(-4, 4);
                    shooter.counter--;
                }
            } else {
                shooter.shake_values = 0;
            }
            renderShooter(shooter);
            gfx_TransparentSprite(grid_buffer,grid.x,grid.y);
            gfx_SetColor(0);
            gfx_Rectangle(grid.x,grid.y,grid.width,grid.height-ROW_HEIGHT);
            
            if (shooter.projectile.visible) {
                gfx_GetSprite(behind_proj_sprite,shooter.projectile.x,shooter.projectile.y);
                draw_behind_proj_sprite = true;
                gfx_TransparentSprite(bubble_sprites[shooter.projectile.color], shooter.projectile.x, shooter.projectile.y);
            }
            if (game_flags & RENDER) {
                shooter.projectile.visible = false;
            }
            if (pop_started) {
                for (i = 0; i < pop_cluster.size; i++) { //animate
                    if (pop_counter & 1) {
                        drawTile(pop_cluster.bubbles[i].color, pop_locations[i].x, pop_locations[i].y);
                    }
                    else {
                        //gfx_SetColor(255);
                        //gfx_FillRectangle(pop_cluster.bubbles[i].x,pop_cluster.bubbles[i].y,TILE_WIDTH,TILE_HEIGHT);
                    }
                    point.x = pop_locations[i].x - pop_counter;
                    point.y = pop_locations[i].y - pop_counter;
                    gfx_TransparentSprite(pop_sprite,point.x,point.y);
                    point.x = pop_locations[i].x + pop_counter + (TILE_WIDTH>>1);
                    gfx_TransparentSprite(pop_sprite_rotations[0],point.x,point.y);
                    point.y = pop_locations[i].y + pop_counter + (TILE_WIDTH>>1);
                    gfx_TransparentSprite(pop_sprite_rotations[2],point.x,point.y);
                    point.x = pop_locations[i].x - pop_counter;
                    gfx_TransparentSprite(pop_sprite_rotations[1],point.x,point.y);
                }
            }
            if (game_flags & FALL) {
                for (i = 0; i < fall_total; i++) {
                    j = fall_data.bubbles[i].y;
                    if (j < LCD_HEIGHT) {
                        drawTile(fall_data.bubbles[i].color, fall_data.bubbles[i].x, fall_data.bubbles[i].y);
                    }
                }
            }
            /*if (current_game == SURVIVAL) {
             gfx_PrintStringXY("SURVIVAL!", 0, 0);
             } else if (current_game == LEVELS) {
             gfx_PrintStringXY("Level", 0, 0);
             gfx_PrintUIntXY((int) level_number + 1, level_number_len, 48, 0);
             }*/
            gfx_PrintStringXY(level_type_text, 0, 0);
            if (current_game == LEVELS) {
                gfx_PrintUIntXY((int) level_number + 1, level_number_len, 48, 0);
            }
            gfx_PrintStringXY("Turn:",0,24);
            gfx_PrintUIntXY(turn_counter,3,48,24);
            gfx_PrintStringXY("Shift rate:",0,32);
            gfx_PrintUIntXY(shift_rate,3,74,32);
            gfx_PrintStringXY("Push time:",0,40);
            gfx_PrintUIntXY(push_down_time,3,74,40);
            gfx_PrintStringXY("Score: ",0,48);
            gfx_SetTextXY(60,48);
            gfx_PrintInt(player_score,5);
            gfx_PrintStringXY("Angle:", 0, 56);
            gfx_PrintIntXY(shooter.angle, 3, 74, 56);
#ifdef DEBUG
            /*Draw a highlight for the highlighted square*/
            if ((highlight_timer-1) & 1) {
                gfx_SetColor(5);
                debug_point = getTileCoordinate(x,y);
                gfx_FillRectangle(grid.x+debug_point.x,grid.y+debug_point.y,TILE_WIDTH,TILE_HEIGHT);
                drawTile(grid.bubbles[y * grid.cols + x].color,grid.x+debug_point.x,grid.y+debug_point.y);
            }
            highlight_timer++;
            gfx_PrintStringXY("X:",0,LCD_HEIGHT-24);
            gfx_PrintStringXY("Y:",0,LCD_HEIGHT-16);
            gfx_PrintUIntXY(x,2,20,LCD_HEIGHT-24);
            gfx_PrintUIntXY(y,2,20,LCD_HEIGHT-16);
            if (game_flags & RENDER) {
                gfx_PrintStringXY("game:RENDER",0,56);
            }
            if (game_flags & SHIFT) {
                gfx_PrintStringXY("game:SHIFT",0,64);
            }
            if (game_flags & POP) {
                gfx_PrintStringXY("game:POP",0,72);
            }
            if (game_flags & FALL) {
                gfx_PrintStringXY("game:FALL",0,80);
            }
            if (game_flags & PUSHDOWN) {
                gfx_PrintStringXY("game:PUSHDOWN",0,88);
            }
            if (game_flags & CHECK) {
                gfx_PrintStringXY("game:CHECK",0,96);
            }
            if (shooter.projectile.visible == true) {
                gfx_PrintStringXY("proj:VISIBLE",0,104);
            }
#endif //DEBUG
        }
        ticks = (float)atomic_load_increasing_32(&timer_1_Counter) / 32768;
        last_fps = fps;
        fps =  1.0 / ticks;
        timer_Control = TIMER1_DISABLE;
        timer_1_Counter = 0;
        timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_UP;
        gfx_SetTextScale(1, 1);
        gfx_PrintStringXY(fps_string, 0, LCD_HEIGHT - 8);
        if (++fps_counter == 7) {
            strcpy(&fps_string[5], printfloat(fps));
            fps_counter = 0;
        }
        gfx_SetTextFGColor(0);
        gfx_BlitBuffer();
        if (lost || won) break;
    }
    if (lost) {
        //init partial redraw
        k = 1;
        i = gfx_GetStringWidth(lose_string);
        lose_animation_behind = gfx_MallocSprite(i,8);
        point.x = (LCD_WIDTH>>1)-(i>>1);
        point.y = (LCD_HEIGHT>>1)-4;
        gfx_GetSprite(lose_animation_behind,point.x,point.y);
        gfx_PrintStringXY(lose_string,point.x,point.y);
        while (k < 4) {
            timer_Control = TIMER1_DISABLE;
            timer_1_Counter = 0;
            timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_UP;
            while (timer_1_Counter < 2730); // 32768 / 60 * 5
            k++;
            //clear old background
            gfx_Sprite(lose_animation_behind,point.x,point.y);
            free(lose_animation_behind);
            //movement code
            gfx_SetTextScale(k,k);
            i = gfx_GetStringWidth(lose_string);
            lose_animation_behind = gfx_MallocSprite(i,k<<3);
            point.x = (LCD_WIDTH>>1)-(i>>1);
            point.y = (LCD_HEIGHT>>1)-(4*k);
            //get new background and print new sprite/string
            gfx_GetSprite(lose_animation_behind,point.x,point.y);
            gfx_PrintStringXY(lose_string,point.x,point.y);
            gfx_BlitBuffer();
        }
        while (!os_GetCSC());
        free(lose_animation_behind);
    }
    game_flags = 0x00;
    shooter.flags = 0x00;
    shooter.projectile.x = shooter.projectile.y = shooter.projectile.speed = shooter.projectile.color = 0;
    free(grid.bubbles);
    free(grid_buffer);
    free(available_colors);
    free(fall_data.bubbles);
    free(fps_string);
    for (i = 0; i < 3; i++) {
        free(pop_sprite_rotations[i]);
    }
}
