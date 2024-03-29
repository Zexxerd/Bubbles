#if 0
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <graphx.h>
#include <keypadc.h>
#include <debug.h>

#include "pixelate.h"
#include "bubble.h"
#include "level.h"
#include "gfx/bubble.h"

void game() {
    uint8_t x,y;
    int i,j,k; //universal counter
    point_t point;

    bool lost;
    uint8_t highlight_timer;
    uint8_t fps_counter;
    uint8_t fps_ratio;
    float fps, last_fps, ticks;
    char * fps_string;

    shooter_t shooter;
    grid_t grid;
    gfx_sprite_t * grid_buffer;
    
    gfx_sprite_t * pop_sprite;
    gfx_sprite_t * pop_sprite_rotations[3];
    
    gfx_sprite_t * behind_16x16_sprite;
    
    gfx_sprite_t * lose_animation_behind;
    char lose_string[] = "You Lose!";
    
    max_color = 6;
    shift_rate = 6;
    push_down_time = 6;
    turn_counter = 0;
    player_score = 0;
    x = y = 0;
    fps = last_fps = 30;
    fps_string = malloc(15);
    
    //popping animation sprites
    pop_counter = 0;
    pop_started = false;
    for (i=0;i<3;i++) {
        pop_sprite_rotations[i] = gfx_MallocSprite(bubble_red_pop_width,bubble_red_pop_height);
        if (pop_sprite_rotations[i] == NULL)
            exit(1);
    }
    
    //falling animation
    fall_counter = 0;
    fall_started = false;

    //shooter
    shooter.x = 160 - (TILE_WIDTH>>1);
    shooter.y = 220 - (TILE_HEIGHT>>1);
    shooter.angle = 0;
    shooter.pal_index  = (sizeof_bubble_pal>>1) + 7;
    shooter.projectile.speed = 5;
    for (i = 0;i < 3;i++)
        shooter.next_bubbles[i] = randInt(0,max_color);
    shooter.flags = 0x00;
    shooter.projectile.x = 0;
    shooter.projectile.y = 0;
    shooter.projectile.color = shooter.next_bubbles[0];
    shooter.projectile.angle = 0;
    
    //grid
    grid.cols = 7;
    grid.rows = 17; //16 + deadzone
    grid.x = 160 - (((TILE_WIDTH * grid.cols) + (TILE_WIDTH>>1))>>1);
    grid.y = 0;
    grid.ball_diameter = TILE_WIDTH;
    game_flags = RENDER;
    grid.width = (TILE_WIDTH * grid.cols) + (TILE_WIDTH>>1);
    grid.height = (ROW_HEIGHT * grid.rows) + (TILE_WIDTH>>2);
    grid.bubbles = (bubble_t *) malloc((grid.cols*grid.rows) * sizeof(bubble_t));
    if (grid.bubbles == NULL) exit(1);
    
    //declare an image buffer for the grid
    grid_buffer = gfx_MallocSprite(grid.cols * TILE_WIDTH + (TILE_WIDTH>>1),ROW_HEIGHT * grid.rows + (TILE_WIDTH>>2));
    if (grid_buffer == NULL) exit(1);
    
    srand(rtc_Time());
    available_colors = (uint8_t *) malloc(8*sizeof(uint8_t));
    setAvailableColors(available_colors,0x7F);
    initGrid(grid,grid.rows,grid.cols,10,NULL);
    possible_collisions = getPossibleCollisions(grid);
    
    
    highlight_timer = 0;
    lost = false;
    
    x = y = 0;
    strcpy(fps_string,"FPS: ");
    fps_counter = 0;
    
    gfx_palette[255] = 0xFFFF;
    gfx_Begin();
    gfx_SetDrawBuffer();
    gfx_SetPalette(bubble_pal,sizeof_bubble_pal,0);
    gfx_SetPalette(bubble_colors,14,sizeof_bubble_pal>>1);
    row_offset = 0;
    
    
    
    
    /*Initialize timer*/
    timer_Control = TIMER1_DISABLE;
    timer_1_Counter = 0;
    timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_UP;
    /*Main game*/
    
    while (!(kb_Data[6] & kb_Clear)) {
        if (lost) break;
        for (i=0;i<grid.cols;i++) {
            if (!(grid.bubbles[grid.cols*(grid.rows-1)+i].flags & EMPTY)) {
                lost = true;
            }
        }
        if (game_flags & RENDER) {
            renderGrid(grid,grid_buffer);
            game_flags &= ~RENDER;
        }
        gfx_FillScreen(255);  //Change render method (speed!)
        kb_Scan();
        if (kb_Data[7] & kb_Left) {
#if DEBUG
            x -= (x > 0);
#endif
            if (shooter.angle > LBOUND)
                shooter.angle-=4;
        }
        if (kb_Data[7] & kb_Right) {
#if DEBUG
            x += (x < grid.cols-1);
#endif
            if (shooter.angle < RBOUND)
                shooter.angle+=4;
        }
#if DEBUG
        if (kb_Data[7] & kb_Up)
            y -= (y > 0);
        if (kb_Data[7] & kb_Down)
            y += (y < grid.rows-1);
#endif
        /*Shoot bubbles*/
        if (kb_Data[1] & kb_2nd) {
            if (!(shooter.flags & ACTIVE_PROJ)) {
                possible_collisions = getPossibleCollisions(grid);
                if (possible_collisions.size) {
                    shooter.projectile.x = shooter.x;
                    shooter.projectile.y = shooter.y;
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
                    shooter.flags |= ACTIVE_PROJ;
                } else {
                    //none
                }
            }
        }
        if (game_flags & POP) {
            if (!pop_started) {
                pop_locations = (int **) malloc(sizeof(int *) * pop_cluster.size);
                dbg_getlocation(pop_cluster.bubbles);
                dbg_getlocation(pop_locations);
                if (pop_locations == NULL) {
                    debug_message("pop_locations null!!!! :(");
                    exit(1);
                }
                
                pop_sprite = bubble_pop_sprites[pop_cluster.bubbles[0].color];
                pop_sprite_rotations[0] = gfx_FlipSpriteY(pop_sprite,pop_sprite_rotations[0]);
                pop_sprite_rotations[1] = gfx_FlipSpriteX(pop_sprite,pop_sprite_rotations[1]);
                pop_sprite_rotations[2] =  gfx_FlipSpriteX(pop_sprite_rotations[0],pop_sprite_rotations[2]);
                dbg_getlocation(pop_sprite);
                for (i = 0;i < 3;i++)
                    dbg_getlocation(pop_sprite_rotations[i]);
                for (i = 0;i < pop_cluster.size;i++) {
                    pop_locations[i] = (int *) malloc(sizeof(int *) * 2); //x, y
                    if (pop_locations[i] == NULL) {
                        debug_message("hooray!");
                        exit(1);
                    }
                    point = getTileCoordinate(pop_cluster.bubbles[i].x,pop_cluster.bubbles[i].y);
                    pop_locations[i][0] =  point.x + grid.x;
                    pop_locations[i][1] =  point.y + grid.y;

                }
                
                pop_started = true;
                dbg_printf("\n");
            }
            for (i = 0;i < pop_cluster.size;i++) { //animate
                if (pop_counter & 1)
                    drawTile(pop_cluster.bubbles[i].color,pop_locations[i][0],pop_locations[i][1]);
                point.x = pop_locations[i][0] - pop_counter;
                point.y = pop_locations[i][1] - pop_counter;
                gfx_TransparentSprite(pop_sprite,point.x,point.y);
                point.x = pop_locations[i][0] + pop_counter + (TILE_WIDTH>>1);
                gfx_TransparentSprite(pop_sprite_rotations[0],point.x,point.y);
                point.y = pop_locations[i][1] + pop_counter + (TILE_WIDTH>>1);
                gfx_TransparentSprite(pop_sprite_rotations[2],point.x,point.y);
                point.x = pop_locations[i][0] - pop_counter;
                gfx_TransparentSprite(pop_sprite_rotations[1],point.x,point.y);
            }
            pop_counter++;
            if (pop_counter == 20) {
                pop_counter = 0;
                pop_started = false;
                game_flags &= ~POP;
                for (i = 0;i < pop_cluster.size;i++) {
                    free(pop_locations[i]);
                }
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
            k = 0;
            for (i = 0;i < fall_total;i++) {
                fall_data[i][0] += (int8_t) lower_byte(fall_data[i][2]);
                fall_data[i][1] += (fall_counter * 5) >> 2;
                    drawTile(grid.bubbles[fall_data[i][3]].color,fall_data[i][0],fall_data[i][1]);
            }
            fall_counter++;
            if (fall_counter == 25) {
                fall_counter = 0;
                fall_started = false;
                game_flags &= ~FALL;
                for(i = 0;i < fall_total;i++) {
                    grid.bubbles[fall_data[i][3]].flags &= ~FALLING;
                    free(fall_data[i]);
                }
                free(fall_data);
            }
        }
        if (game_flags & PUSHDOWN) {
            pushDown(&grid);
            game_flags &= ~PUSHDOWN;
        }
        //FPS
        gfx_PrintStringXY(fps_string,0,LCD_HEIGHT-8);
        if (++fps_counter == 7) {
            ticks = (float)atomic_load_increasing_32(&timer_1_Counter) / 32768;
            last_fps = fps;
            fps =  7.0 / ticks;
            strcpy(&fps_string[5],printfloat(fps));
            fps_counter = 0;
            timer_Control = TIMER1_DISABLE;
            timer_1_Counter = 0;
            timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_UP;
        }
        gfx_SetTextFGColor(0);
        gfx_BlitBuffer(); //render the whole thing
    }
    if (lost) {
        //init partial redraw
        k = 1;
        i = gfx_GetStringWidth(lose_string);
        lose_animation_behind = gfx_MallocSprite(i,8);
        point.x = (LCD_WIDTH>>1)-(i>>1);
        point.y = (LCD_HEIGHT>>1)-4;
        gfx_SetDrawScreen();
        gfx_GetSprite(lose_animation_behind,point.x,point.y);
        gfx_PrintStringXY(lose_string,point.x,point.y);
        while (k < 4) {
            timer_Control = TIMER1_DISABLE;
            timer_1_Counter = 0;
            timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_UP;
            while (timer_1_Counter < 2730);
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
        }
        while (!os_GetCSC());
    }
}
#endif
