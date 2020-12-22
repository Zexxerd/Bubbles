#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <graphx.h>
#include "bubble.h"
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
/*Globals*/
uint8_t row_offset; // 0: even row; 1: odd row
int array_size; //holds foundclusters_size
//enum gamestate current_gamestate;
enum gamestate{
    game_over = 0,
    neutral = 1,
    shoot = 2,
    pop = 3
};
gfx_sprite_t * bubble_sprites[7] = {
    bubble_red,
    bubble_orange,
    bubble_yellow,
    bubble_green,
    bubble_blue,
    bubble_purple,
    bubble_violet
};

int8_t neighbor_offsets[2][6][2] = {{{1, 0}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}},  {{1, 0}, {1, 1}, {0, 1}, {-1, 0}, {0, -1}, {1, -1}}}; //(even, odd),(neighbors),(x,y)

const uint16_t bubble_colors[7] = {
    0x7c00, //red
    0x7e00, //orange
    0x7fe0, //yellow
    0x8992, //green
    0x001f, //blue
    0x401f, //purple
    0x7c1f, //violet
};

/*Functions*/
bubble_t newBubble(uint8_t x,uint8_t y,uint8_t color,uint8_t flags) {
    bubble_t bubble;
    bubble.x = x;
    bubble.y = y;
    bubble.color = color;
    bubble.flags = flags;
    if (bubble.color != color) exit(1);
    return bubble;
}
void drawTile(uint8_t color,int x,int y) {
    if (color > MAX_COLOR) return;
    gfx_TransparentSprite(bubble_sprites[color],x,y);
}
point_t getTileCoordinate(int col,int row) {
    point_t temp;
    temp.x = col * TILE_WIDTH;
    if ((row + row_offset) % 2)
        temp.x += TILE_WIDTH>>1;
    temp.y = row * ROW_HEIGHT;
    return temp;
}
point_t getGridPosition(int x,int y) {
    point_t temp;
    uint8_t xoffset = 0;
    temp.y = y / ROW_HEIGHT;
    if ((y + row_offset) % 2)
        xoffset = TILE_WIDTH >> 1;
    temp.x = (x - xoffset) / TILE_WIDTH;
    return temp;
}
void init_grid(grid_t * grid){
    uint8_t i,j,r;
    point_t p = {0, 0};//getGridPosition(grid->x,grid->y);
//    int grid_size = grid->cols*grid->rows;
    for (i=p.x;i<p.x+grid->rows;i++) {
        for (j=p.y;j<p.y+grid->cols;j++) {
            if (randInt(0,1)) {
                r++;//randInt(0,MAX_COLOR);
                //r &= 7;
                //if (r == 7) r >>= 1;
                grid->bubbles[(i*grid->rows)+j] = newBubble(j,i,r % 7,0);
            } else {
                grid->bubbles[(i*grid->rows)+j] = newBubble(j,i,0,EMPTY);
            }
        }
    }
}
void renderGrid(grid_t * grid) {
    int i,j;
    bubble_t tile;
    point_t coord;
    for (i = 0;i < grid->rows;i++) {
        for (j = 0;j < grid->cols;j++) {
            tile = grid->bubbles[(i*grid->cols)+j];
            if (tile.flags & EMPTY) continue;
            coord = getTileCoordinate(j,i);
            coord.x += grid->x;
            coord.y += grid->y;
            drawTile(tile.color,coord.x,coord.y);
        }
    }
}
void renderShooter(shooter_t * shooter) {
    point_t center;
    center.x = shooter->x + (TILE_WIDTH >> 1);
    center.y = shooter->y + (TILE_HEIGHT >> 1);
    gfx_palette[shooter->pal_index] = gfx_Darken(bubble_colors[shooter->next_bubbles[0]],255&(rtc_Time()%3));
    gfx_SetColor(shooter->pal_index);
    //gfx_FillCircle(center.x,center.y,TILE_WIDTH>>1);
    gfx_Line(center.x,center.y,center.x + ((double)1.5 * TILE_WIDTH) * sin(rad((double)shooter->angle)),center.y - ((double)1.5 * TILE_HEIGHT) * cos(rad((double)shooter->angle)));
    gfx_palette[shooter->pal_index] = bubble_colors[shooter->next_bubbles[0]];
    gfx_TransparentSprite(bubble_sprites[shooter->next_bubbles[0]],shooter->x,shooter->y);
}
/*bool collide(x1,y1,r1,x2,y2,r2) {
    float dx,dy,dist;
    dx = x2 - x1;
    dy = y2 - y1;
    dist = sqrt((dx*dx) + (dy+dy));
    return (dist < r1+r2);
}*/
bool collide(double x1,double y1,double x2,double y2,uint8_t r) {
    double dx,dy,dist;
    dx = x2 - x1;
    dy = y2 - y1;
    dist = sqrt((dx*dx) + (dy+dy));
    return (dist < r);
}
void snapBubble(projectile_t * projectile,grid_t * grid) {
    bool addtile;
    uint8_t i;
    int index;
    bubble_list_t * cluster;
    point_t gridpos = getGridPosition(((int)projectile->x) + (TILE_WIDTH>>1) - grid->x,((int)projectile->y) + (TILE_HEIGHT>>1) - grid->y);
    //if gridpos is outside of the grid, force it in.
    if (gridpos.x < 0) {
        gridpos.x = 0;
    }
    if ((gridpos.x + TILE_WIDTH) >= grid->cols) {
        gridpos.x = 0;
    }
    if (gridpos.y < 0) {
        gridpos.y = 0;
    }
    if ((gridpos.y + TILE_HEIGHT) >= grid->rows) {
        gridpos.y = grid->rows - 1;
    }
    //gfx_PrintIntXY(gridpos.x,3,0,48);
    //gfx_PrintIntXY(gridpos.y,3,32,48);
    //gfx_PrintIntXY(projectile->x,3,0,60);
    //gfx_PrintIntXY(projectile->y,3,32,60);
    //gfx_BlitBuffer();
    //while(!os_GetCSC());
//    if (gridpos.y == grid->rows) {
//        grid->rows++;
//        grid->bubbles = realloc(grid->bubbles,grid->cols*grid->rows * sizeof(bubble_t));
//        if (grid->bubbles == NULL) exit(1);
//    }
    addtile = false;
    index = gridpos.y*grid->cols + grid->x;
    if (!(grid->bubbles[index].flags & EMPTY)) {
        for (i = gridpos.y+1;i < grid->rows; i++) {
            if ((grid->bubbles[(index += grid->cols)].flags) & EMPTY) {
                gridpos.y = i;
                addtile = true;
                break;
            }
        }
    } else {
        addtile = true;
    }
    if (addtile) {
        index = gridpos.y * grid->cols + grid->x;
        grid->bubbles[index].flags &= ~(EMPTY);
        grid->bubbles[index].color = projectile->color;
        /*cluster = findCluster(grid,gridpos.x, gridpos.y, true, true, false);
        if (cluster->size >= 3) {
            // Remove the cluster
            gamestate = pop;
            return;
        }*/
    }
}
#undef gfx_PrintIntXY
void move_proj(grid_t * grid,shooter_t * shooter,uint8_t dt) {
    uint8_t i,j;
    point_t coord,proj_coord;
    projectile_t * projectile = shooter->projectile;
    projectile->x += dt * projectile->speed * sin(rad(projectile->angle));
    projectile->y -= dt * projectile->speed * cos(rad(projectile->angle));
    proj_coord.x = (int) projectile->x;
    proj_coord.y = (int) projectile->y;
    if (proj_coord.x <= grid->x) {
        projectile->angle = 0 - projectile->angle;
        proj_coord.x = grid->x;
        projectile->x = (float) proj_coord.x;
    } else if (proj_coord.x + TILE_WIDTH >= grid->x + grid->width) {
        projectile->angle = 0 - projectile->angle;
        proj_coord.x = grid->x + grid->width - TILE_WIDTH;
        projectile->x = (float) proj_coord.x;
    }
    if (proj_coord.y <= grid->y) {
        projectile->y = (float) grid->y;
        snapBubble(projectile,grid);
        shooter->flags &= ~(ACTIVE_PROJ);
        return;
    }
    proj_coord.x -= grid->x;
    proj_coord.y -= grid->y;
    if (proj_coord.x > grid->width || proj_coord.y > grid->height) {
        return;
    }
    for (i = 0;i < grid->rows;i++) {
        for (j = 0;j < grid->cols;j++) {
            if (grid->bubbles[i*grid->cols+j].flags & EMPTY) {
                continue;
            }
            if (grid->bubbles[i*grid->cols+j].color > MAX_COLOR) grid->bubbles[i*grid->cols+j].flags |= EMPTY;
            coord = getTileCoordinate(j, i);
            if (collide(proj_coord.x,
                        proj_coord.y,
                        coord.x,
                        coord.y,
                        grid->ball_radius)) {
                snapBubble(projectile,grid);
                shooter->flags &= ~(ACTIVE_PROJ);
            }
        }
    }
}
void resetProcessed(grid_t * grid) {
    int i;
    for (i = 0;i < (grid->rows*grid->cols);i++) {
        grid->bubbles[i].flags &= ~((uint8_t)PROCESSED);
    }
}
bubble_list_t * getNeighbors(grid_t * grid,bubble_t tile) {
    uint8_t i;
    point_t neighbor;
    bubble_list_t * neighbors;
    int8_t **this_row_offsets;
    int8_t tilerow = (tile.y + row_offset) % 2; // Even or odd row
    neighbors->size = 0;
    neighbors->bubbles = (bubble_t *) malloc(sizeof(bubble_t));
    // Get the neighbor offsets for the specified tile
    this_row_offsets = (int8_t **) neighbor_offsets[tilerow];
    // Get the neighbors
    for (i = 0;i < 6;i++) {
        // Neighbor coordinate
        neighbor.x = tile.x + this_row_offsets[i][0];
        neighbor.y = tile.y + this_row_offsets[i][1];
        // Make sure the tile is valid
        if (neighbor.x >= 0 && neighbor.x < grid->cols && neighbor.y >= 0 && neighbor.y < grid->rows) {
            neighbors->bubbles = (bubble_t *) realloc(neighbors->bubbles,(++neighbors->size) * sizeof(bubble_t));
            neighbors->bubbles[neighbors->size-1] = grid->bubbles[(neighbor.y*grid->cols)+neighbor.x];
        }
    }
    return neighbors;
}

bubble_list_t * findCluster(grid_t * grid,uint8_t tile_x,uint8_t tile_y,bool matchtype,bool reset,bool skipremoved) {
    int i;
    //int i,toprocess_size,foundcluster_size,neighbors_size;
    //bubble_t *toprocess, *foundcluster, *neighbors;
    bubble_list_t toprocess, foundcluster, neighbors;
    bubble_t targettile,currenttile;
    
    if (reset) resetProcessed(grid);
    
    // Get the target tile. Tile coord must be valid.
    targettile = grid->bubbles[(tile_y * grid->cols) + tile_x];
    //toprocess_size = foundcluster_size = 1;
    //neighbors_size = 0;
    toprocess.size = foundcluster.size = 1;
    neighbors.size = 0;
    // Initialize the toprocess array with the specified tile
    toprocess.bubbles = (bubble_t *) malloc(sizeof(bubble_t));
    foundcluster.bubbles = (bubble_t *) malloc(sizeof(bubble_t));
    toprocess.bubbles[0] = targettile;
    toprocess.bubbles[0].flags |= PROCESSED;
    
    while (toprocess.size) {
        if (toprocess.size < 0) return NULL;
        // Pop the last element from the array
        currenttile = toprocess.bubbles[toprocess.size-1];
        toprocess.bubbles = realloc(toprocess.bubbles,(--toprocess.size) * sizeof(bubble_t));
        // Skip processed and empty tiles
        if (currenttile.flags & EMPTY || currenttile.flags & PROCESSED) {
            continue;
        }
        // Skip tiles with the removed flag
        if (skipremoved && (currenttile.flags & REMOVED)) {
            continue;
        }
 
        // Check if current tile has the right type, if matchtype is true
        if (!matchtype || (currenttile.color == targettile.color)) {
            // Add current tile to the cluster
            foundcluster.bubbles = realloc(foundcluster.bubbles,(++foundcluster.size)*sizeof(bubble_t));
            foundcluster.bubbles[foundcluster.size-1] = currenttile;
 
            // Get the neighbors of the current tile
            neighbors = *getNeighbors(grid,currenttile);
            // Check the type of each neighbor
            for (i = 0;i < neighbors.size;i++) {
                if (!(neighbors.bubbles[i].flags & PROCESSED)) {
                    // Add the neighbor to the toprocess array
                    toprocess.bubbles = realloc(toprocess.bubbles,(++toprocess.size) * sizeof(bubble_t));
                    toprocess.bubbles[toprocess.size-1] = neighbors.bubbles[i];
                    neighbors.bubbles[i].flags |= PROCESSED;
                }
            }
        }
    }
    return &foundcluster;
}
bubble_list_t * findFloatingClusters(grid_t * grid) {
    uint8_t i,j,k;
    bool floating;
    int foundclusters_size;
    bubble_list_t *foundclusters;
    bubble_list_t foundcluster;
    bubble_t tile;
    resetProcessed(grid);
    foundclusters = (bubble_list_t *) malloc(sizeof(bubble_list_t));
    for (i = 0;i < grid->rows;i++) {
        for (j = 0;j < grid->cols; j++) {
            tile = grid->bubbles[(i * grid->cols) + j];
            if (!(tile.flags & PROCESSED)) {
                foundcluster = *findCluster(grid, j, i, false, false, true);
                if (foundcluster.size <= 0) continue;
                floating = true;
                for (k = 0;k < foundcluster.size;k++) {
                    if (!i) {
                        // Tile is attached to the roof
                        floating = false;
                        break;
                    }
                }
                if (floating) {
                    foundclusters = (bubble_list_t *) realloc(foundclusters,(++foundclusters_size) * sizeof(bubble_t *));
                    foundclusters[foundclusters_size-1] = foundcluster;
                }
            }
        }
    }
    array_size = foundclusters_size;
    return foundclusters;
}

