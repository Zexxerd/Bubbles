#if !defined(LEVEL_H)
#define LEVEL_H
#include <tice.h>

#include <bubble.h>
#include <fileioc.h>
#define PACK_SIGNATURE_SIZE 12
#define PACK_VERSION_SIZE 3
#define PACK_N_OF_LEVELS_SIZE 1
#define PACK_NAME_SIZE 25
#define PACK_HEADER_SIZE ((PACK_SIGNATURE_SIZE + PACK_VERSION_SIZE + PACK_N_OF_LEVELS_SIZE) + PACK_NAME_SIZE)

#define screen_errormsg(s) do {\
os_SetCursorPos(0,0);\
os_PutStrFull(s);\
while(!os_GetCSC());\
} while (0)
\
typedef struct pack {
    char signature[12]; // BUBBLES!LPCK
    uint8_t version_info[3];
    uint8_t number_of_levels;
    char pack_name[25];
} pack_t;

typedef struct level {
    char name[25]; //null-terminated
    uint8_t max_color;      //0 is used for error codes
    uint8_t shift_rate;      //determine when to shift down
    uint8_t push_down_time;  //when to push level down instead of adding more bubbles (set to 0 for no shifting)
    int random_seed;
    uint8_t colors; // bits 0-6 used
    uint8_t starting_bubbles[3];
    uint8_t cols, rows;
    bubble_list_t data; //number of bubbles, bubbles
    //data.size is only used for appvar storage, loaded data.bubbles array is always cols*rows big
} level_t;

pack_t loadPackData(uint8_t file_handle) {
    //Precondition: file_handle is open for reading.
    pack_t pack;
    unsigned int return_code;
    return_code = ti_Read(&pack, PACK_HEADER_SIZE, 1, file_handle);
    if (return_code != 1) {
        strcpy(pack.pack_name, "ERROR!BUBBLE PACK READ\0\0");
    }
    pack.pack_name[24] = '\0';
    return pack;
}
int loadLevel(grid_t grid, shooter_t * shooter, level_t level) {
    int i;
    if (grid.rows * grid.cols != level.rows * level.cols) {
        grid.bubbles = realloc(grid.bubbles, sizeof(bubble_t) * (level.rows * level.cols));
        if (grid.bubbles == NULL) exit(1);
    }
    for (i = 0; i < level.data.size; i++) {
        grid.bubbles[i] = level.data.bubbles[i];
    }
    for (i = level.data.size; i < level.cols * level.rows; i++) {
        grid.bubbles[i] = newBubble(i % level.cols, i / level.cols, 0, EMPTY);
    }
    memcpy(shooter->next_bubbles, level.starting_bubbles, sizeof(level.starting_bubbles));
    return i;
}

level_t loadLevelFromFile(uint8_t file_handle, uint8_t offset) {
    //Precondition: file_handle is open for reading.
    level_t level;
    int location;
    int return_code;
    int i;

    level.max_color = 0;
    level.shift_rate = 0;
    level.push_down_time = 0;
    level.random_seed = 0;
    level.colors = 0;
    level.cols = level.rows = 0;
    level.data.size = 0;
    level.data.bubbles = NULL;
    location = 0;
    return_code = ti_Seek(PACK_HEADER_SIZE,SEEK_SET,file_handle);
    if (return_code == EOF) {
        strncpy(level.name,"ERROR!BUBBLE START",25);
        return level;
    } //Fail to start search
    while (location < offset) {
        return_code = ti_Read(&level,sizeof(level_t)-sizeof(bubble_t *),1,file_handle); //get data.size
        if (return_code != 1) {
            level.max_color = 0;
            strncpy(level.name,"ERROR!BUBBLE READ",25);
            return level;
        }  //Fail to read
        return_code = ti_Seek(level.data.size * sizeof(bubble_t),SEEK_CUR,file_handle); //level.data.size?
        if (return_code == EOF) {
            level.max_color = 0;
            strncpy(level.name,"ERROR!BUBBLE SEEK",25);
            return level;
        }
        location++;
    }
    return_code = ti_Read(&level, sizeof(level_t) - sizeof(void *), 1, file_handle);
    if (return_code != 1) {
        level.max_color = 0;
        strncpy(level.name, "ERROR!BUBBLE FOUND LEVEL", 25);
        return level;
    }

    level.data.bubbles = (bubble_t *) malloc(level.cols*level.rows * sizeof(bubble_t));
    if (level.data.bubbles == NULL) {
        exit(1);
    }

    return_code = ti_Read(level.data.bubbles, sizeof(bubble_t), level.data.size, file_handle);
    if (return_code != level.data.size) {
        level.max_color = 0;
        strncpy(level.name,"ERROR!BUBBLE DATA",25);
        return level;
    }

    for (i = level.data.size; i < level.cols * level.rows; i++) {
        level.data.bubbles[i] = newBubble(i%level.cols,i/level.cols,0,EMPTY);
    }

    return level;
}

uint8_t savePack(char filename[], pack_t pack, level_t levels[]) {
    //levels array should have (pack.number_of_levels) levels
    uint8_t return_code;
    uint8_t file_handle;
    int i;
    file_handle = ti_Open(filename, "w");
    //pack
    return_code = ti_Write(&pack, sizeof(pack_t), 1, file_handle);
    if (return_code != 1) {
        return 1;
    }
    for (i = 0; i < pack.number_of_levels; i++) {
        return_code = ti_Write(&levels[i], sizeof(level_t) - sizeof(bubble_t *), 1, file_handle);
        if (return_code != 1) {
            dbg_printf("Level number: %d\n", i);
            dbg_printf("Level count: %d\n", pack.number_of_levels);
            dbg_printf("Write size: %d\n", sizeof(level_t) - sizeof(bubble_t *));
            dbg_printf("return code: %d\n", return_code);
            dbg_printf("Loc of failure (&levels[i]): %p", &levels[i]);
            screen_errormsg("Error during level header write");
            return 1;
        }
        return_code = ti_Write(levels[i].data.bubbles, sizeof(bubble_t), levels[i].data.size, file_handle);
        if (return_code != levels[i].data.size) {
            screen_errormsg("Error during level bubbles write");
            return 1;
        }
    }
    
    return 0; //success!
}

uint8_t saveLevel(char filename[], level_t level, char pack_name[]) {
    //Returns 0 on success, 1 otherwise
    int return_code;
    uint8_t file_handle;
    file_handle = ti_Open(filename, "w");
    //pack info
    return_code = ti_Write("BUBBLES!LPCK\x00\x01\x00\x01", 16, 1, file_handle); //for now it saves 1 level
    if (return_code != 1)
        return 1;
    return_code = ti_Write(pack_name, 24, 1, file_handle);
    if (return_code != 1)
        return 1;
    return_code = ti_Write(&level, sizeof(level_t) - sizeof(bubble_t *), 1, file_handle);
    if (return_code != 1)
        return 1;
    return_code = ti_Write(level.data.bubbles, sizeof(bubble_t), level.data.size, file_handle);
    if (return_code != level.data.size)
        return 1;
    
    return 0;
}

void debugTestOutput() { //used to compare with level editor export
    uint8_t i,j;
    pack_t pack;
    level_t *levels;
    
    levels = (level_t *) malloc(2 * sizeof(level_t));
    strncpy(pack.signature,"BUBBLES!LPCK",PACK_SIGNATURE_SIZE);
    pack.version_info[0] = 0;
    pack.version_info[1] = 1;
    pack.version_info[2] = 0;
    pack.number_of_levels = 2;
    strcpy(pack.pack_name, "My First Level Pack!\0\0\0\0\0");
    
    //strcpy(levels[0].name,"Test level name.Test.ing\0");
    strncpy(levels[0].name, "First!First!First!First!", 25);
    levels[0].max_color = 6;
    levels[0].shift_rate = 5;
    levels[0].push_down_time = 25;
    levels[0].random_seed = 0x10312d;
    levels[0].colors = 0x7F;
    levels[0].starting_bubbles[0] = 3;
    levels[0].starting_bubbles[1] = 6;
    levels[0].starting_bubbles[2] = 1;
    levels[0].cols = 10;
    levels[0].rows = 10;
    levels[0].data.size = 80;
    levels[0].data.bubbles = (bubble_t *) malloc(levels[0].data.size * sizeof(bubble_t));
    if (!levels[0].data.bubbles) exit(1);
    for(i = 0; i < levels[0].rows; i++) {
        for (j = 0; j < levels[0].cols; j++) {
            if (i < 8) {
                levels[0].data.bubbles[(i * levels[0].cols) + j] = newBubble(j, i, i % (levels[0].max_color + 1), 0);
            }
        }
    }
    
    strcpy(levels[1].name,"SecondSecondSecondSecond\0");
    levels[1].max_color = 4;
    levels[1].shift_rate = 4;
    levels[1].push_down_time = 25;
    levels[1].random_seed = 0x402010;
    levels[1].colors = 0x7F;
    levels[1].starting_bubbles[0] = 0;
    levels[1].starting_bubbles[1] = 1;
    levels[1].starting_bubbles[2] = 3;
    levels[1].cols = 7;
    levels[1].rows = 12;
    levels[1].data.size = 63;
    levels[1].data.bubbles = (bubble_t *) malloc(levels[1].data.size * sizeof(bubble_t));
    if (!levels[1].data.bubbles) exit(1);
    for(i = 0;i < levels[1].rows;i++) {
        for (j = 0;j < levels[1].cols;j++) {
            if (i < 9) {
                levels[1].data.bubbles[(i*levels[1].cols)+j] = newBubble(j,i,levels[1].max_color-(i%(levels[1].max_color+1)),0);
            }
        }
    }
    
    dbg_printf("Levels[0]: %p\nLevels[0].bubbles:\n size: %d, loc: %p\n",&levels[0],levels[0].data.size,&levels[0].data.bubbles);
    dbg_printf("Levels[1]: %p\nLevels[1].bubbles:\n size: %d, loc: %p\n",&levels[1],levels[1].data.size,&levels[1].data.bubbles);
    dbg_printf("Level_t size: %d\n",sizeof(level_t));
    os_ClrLCDFull();
    //i = saveLevel("FIRSTLVL",l,"First!First!First!First!");
    i = savePack("TWOLEVEL",pack,levels);
    os_SetCursorPos(0,0);
    if (!i)
        os_PutStrFull("Successful.");
    else
        os_PutStrFull("Failure.");
    while (!os_GetCSC());
}

void debugTestRead() {
    char debug_string[100];
    uint8_t i;
    uint8_t l;
    bool level_changed, item_changed;
    pack_t pack;
    level_t level;
    uint8_t file;
    file = ti_Open("TWOLEVEL","r");
    i = 0;
    l = 0;
    level_changed = true;
    item_changed = true;
    pack = loadPackData(file);
    
    os_ClrLCDFull();
    if (strncmp(pack.pack_name,"ERROR!",6) == 0) {
        screen_errormsg("TESTVAR DOES NOT EXIST!");
        exit(1);
    }
    sprintf(debug_string,"Signature: %s",pack.signature);
    os_SetCursorPos(0,0);
    os_PutStrFull(debug_string);
    
    sprintf(debug_string,"Version info: %d.%d.%d",pack.version_info[0],pack.version_info[1],pack.version_info[2]);
    os_SetCursorPos(1,0);
    os_PutStrFull(debug_string);
    
    sprintf(debug_string,"Number of levels: %d",pack.number_of_levels);
    os_SetCursorPos(2,0);
    os_PutStrFull(debug_string);
    
    os_SetCursorPos(3,0);
    os_PutStrFull("Pack name:");
    
    sprintf(debug_string,"%.24s",pack.pack_name);
    os_SetCursorPos(4,0);
    os_PutStrFull(debug_string);
    while (!os_GetCSC());
    
    while (!(kb_Data[6] & kb_Clear)) {
        kb_Scan();
        if (level_changed) {
            os_ClrLCDFull();
            i = 0;
            level = loadLevelFromFile(file,l);
            sprintf(debug_string,"Max color: %d",level.max_color);
            os_SetCursorPos(0,0);
            os_PutStrFull(debug_string);
                        
            sprintf(debug_string,"Shift rate: %d",level.shift_rate);
            os_SetCursorPos(1,0);
            os_PutStrFull(debug_string);
            
            sprintf(debug_string,"Size: %d",level.data.size);
            os_SetCursorPos(1,16);
            os_PutStrFull(debug_string);
            
            sprintf(debug_string,"Push down time: %d",level.push_down_time);
            os_SetCursorPos(2,0);
            os_PutStrFull(debug_string);
            
            sprintf(debug_string,"Random seed: %d",level.random_seed);
            os_SetCursorPos(3,0);
            os_PutStrFull(debug_string);
            
            sprintf(debug_string,"Colors byte: %d",level.colors);
            os_SetCursorPos(4,0);
            os_PutStrFull(debug_string);
            
            sprintf(debug_string,"Level #%d name:",l);
            os_SetCursorPos(5,0);
            os_PutStrFull(debug_string);
            
            sprintf(debug_string,"%.24s",level.name);
            os_SetCursorPos(6,0);
            os_PutStrFull(debug_string);
            
            sprintf(debug_string,"Start bubbles: %d %d %d",level.starting_bubbles[0],level.starting_bubbles[1],level.starting_bubbles[2]);
            os_SetCursorPos(7,0);
            os_PutStrFull(debug_string);
            
            sprintf(debug_string,"Dim: %dx%d",level.cols,level.rows);
            os_SetCursorPos(0,16);
            os_PutStrFull(debug_string);
            level_changed = false;
            item_changed = true;
        }
        
        if (item_changed) {
            sprintf(debug_string,"Bubble #%d: X: %d, Y: %d",i,level.data.bubbles[i].x,level.data.bubbles[i].y);
            os_SetCursorPos(8,0);
            os_PutStrFull(debug_string);
            
            sprintf(debug_string,"Color: %d, Flags: %d",level.data.bubbles[i].color,level.data.bubbles[i].flags);
            os_SetCursorPos(9,0);
            os_PutStrFull(debug_string);
            
            item_changed = false;
        }
        if (kb_Data[7] & kb_Up) {
            l = (l > 0) ? l - 1 : pack.number_of_levels - 1;
            level_changed = true;
            while (os_GetCSC());
        }
        if (kb_Data[7] & kb_Down) {
            l = (l + 1 < pack.number_of_levels) ? l + 1 : 0;
            level_changed = true;
            while (os_GetCSC());
        }
        if (kb_Data[7] & kb_Left) {
            i = (i > 0) ? i - 1 : level.data.size - 1;
            item_changed = true;
        }
        if (kb_Data[7] & kb_Right) {
            i = (i < level.data.size - 1) ? i + 1 : 0;
            item_changed = true;
        }
        if (item_changed) {
            os_SetCursorPos(8,0);
            os_PutStrFull("                                                  ");
        }
    }
}
#endif
