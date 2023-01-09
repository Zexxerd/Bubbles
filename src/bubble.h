#if !defined(BUBBLE_H)
#define BUBBLE_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <graphx.h>
#include "gfx/bubble.h"

//Constants
#define TILE_WIDTH 16
#define TILE_HEIGHT 16
#define ROW_HEIGHT ((TILE_HEIGHT>>1)+(TILE_HEIGHT>>2)) // 3/4 of the tile height

//degrees to radians
#define deg(a) (a * (180 / M_PI))
#define rad(a) (a * (M_PI / 180))

//extract byte from int
#define lower_byte(x) (x & 255)
#define mid_byte(x) ((x >> 8) & 255)
#define high_byte(x) ((x >> 16) & 255)

//tiles
#define PROCESSED (1<<0)
#define REMOVED   (1<<1)
#define EMPTY     (1<<2)
#define POPPING   (1<<3)
#define FALLING   (1<<4)

//shooter
#define ACTIVE_PROJ (1<<0)
#define DEACTIVATED (1<<1)

//grid
#define RENDER   (1<<0) // redraw grid
#define SHIFT    (1<<1) // the grid goes down one
#define NEW_ROW  (1<<1) // ^^
#define POP      (1<<2) // Bubbles are popping.
#define FALL     (1<<3) // Falling bubbles, wear a helmet

/*debug macros*/
#define gfx_PrintUIntXY(i,length,x,y) do { \
gfx_SetTextXY(x,y);                         \
gfx_PrintUInt(i,length);                     \
} while (0)
#define gfx_PrintIntXY(i,length,x,y)  do { \
gfx_SetTextXY(x,y);                         \
gfx_PrintInt(i,length);                      \
} while (0)
//basic debug message
#define debug_message(message)  \
gfx_FillScreen(255);             \
gfx_PrintStringXY(message,0,116); \
gfx_BlitBuffer();                  \
while (!os_GetCSC())

//explicitly associated with debug.h
#define dbg_getlocation(pointer) do {\
dbg_printf("Location of " #pointer ": %x\n",(unsigned int) &pointer);\
dbg_printf("Location of " #pointer " memory: %x\n",(unsigned int) pointer);\
} while (0)

//#define debug_flag() debug_flag = true //trashy, I know...

/*enum * bubble_colors {
    red = 0x7c00,
    orange = 0x7e00,
    yellow = 0x7fe0,
    green = 0x8992,
    blue = 0x001f,
    purple = 0x401f,
    violet = 0x7c1f,
}*/

typedef struct bubble {
    uint8_t x,y; //col,row
    uint8_t color;
    uint8_t flags; // NULL,NULL,NULL,NULL,NULL,empty?,removed?,processed?
} bubble_t; //this should be used in grid_t
typedef struct bubble_list {
    int size;
    bubble_t * bubbles;
} bubble_list_t;
typedef struct grid {
    int x, y;  // left of level, top of level
    uint8_t flags;
    unsigned int score; //NOTE: create 48-bit type?
    uint8_t ball_diameter;
    int width, height; //col * TILE_WIDTH, row * ROW_HEIGHT
    uint8_t cols;
    uint8_t rows;
    bubble_t * bubbles;
} grid_t;

typedef struct point {
    int x, y;
} point_t;

typedef struct projectile {
    float x, y;
    uint8_t speed;
    uint8_t color;
    bool visible;
    int angle;
} projectile_t;

typedef struct shooter {
    int x,y; //this is the top left corner, add TILE_(WIDTH or HEIGHT)/2 for center
    int angle;
    projectile_t projectile; //may allow for multiple later
    uint8_t pal_index;
    uint8_t next_bubbles[3];
    uint8_t flags; // NULL,NULL,NULL,NULL,NULL,NULL,Deactivate shooter?,Active projectile?
} shooter_t;

char * uint8_to_bin(uint8_t n);

bubble_t newBubble(uint8_t x,uint8_t y,uint8_t color,uint8_t flags);
bubble_list_t copyBubbleList(bubble_list_t original);
uint8_t * getAvailableColors(grid_t grid);
void drawTile(uint8_t color,int x,int y);
point_t getTileCoordinate(int col,int row);
point_t getGridPosition(int x,int y);
void initGrid(grid_t grid,uint8_t rows,uint8_t cols,uint8_t empty_row_start);
void renderGrid(grid_t grid,gfx_sprite_t * grid_buffer);
void renderShooter(shooter_t shooter);
bool collide(float x1,float y1,float x2,float y2,uint8_t r);
void snapBubble(projectile_t * projectile,grid_t grid);
void move_proj(grid_t grid,shooter_t * shooter,float dt);
void resetProcessed(grid_t grid);
bubble_list_t getNeighbors(grid_t grid, uint8_t tilex, uint8_t tiley,bool check_empty);
bubble_list_t findCluster(grid_t grid,uint8_t tile_x,uint8_t tile_y,bool matchtype,bool reset,bool skipremoved);
int findFloatingClusters(grid_t grid);

#endif // BUBBLE_H
