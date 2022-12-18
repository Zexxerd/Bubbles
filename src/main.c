// Program Name: BUBBLES
// Author(s): Zexxerd
// Description: Bubbles!

/*TODO: Find source of memory leak bug*/
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
#include "gfx/bubble.h"

//matrix(x,y) = matrix((y * matrix_cols) + x)

#ifndef TILE_WIDTH
#define TILE_WIDTH 16
#endif
#ifndef TILE_HEIGHT
#define TILE_HEIGHT 16
#endif
#define LBOUND -64
#define RBOUND 64

/*Debug Macros*/
#ifndef gfx_PrintUIntXY
#define gfx_PrintUIntXY(i,length,x,y) gfx_SetTextXY(x,y);\
gfx_PrintUInt(i,length)
#endif
#ifndef gfx_PrintIntXY
#define gfx_PrintIntXY(i,length,x,y) gfx_SetTextXY(x,y);\
gfx_PrintInt(i,length)
#endif

extern uint8_t row_offset; // 0: even row; 1: odd row
extern uint8_t pop_counter;//timer for pop animation
extern bool debug_flag;
extern bubble_list_t * debug_foundcluster;
extern bubble_list_t * pop_cluster;

extern gfx_sprite_t * bubble_sprites[7];
extern const uint16_t bubble_colors[7];

extern bool pop_started;
extern int ** pop_locations;


bool debug_foundcluster_freed;
bool pop_cluster_freed;
bool pop_locations_freed;
//extern enum gamestate;
char * printfloat(float elapsed) {
    real_t elapsed_real;
    char * str;
    str = malloc(10);
    elapsed_real = os_FloatToReal(elapsed <= 0.001f ? 0.0f : elapsed);
    os_RealToStr(str, &elapsed_real, 8, 1, 2);
    return str;
}
int main(void) {
    uint8_t x,y;
    int i;
    
    int debug_foundcluster_timer = 0;
    uint8_t fps_counter;
    float fps,last_fps,ticks;
    uint8_t fps_ratio;
    char debug_string[40];
    char * fps_string;
    
    //point_t point;
    shooter_t * shooter;
    grid_t * grid;
    gfx_sprite_t * grid_buffer;
    bubble_list_t * neighbors;
    bubble_list_t * foundcluster;
    x = y = 0;
    pop_counter = 0;
    fps = last_fps = 30;
    fps_string = malloc(15);

    //allocate memory for shooter
    if ((shooter = (shooter_t *) malloc(sizeof(shooter_t))) == NULL) exit(1);
    dbg_printf("Location of shooter: %x\n",(unsigned int) &shooter); //debug
    shooter->x = 160 - (TILE_WIDTH>>1);
    shooter->y = 220 - (TILE_HEIGHT>>1);
    shooter->angle = 0;
    shooter->pal_index  = (sizeof_bubble_pal>>1) + 7;
    shooter->projectile = (projectile_t *) malloc(sizeof(projectile_t));
    shooter->projectile->speed = 5;
    
    //Allocate memory for grid
    grid = (grid_t *) malloc(sizeof(grid_t));
    if (grid == NULL) exit(1);
    dbg_printf("Location of grid: %x\n",(unsigned int) &grid); //debug
    grid->x = 160 - (((TILE_WIDTH * 7) + (TILE_WIDTH>>1))>>1);
    grid->y = 0;
    grid->ball_diameter = (TILE_WIDTH);
    grid->flags = RENDER;
    grid->cols = 7;
    grid->rows = 17; //16 + deadzone
    grid->width = (TILE_WIDTH * grid->cols) + (TILE_WIDTH>>1);
    grid->height = (ROW_HEIGHT * grid->rows) + (TILE_WIDTH>>2);
    grid->bubbles = (bubble_t *) malloc((grid->cols*grid->rows) * sizeof(bubble_t));
    if (grid->bubbles == NULL) exit(1);
    grid->score = 0;
    //declare an image buffer for the grid
    grid_buffer = gfx_MallocSprite(grid->cols * TILE_WIDTH + (TILE_WIDTH>>1),ROW_HEIGHT * grid->rows + (TILE_WIDTH>>2));
    if (grid_buffer == NULL) exit(1);
    
    srand(rtc_Time());
    initGrid(grid,grid->rows,grid->cols,10);
    gfx_palette[255] = 0xFFFF;
    gfx_Begin();
    gfx_SetDrawBuffer();
    gfx_SetPalette(bubble_pal,sizeof_bubble_pal,0);
    gfx_SetPalette(bubble_colors,14,sizeof_bubble_pal>>1);
    row_offset = 0;
    //x = y = 0;
    strcpy(fps_string,"FPS: ");
    fps_counter = 0;
    
    /*Initialize timer*/
    timer_Control = TIMER1_DISABLE;
    timer_1_Counter = 0;
    timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_UP;
    
    /*Main game*/
    while (!(kb_Data[6] & kb_Clear)) {
        if (grid->flags & RENDER) {
            //gfx_SetDrawBuffer();
            renderGrid(grid,grid_buffer);
            dbg_printf("Location of grid_buffer: %x\n\n",(unsigned int) &grid_buffer);
            grid->flags &= ~RENDER;
        }
        gfx_FillScreen(255);  //Change render method (speed!)
        kb_Scan();
        if (kb_Data[7] & kb_Left) {
            x -= (x > 0);
            if (shooter->angle > LBOUND)
                shooter->angle-=4;
        }
        if (kb_Data[7] & kb_Right) {
            x += (x < grid->cols-1);
            if (shooter->angle < RBOUND)
                shooter->angle+=4;
        }
        if (kb_Data[7] & kb_Up)
            y -= (y > 0);
        if (kb_Data[7] & kb_Down)
            y += (y < grid->rows-1);
        /*Shoot bubbles*/
        if (kb_Data[1] & kb_2nd) {
            if (!(shooter->flags & ACTIVE_PROJ)) {
                shooter->projectile->x = shooter->x;
                shooter->projectile->y = shooter->y;
                shooter->projectile->angle = shooter->angle;
                shooter->projectile->color = shooter->next_bubbles[0];
                shooter->next_bubbles[0] = shooter->next_bubbles[1];
                shooter->next_bubbles[1] = shooter->next_bubbles[2];
                shooter->next_bubbles[2] = randInt(0, MAX_COLOR);
                shooter->flags |= ACTIVE_PROJ;
            }
        }
        if (grid->flags & POP) {
            if (!pop_started) {
                pop_locations = (int **) malloc(sizeof(int**) * pop_cluster->size);
                dbg_getlocation(pop_locations);

                if (pop_locations == NULL) {
                    debug_message("pop_locations null!!!! :(") // add num of writes?
                    exit(1);
                }
                
                for (i = 0;i < pop_cluster->size;i++) {
                    pop_locations[i] = (int *) malloc(sizeof(int*) << 1); //x, y
                    //Debug: pop_locations
                    dbg_getlocation(pop_locations[i]);
                        if (pop_locations[i] == NULL) {
                        gfx_FillScreen(255);
                        gfx_SetTextScale(2,2);
                        sprintf(debug_string,"pop_locations[%d] null!!!!",i);
                        gfx_PrintStringXY(debug_string,0,0);
                        gfx_SetTextScale(1,1);
                        sprintf(debug_string,"size: %d",sizeof(int*)<<1);
                        gfx_PrintStringXY(debug_string,0,16);
                        gfx_BlitBuffer();
                        while(!os_GetCSC());
                        exit(1);
                    }
                    //debug
                    pop_locations[i][0] = TILE_WIDTH * pop_cluster->bubbles[i].x + grid->x;
                    pop_locations[i][1] = ROW_HEIGHT  * pop_cluster->bubbles[i].y + grid->y;
                }
                pop_started = true;
                dbg_printf("\n");
            }
            for (i = 0;i < pop_cluster->size;i++) {
                gfx_TransparentSprite(bubble_sprites[pop_cluster->bubbles[i].color],
                                      pop_locations[i][0] + pop_counter *
                                      (i%2 == 0 ? -1 : 1),
                                      pop_locations[i][1] + pop_counter *
                                      (i%2 == 0 ? -1 : 1));
            }
            pop_counter++;
            if (pop_counter == 20) {
                pop_counter = 0;
                pop_started = false; //remove
                grid->flags &= ~POP;
                free(pop_locations);
                free(pop_cluster);
                pop_locations = NULL;
                pop_cluster = NULL;
                pop_locations_freed = true;
            }
        }
        /*Debug: Show neighbors*/
        if (kb_Data[2] & kb_Alpha) {
            while (kb_Data[2] & kb_Alpha) kb_Scan();
            neighbors = getNeighbors(grid,grid->bubbles[(y*grid->cols) + x].x,grid->bubbles[(y*grid->cols) + x].y);
            
            gfx_SetColor(255);
            gfx_FillRectangle(0,16,100,neighbors->size<<3);
            gfx_PrintStringXY("Neighbors:",0,16);
            for (i = 0;i < neighbors->size;i++){
                sprintf(debug_string,"(%d,%d) %d",neighbors->bubbles[i].x,neighbors->bubbles[i].y,neighbors->bubbles[i].color);
                gfx_PrintStringXY(debug_string,gfx_GetStringWidth("Neighbors:"),16+(i<<3));
            }
            gfx_BlitBuffer();
            while(!(kb_Data[6] & kb_Enter)) kb_Scan();
            free(neighbors);
        }
        /*Debug: Foundcluster*/
        if (kb_Data[1] & kb_Mode) {
            while (kb_Data[1] & kb_Mode) kb_Scan();
            foundcluster = findCluster(grid,x,y,true,true,false);
            
            gfx_SetColor(255);
            gfx_FillRectangle(220,0,100,foundcluster->size<<3);
            gfx_PrintStringXY("Foundcluster:",220,0);
            for (i = 0;i < foundcluster->size;i++){
                sprintf(debug_string,"(%d,%d) (%d,%d)",foundcluster->bubbles[i].x,foundcluster->bubbles[i].y,foundcluster->bubbles[i].color,foundcluster->bubbles[i].flags);
                gfx_PrintStringXY(debug_string,220,16+(i<<4));
                gfx_TransparentSprite(bubble_sprites[foundcluster->bubbles[i].color],LCD_WIDTH-TILE_WIDTH,16+(i<<4));
            }
            gfx_BlitBuffer();
            while(!(kb_Data[6] & kb_Enter)) kb_Scan();
            free(foundcluster);
        }
        
        //gfx_FillScreen(255);  //Change render method (speed!)
        
        if (debug_foundcluster->size && debug_flag) {
            for (i = 0; i < debug_foundcluster->size; i++) {
                drawTile(debug_foundcluster->bubbles[i].color,320-TILE_WIDTH,i * TILE_HEIGHT);
            }
            if (debug_foundcluster_timer > 30) {
                free(debug_foundcluster);
                debug_foundcluster = NULL;
                debug_foundcluster_timer = 0;
                debug_flag = false;
            } else {
                debug_foundcluster_timer++;
            }
        }
            
        
        //Move the projectile
        if (shooter->flags & ACTIVE_PROJ) {
            fps_ratio = (uint8_t) fps/last_fps;
            fps_ratio = fps_ratio || 1;
            move_proj(grid,shooter,fps_ratio << 1);
                gfx_TransparentSprite(bubble_sprites[shooter->projectile->color], shooter->projectile->x, shooter->projectile->y);
        }
        //Display
        renderShooter(shooter);
        gfx_TransparentSprite(grid_buffer,grid->x,grid->y); //grid
        gfx_SetTextXY(0,32);
        gfx_PrintString("Score: ");
        gfx_SetTextXY(60,32);
        gfx_PrintInt(grid->score,5);
        gfx_PrintStringXY("X:",0,0);
        //Debug
        
        /*Draw a highlight for the highlighted square*/
        gfx_SetColor(5);
        gfx_FillRectangle((x*TILE_WIDTH)+(((y+row_offset) % 2)?(TILE_WIDTH>>1):0)+grid->x,(y*ROW_HEIGHT)+grid->y,TILE_WIDTH,TILE_HEIGHT);
        gfx_SetColor(0);
        gfx_Rectangle(grid->x,grid->y,grid->width,grid->height-ROW_HEIGHT);
        gfx_PrintStringXY("X:",0,0);
        gfx_PrintStringXY("Y:",0,8);
        gfx_PrintStringXY("TILE X:",0,16);
        gfx_PrintStringXY("TILE Y:",0,24);
        gfx_PrintUIntXY(x,2,20,0);
        gfx_PrintUIntXY(y,2,20,8);
        gfx_PrintUIntXY(grid->bubbles[y*grid->cols + x].x,2,60,16);
        gfx_PrintUIntXY(grid->bubbles[y*grid->cols + x].y,2,60,24);
        //Debug
        gfx_PrintStringXY(fps_string,0,232);
        fps_counter++;
        if (fps_counter == 7) {
            ticks = (float)atomic_load_increasing_32(&timer_1_Counter) / 32768;
            last_fps = fps;
            fps =  7 / ticks; //(float)fps_counter / ticks;
            strcpy(&fps_string[5],printfloat(fps));
            fps_counter = 0;
            timer_Control = TIMER1_DISABLE;
            timer_1_Counter = 0;
            timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_UP;
        }
        gfx_SetTextFGColor(0);
        gfx_BlitBuffer(); //render the whole thing
    }
    grid->flags = 0;
    shooter->flags = 0;
    shooter->projectile->x = shooter->projectile->y = shooter->projectile->speed = shooter->projectile->color = 0;
    free(grid->bubbles);
    grid->bubbles = NULL;
    free(grid);
    free(shooter->projectile);
    shooter->projectile = NULL;
    free(shooter);
    gfx_End();
}
