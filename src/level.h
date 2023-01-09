#if !defined(LEVEL_H)
#define LEVEL_H
#include <ti/screen.h>

#include <bubble.h>
#include <fileioc.h>
#define PACK_HEADER_BEGIN_SIZE 12
#define PACK_HEADER_VERSION_SIZE 3
#define PACK_HEADER_N_OF_LEVELS_SIZE 1
#define PACK_NAME_SIZE 24
#define PACK_HEADER_SIZE ((PACK_HEADER_BEGIN_SIZE + 4) + PACK_NAME_SIZE)
typedef struct level {
    uint8_t max_color;      //0 is used for error codes
    uint8_t shift_rate;      //determine when to shift down
    uint8_t push_down_time;  //when to push level down instead of adding more bubbles (set to 0 for no shifting)
    int random_seed;
    char name[25]; //null-terminated
    uint8_t starting_bubbles[3];
    uint8_t cols, rows;
    bubble_list_t data; //number of bubbles, bubbles
    //data.size is only used for appvar storage, loaded data.bubbles array is always cols*rows big
} level_t;

int loadLevel(grid_t grid,shooter_t * shooter,level_t level) {
    int i;
    if (grid.rows * grid.cols != level.rows * level.cols) {
        grid.bubbles = realloc(grid.bubbles,sizeof(bubble_t) * (level.rows * level.cols));
        if (grid.bubbles == NULL) exit(1);
    }
    for (i = 0;i < level.data.size;i++) {
        grid.bubbles[i] = level.data.bubbles[i];
    }
    for (i = level.data.size;i < level.cols*level.rows;i++) {
        grid.bubbles[i] = newBubble(i%level.cols,i/level.cols,0,EMPTY);
    }
    memcpy(shooter->next_bubbles,level.starting_bubbles,sizeof(level.starting_bubbles));
    return i;
}

level_t loadLevelFromFile(uint8_t file_handle,uint8_t offset) {
    //Precondition: file_handle is open.
    level_t level;
    int location;
    int return_code;
    int i;
    
    level.max_color = 0;
    level.cols = level.rows = 0;
    level.data.size = 0;
    level.data.bubbles = NULL;
    location = 0;
    
    return_code = ti_Seek(PACK_HEADER_SIZE,SEEK_SET,file_handle);
    if (return_code == EOF) {
        level.max_color = 0;
        strncpy(level.name,"BUBBLE START ERROR",25);
        return level;
    } //return "\0BUBSTAERROR\0"; //Fail to start search
    while (location < offset) {
        return_code = ti_Read(&level,sizeof(level_t)-sizeof(void *),1,file_handle); //get data.size
        if (return_code != 1) {
            level.max_color = 0;
            strncpy(level.name,"BUBBLE READ ERROR",25);
            return level;
        }  //Fail to read
        return_code = ti_Seek(level.data.size,SEEK_CUR,file_handle);
        if (return_code == EOF) {
            level.max_color = 0;
            strncpy(level.name,"BUBBLE SEEK ERROR",25);
            return level;
        }
        location++;
    }
    return_code = ti_Read(&level,sizeof(level_t)-sizeof(void *),1,file_handle);
    if (return_code != 1) {
        level.max_color = 0;
        strncpy(level.name,"BUBBLE FOUND LEVEL ERROR",25);
        return level;
    }
    
    level.data.bubbles = (bubble_t *) malloc(level.cols*level.rows * sizeof(bubble_t));
    if (level.data.bubbles == NULL) {
        exit(1);
    }
    
    return_code = ti_Read(level.data.bubbles,sizeof(bubble_t),level.data.size,file_handle);
    if (return_code != level.data.size) {
        level.max_color = 0;
        strncpy(level.name,"BUBBLE DATA ERROR",25);
        return level;
    }
    
    for (i = level.data.size;i < level.cols * level.rows;i++) {
        level.data.bubbles[i] = newBubble(i%level.cols,i/level.cols,0,EMPTY);
    }
    
    return level;
}
#endif //LEVEL_H

uint8_t saveLevel(char filename[],level_t level,char pack_name[]) {
    //Returns 0 on success, 1 otherwise
    int return_code;
    uint8_t file_handle;
    file_handle = ti_Open(filename,"w");
    return_code = ti_Write("BUBBLES!LPCK\x00\x01\x00\x01",16,1,file_handle);
    if (return_code != 1)
        return 1;
    return_code = ti_Write(pack_name,24,1,file_handle);
    if (return_code != 1)
        return 1;
    return_code = ti_Write(&level,sizeof(level_t)-sizeof(void *),1,file_handle);
    if (return_code != 1)
        return 1;
    return_code = ti_Write(level.data.bubbles,sizeof(bubble_t),level.data.size,file_handle);
    if (return_code != level.data.size)
        return 1;
    
    return 0;
}

void debugTestOutput() {
    uint8_t i,j;
    level_t l;
    
    l.max_color = 6;
    l.shift_rate = 5;
    l.push_down_time = 25;
    l.random_seed = 0x10312d;
    strcpy(l.name,"Test level name.Test.ing\0");
    l.starting_bubbles[0] = 3;
    l.starting_bubbles[1] = 6;
    l.starting_bubbles[2] = 1;
    l.cols = 10;
    l.rows = 10;
    l.data.size = 80;
    l.data.bubbles = (bubble_t *) malloc(80 * sizeof(bubble_t));
    for(i = 0;i < l.rows;i++) {
        for (j = 0;j < l.cols;j++) {
            l.data.bubbles[(i*l.cols)+j] = newBubble(j,i,i%(l.max_color+1),0);
        }
    }
    /*for (i = 80;i < l.cols*l.rows;i++) {
        l.data.bubbles[i] = newBubble(i%l.cols,i/l.rows,0,EMPTY);
    }*/
    saveLevel("FIRSTLVL",l,"First!First!First!First!");
    os_ClrLCDFull();
    os_SetCursorPos(0,0);
    os_PutStrFull("Successful.");
    while (!os_GetCSC());
}
