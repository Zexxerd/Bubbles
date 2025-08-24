#if !defined(BUBBLE_H)
#define BUBBLE_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <graphx.h>
#include "gfx/bubble.h"

//Constants
#define WHITE 0xFFFF
#define BLACK 0x0000


#define TILE_WIDTH 16
#define TILE_HEIGHT 16
#define TILE_WIDTH1_5 ((TILE_WIDTH >> 1) * 3)
#define TILE_HEIGHT1_5 ((TILE_HEIGHT >> 1) * 3)

#define LBOUND -64
#define RBOUND 64
#define SHOOTER_STEP 4

#define matrixIndex(x, y, columns) ((y * columns) + x)
#define getRangeIndex(angle, lower, step) ((angle - lower) / step)

#define ROW_HEIGHT ((TILE_HEIGHT>>1)+(TILE_HEIGHT>>2)) // 3/4 of the tile height
#define MAX_POSSIBLE_COLOR 6 // highest possible index of max_color

//degrees to radians
#define deg(a) (a * (180 / M_PI))
#define rad(a) (a * (M_PI / 180))

//extract byte from int
#define low_byte(x) (x & 255)
#define mid_byte(x) ((x >> 8) & 255)
#define high_byte(x) ((x >> 16) & 255)

//tiles
#define PROCESSED (1<<0)
#define REMOVED   (1<<1)
#define EMPTY     (1<<2)
#define POPPING   (1<<3)
#define FALLING   (1<<4)

//animated tiles
#define ACTIVE    (1<<0)

//shooter
#define ACTIVE_PROJ    (1<<0)
#define DEACTIVATED    (1<<1)
#define REDRAW_SHOOTER (1<<2)
#define SHAKE (1<<3)


//grid
#define RENDER    (1<<0) // redraw grid
#define SHIFT     (1<<1) // the grid goes down one
#define NEW_ROW   (1<<1) // ^^
#define POP       (1<<2) // Bubbles are popping.
#define FALL      (1<<3) // Falling bubbles, wear a helmet
#define PUSHDOWN  (1<<4) // Level has run out of bubbles, or push interval has passed, go down
#define NEW_LEVEL (1<<5) // New level
#define CHECK     (1<<6) // Check if bubbles have overflowed

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
#ifdef DEBUG
#define debug_message(message)  \
gfx_FillScreen(255);             \
gfx_PrintStringXY(message,0,116); \
gfx_BlitBuffer();                  \
while (!os_GetCSC())
#else
#define debug_message(message) do {} while (0)
#endif

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
    uint8_t flags; // NULL,NULL,NULL,falling?,popping?,empty?,removed?,processed?
} bubble_t; //this should be used in grid_t
typedef struct falling_bubble {
    unsigned int x;
    unsigned int y;
    uint8_t color;
    int8_t velocity; //flags i guess
} falling_bubble_t;
typedef struct bubble_list {
    int size;
    bubble_t * bubbles;
} bubble_list_t;
typedef struct falling_bubble_list {
    int size;
    falling_bubble_t * bubbles;
} falling_bubble_list_t;

typedef struct grid {
    int x, y;  // left of level, top of level
    //uint8_t flags;
    //unsigned int score; //NOTE: create 48-bit type?
    uint8_t ball_diameter;
    int width, height; //col * TILE_WIDTH, row * ROW_HEIGHT
    uint8_t cols;
    uint8_t rows;
    bubble_t * bubbles;
    bubble_list_t possible_collisions;
} grid_t;

typedef struct point {
    int x, y;
} point_t;

typedef struct projectile {
    float x, y;
    uint8_t speed;
    int angle;
    uint8_t color;
    bool visible;
} projectile_t;

typedef struct shooter {
    int x,y; //this is the top left corner, add TILE_(WIDTH or HEIGHT)/2 for center
    int angle; // 0 is up, <0 is left, >0 is right
    projectile_t projectile; //may allow for multiple later
    uint8_t pal_index;
    uint8_t next_bubbles[3];
    uint8_t flags; // NULL,NULL,NULL,NULL,Shake?,Redraw?,Deactivate shooter?,Active projectile?
    float ** vectors;
    uint8_t counter;
    int8_t shake_values; //lower 4 bits are x, upper 4 bits are y
} shooter_t;

char * uint8_to_bin(uint8_t n);

bubble_t newBubble(uint8_t x,uint8_t y,uint8_t color,uint8_t flags);
bubble_list_t copyBubbleList(bubble_list_t original);
bubble_list_t * addToBubbleList(bubble_list_t * dest, bubble_list_t * src);
void setAvailableColors(uint8_t * target,uint8_t colors);
uint8_t * getAvailableColors(grid_t grid, uint8_t * buffer);
void drawTile(uint8_t color,int x,int y);
point_t getTileCoordinate(int col,int row);
point_t getGridPosition(int x,int y);
void initGrid(grid_t grid,uint8_t rows,uint8_t cols,uint8_t empty_row_start,uint8_t * available_colors);
void addNewRow(grid_t grid,uint8_t * available_colors,uint8_t chance);
void pushDown(grid_t * grid);
void renderGrid(grid_t grid,gfx_sprite_t * grid_buffer);
float ** generateVectors(int8_t lower,int8_t higher,uint8_t step);
void renderShooter(shooter_t shooter);
bool collide(float x1,float y1,float x2,float y2,uint8_t r);
void moveProj(grid_t grid,shooter_t * shooter,float dt);
void snapBubble(shooter_t * shooter,grid_t grid, float dt);
void resetProcessed(grid_t grid);
bubble_list_t getNeighbors(grid_t grid, uint8_t tilex, uint8_t tiley,bool add_empty);
bubble_list_t findCluster(grid_t grid,uint8_t tile_x,uint8_t tile_y,bool matchtype,bool reset,bool skipremoved);
int findFloatingClusters(grid_t grid);
bubble_list_t getPossibleCollisions(grid_t grid);
#endif // BUBBLE_H
