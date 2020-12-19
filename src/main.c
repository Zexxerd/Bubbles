// Program Name: BUBBLES
// Author(s): SomeCoolGuy
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
#ifndef TILE_WIDTH
#define TILE_WIDTH 16
#endif
#ifndef TILE_HEIGHT
#define TILE_HEIGHT 16
#endif
#define LBOUND -84
#define RBOUND 84
#define gfx_PrintUIntXY(i,length,x,y) gfx_SetTextXY(x,y);\
gfx_PrintUInt(i,length)
#define gfx_PrintIntXY(i,length,x,y) gfx_SetTextXY(x,y);\
gfx_PrintInt(i,length)
extern uint8_t row_offset;
extern gfx_sprite_t * bubble_sprites[7];
extern const uint16_t bubble_colors[7];
//extern enum gamestate;
void main(void) {
    int x,y,i;
    point_t point;
    shooter_t * shooter;
    grid_t * grid;
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
    grid->ball_radius = TILE_WIDTH>>1;
    grid->cols = 7;
    grid->rows = 14;
    grid->width = (TILE_WIDTH * grid->cols) + (TILE_WIDTH>>1);
    grid->height = ROW_HEIGHT * grid->rows + (TILE_WIDTH>>2);//(TILE_HEIGHT * grid->rows) - ((TILE_WIDTH>>2) * (grid->rows - 1));
    grid->bubbles = (bubble_t *) malloc(98 * sizeof(bubble_t)); //4*98 = 392
    if (grid->bubbles == NULL) exit(1);
    srand(rtc_Time());
    init_grid(grid);
    gfx_palette[255] = 0xFFFF;
    gfx_Begin();
    gfx_SetDrawBuffer();
    gfx_SetPalette(bubble_pal,78,0);
    row_offset = 0;
    x = y = 0;
    //
    for (i=0;i<grid->rows*grid->cols;i++) {
        gfx_FillScreen(255);
        gfx_PrintStringXY("Index color",72,0);
        gfx_PrintStringXY("Index flags (4=EMPTY,Other=ERROR)",72,8);
        gfx_PrintStringXY("Index",72,16);
        gfx_PrintStringXY("MAX_COLOR",72,24);
        gfx_PrintUIntXY(grid->bubbles[i].color,8,0,0);
        gfx_PrintUIntXY(grid->bubbles[i].flags,8,0,8);
        gfx_PrintUIntXY(i,3,0,16);
        gfx_PrintUIntXY(MAX_COLOR,3,0,24);
        gfx_BlitBuffer();
        while(!os_GetCSC());
    }
    gfx_End();
    exit(1);
    //
    while (!(kb_Data[6] & kb_Clear)) {
        kb_Scan();
        gfx_FillScreen(255);
        if (kb_Data[7] & kb_Left) {
            x--;
            if (shooter->angle > LBOUND)
                shooter->angle-=4;
        }
        if (kb_Data[7] & kb_Right) {
            x++;
            if (shooter->angle < RBOUND)
                shooter->angle+=4;
        }
        if (kb_Data[7] & kb_Up)
            y--;
        if (kb_Data[7] & kb_Down)
            y++;
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
                //memcpy((void *)shooter->next_bubbles,((void *)shooter->next_bubbles) + 1, 2);
                shooter->flags |= ACTIVE_PROJ;
            }
        }
        if (shooter->flags & ACTIVE_PROJ) {
            move_proj(grid,shooter,1);
            //gfx_TransparentSprite(bubble_sprites[shooter->projectile->color], shooter->projectile->x, shooter->projectile->y);
            gfx_PrintIntXY(shooter->projectile->x,3,0,24);
            gfx_PrintIntXY(shooter->projectile->y,3,32,24);
            /*gfx_PrintIntXY(shooter->projectile->angle,3,0,8);*/
            gfx_TransparentSprite(bubble_sprites[shooter->projectile->color], shooter->projectile->x, shooter->projectile->y);
        }

        point = getGridPosition(x,y);
        gfx_PrintIntXY(x,3,0,0);
        gfx_PrintIntXY(y,3,32,0);
        gfx_PrintIntXY(point.x,3,0,8);
        gfx_PrintIntXY(point.y,3,32,8);
        renderShooter(shooter);
        renderGrid(grid);
        gfx_SetColor(0);
        gfx_Rectangle(grid->x,grid->y,grid->width,grid->height);
        gfx_PrintIntXY(shooter->angle,3,0,32);
        gfx_PrintIntXY(shooter->flags,3,32,32);
        gfx_BlitBuffer();
    }
    free(grid);
    free(shooter->projectile);
    gfx_End();
}
