#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <graphx.h>
#include "gfx/bubble.h"
#define TILE_WIDTH 16
#define TILE_HEIGHT 16
#define MAX_COLOR 6
#define ROW_HEIGHT ((TILE_HEIGHT>>1)+(TILE_HEIGHT>>2)) // 3/4 of the tile height
#define deg(a) (a * (180 / M_PI))
#define rad(a) (a * (M_PI / 180))
//tiles
#define PROCESSED (1<<0)
#define REMOVED   (1<<1)
#define EMPTY     (1<<2)
//shooter
#define ACTIVE_PROJ (1<<0)
#define DEACTIVATED (1<<1)
#define gfx_PrintIntXY(i,length,x,y) gfx_SetTextXY(x,y);\
gfx_PrintInt(i,length)
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
} bubble_t;
typedef struct bubble_list {
    int size;
    bubble_t * bubbles;
} bubble_list_t;
typedef struct grid {
    int x, y;  // left of level, top of level
    uint8_t ball_radius;
    int width, height; //col * TILE_WIDTH, row * ROW_HEIGHT
    uint8_t cols;
    uint8_t rows;
    bubble_t * bubbles;
} grid_t;

typedef struct point {
    int x,y;
} point_t;

typedef struct projectile {
    float x,y;
    uint8_t speed;
    uint8_t color;
    int angle;
} projectile_t;

typedef struct shooter {
    int x,y; //center of shooter is half a tile added to these.
    int angle;
    projectile_t * projectile;
    uint8_t pal_index;
    uint8_t next_bubbles[3];
    uint8_t flags; // NULL,NULL,NULL,NULL,NULL,NULL,Deactivate shooter?,Active projectile?
} shooter_t;

char * uint8_to_bin(uint8_t n);
bubble_t newBubble(uint8_t x,uint8_t y,uint8_t color,uint8_t flags);
void drawTile(uint8_t color,int x,int y);
point_t getTileCoordinate(int col,int row);
point_t getGridPosition(int x,int y);
void init_grid(grid_t * grid,uint8_t rows,uint8_t cols,uint8_t empty_row_start);
void renderGrid(grid_t * grid);
void renderShooter(shooter_t * shooter);
bool collide(double x1,double y1,double x2,double y2,uint8_t r);
void snapBubble(projectile_t * projectile,grid_t * grid);
void move_proj(grid_t * grid,shooter_t * shooter,uint8_t dt);
void resetProcessed(grid_t * grid);
bubble_list_t * getNeighbors(grid_t * grid,bubble_t tile);
bubble_list_t * findCluster(grid_t * grid,uint8_t tile_x,uint8_t tile_y,bool matchtype,bool reset,bool skipremoved);
bubble_list_t * findFloatingClusters(grid_t * grid);
#undef gfx_PrintIntXY
