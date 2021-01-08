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

extern uint8_t row_offset;
extern bool debug_flag;
extern gfx_sprite_t * bubble_sprites[7];
extern const uint16_t bubble_colors[7];
//extern enum gamestate;
char * printfloat(float elapsed) {
    real_t elapsed_real;
    char * str;
    str = malloc(10);
    elapsed_real = os_FloatToReal(elapsed <= 0.001f ? 0.0f : elapsed);
    os_RealToStr(str, &elapsed_real, 8, 1, 2);
    return str;
}
void main(void) {
    int x,y,i;
    uint8_t fps_counter;
    float fps,ticks;
    //char debug_string[21];
    char * fps_string;
    point_t point;
    shooter_t * shooter;
    grid_t * grid;
    fps = 0;
    fps_string = malloc(15);
    shooter = (shooter_t *) malloc(sizeof(shooter_t));
    shooter->x = 160 - (TILE_WIDTH>>1);
    shooter->y = 220 - (TILE_HEIGHT>>1);
    shooter->angle = 0;
    shooter->pal_index = 39;
    shooter->projectile = (projectile_t *) malloc(sizeof(projectile_t));
    shooter->projectile->speed = 5;
    grid = (grid_t *) malloc(sizeof(grid_t));
    if (grid == NULL) exit(1);
    grid->x = 160 - (((TILE_WIDTH * 7) + (TILE_WIDTH>>1))>>1);
    grid->y = 0;
    grid->ball_radius = 8;
    grid->cols = 7;
    grid->rows = 16;
    grid->width = (TILE_WIDTH * grid->cols) + (TILE_WIDTH>>1);
    grid->height = ROW_HEIGHT * grid->rows + (TILE_WIDTH>>2);
    grid->bubbles = (bubble_t *) malloc((grid->cols*grid->rows) * sizeof(bubble_t));
    if (grid->bubbles == NULL) exit(1);
    srand(rtc_Time());
    init_grid(grid,grid->rows,grid->cols,10);
    gfx_palette[255] = 0xFFFF;
    gfx_Begin();
    gfx_SetDrawBuffer();
    gfx_SetPalette(bubble_pal,78,0);
    row_offset = 0;
    //x = y = 0;
    strcpy(fps_string,"FPS: ");
    fps_counter = 0;
    timer_Control = TIMER1_DISABLE;
    timer_1_Counter = 0;
    timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_UP;
    while (!(kb_Data[6] & kb_Clear)) {
        kb_Scan();
        gfx_FillScreen(255);
        if (kb_Data[7] & kb_Left) {
//            x--;
            if (shooter->angle > LBOUND)
                shooter->angle-=4;
        }
        if (kb_Data[7] & kb_Right) {
//            x++;
            if (shooter->angle < RBOUND)
                shooter->angle+=4;
        }
        if (kb_Data[7] & kb_Up)
//            y--;
        if (kb_Data[7] & kb_Down)
//            y++;
        if (kb_Data[2] & kb_Alpha)
            row_offset ^= 1;
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
        if (shooter->flags & ACTIVE_PROJ) {
            move_proj(grid,shooter,2);
            gfx_TransparentSprite(bubble_sprites[shooter->projectile->color], shooter->projectile->x, shooter->projectile->y);
        }
        
        renderShooter(shooter);
        renderGrid(grid);
        gfx_SetColor(0);
        gfx_Rectangle(grid->x,grid->y,grid->width,grid->height);
        gfx_PrintStringXY(fps_string,0,232);
        fps_counter++;
        if (fps_counter == 15) {
            ticks = (float)atomic_load_increasing_32(&timer_1_Counter) / 32768;
            fps = (float) fps_counter / ticks;
            strcpy(&fps_string[5],printfloat(fps));
            fps_counter = 0;
            timer_Control = TIMER1_DISABLE;
            timer_1_Counter = 0;
            timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_UP;
        }
        gfx_BlitBuffer();
/*        if (debug_flag) {
            timer_Control = TIMER1_DISABLE; //Just a pause
            while(os_GetCSC());
            while(!os_GetCSC());
            debug_flag = false;
            timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_UP;
        }*/
    }
    free(grid->bubbles);
    grid->bubbles = NULL;
    free(grid);
    free(shooter->projectile);
    shooter->projectile = NULL;
    free(shooter);
    gfx_End();
}
