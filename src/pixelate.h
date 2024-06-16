#if !defined(PIXELATE_H)
#define PIXELATE_H

#include <graphx.h>
/*TODO: Make a pixelate function that takes width and height args instead of depth args.*/
void pixelate_self(gfx_sprite_t * sprite,uint8_t depth) {
    /*Pixelates a sprite in place. 0 is least pixelation, 255 is most.*/
    gfx_sprite_t * holder;
    const uint8_t temp_width = (uint8_t)(((float)sprite->width)-(((float)sprite->width)/255.0)*depth);
    const uint8_t temp_height = (uint8_t)(((float)sprite->height)-(((float)sprite->height)/255.0)*depth);
    uint8_t *data = (uint8_t *) malloc(2+(temp_width*temp_height));
    data[0] = temp_width || 1; //prevents sprite width of 0
    data[1] = temp_height || 1;
    holder = (gfx_sprite_t *) data;
    gfx_ScaleSprite(sprite,holder);
    gfx_ScaleSprite(holder,sprite);
    free(holder);
}
gfx_sprite_t * pixelate(gfx_sprite_t * sprite_in,gfx_sprite_t * sprite_out,uint8_t depth) {
    /*Returns a pixelated version of the input. sprite_out doesn't need to have the same dimensions as sprite_in. 0 is least pixelation, 255 is most.*/
    gfx_sprite_t *holder;
    const uint8_t temp_width = (uint8_t)(((float)sprite_in->width)-(((float)sprite_in->width)/255.0)*depth);
    const uint8_t temp_height = (uint8_t)(((float)sprite_in->height)-(((float)sprite_in->height)/255.0)*depth);
    uint8_t *data = (uint8_t *) malloc(2+(temp_width*temp_height));
    data[0] = temp_width || 1; //prevents sprite width of 0
    data[1] = temp_height || 1;
    holder = (gfx_sprite_t *) data;
    gfx_ScaleSprite(sprite_in,holder);
    gfx_ScaleSprite(holder,sprite_out);
    free(holder);
    return sprite_out;
}
#endif
