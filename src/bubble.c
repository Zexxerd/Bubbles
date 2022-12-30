#include <tice.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <debug.h>

#include <graphx.h>
#include <keypadc.h>
#include <debug.h>
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
#define POPPING   (1<<3)
#define FALLING   (1<<4)
//shooter
#define ACTIVE_PROJ (1<<0)
#define DEACTIVATED (1<<1)

/*Globals*/
uint8_t row_offset; // 0: even row; 1: odd row
unsigned int player_score;

bool debug_flag;
char string[100];

bool pop_started; //handles initial condition
int ** pop_locations;
bubble_list_t pop_cluster;
uint8_t pop_counter;// timer for pop animation

bool fall_started;
int ** fall_data;
int fall_total;
//bubble_list_t * floating_clusters;
uint8_t fall_counter;// timer for pop animation

uint8_t game_flags;

//enum gamestate current_gamestate;
enum gamestate {
    game_over = 0,
    neutral = 1,
    shoot = 2,
    pop = 3,
};

gfx_sprite_t * bubble_sprites[7] = { //bubbles sprites are stored here
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
    0x7ee0, //yellow
    0x0305, //green
    0x019f, //blue
    0x401f, //purple
    0x7c1f, //violet
};

/*Functions*/
char * uint8_to_bin(uint8_t n) {
    /*This returns the MSB-first binary representation of an 8-bit unsigned number.
      Inputting an 8-bit signed number should probably work the same way.*/
    static char * ret;
    uint8_t i;
    if (ret == NULL) {
        ret = malloc(9*sizeof(char));
        if (ret == NULL)
            exit(1);
    }
    for (i = 0;i < 8;i++) {
        ret[7 - i] = n & 1 ? '1' : '0';
        n >>= 1;
    }
    ret[8] = '\0';
    return ret;
}
bubble_t newBubble(uint8_t x,uint8_t y,uint8_t color,uint8_t flags) {
    //Defines a new bubble.
    bubble_t bubble;
    bubble.x = x;
    bubble.y = y;
    bubble.color = color;
    bubble.flags = flags;
    //if (bubble.color != color) exit(1); //this appears to work //featuring my past paranoia!
    return bubble;
}

bubble_list_t copyBubbleList(bubble_list_t original) { // deep copy
    bubble_list_t new;
    new.size = original.size;
    new.bubbles = (bubble_t *) malloc(original.size * sizeof(bubble_t));
    if (new.bubbles == NULL) exit(1);
    for (int i = 0;i < original.size;i++) {
        new.bubbles[i] = original.bubbles[i];
    }
    return new;
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
    temp.y = ((row+1) * ROW_HEIGHT) - ROW_HEIGHT;
    return temp;
}
point_t getGridPosition(int x,int y) {
    point_t temp;
    temp.y = y / ROW_HEIGHT;
    temp.x = (x - (((temp.y + row_offset) % 2) ? TILE_WIDTH >> 1 : 0)) / TILE_WIDTH;
    return temp;
}
void initGrid(grid_t grid,uint8_t rows,uint8_t cols,uint8_t empty_row_start) {
    /*example: initGrid(grid,16,7,15) for a 16*7 grid with the 15th and 16th (from 1st) rows empty*/
    uint8_t i,j,r;
//  point_t p = {0, 0};//getGridPosition(grid->x,grid->y);
    r = 0;
    if (!empty_row_start) empty_row_start = 1;
    for (i = 0;i<rows;i++) {
        for (j = 0;j<cols;j++) {
            if (i >= empty_row_start-1) {
                grid.bubbles[(i*cols)+j] = newBubble(j,i,0,EMPTY);
            } else {
                r = randInt(0,6);
                grid.bubbles[(i*cols)+j] = newBubble(j,i,r,0);
            }
        }
    }
}

void renderGrid(grid_t grid,gfx_sprite_t * grid_buffer) {
    /*Uses the drawing buffer to render the grid to the grid_buffer sprite. This sprite can be drawn later as many times as needed.*/
    int i,j;
    bubble_t tile;
    point_t coord;
    gfx_ZeroScreen();
    for (i = 0;i < grid.rows;i++) {
        for (j = 0;j < grid.cols;j++) {
            tile = grid.bubbles[(i*grid.cols)+j];
            if (tile.flags & EMPTY) continue; //skip if empty
            coord = getTileCoordinate(j,i);
//            coord.x += grid.x;
//            coord.y += grid.y;
            drawTile(tile.color,coord.x,coord.y);
        }
    }
    grid_buffer->width  = grid.cols * TILE_WIDTH + (TILE_WIDTH>>1);
    grid_buffer->height = grid.rows * ROW_HEIGHT + (TILE_WIDTH>>2); //TILE_HEIGHT + ?
    gfx_GetSprite_NoClip(grid_buffer,0,0);
}
void renderShooter(shooter_t shooter) {
    point_t center;
    center.x = shooter.x + (TILE_WIDTH >> 1);
    center.y = shooter.y + (TILE_HEIGHT >> 1);
    gfx_palette[shooter.pal_index] = gfx_Lighten(bubble_colors[shooter.next_bubbles[0]],255-(rtc_Time()%3));
    gfx_SetColor(shooter.pal_index);
    //gfx_FillCircle(center.x,center.y,TILE_WIDTH>>1);
    gfx_Line(center.x,center.y,center.x + (1.5 * TILE_WIDTH) * sin(rad(shooter.angle)),center.y - (1.5 * TILE_HEIGHT) * cos(rad(shooter.angle)));
    //gfx_palette[shooter->pal_index] = bubble_colors[shooter->next_bubbles[0]];
    gfx_TransparentSprite(bubble_sprites[shooter.next_bubbles[0]],shooter.x,shooter.y);
    gfx_SetColor((sizeof_bubble_pal>>1)+shooter.next_bubbles[1]);
    gfx_FillCircle(center.x - 15,center.y + 6,5);
    gfx_SetColor((sizeof_bubble_pal>>1)+shooter.next_bubbles[2]);
    gfx_FillCircle(center.x - 30,center.y + 12,2);
}
bool collide(float x1,float y1,float x2,float y2,uint8_t r) {
    float dx,dy,dist;
    dx = x2 - x1;
    dy = y2 - y1;
    dist = sqrt((dx * dx) + (dy * dy));
    return (dist < r);
}
void move_proj(grid_t grid,shooter_t * shooter,float dt) {
    uint8_t i,j;
    uint8_t index; // uint8_t for now
    point_t coord,proj_coord; //x,y not col/row
    projectile_t * projectile = &shooter->projectile;
    projectile->x += dt * projectile->speed * sin(rad(projectile->angle));
    projectile->y -= dt * projectile->speed * cos(rad(projectile->angle));
    proj_coord.x =  projectile->x - grid.x;
    proj_coord.y =  projectile->y - grid.y;
    if (proj_coord.x <= 0) {
        projectile->angle = 0 - projectile->angle;
        proj_coord.x = 0;
        projectile->x = grid.x;
    } else if (proj_coord.x + TILE_WIDTH >= grid.width) {
        projectile->angle = 0 - projectile->angle;
        proj_coord.x = grid.width - TILE_WIDTH;
        projectile->x = proj_coord.x + grid.x;
    }
    if (proj_coord.y <= 0) {
        projectile->y = 0; //grid.y
        snapBubble(projectile, grid);
        shooter->flags &= ~(ACTIVE_PROJ);
        return;
    }
    if (proj_coord.x > grid.width || proj_coord.y > grid.height) {
        return;
    }
    for (i = 0;i < grid.rows;i++) {
        for (j = 0;j < grid.cols;j++) {
            index = i*grid.cols+j;
            if (grid.bubbles[index].flags & EMPTY) {
                continue;
            }
            coord = getTileCoordinate(j, i);
            if (collide(proj_coord.x + (TILE_WIDTH>>1),
                        proj_coord.y + (TILE_HEIGHT>>1),
                        coord.x + (TILE_WIDTH>>1),
                        coord.y + (TILE_HEIGHT>>1),
                        grid.ball_diameter)) {
                /*Debug: Highlight collision
                 gfx_SetColor(10);
                gfx_Circle(projectile->x,projectile->y,grid->ball_diameter>>1);
                gfx_SetColor(20);
                gfx_Circle(coord.x+grid->x,coord.y+grid->y,grid->ball_diameter>>1);
                gfx_BlitBuffer();
                while(!os_GetCSC());
                 */
                snapBubble(projectile,grid);
                shooter->flags &= ~(ACTIVE_PROJ);
                //free(projectile);
                return;
            }
        }
    }
}
void snapBubble(projectile_t * projectile,grid_t grid) {
    bool addtile;
    uint8_t i,j;
    uint8_t index; //uint8_t or int
    bubble_list_t cluster;
    point_t gridpos;
    gridpos = getGridPosition(projectile->x + (TILE_WIDTH>>1) - grid.x,
                              projectile->y + (TILE_HEIGHT>>1) - grid.y);
    addtile = false;
    //if gridpos is outside of the grid, force it in.
    if (gridpos.x < 0) {
        gridpos.x = 0;
    }
    if (gridpos.x >= grid.cols) {
        gridpos.x = grid.cols - 1;
    }
    if (gridpos.y < 0) {
        gridpos.y = 0;
    }
    if (gridpos.y >= grid.rows) {
        gridpos.y = grid.rows - 1;
    }
    if (!(grid.bubbles[gridpos.y * grid.cols + gridpos.x].flags & EMPTY)) {
        for (i = gridpos.y + 1;i < grid.rows;i++) {
            if ((grid.bubbles[i*grid.cols + gridpos.x].flags) & EMPTY) {
                gridpos.y = i;
                addtile = true;
                break;
            }
        }
    } else {
        addtile = true;
    }
    if (addtile) {
        projectile->visible = false;
        index = gridpos.y * grid.cols + gridpos.x;
        if (!(grid.bubbles[index].flags & EMPTY)) exit(1); //not empty, exit
        grid.bubbles[index].flags &= ~EMPTY;
        grid.bubbles[index].color = projectile->color;
        game_flags |= RENDER;
        cluster = findCluster(grid,gridpos.x, gridpos.y, true, true, false);

        if (cluster.size >= 3) {
            player_score += 100 * (cluster.size - 2);
            if (game_flags & POP) { //animation already started, free bubbles to let it restart
                pop_counter = 0;
                pop_started = false;
                for(i = 0;i < pop_cluster.size;i++) {
                    free(pop_locations[i]);
                }
                free(pop_locations);
                free(pop_cluster.bubbles);
            }
            game_flags |= POP; //pop animation starts
            pop_cluster = copyBubbleList(cluster);
            for (i = 0;i < cluster.size;i++) {
                grid.bubbles[cluster.bubbles[i].y * grid.cols + cluster.bubbles[i].x].flags |= EMPTY;
            }
            if (game_flags & FALL) {
                game_flags &= ~FALL;
                fall_started = false;
                /*for (i = 0;i < grid.rows*grid.cols;i++) {
                    if (grid.bubbles[i].flags & FALLING) {
                        grid.bubbles[i].flags |= EMPTY;
                        grid.bubbles[i].flags &= ~FALLING;
                    }
                }*/
                for (i = 0;i < fall_total;i++) {
                    grid.bubbles[fall_data[i][2] >> 8].flags &= ~FALLING;
                    grid.bubbles[i].flags |= EMPTY;
                    free(fall_data[i]);
                }
                free(fall_data);
            }
            fall_total = findFloatingClusters(grid);
            if (fall_total > 0) {
                game_flags |= FALL;
                fall_data = (int **) malloc(fall_total * sizeof(int *));
                dbg_getlocation(fall_data);
                if (fall_data == NULL) {
                    debug_message("fall data nul12ueno1u8932");
                    exit(1);
                }
                for (i = 0;i < fall_total;i++) {
                    fall_data[i] = (int *) malloc(4 * sizeof(int)); //x,y,(index:16,velocity:8)
                    dbg_getlocation(fall_data[i]);
                    if (fall_data[i] == NULL) {
                        dbg_printf("It was %d!!\n",i);
                        debug_message("fall data membr fiweon394f43 null!!!");
                        exit(1);
                    }
                }
                j = 0;
                for (i = 0;i < grid.rows*grid.cols;i++) {
                    if (grid.bubbles[i].flags & FALLING) {
                        gridpos = getTileCoordinate(grid.bubbles[i].x,grid.bubbles[i].y);
                        fall_data[j][0] = gridpos.x + grid.x;
                        fall_data[j][1] = gridpos.y + grid.y;
                        fall_data[j][2] = randInt(-2,2);
                        fall_data[j][3] = i;
                        j++;
                        grid.bubbles[i].flags |= EMPTY;
                    }
                }
            }
        }
        free(cluster.bubbles);
    }
}
void resetProcessed(grid_t grid) {
    int i;
    for (i = 0;i < (grid.rows * grid.cols);i++) {
        grid.bubbles[i].flags &= ~(PROCESSED);
    }
}
bubble_list_t getNeighbors(grid_t grid, uint8_t tilex, uint8_t tiley) {
    /*WORKING*/
    uint8_t i;
    point_t neighbor;
    bubble_list_t neighbors;
    static bubble_t * neighbors_bubbles;
    //int8_t **this_row_offsets;
    uint8_t tilerow = (tiley + row_offset) % 2; // Even or odd row
    
    //debug
    neighbors.size = 0;
    if (neighbors_bubbles == NULL) { //if not initialized
        if ((neighbors_bubbles = (bubble_t *) malloc(6*sizeof(bubble_t))) == NULL)
            exit(1); //oopsie daisy!
    }
    
    // Get the neighbor offsets for the specified tile
    //this_row_offsets = (int8_t **) neighbor_offsets[tilerow];
    // Get the neighbors
    for (i = 0;i < 6;i++) {
        // Neighbor coordinate
        neighbor.x = tilex + neighbor_offsets[tilerow][i][0];
        neighbor.y = tiley + neighbor_offsets[tilerow][i][1];
        // Make sure the tile is valid
        if (neighbor.x >= 0 && neighbor.x < grid.cols && neighbor.y >= 0 && neighbor.y < grid.rows) {
            neighbors_bubbles[neighbors.size++] = grid.bubbles[(neighbor.y*grid.cols)+neighbor.x];
        }
    }
    neighbors.bubbles = neighbors_bubbles;
    return neighbors;
}

bubble_list_t findCluster(grid_t grid,uint8_t tile_x,uint8_t tile_y,bool matchtype,bool reset,bool skipremoved) {
    int i;
    static bubble_list_t toprocess;
    bubble_list_t foundcluster;
    bubble_list_t neighbors;
    bubble_t targettile, currenttile;
        
    toprocess.size = 1;
    foundcluster.size = 1;
    neighbors.size = 0;

    if (reset) resetProcessed(grid);
    
    // Get the target tile. Tile coord must be valid.
    targettile = grid.bubbles[(tile_y * grid.cols) + tile_x];
    
    // Initialize the toprocess array with the specified tile
    if (toprocess.bubbles == NULL) {
        if ((toprocess.bubbles = (bubble_t *) malloc((grid.rows)*grid.cols*sizeof(bubble_t))) == NULL)
            exit(1); //oopsie doopsie!
    }
    
    if ((foundcluster.bubbles = (bubble_t *) malloc((grid.rows)*grid.cols*sizeof(bubble_t))) == NULL) {
        exit(1); //oopers doopers!
    }
    toprocess.bubbles[0] = targettile;
    grid.bubbles[(tile_y * grid.cols) + tile_x].flags |= PROCESSED;
    toprocess.bubbles[0].flags |= PROCESSED;
    while (toprocess.size) {
        // Pop the last element from the array
        currenttile = toprocess.bubbles[toprocess.size-1];
        toprocess.size--;
//        toprocess.bubbles = realloc(toprocess.bubbles,(--toprocess.size) * sizeof(bubble_t));
        // Skip processed and empty tiles
        if (currenttile.flags & EMPTY) {
            continue;
        }
        // Skip tiles with the removed flag
        if ((currenttile.flags & REMOVED) && skipremoved) {
            continue;
        }
 
        // Check if current tile has the right type, if matchtype is true
        if (!matchtype || (currenttile.color == targettile.color)) {
            // Add current tile to the cluster
//            foundcluster.bubbles = realloc(foundcluster.bubbles,(++foundcluster.size)*sizeof(bubble_t));
            foundcluster.bubbles[foundcluster.size-1] = currenttile;
            foundcluster.size++;
            // Get the neighbors of the current tile
            neighbors = getNeighbors(grid,currenttile.x,currenttile.y);
            // Check the type of each neighbor
            for (i = 0;i < neighbors.size;i++) {
                if (!(neighbors.bubbles[i].flags & PROCESSED)) {
                    // Add the neighbor to the toprocess array
                    //toprocess.bubbles = realloc(toprocess.bubbles,(++toprocess.size) * sizeof(bubble_t))
                    toprocess.size++;
                    toprocess.bubbles[toprocess.size-1] = neighbors.bubbles[i];
                    neighbors.bubbles[i].flags |= PROCESSED;
                    grid.bubbles[(neighbors.bubbles[i].y * grid.cols) + neighbors.bubbles[i].x].flags |= PROCESSED;
                }
            }
        }
    }
    foundcluster.bubbles = (bubble_t *) realloc(foundcluster.bubbles,(--foundcluster.size)*sizeof(bubble_t));
    if (foundcluster.bubbles == NULL) {
        exit(1); //oopsie daloopsie!
    }
    return foundcluster;
}

int findFloatingClusters(grid_t grid) { //rename to findFloatingBubbles?
    uint8_t i,j,k;
    bool floating;
    int fall_total;
    bubble_list_t foundcluster;
    bubble_t tile;
    resetProcessed(grid);
    fall_total = 0;
    for (i = 0;i < grid.rows;i++) {
        for (j = 0;j < grid.cols; j++) {
            tile = grid.bubbles[(i * grid.cols) + j];
            if (!(tile.flags & PROCESSED)) {
                foundcluster = findCluster(grid, j, i, false, false, true);
                if (foundcluster.size > 0) {
                    floating = true;
                    for (k = 0;k < foundcluster.size;k++) {
                        if (!foundcluster.bubbles[k].y) {
                            // Tile is attached to the roof
                            floating = false;
                            break;
                        }
                    }
                    if (floating) {
                        for (i = 0;i < foundcluster.size;i++) {
                            grid.bubbles[(foundcluster.bubbles[i].y * grid.cols) + foundcluster.bubbles[i].x].flags |= FALLING; //| EMPTY;
                            fall_total++;
                        }
                    }
                }
                free(foundcluster.bubbles);
            }
        }
    }
    return fall_total;
}
