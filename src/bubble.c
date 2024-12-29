#include <tice.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <debug.h>

#include <graphx.h>
#include <keypadc.h>
#include "bubble.h"

#define TILE_WIDTH 16
#define TILE_HEIGHT 16
#define ROW_HEIGHT ((TILE_HEIGHT>>1)+(TILE_HEIGHT>>2)) // 3/4 of the tile height
#define deg(a) (a * (180 / M_PI))
#define rad(a) (a * (M_PI / 180))

/*Globals*/
uint8_t row_offset; // 0: even row; 1: odd row
uint8_t max_color;
unsigned int player_score;
unsigned int turn_counter;
unsigned int push_down_time;
uint8_t shift_rate;
uint8_t * available_colors;

//bool debug_flag;
char string[100];

bool pop_started; //handles initial condition
point_t * pop_locations;
bubble_list_t pop_cluster;
uint8_t pop_counter;// timer for pop animation

bool fall_started;
falling_bubble_list_t fall_data;
int fall_total;
uint8_t fall_counter;// timer for fall animation


uint8_t game_flags;

gfx_sprite_t * bubble_sprites[7] = { //bubbles sprites are stored here
    bubble_red,
    bubble_orange,
    bubble_yellow,
    bubble_green,
    bubble_blue,
    bubble_purple,
    bubble_violet
};

gfx_sprite_t * bubble_pop_sprites[7] = {
    bubble_red_pop,
    bubble_orange_pop,
    bubble_yellow_pop,
    bubble_green_pop,
    bubble_blue_pop,
    bubble_purple_pop,
    bubble_violet_pop
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
    return bubble;
}

bubble_list_t copyBubbleList(bubble_list_t original) {
    // Get a deep copy of the original bubble list
    bubble_list_t new;
    new.size = original.size;
    new.bubbles = (bubble_t *) malloc(original.size * sizeof(bubble_t));
    if (new.bubbles == NULL) exit(1);
    for (int i = 0;i < original.size;i++) {
        new.bubbles[i] = original.bubbles[i];
    }
    return new;
}

bubble_list_t * addToBubbleList(bubble_list_t * dest, bubble_list_t * src) {
    // add to original bubble list
    uint8_t old_size;
    old_size = dest->size;
    dest->size += src->size;
    dest->bubbles = (bubble_t *) realloc(dest->bubbles,(dest->size) * sizeof(bubble_t));
    if (dest->bubbles == NULL) exit(1);
    for (int i = 0;i < src->size;i++) {
        dest->bubbles[old_size+i] = src->bubbles[i];
    }
    return dest;
}

void setAvailableColors(uint8_t * target,uint8_t colors) {
    //Precondition: target is an 8-element uint8_t array
    uint8_t i, size;
    size = 0;
    memset(target,0xFF,MAX_POSSIBLE_COLOR + 2);
    for (i = 0;i < sizeof(uint8_t) * 7;i++) {
        if (colors & (1<<i)) {
            target[++size] = i;
        }
    }
    target[0] = size;
}
uint8_t * getAvailableColors(grid_t grid) {
    //Precondition: there is at least 1 bubble in grid
    //Postcondition: returned array is set if uninitialized and filled with available colors in grid.
    //the returned array is in size + data format
    uint8_t i,j,array_index;
    bool is_in;
    static uint8_t * found_colors;
    if (found_colors == NULL) {
        found_colors = (uint8_t *) malloc((MAX_POSSIBLE_COLOR+2) * sizeof(uint8_t));
        if (found_colors == NULL) {
            exit(1); //oooooooooops!
        }
    }
    
    memset(found_colors, 0xFF, (MAX_POSSIBLE_COLOR + 2));
    array_index = 1;
    for (i = 0; i < grid.rows * grid.cols; i++) {
        is_in = false;
        if (!(grid.bubbles[i].flags & EMPTY)) {
            for (j = 1;j < array_index;j++) {
                if (grid.bubbles[i].color == found_colors[j]) {
                    is_in = true;
                    break;
                }
            }
            if (!is_in) {
                found_colors[array_index++] = grid.bubbles[i].color;
                if (array_index > (MAX_POSSIBLE_COLOR+2)) {
                    exit(1);
                }
            }
        }
    }
    found_colors[0] = array_index - 1;
    return found_colors;
}

void drawTile(uint8_t color, int x, int y) {
    if (color > max_color) return;
    gfx_TransparentSprite(bubble_sprites[color], x, y);
}
point_t getTileCoordinate(int col, int row) {
    point_t temp;
    temp.x = col * TILE_WIDTH;
    if ((row + row_offset) % 2)
        temp.x += TILE_WIDTH>>1;
    temp.y = ((row+1) * ROW_HEIGHT) - ROW_HEIGHT;
    return temp;
}
point_t getGridPosition(int x, int y) {
    point_t temp;
    temp.y = y / ROW_HEIGHT;
    temp.x = (x - (((temp.y + row_offset) % 2) ? TILE_WIDTH >> 1 : 0)) / TILE_WIDTH;
    return temp;
}

void initGrid(grid_t grid,uint8_t rows,uint8_t cols,uint8_t empty_row_start,uint8_t * available_colors) {
    /*example: initGrid(grid,16,7,15) for a 16*7 grid with the 15th and 16th rows empty*/
    uint8_t i,j,r;
//  point_t p = {0, 0};//getGridPosition(grid->x,grid->y);
    r = 0;
    if (!empty_row_start) empty_row_start = 1;
    for (i = 0;i<rows;i++) {
        for (j = 0;j<cols;j++) {
            if (i >= empty_row_start - 1) {
                grid.bubbles[(i*cols)+j] = newBubble(j,i,0,EMPTY);
            } else {
                if (available_colors) {
                    r = available_colors[randInt(1,available_colors[0])];
                } else {
                    r = randInt(0,max_color);
                }
                grid.bubbles[(i*cols)+j] = newBubble(j,i,r,0);
            }
        }
    }
}

void addNewRow(grid_t grid,uint8_t * available_colors,uint8_t chance) {
    uint8_t i,j;
    row_offset ^= 1;
    for (i = grid.rows-1;i;i--) {
        for (j = grid.cols;j;j--) {
            grid.bubbles[i * grid.cols + (j - 1)] = grid.bubbles[(i-1) * grid.cols + (j - 1)];
            grid.bubbles[i * grid.cols + (j - 1)].y++;
        }
    }
    for (j = 0;j < grid.cols;j++) {
        if (available_colors) {
            grid.bubbles[j] = newBubble(j,0,available_colors[randInt(1,available_colors[0])],(randInt(1,10) > chance) ? EMPTY : 0);
        } else {
            grid.bubbles[j] = newBubble(j,0,randInt(0,max_color),(randInt(1,10) > chance) ? EMPTY : 0);
        }
    }
}

void pushDown(grid_t * grid) {
    if (grid->rows > 2)
        grid->rows--;
    grid->y += ROW_HEIGHT;
    grid->height -= ROW_HEIGHT;
    game_flags |= RENDER;
}

void renderGrid(grid_t grid,gfx_sprite_t * grid_buffer) {
    /*Uses the drawing buffer to render the grid to the grid_buffer sprite. This sprite can be drawn later as many times as needed.*/
    /*Precondition: grid.cols is less than 20 (LCD_WIDTH/TILE_WIDTH)
                    grid.rows is less than 26 (LCD_HEIGHT/ROW_HEIGHT)
                    Product of grid.cols and grid.rows is less than 257*/
    int i,j;
    bubble_t tile;
    point_t coord;
    
    gfx_SetDrawBuffer();
    gfx_ZeroScreen();
    for (i = 0;i < grid.rows;i++) {
        for (j = 0;j < grid.cols;j++) {
            tile = grid.bubbles[(i*grid.cols)+j];
            if (tile.flags & EMPTY) continue; //skip if empty
            coord = getTileCoordinate(j,i);
            gfx_TransparentSprite_NoClip(bubble_sprites[tile.color],coord.x,coord.y);
        }
    }
    grid_buffer->width  = grid.cols * TILE_WIDTH + (TILE_WIDTH>>1);
    grid_buffer->height = grid.rows * ROW_HEIGHT + (TILE_WIDTH>>2); //TILE_HEIGHT + ?
    gfx_GetSprite_NoClip(grid_buffer,0,0);
}
float ** generateVectors(int8_t lower, int8_t higher, uint8_t step) {
    //Generates an array of trigonometric vector coordinates for shooter to use.
    static float ** vectors;
    uint8_t size;
    int i;
    uint8_t j;
    size = (((int) higher) - lower) / step + 1; //(64 - -64) / 4 + 1 = 33 pairs
    if (vectors == NULL) {
        vectors = (float **) malloc(sizeof(float *) * 2);
        if (vectors == NULL) {
            exit(1);
        }
        for (j = 0; j < 2; j++) {
            vectors[j] = (float *) malloc(sizeof(float) * size + 1);
        }
    }
    for (i = 0; i < size; i++) {
        vectors[0][i] = sin(rad(lower + (step * i)));
        vectors[1][i] = cos(rad(lower + (step * i)));
    }
    return vectors;
}

void renderShooter(shooter_t shooter) {
    point_t center;
    center.x = shooter.x + (TILE_WIDTH >> 1);
    center.y = shooter.y + (TILE_HEIGHT >> 1);
    gfx_palette[shooter.pal_index] = gfx_Lighten(bubble_colors[shooter.next_bubbles[0]], 255 - (((timer_1_Counter>>2)&15)<<2));
    gfx_SetColor(shooter.pal_index);
    gfx_Line(center.x,center.y,center.x + TILE_WIDTH1_5 * shooter.vectors[0][getRangeIndex(shooter.angle,LBOUND,SHOOTER_STEP)],center.y - TILE_HEIGHT1_5 * shooter.vectors[1][getRangeIndex(shooter.angle,LBOUND,SHOOTER_STEP)]);
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
void moveProj(grid_t grid, shooter_t * shooter, float dt) {
    uint8_t i;
    point_t coord,proj_coord; //x,y not col/row
    projectile_t * projectile = &shooter->projectile;
    /*gfx_SetColor(1);
    gfx_Rectangle(40,40,40,40);
    gfx_BlitBuffer();
    while(kb_Data[1] & kb_2nd) kb_Scan();*/
    projectile->x += dt * projectile->speed * shooter->vectors[0][getRangeIndex(projectile->angle,LBOUND,SHOOTER_STEP)];
    projectile->y -= dt * projectile->speed * shooter->vectors[1][getRangeIndex(projectile->angle,LBOUND,SHOOTER_STEP)];

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
        projectile->y = grid.y;
        snapBubble(shooter, grid, dt);
        shooter->flags &= ~ACTIVE_PROJ;
        return;
    }
    if (proj_coord.x > grid.width || proj_coord.y > grid.height) {
        return;
    }
    for (i = 0; i < grid.possible_collisions.size; i++) {
        coord = getTileCoordinate(grid.possible_collisions.bubbles[i].x, grid.possible_collisions.bubbles[i].y);
        if (collide(proj_coord.x + (TILE_WIDTH>>1),
                    proj_coord.y + (TILE_HEIGHT>>1),
                    coord.x + (TILE_WIDTH>>1),
                    coord.y + (TILE_HEIGHT>>1),
                    grid.ball_diameter)) {
            snapBubble(shooter, grid, dt);
            shooter->flags &= ~ACTIVE_PROJ;
            return;
        }
    }
}

void snapBubble(shooter_t * shooter, grid_t grid, float dt) {
    bool addtile;
    uint8_t i,j;
    uint8_t index; //uint8_t or int
    bubble_list_t cluster;
    point_t gridpos, back_coords, debug_coords;

    gridpos = getGridPosition(shooter->projectile.x + (TILE_WIDTH>>1) - grid.x,
                              shooter->projectile.y + (TILE_HEIGHT>>1) - grid.y);
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
    index = getRangeIndex(shooter->projectile.angle, LBOUND, SHOOTER_STEP);
    back_coords = getGridPosition(round(shooter->projectile.x - shooter->projectile.speed * shooter->vectors[0][index] * dt),
                                  round(shooter->projectile.y + shooter->projectile.speed * shooter->vectors[1][index] * dt));
    for (i = gridpos.y; i < grid.rows; i++) {
        gfx_SetColor(1);
#ifdef DEBUG
        debug_coords = getTileCoordinate(gridpos.x, i);
        debug_coords.x += grid.x;
        debug_coords.y += grid.y;
        gfx_PrintStringXY("First color:", 0, 0);
        gfx_PrintStringXY("Second color:", 0, 16);
        gfx_PrintUIntXY(gfx_palette[1], 8, 104, 0);
        gfx_PrintUIntXY(gfx_palette[29], 8, 104, 16);
        gfx_PrintStringXY("Collision location:", 0, 32);
        gfx_PrintUIntXY(debug_coords.x, 5, 144, 32);
        gfx_PrintUIntXY(debug_coords.y, 5, 216, 32);
        gfx_FillRectangle(debug_coords.x, debug_coords.y, TILE_WIDTH, TILE_HEIGHT);
#endif
        if (grid.bubbles[i * grid.cols + gridpos.x].flags & EMPTY) {
            gridpos.y = i;
            addtile = true;
            break;
        } else {
            if (back_coords.y >= grid.rows) break;
            gfx_SetColor(29);
#ifdef DEBUG
            debug_coords = getTileCoordinate(back_coords.y, back_coords.x);
            gfx_FillRectangle(debug_coords.x, debug_coords.y, TILE_WIDTH, TILE_HEIGHT);
#endif
            if (grid.bubbles[back_coords.y * grid.cols + back_coords.x].flags & EMPTY) {
                gridpos = back_coords;
                addtile = true;
#ifdef DEBUG
                gfx_PrintStringXY("Final location:", 0, 48);
                gfx_PrintUIntXY(debug_coords.x, 5, 144, 48);
                gfx_PrintUIntXY(debug_coords.y, 5, 216, 48);
#endif
                break;
            }
#ifdef DEBUG
            gfx_FillRectangle(shooter->projectile.x + (TILE_WIDTH>>1), shooter->projectile.y + (TILE_HEIGHT>>1), TILE_WIDTH, TILE_HEIGHT);
#endif
            back_coords.y++;
        }
    }
#ifdef DEBUG
    gfx_BlitBuffer();
    while(!os_GetCSC());
#endif
    /*if (!(grid.bubbles[gridpos.y * grid.cols + gridpos.x].flags & EMPTY)) {
        gfx_SetColor(1);
        gfx_Rectangle(projectile->x,projectile->y,TILE_WIDTH,TILE_HEIGHT);
        back_coords = getGridPosition(projectile->x - sin(rad(projectile->angle) * TILE_WIDTH),projectile->y + cos(rad(projectile->angle)) * TILE_HEIGHT);
        if (grid.bubbles[back_coords.y * grid.cols + back_coords.x].flags & EMPTY) {
            for (i = gridpos.y + 1;i < grid.rows;i++) {
                if ((grid.bubbles[i*grid.cols + gridpos.x].flags) & EMPTY) {
                    gridpos.y = i;
                    addtile = true;
                    break;
                }
            }
        } else {
            gridpos = back_coords;
            addtile = true;
            //debug_flag = true;
        }
    } else {
        addtile = true;
    }*/
    if (addtile) {
        index = gridpos.y * grid.cols + gridpos.x;
        if (!(grid.bubbles[index].flags & EMPTY)) exit(1); //not empty, exit
        grid.bubbles[index].flags &= ~EMPTY;
        grid.bubbles[index].color = shooter->projectile.color;
        game_flags |= RENDER;
        cluster = findCluster(grid, gridpos.x, gridpos.y, true, true, false);

        if (cluster.size >= 3) {
            player_score += 100 * (cluster.size - 2);
            if (game_flags & POP) { //animation already started, free bubbles to let it restart
                pop_counter = 0;
                pop_started = false;
                free(pop_locations);
                free(pop_cluster.bubbles);
            }
            game_flags |= POP; //pop animation starts
            pop_cluster = copyBubbleList(cluster);
            for (i = 0; i < cluster.size; i++) {
                grid.bubbles[cluster.bubbles[i].y * grid.cols + cluster.bubbles[i].x].flags |= EMPTY;
            }
            if (game_flags & FALL) {
                game_flags &= ~FALL;
                fall_started = false;
            }
            fall_total = findFloatingClusters(grid);
            if (fall_total > 0) {
                game_flags |= FALL;
                fall_data.size = fall_total;
                for (i = 0; i < fall_total; i++) {
                    player_score += 50 * (i + 1);
                }
                j = 0;
                for (i = 0; i < grid.rows * grid.cols; i++) {
                    if (grid.bubbles[i].flags & FALLING) {
                        gridpos = getTileCoordinate(grid.bubbles[i].x, grid.bubbles[i].y);
                        fall_data.bubbles[j].x = gridpos.x + grid.x;
                        fall_data.bubbles[j].y = gridpos.y + grid.y;
                        fall_data.bubbles[j].velocity = randInt(-2, 2);
                        fall_data.bubbles[j].color = grid.bubbles[i].color;
                        grid.bubbles[i].flags |= EMPTY;
                        j++;
                    }
                    grid.bubbles[i].flags &= ~FALLING;
                }
            }
        } else {
            turn_counter++;
            if (!(turn_counter % shift_rate)) {
                if (turn_counter <= push_down_time) {
                    addNewRow(grid, available_colors, 9);
                } else {
                    game_flags |= PUSHDOWN;
                }
            }
        }
        free(cluster.bubbles);
    }
    game_flags |= CHECK;
}
void resetProcessed(grid_t grid) {
    int i;
    for (i = 0;i < (grid.rows * grid.cols);i++) {
        grid.bubbles[i].flags &= ~(PROCESSED);
    }
}
bubble_list_t getNeighbors(grid_t grid, uint8_t tilex, uint8_t tiley,bool add_empty) {
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
    // Get the neighbors
    for (i = 0;i < 6;i++) {
        // Neighbor coordinate
        neighbor.x = tilex + neighbor_offsets[tilerow][i][0];
        neighbor.y = tiley + neighbor_offsets[tilerow][i][1];
        // Make sure the tile is valid
        if (neighbor.x >= 0 &&
            neighbor.x < grid.cols &&
            neighbor.y >= 0 &&
            neighbor.y < grid.rows) {
            if (!(grid.bubbles[(neighbor.y*grid.cols)+neighbor.x].flags & EMPTY) || add_empty) {
                neighbors_bubbles[neighbors.size++] = grid.bubbles[(neighbor.y*grid.cols)+neighbor.x];
            }
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
        if ((toprocess.bubbles = (bubble_t *) malloc((grid.rows)*grid.cols*sizeof(bubble_t))) == NULL) // malloc(256*sizeof(bubble_t))
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
        currenttile = toprocess.bubbles[toprocess.size - 1];
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
            neighbors = getNeighbors(grid,currenttile.x,currenttile.y,true);
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
bubble_list_t getPossibleCollisions(grid_t grid) {
    //Uses neighbors to deduce whether a collision is possible, reducing time spent calculating projectile collisions
    uint8_t i;
    unsigned int size;
    static bubble_list_t possible_collisions;
    possible_collisions.size = 0;
    if (possible_collisions.bubbles == NULL) {
        possible_collisions.bubbles = (bubble_t *) malloc(grid.rows * grid.cols * sizeof(bubble_t));
        if (possible_collisions.bubbles == NULL) {
            exit(1); //failure...
        }
    }
    size = grid.rows * grid.cols;
    for (i = 0; i < size; i++) {
        if (!(grid.bubbles[i].flags & EMPTY)) {
            if (getNeighbors(grid, grid.bubbles[i].x, grid.bubbles[i].y, true).size != getNeighbors(grid, grid.bubbles[i].x, grid.bubbles[i].y, false).size) {
                possible_collisions.bubbles[possible_collisions.size++] = grid.bubbles[i];
            }
        }
    }
    return possible_collisions;
}
