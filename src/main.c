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

#include "pixelate.h"
#include "bubble.h"
#include "level.h"
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
#define LBOUND -64
#define RBOUND 64

extern uint8_t row_offset; // 0: even row shifted; 1: odd row shifted
extern uint8_t max_color;

extern uint8_t game_flags; //global because our grid is no longer a pointer
extern unsigned int player_score;
extern unsigned int turn_counter;

extern bool debug_flag;

extern gfx_sprite_t * bubble_sprites[7];
extern gfx_sprite_t * bubble_pop_sprites[7];
extern const uint16_t bubble_colors[7];

extern bool pop_started;
extern int ** pop_locations;
extern bubble_list_t pop_cluster;
extern uint8_t pop_counter; // timer for pop animation

extern bool fall_started;
extern int ** fall_data;
extern int fall_total;
extern uint8_t fall_counter;

char * printfloat(float elapsed) {
    real_t elapsed_real;
    static char str[10];
    elapsed_real = os_FloatToReal(elapsed <= 0.001f ? 0.0f : elapsed);
    os_RealToStr(str, &elapsed_real, 8, 1, 2);
    return str;
}
int main(void) {
    uint8_t x,y;
    int i,j,k; //universal counter
    point_t point;

    uint8_t highlight_timer;
    uint8_t fps_counter;
    uint8_t fps_ratio;
    float fps, last_fps, ticks;
    char * fps_string;
#ifdef DEBUG
    int debug_fall_total;
    point_t debug_point;
    bubble_list_t neighbors;
    bubble_list_t foundcluster;
#endif //DEBUG
    
    shooter_t shooter;
    grid_t grid;
    gfx_sprite_t * grid_buffer;
    
    gfx_sprite_t * pop_sprite;
    gfx_sprite_t * pop_sprite_rotations[3];

    uint8_t * available_colors;
    max_color = 6;
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
    //declare image buffers for
    
    srand(rtc_Time());
    initGrid(grid,grid.rows,grid.cols,10);
    gfx_palette[255] = 0xFFFF;
    gfx_Begin();
    gfx_SetDrawBuffer();
    gfx_SetPalette(bubble_pal,sizeof_bubble_pal,0);
    gfx_SetPalette(bubble_colors,14,sizeof_bubble_pal>>1);
    row_offset = 0;
    
    x = y = 0;
    strcpy(fps_string,"FPS: ");
    fps_counter = 0;
    
    /*Initialize timer*/
    timer_Control = TIMER1_DISABLE;
    timer_1_Counter = 0;
    timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_UP;
    
    highlight_timer = 0;
    
    /*Main game*/
    
    while (!(kb_Data[6] & kb_Clear)) {
        if (game_flags & RENDER) {
            //gfx_SetDrawBuffer();
            renderGrid(grid,grid_buffer);
            //dbg_printf("Location of grid_buffer: %x\n\n",(unsigned int) &grid_buffer);
            game_flags &= ~RENDER;
        }
        gfx_FillScreen(255);  //Change render method (speed!)
        kb_Scan();
        if (kb_Data[7] & kb_Left) {
            x -= (x > 0);
            if (shooter.angle > LBOUND)
                shooter.angle-=4;
        }
        if (kb_Data[7] & kb_Right) {
            x += (x < grid.cols-1);
            if (shooter.angle < RBOUND)
                shooter.angle+=4;
        }
        if (kb_Data[7] & kb_Up)
            y -= (y > 0);
        if (kb_Data[7] & kb_Down)
            y += (y < grid.rows-1);
        /*Shoot bubbles*/
        if (kb_Data[1] & kb_2nd) {
            if (!(shooter.flags & ACTIVE_PROJ)) {
                shooter.projectile.x = shooter.x;
                shooter.projectile.y = shooter.y;
                shooter.projectile.angle = shooter.angle;
                shooter.projectile.color = shooter.next_bubbles[0];
                shooter.next_bubbles[0] = shooter.next_bubbles[1];
                shooter.next_bubbles[1] = shooter.next_bubbles[2];
                available_colors = getAvailableColors(grid);
                shooter.next_bubbles[2] = available_colors[1 + randInt(0, available_colors[0]-1)];
#ifdef DEBUG
                gfx_FillScreen(255);
                gfx_PrintStringXY("Size: ",0,0);
                gfx_PrintUIntXY(available_colors[0],8,48,0);
                for (j = 0;j < available_colors[0];j++) {
                    gfx_PrintUIntXY(available_colors[j+1],8,120,16+j*8);
                }
                gfx_BlitBuffer();
                while(!os_GetCSC());
#endif //DEBUG
                shooter.flags |= ACTIVE_PROJ;
                shooter.projectile.visible = true;
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
                for (i = 0;i < fall_total;i++) {
                    dbg_printf("color of #%d: %d\n",i,grid.bubbles[fall_data[i][3]].color);
                }
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
#ifdef DEBUG
        /*Debug: Show neighbors*/
        if (kb_Data[2] & kb_Alpha) {
            while (kb_Data[2] & kb_Alpha) kb_Scan();
            neighbors = getNeighbors(grid,grid.bubbles[(y*grid.cols) + x].x,grid.bubbles[(y*grid.cols) + x].y,false);
            
            gfx_SetColor(255);
            gfx_FillRectangle(0,16,100,neighbors.size<<3);
            gfx_PrintStringXY("Neighbors:",0,16);
            for (i = 0;i < neighbors.size;i++) {
                //sprintf(debug_string,"(%d,%d) %d",neighbors.bubbles[i].x,neighbors.bubbles[i].y,neighbors.bubbles[i].color);
                debug_point.x = gfx_GetStringWidth("Neighbors:");
                debug_point.y = 16+(i<<3);
                gfx_PrintStringXY("(",debug_point.x,debug_point.y);
                gfx_PrintUIntXY(neighbors.bubbles[i].x,2,debug_point.x+8,debug_point.y);
                gfx_PrintStringXY(",",debug_point.x+24,debug_point.y);
                gfx_PrintUIntXY(neighbors.bubbles[i].y,2,debug_point.x+40,debug_point.y);
                gfx_PrintStringXY(")",debug_point.x+56,debug_point.y);
                gfx_PrintUIntXY(neighbors.bubbles[i].color,2,debug_point.x+64,debug_point.y);
            }
            gfx_BlitBuffer();
            while(!os_GetCSC());
        }
        /*Debug: Foundcluster*/
        if (kb_Data[1] & kb_Mode) {
            while (kb_Data[1] & kb_Mode) kb_Scan();
            foundcluster = findCluster(grid,x,y,true,true,false);
            gfx_SetColor(255);
            gfx_FillRectangle(220,0,100,foundcluster.size<<3);
            gfx_PrintStringXY("Foundcluster:",220,0);
            for (i = 0;i < foundcluster.size;i++){
                debug_point.x = 220;
                debug_point.y = 16+(i<<4);
                gfx_PrintUIntXY(foundcluster.bubbles[i].x,2,debug_point.x,debug_point.y);
                gfx_PrintUIntXY(foundcluster.bubbles[i].y,2,debug_point.x+24,debug_point.y);
                gfx_PrintUIntXY(foundcluster.bubbles[i].color,2,debug_point.x+56,debug_point.y);
                gfx_TransparentSprite(bubble_sprites[foundcluster.bubbles[i].color],LCD_WIDTH-TILE_WIDTH,16+(i<<4));
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
#endif //DEBUG
        //Move the projectile
        if (shooter.flags & ACTIVE_PROJ) {
            fps_ratio = (uint8_t) (fps/last_fps);
            fps_ratio = fps_ratio || 1;
            moveProj(grid,&shooter,fps_ratio << 1);
            gfx_TransparentSprite(bubble_sprites[shooter.projectile.color], shooter.projectile.x, shooter.projectile.y);
        }
        //Display
        renderShooter(shooter);
        gfx_TransparentSprite(grid_buffer,grid.x,grid.y); //grid
        gfx_SetColor(0);
        gfx_Rectangle(grid.x,grid.y,grid.width,grid.height-ROW_HEIGHT);
        gfx_SetTextXY(0,32);
        gfx_PrintString("Score: ");
        gfx_SetTextXY(60,32);
        gfx_PrintInt(player_score,5);
        
#ifdef DEBUG
        /*Draw a highlight for the highlighted square*/
        if ((highlight_timer-1) & 1) {
            gfx_SetColor(5);
            debug_point = getTileCoordinate(x,y);
            gfx_FillRectangle(grid.x+debug_point.x,grid.y+debug_point.y,TILE_WIDTH,TILE_HEIGHT);
            drawTile(grid.bubbles[y * grid.cols + x].color,grid.x+debug_point.x,debug_point.y);
        }
        highlight_timer++;
        gfx_PrintStringXY("X:",0,0);
        gfx_PrintStringXY("Y:",0,8);
        gfx_PrintUIntXY(x,2,20,0);
        gfx_PrintUIntXY(y,2,20,8);
        gfx_PrintStringXY("Turn:",0,24);
        gfx_PrintUIntXY(turn_counter,3,48,24);
        if (game_flags & RENDER) {
            gfx_PrintStringXY("game:RENDER",0,40);
        }
        if (game_flags & SHIFT) {
            gfx_PrintStringXY("game:SHIFT",0,48);
        }
        if (game_flags & POP) {
            gfx_PrintStringXY("game:POP",0,56);
        }
        if (game_flags & FALL) {
            gfx_PrintStringXY("game:FALL",0,64);
        }
#endif //DEBUG
        //FPS
        gfx_PrintStringXY(fps_string,0,232);
        fps_counter++;
        if (fps_counter == 7) {
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
    game_flags = 0;
    shooter.flags = 0;
    shooter.projectile.x = shooter.projectile.y = shooter.projectile.speed = shooter.projectile.color = 0;
    free(grid.bubbles);
    gfx_End();
}
