#ifndef CIPHERSPACE_GAME_H
#define CIPHERSPACE_GAME_H
#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>
#include <string.h>

enum { TITLE, YARD, HATCH, REPAIR, EMPTY_SEAT, DESTINATION, LAUNCH, FLIGHT, BEACON, HELLO, LANDING, ENDING, NAME_SETUP };
enum { PILOT_CARD=13, DOOR_NOTE, HATCH_OPEN, CRYSTAL_REPAIR, SHIP_WAKE, OWNER_LOG,
       MOON_NOTE, BOARDING, ASCENT, MOON_APPROACH, MOON_WALK, FIRST_CONTACT,
       ALIEN_REPLY, FRIENDSHIP };
enum { PLAY, HELP_MENU, WORD_HELP, PUZZLE_HELP, PAUSE_MENU, RESET_CONFIRM };
enum { CIRCLE, TRIANGLE, SQUARE, STAR, DIAMOND, PLUS, HEART };
typedef struct {
    uint8_t scene, card, repaired, solved;
    uint8_t answers[3][7];
    uint8_t ship_x, ship_y, sound;
    char name[9];
} Game;
extern Game game;
extern uint8_t ui_mode, selection, slot, choice, hint_step, word_index, wrong, has_save;
extern uint8_t name_choice, name_length, name_error;
extern uint8_t launch_count, render_tile_offset;
extern uint8_t anim_frame, ticker_offset, nav_ready, cached_art;
extern uint16_t scene_ticks;
extern uint8_t pending_art_signed, visible_art;
extern volatile uint8_t display_art_signed;
extern uint16_t render_art_palettes[28];
extern const char letters[8];
extern const uint8_t words[3][4], lengths[3], options[3][4], option_counts[3];
extern const uint8_t font_bits[512], glyph_bits[56], ui_tiles[2304], sprite_tiles[128];
extern const uint8_t big_font_tiles[4096], big_glyph_tiles[448];
extern const uint16_t ui_palette[4];
extern const uint16_t ui_screen_palettes[32];
extern uint8_t bgtiles[360], bgattrs[360];
uint8_t puzzle_id(void);
uint8_t can_undo(void);
void render_game(void) BANKED;
void load_scene(uint8_t index) NONBANKED;
void load_art(uint8_t id,const uint8_t *tiles,const uint8_t *attrs,const uint16_t *palettes) NONBANKED;
void load_alias(uint8_t tile,uint8_t source) NONBANKED;
void load_flight(void) NONBANKED;
void load_cinema(uint8_t index) NONBANKED;
void load_cinema_actors(void) NONBANKED;
void load_big(uint8_t tile, uint8_t symbol, uint8_t is_letter, uint8_t inverse) NONBANKED;
void sprite_position(void);
void sound_cue(uint8_t kind);
#endif
