#include <tice.h>

/* Standard headers - it's recommended to leave them included */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <graphx.h>
#include <keypadc.h>
#include <debug.h>

#include "key.h"
#include "pixelate.h"
#include "bubble.h"
#include "game.h"
#include "gfx/bubble.h"

//matrix(x,y) = matrix((y * matrix_cols) + x)
/**
 Note: Pop bubbles before checking deadzone.
 */

/**
 TODO: Add animation bubble list instead of pop sprite/fall sprite int arrays
 Mark this mode as survival;
 */
#ifndef TILE_WIDTH
#define TILE_WIDTH 16
#endif
#ifndef TILE_HEIGHT
#define TILE_HEIGHT 16
#endif
#ifndef MAX_ROWS
#define MAX_ROWS 17 //16 + deadzone
#endif
#ifndef MAX_COLS
#define MAX_COLS 7 //SURVIVAL
#endif
#ifndef MIN_ROWS
#define MIN_ROWS 5
#endif
#ifndef MAX_COLS
#define MAX_COLS 7
#endif

static char * printfloat(float elapsed) {
    real_t elapsed_real;
    static char str[10];
    elapsed_real = os_FloatToReal(elapsed <= 0.001f ? 0.0f : elapsed);
    os_RealToStr(str, &elapsed_real, 8, 1, 2);
    return str;
}

int min(int a, int b) {
    return a > b ? b : a;
}

uint8_t pop_behind_size;
uint8_t fall_behind_size;
static anim_behind_t pop_behind[ANIM_POP_BEHIND_MAX];
static anim_behind_t fall_behind[ANIM_FALL_BEHIND_MAX];

static void captureBehindSprite(anim_behind_t *behind, int x, int y) {
    if (!behind->sprite) {
        behind->sprite = gfx_MallocSprite(TILE_WIDTH, TILE_HEIGHT);
        if (!behind->sprite) exit(1);
    }
    gfx_GetSprite(behind->sprite, x, y);
    behind->pos.x = x;
    behind->pos.y = y;
    behind->valid = true;
}

static void restoreBehindSprite(anim_behind_t *behind) {
    if (behind->valid && behind->sprite) {
        gfx_Sprite(behind->sprite, behind->pos.x, behind->pos.y);
        behind->valid = false;
    }
}

extern uint8_t row_offset; // 0: even row shifted; 1: odd row shifted
extern uint8_t max_color;
extern uint8_t * available_colors;

extern uint8_t game_flags; //global because our grid no longer holds it
extern uint8_t new_row_rate;
extern unsigned int turn_counter;
extern unsigned int global_counter; //always increments, unlike turn counter
extern unsigned int push_down_time;


//Game-mode specific variables
extern enum game_mode current_game;
extern enum game_result game_status;
extern char win_string[];
extern char lose_string[];
extern char option_strings[4][18];

//Survival mode
uint8_t auto_new_row_counter; //counts automatic new row shifts in survival mode

#ifdef DEBUG
extern bool debug_flag;
#endif

extern gfx_sprite_t * bubble_sprites[7];
extern gfx_sprite_t * bubble_pop_sprites[7];
extern const uint16_t bubble_colors[7];

extern bool pop_started;
extern point_t pop_locations[MAX_ROWS * MAX_COLS];
extern bubble_list_t pop_cluster;
extern bubble_t pop_cluster_bubbles[];
extern uint8_t pop_counter; // timer for pop animation

extern bool fall_started;
extern falling_bubble_list_t fall_data;
extern falling_bubble_t fall_data_bubbles[MAX_ROWS * MAX_COLS];

extern int fall_total;
extern uint8_t fall_counter;

extern bool kb_2nd_press, kb_2nd_prev;
extern bool kb_clear_press, kb_clear_prev;
extern bool kb_up_press, kb_up_prev;
extern bool kb_down_press, kb_down_prev;

void game(void) {
    int i,j,k; //universal counter
    point_t point;
    kb_key_t key;
    kb_key_t prevkey;
    
    int level_start_y_pos;
    uint8_t fps_counter;
    float fps_ratio;
    float fps, last_fps, ticks;
    char * fps_string;
    char * end_of_game_string;
#ifdef DEBUG
    uint8_t x, y;
    uint8_t highlight_timer;
    int debug_fall_total;
    point_t debug_point;
    bubble_t debug_bubble;
    bubble_list_t neighbors;
    bubble_list_t foundcluster;
    bool move_bubble_grid;
    //debugTestOutput();
    //debugTestRead();
    //exit(1);
#endif //DEBUG
    
    //level
    uint8_t level_number;
    uint8_t level_number_len;
    bool level_start_finished;
    char * level_type_text;

    //grid, shooter
    shooter_t shooter;
    grid_t grid;
    gfx_sprite_t * grid_buffer;
    gfx_sprite_t * behind_proj_sprite;
    gfx_sprite_t * behind_shooter_sprite;
    //partial redraw
    bool prev_proj_visible;

    point_t prev_shooter = {-1, -1};
    point_t prev_proj = {-1, -1};
    //key presses

    //pop_sprite
    gfx_sprite_t * pop_sprite;
    gfx_sprite_t * pop_sprite_rotations[3];

    gfx_sprite_t * behind_pop_sprites;
    
    bubble_list_t animation_list;
    gfx_sprite_t * lose_animation_behind;
    
    //grid settings
    max_color = 3; //max color index, 0 to max_color inclusive
    new_row_rate = 10;
    if (current_game == SURVIVAL) {
        push_down_time = 16777215; //max int
    } else {
        push_down_time = 6;
    }
    turn_counter = 0;
    global_counter = 0;
    player_score = 0;
    fps = last_fps = 30;
    fps_string = malloc(15 * sizeof(char));

    //level
    level_number = 0;
    level_number_len = 1;
    level_start_finished = false;
    level_start_y_pos = 0;
    level_type_text = "";

    //shooter
    shooter.x = 160 - (TILE_WIDTH >> 1);
    shooter.y = 220 - (TILE_HEIGHT >> 1);
    shooter.angle = 0;
    shooter.pal_index  = (sizeof_bubble_pal >> 1) + 7;
    shooter.projectile.x = 0;
    shooter.projectile.y = 0;
    shooter.projectile.speed = 5;
    for (i = 0; i < 3; i++)
        shooter.next_bubbles[i] = randInt(0, max_color);
    shooter.flags = DEACTIVATED;
    shooter.vectors = generateVectors(-64, 64, 4);
    shooter.projectile.color = shooter.next_bubbles[0];
    shooter.projectile.visible = false;
    shooter.projectile.angle = 0;
    shooter.counter = 0;
    //grid
    grid.cols = 7;
    grid.rows = MAX_ROWS;
    grid.x = centerX(((TILE_WIDTH * grid.cols) + (TILE_WIDTH >> 1)), LCD_WIDTH);
    grid.y = 0;
    grid.ball_diameter = TILE_WIDTH - 3; //TILE_WIDTH/2 is way too small, TILE_WIDTH can feel too big
    grid.width = (TILE_WIDTH * grid.cols) + (TILE_WIDTH>>1);
    grid.height = (ROW_HEIGHT * grid.rows) + (TILE_WIDTH>>2);
    grid.bubbles = (bubble_t *) malloc((grid.cols*grid.rows) * sizeof(bubble_t));
    if (grid.bubbles == NULL) exit(1);
    grid.possible_collisions.bubbles = NULL;
    //declare an image buffer for the grid
    grid_buffer = gfx_MallocSprite(grid.cols * TILE_WIDTH + (TILE_WIDTH>>1),ROW_HEIGHT * MAX_ROWS + (TILE_WIDTH>>2));
    if (grid_buffer == NULL) exit(1);
    game_flags = RENDER | NEW_LEVEL;
    auto_new_row_counter = 0;

    srand(rtc_Time());
    available_colors = (uint8_t *) malloc((MAX_POSSIBLE_COLOR + 2) * sizeof(uint8_t));
    setAvailableColors(available_colors, 0x7F); // all colors
    initGrid(grid,grid.rows,grid.cols, 10, NULL);
    grid.possible_collisions = getPossibleCollisions(grid);
    
    //pop_cluster
    pop_cluster.bubbles = pop_cluster_bubbles;

    //popping animation sprites
    pop_counter = 0;
    pop_started = false;
    for (i = 0; i < 3; i++) {
        pop_sprite_rotations[i] = gfx_MallocSprite(bubble_red_pop_width, bubble_red_pop_height);
        if (pop_sprite_rotations[i] == NULL)
            exit(1);
    }
    //falling animation
    fall_counter = 0;
    fall_started = false;
    fall_data.bubbles = fall_data_bubbles;
    fall_data.size = 0;
    fall_total = 0;

    //redraw
    behind_shooter_sprite = gfx_MallocSprite(TILE_WIDTH, TILE_HEIGHT);
    if (!behind_shooter_sprite) {
        debug_message("behind_shooter_sprite alloc fail!?>!! >:(");
        exit(1);
    }
    behind_proj_sprite = gfx_MallocSprite(TILE_WIDTH, TILE_HEIGHT);
    if (!behind_proj_sprite) {
        debug_message("behind_proj_sprite alloc fail!!! >:(");
        exit(1);
    }
    prev_proj_visible = false;

    pop_behind_size = 0;
    fall_behind_size = 0;

    memset(pop_behind, 0, ANIM_POP_BEHIND_MAX * sizeof(anim_behind_t));
    memset(fall_behind, 0, ANIM_FALL_BEHIND_MAX * sizeof(anim_behind_t));

    game_status = RUNNING;

    strcpy(fps_string,"FPS: ");
    fps_counter = 0;
    row_offset = 0;

#ifdef DEBUG
    x = y = 0;
    highlight_timer = 0;
    move_bubble_grid = false;
#endif
    gfx_FillScreen(255);
    /*Initialize timer*/ 
    timer_Control = TIMER1_DISABLE;
    timer_1_Counter = 0;
    timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_UP;

    /*Main game*/
    //Starting level
    /*
     Note: Peform graphics buffer-using logic before clearing screen
     */
    key = 255; //invalid
    while (true) {
        kb_Scan();
        prevkey = key;
        key = kb_AnyKey();
        kb_2nd_prev = kb_2nd_press;
        kb_2nd_press = kb_Data[1] & kb_2nd;
        kb_clear_prev = kb_clear_press;
        kb_clear_press = kb_Data[6] & kb_Clear;
        kb_up_prev = kb_up_press;
        kb_up_press = kb_Data[7] & kb_Up;
        kb_down_prev = kb_down_press;
        kb_down_press = kb_Data[7] & kb_Down;
        /*if (game_flags & RENDER) {
            renderGrid(grid, grid_buffer);
            game_flags &= ~RENDER;
        }*/
        
        //gfx_FillScreen(255);  //Goal: change render method to partial
        if (game_flags & NEW_LEVEL) {
            if (!level_start_finished) {
                gfx_SetTextScale(4, 4);
                if (level_start_y_pos == 0) {
                    gfx_palette[0] = WHITE;
                }
                switch (current_game) {
                    case SURVIVAL:
                        level_type_text = option_strings[SURVIVAL];
                        break;
                    case LEVELS:
                        level_type_text = "Level";
                        break;
                    default:
                        level_type_text = "";
                        break;
                }
                if (level_start_y_pos < 64) {
                    gfx_palette[0] = ((32 - (level_start_y_pos >> 1)) << 11) | ((64 - level_start_y_pos) << 5) | (32 - (level_start_y_pos >> 1));
                    level_start_y_pos += 2;
                }
                gfx_FillScreen(255);
                if (current_game == SURVIVAL) {
                    gfx_PrintStringXY(level_type_text, 0, level_start_y_pos);
                } else if (current_game == LEVELS) {
                    gfx_PrintStringXY(level_type_text, 0, level_start_y_pos);
                    gfx_PrintUIntXY(level_number + 1, level_number_len, gfx_GetStringWidth(level_type_text), level_start_y_pos);
                }
                if (single_release(key, prevkey) && (level_start_y_pos >= 40)) {
                    gfx_palette[0] = BLACK;
                    gfx_SetTextScale(1, 1);
                    shooter.flags &= ~DEACTIVATED;
                    level_start_finished = true;
                    game_flags &= ~NEW_LEVEL;
                    kb_Reset();
                }
                gfx_BlitBuffer();
                continue;
            } else {
                if (grid.rows < 10) {
                    grid.rows++;
                    grid.y -= ROW_HEIGHT;
                    grid.height += ROW_HEIGHT;
                } else {
                    shooter.flags &= ~DEACTIVATED;
                    game_flags &= ~NEW_LEVEL;
                }
            }
        }
        if (kb_Data[7] & kb_Left) {
#ifdef DEBUG
            x -= (x > 0);
#endif
            if (shooter.angle > LBOUND) {
                shooter.angle -= SHOOTER_STEP;
                //shooter.flags |= REDRAW_SHOOTER;
            }
            
        }
        if (kb_Data[7] & kb_Right) {
#ifdef DEBUG
            x += (x < grid.cols-1);
#endif
            if (shooter.angle < RBOUND) {
                shooter.angle += SHOOTER_STEP;
                //shooter.flags |= REDRAW_SHOOTER;
            }
        }
        #ifdef DEBUG //in debug mode, we want to disable moving the grid when comma is pressed
        if (move_bubble_grid) {
        #endif
        if (single_press(kb_up_press, kb_up_prev)) { //TODO: Define max rows based on mode
            if (!(shooter.flags & ACTIVE_PROJ)) {
                if (grid.rows < MAX_ROWS) {
                        grid.rows++;
                        grid.y -= ROW_HEIGHT;
                        grid.height += ROW_HEIGHT;
                        game_flags |= RENDER;
                }
            }
        }
        if (single_press(kb_down_press, kb_down_prev)) {
            if (!(shooter.flags & ACTIVE_PROJ)) {
                if (grid.rows > MIN_ROWS) {
                    if (!rowHasBubbles(grid, grid.rows - 2)) {
                        grid.rows--;
                        grid.y += ROW_HEIGHT;
                        grid.height -= ROW_HEIGHT;
                        game_flags |= RENDER;
                    }
                }
            }
        }
        #ifdef DEBUG
        }
        #endif
#ifdef DEBUG
        if (kb_Data[7] & kb_Up && kb_Data[7])
            y -= (y > 0);
        if (kb_Data[7] & kb_Down)
            y += (y < grid.rows - 1);
#endif
        /*Shoot bubbles*/
        if (kb_Data[1] & kb_2nd) {
            if (!(shooter.flags & DEACTIVATED)) {
                if (!(shooter.flags & ACTIVE_PROJ)) {
                    if (grid.possible_collisions.size) {
                        shooter.projectile.x = shooter.x;
                        shooter.projectile.y = shooter.y;
                        shooter.projectile.speed = 5;
                        shooter.projectile.angle = shooter.angle;
                        shooter.projectile.color = shooter.next_bubbles[0];
                        j = 0;
                        for (i = 1; i < available_colors[0] + 1; i++) {
                            if (available_colors[i] == shooter.next_bubbles[1]) {
                                shooter.next_bubbles[0] = shooter.next_bubbles[1];
                                j = 1;
                                break;
                            }
                        }
                        if (!j) {
                            shooter.next_bubbles[0] = available_colors[randInt(1, available_colors[0])];
                        }
                        shooter.next_bubbles[1] = shooter.next_bubbles[2];
                        getAvailableColors(grid, available_colors);
                        if (!available_colors[0]) {
                            available_colors[0] = 1; //one color
                            available_colors[1] = 0; //red
                        }
                        shooter.next_bubbles[2] = available_colors[randInt(1, available_colors[0])];
                        shooter.projectile.visible = true;
                        shooter.flags |= ACTIVE_PROJ;
                    } else {
                        if (!(game_flags & POP)) {
                            shooter.flags |= SHAKE;
                            shooter.counter = (uint8_t) fps / 3;
                        }
                    }
                }
            } else {
                shooter.flags |= SHAKE; //start shaking animation
                shooter.counter = (uint8_t) fps / 3; //shake for 1/3 second
            }
        }
        if (single_press(kb_clear_press, kb_clear_prev)) {
            game_status = QUIT;
        }
#ifdef DEBUG
        /*Debug: Show neighbors*/
        if (kb_Data[2] & kb_Alpha) {
            while (kb_Data[2] & kb_Alpha) kb_Scan();
            debug_bubble = grid.bubbles[(y * grid.cols) + x];
            neighbors = getNeighbors(grid,debug_bubble.x,debug_bubble.y,false);
            gfx_SetColor(255);
            debug_point.x = gfx_GetStringWidth("Current:");
            debug_point.y = 0;
            gfx_FillRectangle(0, 0, 100, (debug_point.y + neighbors.size) << 3);
            gfx_PrintStringXY("Current:", 0, 0);
            gfx_PrintStringXY("(", debug_point.x, 0);
            gfx_PrintUIntXY(debug_bubble.x, 2, debug_point.x + 8, 0);
            gfx_PrintStringXY(",", debug_point.x + 24, debug_point.y);
            gfx_PrintUIntXY(debug_bubble.y, 2, debug_point.x + 40, 0);
            gfx_PrintStringXY(")", debug_point.x + 56, debug_point.y);
            gfx_PrintUIntXY(debug_bubble.color, 2, debug_point.x + 64, 0);

            debug_point.x = gfx_GetStringWidth("Neighbors:");
            debug_point.y = 32;
            gfx_PrintStringXY("Neighbors:",0,debug_point.y);
            for (i = 0; i < neighbors.size; i++) {
                j = debug_point.y + (i << 3);
                gfx_PrintStringXY("(",debug_point.x,j);
                gfx_PrintUIntXY(neighbors.bubbles[i].x, 2,debug_point.x + 8, j);
                gfx_PrintStringXY(",", debug_point.x + 24, debug_point.y);
                gfx_PrintUIntXY(neighbors.bubbles[i].y, 2, debug_point.x + 40, j);
                gfx_PrintStringXY(")", debug_point.x + 56, debug_point.y);
                gfx_PrintUIntXY(neighbors.bubbles[i].color, 2, debug_point.x + 64, j);
            }
            gfx_BlitBuffer();
            while(!os_GetCSC());
        }
        /*Debug: Foundcluster*/
        if (kb_Data[1] & kb_Mode) {
            while (kb_Data[1] & kb_Mode) kb_Scan();
            foundcluster = findCluster(grid, x, y, true, true, false);
            gfx_SetColor(255);
            gfx_FillRectangle(220, 0, 100, foundcluster.size<<3);
            gfx_PrintStringXY("Foundcluster:", 220, 0);
            for (i = 0; i < foundcluster.size; i++){
                debug_point.x = 220;
                debug_point.y = 16 + (i<<4);
                gfx_PrintUIntXY(foundcluster.bubbles[i].x, 2, debug_point.x, debug_point.y);
                gfx_PrintUIntXY(foundcluster.bubbles[i].y, 2, debug_point.x + 24, debug_point.y);
                gfx_PrintUIntXY(foundcluster.bubbles[i].color, 2, debug_point.x + 56, debug_point.y);
                gfx_TransparentSprite(bubble_sprites[foundcluster.bubbles[i].color], LCD_WIDTH - TILE_WIDTH, 16 + (i<<4));
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
        /*Debug: Available colors*/
        if (kb_Data[4] & kb_Prgm) {
            gfx_FillScreen(255);
            gfx_PrintStringXY("Size: ",0,0);
            gfx_PrintUIntXY(available_colors[0],1,48,0);
            gfx_PrintStringXY("Max color (inclusive):",0,8);
            gfx_PrintUIntXY(max_color,1,176,8);
            for (j = 0;j < available_colors[0];j++) {
                gfx_PrintUIntXY(available_colors[j+1],8,120,16+j*8);
            }
            gfx_BlitBuffer();
            while(!os_GetCSC());
        }
        /*Debug: Possible collisions*/
        if (kb_Data[5] & kb_Vars) {
            gfx_FillScreen(255);
            grid.possible_collisions = getPossibleCollisions(grid);
            for (i = 0;i < grid.possible_collisions.size;i++) {
                point = getTileCoordinate(grid.possible_collisions.bubbles[i].x,grid.possible_collisions.bubbles[i].y);
                point.x += grid.x;
                point.y += grid.y;
                drawTile(grid.possible_collisions.bubbles[i].color,point.x,point.y);
            }
            gfx_BlitBuffer();
            while (!os_GetCSC());
        }
        /*Debug: Pushdown*/
        if (kb_Data[2] & kb_Recip) {
            pushDown(&grid);
            while(!os_GetCSC());
        }
        /*Debug: Shift Rate*/
        if (kb_Data[3] & kb_Sin) {
            new_row_rate--;
        }
        if (kb_Data[4] & kb_Cos) {
            new_row_rate++;
        }
        if (kb_Data[5] & kb_Tan) {
            push_down_time--;
        }
        if (kb_Data[6] & kb_Power) {
            push_down_time++;
        }
        /*Debug: Set deactivated*/
        if (kb_Data[2] & kb_Square) {
            shooter.flags ^= DEACTIVATED;
        }
        if (kb_Data[3] & kb_Comma) {
            move_bubble_grid = false;
        } else {
            move_bubble_grid = true;
        }
#endif //DEBUG
        if (game_flags & POP) {
            if (!pop_started) {
                pop_sprite = bubble_pop_sprites[pop_cluster.bubbles[0].color];
                pop_sprite_rotations[0] = gfx_FlipSpriteY(pop_sprite,pop_sprite_rotations[0]);
                pop_sprite_rotations[1] = gfx_FlipSpriteX(pop_sprite,pop_sprite_rotations[1]);
                pop_sprite_rotations[2] = gfx_FlipSpriteX(pop_sprite_rotations[0],pop_sprite_rotations[2]);
                pop_behind_size = min(pop_cluster.size, ANIM_POP_BEHIND_MAX);
                for (i = 0;i < pop_behind_size; i++) {
                    point = getTileCoordinate(pop_cluster.bubbles[i].x,pop_cluster.bubbles[i].y);
                    pop_locations[i].x = point.x + grid.x;
                    pop_locations[i].y = point.y + grid.y;
                    if (!pop_behind[i].valid) {
                        if (!pop_behind[i].sprite) {
                            pop_behind[i].sprite = gfx_MallocSprite(TILE_WIDTH, TILE_HEIGHT);
                            if (pop_behind[i].sprite == NULL) {
                                exit(1); //:(
                            }
                        }
                        pop_behind[i].pos = pop_locations[i];
                        pop_behind[i].img = ANIM_BUBBLE;
                        pop_behind[i].valid = false;
                    }
                }
                pop_started = true;
            }
            if (pop_counter == 20) {
                pop_counter = 0;
                pop_started = false;
                game_flags &= ~POP;
                for (i = 0; i < ANIM_POP_BEHIND_MAX; i++) {
                    restoreBehindSprite(&pop_behind[i]);
                }
                pop_cluster.size = 0;
            }
        }
        if (game_flags & FALL) {
            if (!fall_started) {
                fall_counter = 0;
                fall_started = true;
                fall_behind_size = fall_data.size;
                for (i = 0; i < fall_behind_size; i++) {
                    if (!fall_behind[i].valid) {
                        if (!fall_behind[i].sprite) {
                            fall_behind[i].sprite = gfx_MallocSprite(TILE_WIDTH, TILE_HEIGHT);
                            if (fall_behind[i].sprite == NULL) {
                                exit(1); //>:((((
                            }
                        }
                    }
                    fall_behind[i].valid = false;
                }
            }
            //Animate
            for (i = 0; i < fall_total; i++) {
                restoreBehindSprite(&fall_behind[i]);
                if (fall_data.bubbles[i].y < LCD_HEIGHT) {
                    fall_data.bubbles[i].x += (int8_t) fall_data.bubbles[i].velocity;
                    fall_data.bubbles[i].y += (fall_counter * 5) >> 2;
                }
            }
            fall_counter++;
            if (fall_counter == 25) {
                fall_started = false;
                game_flags &= ~FALL;
            }
        }
        if (game_flags & CHECK) {
            if (rowHasBubbles(grid, grid.rows - 1)) {
                if (current_game == SURVIVAL) {
                    if (grid.rows < MAX_ROWS) {
                        grid.rows++;
                        grid.y -= ROW_HEIGHT;
                        grid.height += ROW_HEIGHT;
                        game_flags |= RENDER;
                    } else {
                        game_status = LOSE;
                    }
                }
            }
            j = 0;
            for (i = 0; i < grid.cols * grid.rows; i++) {
                if (!(grid.bubbles[grid.cols * (grid.rows - 1) + i].flags & EMPTY)) {
                    j = 1; //Grid is not empty
                    break;
                }
            }
            if (!j) {
                #ifdef DEBUG
                dbg_printf("New level!");
                #endif
                if (current_game == LEVELS) {
                    game_flags |= NEW_LEVEL;
                }
            }
        }
        if (game_flags & PUSHDOWN) {
            pushDown(&grid);
            game_flags &= ~PUSHDOWN;
        }
        if (game_flags & CHECK) { // Check for game over after popping bubbles
            grid.possible_collisions = getPossibleCollisions(grid);
            if (grid.possible_collisions.size == 0) {
                switch (current_game) {
                    case SURVIVAL: //goal: add a 40000pt bonus message for clearing the board
                        player_score += 40000;
                        if (!auto_new_row_counter) {
                            auto_new_row_counter = 8;
                            game_flags |= AUTO_FILL;
                        }
                        setAvailableColors(available_colors, (1 << (max_color + 1)) - 1);
                         // force a grid shift
                        //addNewRow(&grid, grid.available_colors, 9);

                        new_row_rate = (new_row_rate > 3) ? new_row_rate - 1 : 3;
                        break;
                    default:
                        break;
                }
            }
            if (current_game == SURVIVAL) {
                if (max_color < MAX_POSSIBLE_COLOR && global_counter && global_counter % 15 == 0) {
                    available_colors[++available_colors[0]] = ++max_color;
                }
            }
            game_flags &= ~CHECK;
        }
        if (current_game == SURVIVAL && auto_new_row_counter) {
            auto_new_row_counter--;
            if (!auto_new_row_counter) {
                shooter.flags &= ~DEACTIVATED;
            }
            game_flags |= NEW_ROW | RENDER;
        }
        
        //Move the projectile
        
        if (shooter.flags & ACTIVE_PROJ) {
            fps_ratio = fps / last_fps;
            moveProj(grid, &shooter, fps_ratio * 2);
        }
        
        if (game_flags & NEW_ROW) {
            addNewRow(grid, available_colors, 9);
            if (game_flags & AUTO_FILL) {
                if (rowHasBubbles(grid, grid.rows - 1)) { //last row full, push upward
                    grid.rows++;
                    grid.y -= ROW_HEIGHT;
                    grid.height += ROW_HEIGHT;
                }
                if (!auto_new_row_counter) {
                    game_flags &= ~AUTO_FILL;
                }
            }
            if (current_game == SURVIVAL && !auto_new_row_counter) {
                grid.possible_collisions = getPossibleCollisions(grid);
            }
            game_flags &= ~NEW_ROW;
        }

        //Display
        if (!(game_flags & NEW_LEVEL)) {
            if (shooter.flags & SHAKE) {
                #ifdef DEBUG
                dbg_printf("shaker: (%d, %d) %d\n",(shooter.shake_values & 0xF0) >> 4, shooter.shake_values & 0x0F, shooter.counter);
                #endif
                if (!shooter.counter) {
                    shooter.flags &= ~SHAKE;
                } else {
                    shooter.shake_values = (randInt(-2, 2) << 4) | (randInt(-2, 2) & 15);
                    shooter.counter--;
                }
            } else {
                shooter.shake_values = 0;
            }

            //create grid sprite
            if (game_flags & RENDER) {
                renderGrid(grid, grid_buffer);
                game_flags &= ~RENDER;
                if (grid.y) {
                    gfx_SetColor(255); //WHITE
                    gfx_FillRectangle(grid.x, 0, grid.width, grid.y);
                }
                gfx_TransparentSprite(grid_buffer, grid.x, grid.y);
                if (pop_started) { //capture area behind popping bubbles
                    for (i = 0; i < pop_behind_size; i++) {
                        if (pop_behind[i].img == ANIM_BUBBLE) {
                            captureBehindSprite(&pop_behind[i], pop_locations[i].x, pop_locations[i].y);
                        }
                    }
                }
                if (fall_started) { //capture area behind falling bubbles
                    for (i = 0; i < fall_behind_size; i++) {
                        captureBehindSprite(&fall_behind[i], fall_data.bubbles[i].x, fall_data.bubbles[i].y);
                    }
                }
                if (prev_proj_visible) {
                    gfx_GetSprite(behind_proj_sprite, prev_proj.x, prev_proj.y);
                }

            }
            if (shooter.flags & PROJ_HIT) { //show projectile for impact frame
                shooter.projectile.visible = false;
                shooter.flags &= ~PROJ_HIT;
            }

            gfx_SetColor(0); //BLACK
            gfx_Rectangle(grid.x, grid.y, grid.width, grid.height - ROW_HEIGHT);

            /*if (prev_shooter.x >= 0) {
                gfx_Sprite(behind_shooter_sprite, prev_shooter.x, prev_shooter.y);
            }*/
            //gfx_GetSprite(behind_shooter_sprite, shooter.x, shooter.y);
            renderShooter(shooter);
            //prev_shooter.x = shooter.x;
            //prev_shooter.y = shooter.y;

            //if (prev_proj_visible) {
            //    gfx_Sprite(behind_proj_sprite, prev_proj.x, prev_proj.y);
            //}
            if (shooter.flags & PROJ_HIT) { //show projectile for impact frame
                shooter.projectile.visible = false;
                shooter.flags &= ~PROJ_HIT;
            }
            
            if (shooter.projectile.visible) {
                //gfx_GetSprite(behind_proj_sprite, shooter.projectile.x, shooter.projectile.y);
                gfx_TransparentSprite(bubble_sprites[shooter.projectile.color], shooter.projectile.x, shooter.projectile.y);
                prev_proj.x = shooter.projectile.x;
                prev_proj.y = shooter.projectile.y;
                prev_proj_visible = true;
            } else {
                prev_proj_visible = false;
            }

            if (fall_started) { //capture area behind falling bubbles
                for (i = 0; i < fall_behind_size; i++) {
                    captureBehindSprite(&fall_behind[i], fall_data.bubbles[i].x, fall_data.bubbles[i].y);
                }
            }

            if (pop_started) {
                if (pop_counter & 1) {
                    for (i = 0; i < pop_behind_size; i++) {
                        switch (pop_behind[i].img) {
                            case ANIM_BUBBLE:
                                captureBehindSprite(&pop_behind[i], pop_locations[i].x, pop_locations[i].y);       
                        }
                    }
                }
                for (i = 0; i < pop_behind_size; i++) { //animate
                    switch (pop_behind[i].img) {
                        case ANIM_BUBBLE:
                            if (pop_counter & 1) {
                                drawTile(pop_cluster.bubbles[i].color, pop_locations[i].x, pop_locations[i].y);
                            } else {
                                restoreBehindSprite(&pop_behind[i]);       
                            }
                            break;
                        default:
                            break;
                    }
                    point.x = pop_locations[i].x - pop_counter;
                    point.y = pop_locations[i].y - pop_counter;
                    //disable pop particles for now
                    /*if (pop_counter & 2 || !pop_counter) {
                        gfx_TransparentSprite(pop_sprite,point.x,point.y);
                        point.x = pop_locations[i].x + pop_counter + (TILE_WIDTH>>1);
                        gfx_TransparentSprite(pop_sprite_rotations[0],point.x,point.y);
                        point.y = pop_locations[i].y + pop_counter + (TILE_WIDTH>>1);
                        gfx_TransparentSprite(pop_sprite_rotations[2],point.x,point.y);
                        point.x = pop_locations[i].x - pop_counter;
                        gfx_TransparentSprite(pop_sprite_rotations[1],point.x,point.y);
                    }*/
                }
                pop_counter++;
            }
            if (fall_started) {
                for (i = 0; i < fall_data.size; i++) {
                    if (fall_data.bubbles[i].y < LCD_HEIGHT) {
                        drawTile(fall_data.bubbles[i].color, fall_data.bubbles[i].x, fall_data.bubbles[i].y);
                    }
                }
            }
            /*if (current_game == SURVIVAL) {
             gfx_PrintStringXY("SURVIVAL!", 0, 0);
             } else if (current_game == LEVELS) {
             gfx_PrintStringXY("Level", 0, 0);
             gfx_PrintUIntXY((int) level_number + 1, level_number_len, 48, 0);
             }*/
            gfx_SetColor(255);
            #ifndef DEBUG
            gfx_FillRectangle(0, 0, grid.x - 1, 64);
            #endif
            #ifdef DEBUG
            gfx_FillRectangle(0, 0, grid.x - 1, 120);
            #endif
            gfx_PrintStringXY(level_type_text, 0, 0);
            if (current_game == LEVELS) {
                gfx_PrintUIntXY((int) level_number + 1, level_number_len, 48, 0);
            }
            gfx_PrintStringXY("Turn:",0,24);
            gfx_PrintUIntXY(turn_counter,3,48,24);
            gfx_PrintStringXY("Row rate:",0,32);
            gfx_PrintUIntXY(new_row_rate,3,74,32);
            gfx_PrintStringXY("Push time:",0,40);
            if (push_down_time == 16777215) {
                gfx_PrintStringXY("Max", 74, 40);
            } else if (push_down_time == 0) {
                gfx_PrintStringXY("NW", 74, 40);
            } else {
                gfx_PrintIntXY(push_down_time,3,74,40);
            }
            gfx_PrintStringXY("Score: ",0,48);
            gfx_PrintUIntXY(player_score, 6, 52, 48);
            gfx_PrintStringXY("Angle:", 0, 56);
            gfx_PrintIntXY(shooter.angle, 3, 74, 56);
            #ifdef DEBUG
            gfx_FillRectangle(240, 0, 80, 16);
            gfx_PrintStringXY("Rows:", 240, 0);
            gfx_PrintIntXY(grid.rows, 3, 280, 0);
            gfx_PrintStringXY("Cols:", 240, 8);
            gfx_PrintIntXY(grid.cols, 3, 280, 8);
            #endif
#ifdef DEBUG
            /*Draw a highlight for the highlighted square*/
            if ((highlight_timer-1) & 1) {
                gfx_SetColor(5);
                debug_point = getTileCoordinate(x,y);
                gfx_FillRectangle(grid.x+debug_point.x,grid.y+debug_point.y,TILE_WIDTH,TILE_HEIGHT);
                drawTile(grid.bubbles[y * grid.cols + x].color,grid.x+debug_point.x,grid.y+debug_point.y);
            }
            highlight_timer++;
            gfx_PrintStringXY("X:",0,LCD_HEIGHT-24);
            gfx_PrintStringXY("Y:",0,LCD_HEIGHT-16);
            gfx_PrintUIntXY(x,2,20,LCD_HEIGHT-24);
            gfx_PrintUIntXY(y,2,20,LCD_HEIGHT-16);
            if (game_flags & RENDER) {
                gfx_PrintStringXY("game:RENDER",0,64);
            }
            if (game_flags & NEW_ROW) {
                gfx_PrintStringXY("game:NEWROW",0,72);
            }
            if (game_flags & POP) {
                gfx_PrintStringXY("game:POP",0,80);
            }
            if (game_flags & FALL) {
                gfx_PrintStringXY("game:FALL",0,88);
            }
            if (game_flags & PUSHDOWN) {
                gfx_PrintStringXY("game:PUSHDOWN",0,96);
            }
            if (game_flags & CHECK) {
                gfx_PrintStringXY("game:CHECK",0,104);
            }
            if (shooter.projectile.visible == true) {
                gfx_PrintStringXY("proj:VISIBLE",0,112);
            }
#endif //DEBUG
        }
        ticks = (float)atomic_load_increasing_32(&timer_1_Counter) / 32768;
        last_fps = fps;
        fps =  1.0 / ticks;
        timer_Control = TIMER1_DISABLE;
        timer_1_Counter = 0;
        timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_UP;
        gfx_SetTextScale(1, 1);
        gfx_PrintStringXY(fps_string, 0, LCD_HEIGHT - 8);
        if (++fps_counter == 7) {
            strcpy(&fps_string[5], printfloat(fps));
            fps_counter = 0;
        }
        gfx_SetTextFGColor(0);
        gfx_BlitBuffer();
        if (game_status == QUIT) break;
        if (game_status == LOSE || game_status == WIN) break;
    }
    //End of game animation
    if (game_status != QUIT) {
        //init partial redraw
        end_of_game_string = game_status == LOSE ? lose_string : win_string;
        k = 1;
        i = gfx_GetStringWidth(end_of_game_string);
        lose_animation_behind = gfx_MallocSprite(i,8);
        if (lose_animation_behind == NULL) {
            debug_message("lose_animation_behind null!!!! :(");
            gfx_FillScreen(255);
            gfx_PrintStringXY(end_of_game_string, 0, 0);
            gfx_BlitBuffer();
            while (!os_GetCSC());
            exit(1);
        }
        point.x = (LCD_WIDTH>>1)-(i>>1);
        point.y = (LCD_HEIGHT>>1)-4;
        gfx_GetSprite(lose_animation_behind,point.x,point.y);
        gfx_PrintStringXY(end_of_game_string,point.x,point.y);
        while (k < 4) {
            timer_Control = TIMER1_DISABLE;
            timer_1_Counter = 0;
            timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_UP;
            while (timer_1_Counter < 2730); // 32768 / 60 * 5
            k++;
            //clear old background
            gfx_Sprite(lose_animation_behind,point.x,point.y);
            free(lose_animation_behind);
            //movement code
            gfx_SetTextScale(k, k);
            i = gfx_GetStringWidth(end_of_game_string);
            lose_animation_behind = gfx_MallocSprite(i, k<<3);
            if (lose_animation_behind == NULL) {
                debug_message("lose_animation_behind null :O");
                gfx_FillScreen(255);
                gfx_PrintStringXY(end_of_game_string, 0, 0);
                gfx_BlitBuffer();
                while (!os_GetCSC());
                exit(1);
            }
            point.x = (LCD_WIDTH>>1)-(i>>1);
            point.y = (LCD_HEIGHT>>1)-(4*k);
            //get new background and print new sprite/string
            gfx_GetSprite(lose_animation_behind,point.x,point.y);
            gfx_PrintStringXY(end_of_game_string,point.x,point.y);
            gfx_BlitBuffer();
        }
        while (!os_GetCSC());
        #ifdef DEBUG
        dbg_printf("exiting game loop\n");
        #endif
        free(lose_animation_behind);
    }
    game_flags = 0x00;
    shooter.flags = 0x00;
    shooter.projectile.x = shooter.projectile.y = shooter.projectile.speed = shooter.projectile.color = 0;
    free(grid.bubbles);
    free(grid_buffer);
    free(available_colors);
    fall_data.size = fall_total = 0;
    for (i = 0; i < 3; i++) {
        free(pop_sprite_rotations[i]);
    }
    for (i = 0; i < ANIM_POP_BEHIND_MAX >> 1; i++) {
        if (pop_behind[i].sprite) {
            free(pop_behind[i].sprite);
        }
    }
    for (i = 0; i < ANIM_FALL_BEHIND_MAX >> 1; i++) {
        if (fall_behind[i].sprite) {
            free(fall_behind[i].sprite);
        }
    }
}
