#include "game.h"
#include "cinema.h"

typedef struct {
    const uint8_t *tiles;
    const uint8_t *attrs;
    const uint16_t *palettes;
} CinemaFrame;

#define PICTURE(n) {cinema##n##_tiles,cinema##n##_attrs,cinema##n##_palettes}
static const CinemaFrame pictures[CINEMA_FRAME_COUNT]={
    PICTURE(0),PICTURE(1),PICTURE(2),PICTURE(3),PICTURE(4),PICTURE(5),
    PICTURE(6),PICTURE(7),PICTURE(8),PICTURE(9),PICTURE(10),PICTURE(11),
    PICTURE(12),PICTURE(13),PICTURE(14),PICTURE(15),PICTURE(16),PICTURE(17),
    PICTURE(18),PICTURE(19),PICTURE(20),PICTURE(21),PICTURE(22),PICTURE(23)
};

void load_cinema(uint8_t index) NONBANKED {
    uint8_t bank=CURRENT_BANK;
    const CinemaFrame *picture;
    if(index>=CINEMA_FRAME_COUNT)return;
    picture=&pictures[index];
    SWITCH_ROM(7+index/3);
    load_art(16+index,picture->tiles,picture->attrs,picture->palettes);
    SWITCH_ROM(bank);
}

void load_cinema_actors(void) NONBANKED {
    uint8_t bank=CURRENT_BANK;
    SWITCH_ROM(15);VBK_REG=0;
    set_sprite_data(244,12,cinema_actor_tiles);
    set_sprite_palette(1,2,cinema_actor_palettes);
    SWITCH_ROM(bank);
}
