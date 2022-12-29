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

extern uint8_t game_flags; //global because our grid is no longer a pointer
extern unsigned int player_score;
extern bool debug_flag;

extern gfx_sprite_t * bubble_sprites[7];
extern const uint16_t bubble_colors[7];

extern bool pop_started;
extern int ** pop_locations;
extern bubble_list_t pop_cluster;
extern uint8_t pop_counter; // timer for pop animation

extern bool fall_started;
extern int ** fall_locations;
extern bubble_list_t * floating_clusters;
extern int floating_clusters_size; //holds foundclusters_size
extern int floating_clusters_total;
extern uint8_t fall_counter;
//bool pop_cluster_freed;
//bool pop_locations_freed;

//extern enum gamestate;
char * printfloat(float elapsed) {
    real_t elapsed_real;
    static char str[10];
    elapsed_real = os_FloatToReal(elapsed <= 0.001f ? 0.0f : elapsed);
    os_RealToStr(str, &elapsed_real, 8, 1, 2);
    return str;
}
int main(void) {
    uint8_t x,y;
    int i,j,k; //universal counter
    
    uint8_t highlight_timer;
    uint8_t fps_counter;
    float fps,last_fps,ticks;
    uint8_t fps_ratio;
    char debug_string[40];
    char * fps_holder;
    char * fps_string;

    point_t debug_point;
    shooter_t shooter;
    grid_t grid;
    gfx_sprite_t * grid_buffer;
    bubble_list_t neighbors;
    bubble_list_t foundcluster;
    bubble_list_t * debug_foundclusters;
    x = y = 0;
    pop_counter = 0;
    fps = last_fps = 30;
    fps_string = malloc(15);

    pop_started = false;
    fall_started = false;
    //shooter
    shooter.x = 160 - (TILE_WIDTH>>1);
    shooter.y = 220 - (TILE_HEIGHT>>1);
    shooter.angle = 0;
    shooter.pal_index  = (sizeof_bubble_pal>>1) + 7;
    shooter.projectile.speed = 5;
    for (i = 0;i < 3;i++)
        shooter.next_bubbles[i] = randInt(0,MAX_COLOR);
    shooter.flags = 0x00;
    
    shooter.projectile.x = 0;
    shooter.projectile.y = 0;
    shooter.projectile.color = shooter.next_bubbles[0];
    shooter.projectile.angle = 0;
    //grid
    grid.x = 160 - (((TILE_WIDTH * 7) + (TILE_WIDTH>>1))>>1);
    grid.y = 0;
    grid.ball_diameter = TILE_WIDTH;
    game_flags = RENDER;
    grid.cols = 7;
    grid.rows = 17; //16 + deadzone
    grid.width = (TILE_WIDTH * grid.cols) + (TILE_WIDTH>>1);
    grid.height = (ROW_HEIGHT * grid.rows) + (TILE_WIDTH>>2);
    grid.bubbles = (bubble_t *) malloc((grid.cols*grid.rows) * sizeof(bubble_t));
    if (grid.bubbles == NULL) exit(1);
    player_score = 0;
    //declare an image buffer for the grid
    grid_buffer = gfx_MallocSprite(grid.cols * TILE_WIDTH + (TILE_WIDTH>>1),ROW_HEIGHT * grid.rows + (TILE_WIDTH>>2));
    if (grid_buffer == NULL) exit(1);
    
    srand(rtc_Time());
    initGrid(grid,grid.rows,grid.cols,10);
    gfx_palette[255] = 0xFFFF;
    gfx_Begin();
    gfx_SetDrawBuffer();
    gfx_SetPalette(bubble_pal,sizeof_bubble_pal,0);
    gfx_SetPalette(bubble_colors,14,sizeof_bubble_pal>>1);
    row_offset = 0;
    
    x = y = 0;
    strcpy(fps_string,"FPS: ");
    fps_counter = 0;
    
    /*Initialize timer*/
    timer_Control = TIMER1_DISABLE;
    timer_1_Counter = 0;
    timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_UP;
    
    highlight_timer = 0;
    
    /*Main game*/
    while (!(kb_Data[6] & kb_Clear)) {
        if (game_flags & RENDER) {
            //gfx_SetDrawBuffer();
            renderGrid(grid,grid_buffer);
            dbg_printf("Location of grid_buffer: %x\n\n",(unsigned int) &grid_buffer);
            game_flags &= ~RENDER;
        }
        gfx_FillScreen(255);  //Change render method (speed!)
        kb_Scan();
        if (kb_Data[7] & kb_Left) {
            x -= (x > 0);
            if (shooter.angle > LBOUND)
                shooter.angle-=4;
        }
        if (kb_Data[7] & kb_Right) {
            x += (x < grid.cols-1);
            if (shooter.angle < RBOUND)
                shooter.angle+=4;
        }
        if (kb_Data[7] & kb_Up)
            y -= (y > 0);
        if (kb_Data[7] & kb_Down)
            y += (y < grid.rows-1);
        /*Shoot bubbles*/
        if (kb_Data[1] & kb_2nd) {
            if (!(shooter.flags & ACTIVE_PROJ)) {
                shooter.projectile.x = shooter.x;
                shooter.projectile.y = shooter.y;
                shooter.projectile.angle = shooter.angle;
                shooter.projectile.color = shooter.next_bubbles[0];
                shooter.next_bubbles[0] = shooter.next_bubbles[1];
                shooter.next_bubbles[1] = shooter.next_bubbles[2];
                shooter.next_bubbles[2] = randInt(0, MAX_COLOR);
                shooter.flags |= ACTIVE_PROJ;
            }
        }
        if (game_flags & POP) {
            if (!pop_started) {
                pop_locations = (int **) malloc(sizeof(int *) * pop_cluster.size);
                dbg_getlocation(pop_cluster.bubbles);
                dbg_getlocation(pop_locations);

                if (pop_locations == NULL) {
                    debug_message("pop_locations null!!!! :(") // add num of writes?
                    exit(1);
                }
                
                for (i = 0;i < pop_cluster.size;i++) {
                    pop_locations[i] = (int *) malloc(sizeof(int *) << 1); //x, y
                    /*Debug: pop_locations
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
                    }*/
                    pop_locations[i][0] = TILE_WIDTH * pop_cluster.bubbles[i].x + grid.x;
                    pop_locations[i][1] = ROW_HEIGHT  * pop_cluster.bubbles[i].y + grid.y;
                }
                pop_started = true;
                dbg_printf("\n");
            }
            for (i = 0;i < pop_cluster.size;i++) { //animate
                gfx_TransparentSprite(bubble_sprites[pop_cluster.bubbles[i].color],
                                      pop_locations[i][0] + pop_counter *
                                      (i%2 == 0 ? -1 : 1),
                                      pop_locations[i][1] + pop_counter *
                                      (i%2 == 0 ? -1 : 1));
            }
            pop_counter++;
            if (pop_counter == 20) {
                pop_counter = 0;
                pop_started = false;
                game_flags &= ~POP;
                for (i = 0;i < pop_cluster.size;i++) {
                    free(pop_locations[i]);
                }
                free(pop_locations);
                free(pop_cluster.bubbles);
                pop_locations = NULL;
//                pop_locations_freed = true;
            }
        }
        /*
        if (game_flags & FALLING) {
            if (!fall_started) {
                fall_started = true;
                fall_counter = 0;
                floating_clusters_total = 0;
                for (i = 0;i < floating_clusters_size;i++) {
                    floating_clusters_total += floating_clusters[i].size;
                }
                fall_locations = (int **) malloc(sizeof(int*) * floating_clusters_total);
                k = 0;
                for (i = 0;i < floating_clusters_total;i++) {
                    for (j = 0;j < floating_clusters[i].size;j++, k++) {
                        fall_locations[k] = (int *) malloc(sizeof(int) * 3); //x, y,xvel
                        fall_locations[k][0] = TILE_WIDTH * floating_clusters[i].bubbles[j].x + grid.x;
                        fall_locations[k][1] = ROW_HEIGHT * floating_clusters[i].bubbles[j].y + grid.y;
                        fall_locations[k][2] = randInt(-5,5);
                    }
                }
            }
            //Animate
            k = 0;
            for (i = 0;i < floating_clusters_size;i++) {
                for (j = 0;j < floating_clusters[i].size;j++, k++) {
                    fall_locations[k][0] += fall_locations[k][2];
                    fall_locations[k][1] += ((fall_counter + 1) * 2);
                    gfx_TransparentSprite(bubble_sprites[floating_clusters[i].bubbles[j].color],fall_locations[k][0],fall_locations[k][1]);
                }
            }
            dbg_printf("Fall counter: %d\n",fall_counter);
            fall_counter++;
            if (fall_counter == 20) {
                fall_counter = 0;
                fall_started = false;
                game_flags &= ~FALLING;
                k = 0;
                for (i = 0;i < floating_clusters_size;i++) {
                    for (j = 0;j < floating_clusters[i].size;j++,k++) {
                        free(floating_clusters[i].bubbles);
                        free(fall_locations[k]);
                    }
                }
                free(floating_clusters);
                free(fall_locations);
                fall_locations = NULL;
            }
        }
        */
        /*Debug: Show neighbors*/
        if (kb_Data[2] & kb_Alpha) {
            while (kb_Data[2] & kb_Alpha) kb_Scan();
            neighbors = getNeighbors(grid,grid.bubbles[(y*grid.cols) + x].x,grid.bubbles[(y*grid.cols) + x].y);
            
            gfx_SetColor(255);
            gfx_FillRectangle(0,16,100,neighbors.size<<3);
            gfx_PrintStringXY("Neighbors:",0,16);
            for (i = 0;i < neighbors.size;i++){
                sprintf(debug_string,"(%d,%d) %d",neighbors.bubbles[i].x,neighbors.bubbles[i].y,neighbors.bubbles[i].color);
                gfx_PrintStringXY(debug_string,gfx_GetStringWidth("Neighbors:"),16+(i<<3));
            }
            gfx_BlitBuffer();
            while(!(kb_Data[6] & kb_Enter)) kb_Scan();
        }
        /*Debug: Foundcluster*/
        if (kb_Data[1] & kb_Mode) {
            while (kb_Data[1] & kb_Mode) kb_Scan();
            foundcluster = findCluster(grid,x,y,true,true,false);
            
            gfx_SetColor(255);
            gfx_FillRectangle(220,0,100,foundcluster.size<<3);
            gfx_PrintStringXY("Foundcluster:",220,0);
            for (i = 0;i < foundcluster.size;i++){
                sprintf(debug_string,"(%d,%d) (%d,%d)",foundcluster.bubbles[i].x,foundcluster.bubbles[i].y,foundcluster.bubbles[i].color,foundcluster.bubbles[i].flags);
                gfx_PrintStringXY(debug_string,220,16+(i<<4));
                gfx_TransparentSprite(bubble_sprites[foundcluster.bubbles[i].color],LCD_WIDTH-TILE_WIDTH,16+(i<<4));
            }
            gfx_BlitBuffer();
            while(!(kb_Data[6] & kb_Enter)) kb_Scan();
            free(foundcluster.bubbles);
        }
        /*Debug: Change shooter color*/
        if (kb_Data[2] & kb_Math) {
            if (!shooter.next_bubbles[0]--)
                shooter.next_bubbles[0] = MAX_COLOR;
            while (kb_Data[2] & kb_Math) kb_Scan();
            game_flags |= RENDER;
        }
        if (kb_Data[3] & kb_Apps) {
            if (shooter.next_bubbles[0]++ == MAX_COLOR)
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
        
        if (kb_Data[4] & kb_Stat) {
            debug_foundclusters = findFloatingClusters(grid);
            dbg_getlocation(debug_foundclusters);
            dbg_printf("Size of debug_foundclusters: %d\n",floating_clusters_size);
            
            for (i = 0;i < floating_clusters_size;i++) {
                dbg_printf("Size of cluster #%d: %d\n",i,debug_foundclusters[i].size);
                for(j = 0;j < debug_foundclusters[i].size;j++) {
                    dbg_printf("Flags of cluster #%d, bubble #%d: %d\n",i,j,debug_foundclusters[i].bubbles[j].flags);
                    drawTile(debug_foundclusters[i].bubbles[j].color,i*16,j*16);
                }
            }
            gfx_BlitBuffer();
            while (kb_Data[4] & kb_Stat) kb_Scan();

            for (i = 0;i < floating_clusters_size;i++) {
                for(j = 0;j < debug_foundclusters[i].size;j++) {
                    free(debug_foundclusters[i].bubbles);
                }
            }
            free(debug_foundclusters);
        }
        //Move the projectile
        if (shooter.flags & ACTIVE_PROJ) {
            fps_ratio = (uint8_t) fps/last_fps;
            fps_ratio = fps_ratio || 1;
            move_proj(grid,&shooter,fps_ratio << 1);
                gfx_TransparentSprite(bubble_sprites[shooter.projectile.color], shooter.projectile.x, shooter.projectile.y);
        }
        //Display
        renderShooter(shooter);
        gfx_TransparentSprite(grid_buffer,grid.x,grid.y); //grid
        gfx_SetTextXY(0,32);
        gfx_PrintString("Score: ");
        gfx_SetTextXY(60,32);
        gfx_PrintInt(player_score,5);
        gfx_PrintStringXY("X:",0,0);
        //Debug
        
        /*Draw a highlight for the highlighted square*/
        if ((highlight_timer-1) & 1) {
            gfx_SetColor(5);
            //gfx_FillRectangle((x*TILE_WIDTH)+(((y+row_offset) % 2)?(TILE_WIDTH>>1):0)+grid.x,(y*ROW_HEIGHT)+grid.y,TILE_WIDTH,TILE_HEIGHT);
            debug_point = getTileCoordinate(x,y);
            gfx_FillRectangle(grid.x+debug_point.x,grid.y+debug_point.y,TILE_WIDTH,TILE_HEIGHT);
            drawTile(grid.bubbles[y * grid.cols + x].color,grid.x+debug_point.x,debug_point.y);
        }
        highlight_timer++;
        gfx_SetColor(0);
        gfx_Rectangle(grid.x,grid.y,grid.width,grid.height-ROW_HEIGHT);
        gfx_PrintStringXY("X:",0,0);
        gfx_PrintStringXY("Y:",0,8);
        gfx_PrintUIntXY(x,2,20,0);
        gfx_PrintUIntXY(y,2,20,8);
        //Debug
        gfx_PrintStringXY(fps_string,0,232);
        fps_counter++;
        if (fps_counter == 7) {
            ticks = (float)atomic_load_increasing_32(&timer_1_Counter) / 32768;
            last_fps = fps;
            fps =  7 / ticks; //(float)fps_counter / ticks;
            fps_holder = printfloat(fps);
            strcpy(&fps_string[5],fps_holder);
            fps_counter = 0;
            timer_Control = TIMER1_DISABLE;
            timer_1_Counter = 0;
            timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_UP;
        }
        gfx_SetTextFGColor(0);
        gfx_BlitBuffer(); //render the whole thing
    }
    game_flags = 0;
    shooter.flags = 0;
    shooter.projectile.x = shooter.projectile.y = shooter.projectile.speed = shooter.projectile.color = 0;
    free(grid.bubbles);
    gfx_End();
}
