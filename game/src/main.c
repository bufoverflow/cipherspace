#include "game.h"
#include "scenes.h"
#include "flight.h"

Game game;
uint8_t ui_mode, selection, slot, choice, hint_step, word_index, wrong, has_save;
uint8_t name_choice, name_length, name_error;
uint8_t launch_count, render_tile_offset;
uint16_t render_art_palettes[28];
const char letters[8] = "OPENMHI";
const uint8_t words[3][4] = {{CIRCLE,TRIANGLE,SQUARE,STAR},{DIAMOND,CIRCLE,CIRCLE,STAR},{PLUS,HEART,255,255}};
const uint8_t lengths[3] = {4,4,2};
const uint8_t options[3][4] = {{SQUARE,STAR,CIRCLE,TRIANGLE},{STAR,DIAMOND,CIRCLE,255},{HEART,PLUS,255,255}};
const uint8_t option_counts[3] = {4,3,2};
const uint16_t ui_palette[4] = {RGB(2,4,8),RGB(13,27,23),RGB(30,24,13),RGB(31,31,29)};
static uint8_t old_keys, repeat_clock, frame, resume_scene, save_slot, save_sequence, reset_hold;
static uint8_t setup_scene;
static uint8_t undo_values[16][7], undo_slots[16], undo_count;
static uint8_t tune_step, tune_clock;
static uint8_t big_buffer[64], scratch[64], save_record[64];
static uint8_t cached_art = 255;
static uint8_t big_valid[24], big_symbols[24], big_styles[24];
static uint16_t launch_started;

uint8_t puzzle_id(void) {
    if (game.scene == HATCH) return 0;
    if (game.scene == DESTINATION) return 1;
    if (game.scene == HELLO) return 2;
    return 255;
}

uint8_t can_undo(void) { return undo_count != 0; }

void load_scene(uint8_t index) NONBANKED {
    uint8_t bank = CURRENT_BANK, row, col;
    const uint8_t *tiles, *attrs;
    const uint16_t *palettes;
    switch (index) {
        case 1: tiles=scene1_tiles; attrs=scene1_attrs; palettes=scene1_palettes; break;
        case 2: tiles=scene2_tiles; attrs=scene2_attrs; palettes=scene2_palettes; break;
        case 3: tiles=scene3_tiles; attrs=scene3_attrs; palettes=scene3_palettes; break;
        default: tiles=scene0_tiles; attrs=scene0_attrs; palettes=scene0_palettes; break;
    }
    SWITCH_ROM(index+2);
    if(cached_art!=index){
        VBK_REG=0;set_bkg_data(0,240,tiles);
        memcpy(render_art_palettes,palettes,sizeof(render_art_palettes));
        cached_art=index;
    }
    for (row=0;row<12;row++) {
        for(col=0;col<20;col++) scratch[col]=row*20+col;
        memcpy(bgtiles+(uint16_t)row*20,scratch,20);
    }
    memcpy(bgattrs,attrs,240);
    SWITCH_ROM(bank);
}

void load_flight(void) NONBANKED {
    uint8_t bank=CURRENT_BANK, row, col, art=game.scene==FLIGHT?4:5;
    SWITCH_ROM(6);
    if(cached_art!=art){
        VBK_REG=0;
        set_bkg_data(0,240,game.scene==FLIGHT?flight_tiles:beacon_tiles);
        memcpy(render_art_palettes,game.scene==FLIGHT?flight_palettes:beacon_palettes,sizeof(render_art_palettes));
        cached_art=art;
    }
    for(row=0;row<12;row++) {
        for(col=0;col<20;col++)scratch[col]=row*20+col;
        memcpy(bgtiles+(uint16_t)row*20,scratch,20);
    }
    memcpy(bgattrs,game.scene==FLIGHT?flight_attrs:beacon_attrs,240);
    SWITCH_ROM(bank);
}

void load_big(uint8_t tile, uint8_t symbol, uint8_t is_letter, uint8_t inverse) NONBANKED {
    uint8_t i,index,style=is_letter|(inverse<<1);
    const uint8_t *bits;
    /* Logical tiles 144..191 have a separate copy for each background map.
       Upload only to the hidden copy, never to a glyph being displayed. */
    if(tile<144||tile>188||(tile&3))return;
    if(is_letter?(symbol<32||symbol>95):symbol>6)return;
    index=((tile-144)>>2)+(render_tile_offset?12:0);
    if(big_valid[index]&&big_symbols[index]==symbol&&big_styles[index]==style)return;
    if(is_letter) bits=big_font_tiles+((uint16_t)(symbol-32)<<6);
    else bits=big_glyph_tiles+((uint16_t)symbol<<6);
    if(inverse){for(i=0;i<64;i++)big_buffer[i]=bits[i]^255;bits=big_buffer;}
    VBK_REG=1;set_bkg_data(tile+render_tile_offset,4,bits);VBK_REG=0;
    big_symbols[index]=symbol;big_styles[index]=style;big_valid[index]=1;
}

void sprite_position(void) {
    uint8_t x=game.scene==BEACON?132:game.ship_x;
    uint8_t y=game.scene==BEACON?72:game.ship_y;
    move_sprite(0,x,y+8); move_sprite(1,x+8,y+8);
    move_sprite(2,x,y+16); move_sprite(3,x+8,y+16);
}

static uint16_t checksum(const uint8_t *record) {
    uint16_t crc=0xffff;
    uint8_t i,b;
    for(i=1;i<62;i++) {
        crc^=(uint16_t)record[i]<<8;
        for(b=0;b<8;b++)crc=(crc&0x8000)?(crc<<1)^0x1021:crc<<1;
    }
    return crc;
}

static uint8_t read_record(uint8_t which) {
    volatile uint8_t *s=(volatile uint8_t *)(0xa000u+(uint16_t)which*64u);
    uint8_t i;
    uint16_t crc;
    for(i=0;i<64;i++)save_record[i]=s[i];
    if(save_record[0]!=0xa7)return 0;
    if(!((save_record[1]==1&&save_record[3]==28)||(save_record[1]==2&&save_record[3]==sizeof(Game))))return 0;
    crc=checksum(save_record);
    return save_record[62]==(uint8_t)crc && save_record[63]==(uint8_t)(crc>>8);
}

static uint8_t valid_game(void) {
    uint8_t i,j;
    if(game.scene<YARD||game.scene>ENDING||game.card>3||game.sound>1||game.repaired>1||game.solved>7)return 0;
    if(game.ship_x<8||game.ship_x>151||game.ship_y<16||game.ship_y>87)return 0;
    for(i=0;i<3;i++)for(j=0;j<7;j++)if(game.answers[i][j]!=255&&game.answers[i][j]>6)return 0;
    for(i=0;i<9;i++){
        if(!game.name[i])return i!=0||save_record[1]==1;
        if(i==8||game.name[i]<'A'||game.name[i]>'Z')return 0;
    }
    return 0;
}

static void unpack_record(void) {
    /* Version 1 contains the same first 28 bytes but has no player name. */
    memset(&game,0,sizeof(Game));
    memcpy(&game,save_record+4,save_record[3]);
}

static void save_game(void) {
    uint8_t i,target;
    uint16_t crc;
    volatile uint8_t *dest;
    if(game.scene==TITLE||game.scene==NAME_SETUP||(game.scene==LAUNCH&&game.card>=2))return;
    memset(save_record,0,64);
    save_record[1]=2; save_record[2]=++save_sequence; save_record[3]=sizeof(Game);
    memcpy(save_record+4,&game,sizeof(Game));
    crc=checksum(save_record);save_record[62]=(uint8_t)crc;save_record[63]=(uint8_t)(crc>>8);
    target=save_slot^1; dest=(volatile uint8_t *)(0xa000u+(uint16_t)target*64u);
    ENABLE_RAM;SWITCH_RAM(0);
    dest[0]=0; /* Commit marker is written last, leaving the older slot intact. */
    for(i=1;i<64;i++)dest[i]=save_record[i];
    dest[0]=0xa7;
    DISABLE_RAM;
    save_slot=target;has_save=1;resume_scene=game.scene;
}

static void begin_name(uint8_t next) {
    setup_scene=next;game.scene=NAME_SETUP;
    name_choice=name_length=name_error=0;
    memset(game.name,0,sizeof(game.name));
}

static void new_game(void) {
    memset(&game,0,sizeof(Game));memset(game.answers,255,21);
    game.scene=YARD;game.ship_x=24;game.ship_y=75;game.sound=1;
    ui_mode=PLAY;slot=choice=selection=wrong=undo_count=launch_count=0;
    begin_name(YARD);
}

static void load_game(void) {
    uint8_t a,b,seq_a,seq_b;
    ENABLE_RAM;SWITCH_RAM(0);
    a=read_record(0);seq_a=save_record[2];
    b=read_record(1);seq_b=save_record[2];
    if(a||b) {
        save_slot=(b&&(!a||(uint8_t)(seq_b-seq_a)<128))?1:0;
        read_record(save_slot);save_sequence=save_record[2];unpack_record();
        has_save=valid_game();
        if(!has_save&&(a&&b)){save_slot^=1;read_record(save_slot);save_sequence=save_record[2];unpack_record();has_save=valid_game();}
    }
    DISABLE_RAM;
    if(!has_save){memset(&game,0,sizeof(Game));memset(game.answers,255,21);game.sound=1;game.ship_x=24;game.ship_y=75;resume_scene=YARD;}
    else{
        /* Countdown is runtime-only; an older/malformed transient checkpoint
           returns to its compatible Lift Off prompt instead of skipping it. */
        if(game.scene==LAUNCH&&game.card>=2)game.card=1;
        resume_scene=game.scene;
    }
    game.scene=TITLE;
}

void sound_cue(uint8_t kind) {
    uint16_t frequency;
    if(!game.sound)return;
    frequency=kind==2?1751:(kind==1?1547:1310);
    NR10_REG=0;NR11_REG=0x80;NR12_REG=0x63;
    NR13_REG=(uint8_t)frequency;NR14_REG=0x80|(frequency>>8);
    if(kind==2){tune_step=0;tune_clock=1;}
}

static void audio_tick(void) {
    static const uint16_t notes[4]={1547,1650,1751,1798};
    uint16_t f;
    if(!game.sound)return;
    if(tune_clock&&!--tune_clock) {
        f=notes[tune_step];NR21_REG=0x80;NR22_REG=0x42;NR23_REG=(uint8_t)f;NR24_REG=0x80|(f>>8);
        if(++tune_step<4)tune_clock=10;
    }
}

static void enter(uint8_t next) {
    game.scene=next;game.card=0;slot=choice=wrong=undo_count=launch_count=0;ui_mode=PLAY;
    save_game();sound_cue(1);render_game();
}

static uint8_t is_solved(uint8_t p) { return game.solved&(1u<<p); }

static void focus_puzzle(void) {
    uint8_t p=puzzle_id();
    wrong=0;
    if(p!=255){
        slot=0;while(slot<lengths[p]-1&&game.answers[p][words[p][slot]]==words[p][slot])slot++;
        wrong=game.answers[p][words[p][slot]]!=255&&game.answers[p][words[p][slot]]!=words[p][slot];
    }
}

static void start_launch(void) {
    /* The last durable state remains valid for the existing v2 save layout. */
    save_game();
    game.card=2;launch_count=3;launch_started=sys_time;
    sound_cue(1);render_game();
}

static void launch_tick(void) {
    if(game.scene!=LAUNCH||game.card<2||(uint16_t)(sys_time-launch_started)<60)return;
    launch_started+=60;
    if(launch_count){launch_count--;sound_cue(launch_count?1:2);render_game();}
    else enter(FLIGHT);
}

static void name_input(uint8_t keys) {
    uint8_t finish=keys&J_START;
    if(keys&(J_UP|J_DOWN|J_LEFT|J_RIGHT))name_error=0;
    if(keys&J_UP)name_choice=(name_choice+21)%28;
    if(keys&J_DOWN)name_choice=(name_choice+7)%28;
    if(keys&J_LEFT)name_choice=name_choice%7?name_choice-1:name_choice+6;
    if(keys&J_RIGHT)name_choice=name_choice%7==6?name_choice-6:name_choice+1;
    if((keys&J_B)||((keys&J_A)&&name_choice==26)){
        if(name_length)game.name[--name_length]=0;
        name_error=0;
    }else if(keys&J_A){
        if(name_choice==27)finish=1;
        else if(name_length<8){game.name[name_length++]='A'+name_choice;game.name[name_length]=0;name_error=0;sound_cue(0);}
        else name_error=2;
    }
    if(finish){
        if(!name_length)name_error=1;
        else{
            game.scene=setup_scene;focus_puzzle();save_game();sound_cue(2);
        }
    }
    render_game();
}

static void puzzle_input(uint8_t keys,uint8_t p) {
    uint8_t i,l,missing=255,mismatch=255;
    if(is_solved(p)) {if(keys&J_A)enter(game.scene+1);return;}
    if(keys&J_LEFT)choice=choice?choice-1:option_counts[p]-1;
    if(keys&J_RIGHT)choice=(choice+1)%option_counts[p];
    if(keys&J_UP)slot=slot?slot-1:lengths[p]-1;
    if(keys&J_DOWN)slot=(slot+1)%lengths[p];
    if(keys&(J_UP|J_DOWN)){
        l=words[p][slot];wrong=game.answers[p][l]!=255&&game.answers[p][l]!=l;
    }
    if(keys&J_B) {
        if(undo_count){
            --undo_count;memcpy(game.answers[p],undo_values[undo_count],7);slot=undo_slots[undo_count];
            l=words[p][slot];wrong=game.answers[p][l]!=255&&game.answers[p][l]!=l;save_game();
        }
        else if(p==2){enter(LANDING);return;}
    }
    if(keys&J_A) {
        if(undo_count==16){for(i=0;i<15;i++){memcpy(undo_values[i],undo_values[i+1],7);undo_slots[i]=undo_slots[i+1];}undo_count=15;}
        undo_slots[undo_count]=slot;
        memcpy(undo_values[undo_count++],game.answers[p],7);
        game.answers[p][words[p][slot]]=options[p][choice];
        for(i=0;i<lengths[p];i++) {
            l=words[p][i];
            if(game.answers[p][l]==255){if(missing==255)missing=i;}
            else if(game.answers[p][l]!=l&&mismatch==255)mismatch=i;
        }
        if(game.answers[p][words[p][slot]]!=words[p][slot]){wrong=1;sound_cue(0);}
        else if(mismatch!=255){slot=mismatch;wrong=1;sound_cue(0);}
        else if(missing!=255){slot=missing;wrong=0;sound_cue(0);}
        else{game.solved|=1u<<p;wrong=0;sound_cue(2);}
        save_game();
    }
    render_game();
}

static void menu_input(uint8_t keys) {
    if(ui_mode==RESET_CONFIRM){if((keys&J_B)&&!(joypad()&J_A)){ui_mode=PLAY;reset_hold=0;render_game();}return;}
    if(ui_mode==PAUSE_MENU) {
        if(keys&(J_A|J_START)){ui_mode=PLAY;render_game();}
        else if(keys&J_B){game.sound^=1;if(!game.sound){NR12_REG=NR22_REG=0;tune_clock=0;}save_game();render_game();}
        else if(keys&J_SELECT){save_game();resume_scene=game.scene;game.scene=TITLE;ui_mode=PLAY;render_game();}
        return;
    }
    if(keys&J_SELECT){ui_mode=PLAY;render_game();return;}
    if(keys&J_B){ui_mode=ui_mode==HELP_MENU?PLAY:HELP_MENU;render_game();return;}
    if(ui_mode==HELP_MENU) {
        if(keys&J_UP)selection=selection?selection-1:2;
        if(keys&J_DOWN)selection=(selection+1)%3;
        if(keys&J_A){ui_mode=selection==0?WORD_HELP:selection==1?PUZZLE_HELP:PLAY;hint_step=0;}
    } else if(ui_mode==WORD_HELP) {
        if(keys&J_LEFT)word_index=word_index?word_index-1:4;
        if(keys&(J_RIGHT|J_A))word_index=(word_index+1)%5;
    } else if(ui_mode==PUZZLE_HELP && (keys&J_A)) {
        if(hint_step<2)hint_step++;else ui_mode=PLAY;
    }
    render_game();
}

static void input(uint8_t keys) {
    uint8_t p;
    if(game.scene==LAUNCH&&game.card>=2)return;
    if(ui_mode!=PLAY){menu_input(keys);return;}
    if(game.scene==NAME_SETUP){name_input(keys);return;}
    if(game.scene==TITLE) {
        if(keys&J_A){
            if(has_save){
                game.scene=resume_scene;
                if(!game.name[0])begin_name(game.scene);else focus_puzzle();
            }else new_game();
            sound_cue(2);render_game();
        }
        else if(keys&J_B){ui_mode=RESET_CONFIRM;reset_hold=0;render_game();}
        return;
    }
    if(keys&J_START){save_game();ui_mode=PAUSE_MENU;render_game();return;}
    if(keys&J_SELECT){ui_mode=HELP_MENU;selection=0;word_index=game.scene==BEACON?2:game.scene>=LANDING?3:game.scene==FLIGHT||game.scene==EMPTY_SEAT?1:0;render_game();return;}
    p=puzzle_id();
    if(p!=255){puzzle_input(keys,p);return;}
    if(keys&J_B && game.card){game.card--;render_game();return;}
    if(!(keys&J_A))return;
    switch(game.scene) {
        case YARD: if(game.card<1){game.card++;render_game();}else enter(HATCH);break;
        case REPAIR: if(!game.repaired){game.repaired=1;save_game();sound_cue(2);render_game();}else enter(EMPTY_SEAT);break;
        case EMPTY_SEAT: if(game.card<2){game.card++;save_game();render_game();}else enter(DESTINATION);break;
        case LAUNCH: if(game.card<1){game.card++;render_game();}else start_launch();break;
        case FLIGHT: break;
        case BEACON: if(game.card<2){game.card++;save_game();render_game();}else enter(HELLO);break;
        case LANDING: if(game.card<1){game.card++;save_game();render_game();}else enter(ENDING);break;
        case ENDING: resume_scene=ENDING;game.scene=TITLE;render_game();break;
        default: break;
    }
}

void main(void) {
    uint8_t keys,pressed,i,dx,dy;
    DISPLAY_OFF;
    if(_cpu!=CGB_TYPE){/* The cartridge header already declares GBC-only. */for(;;)wait_vbl_done();}
    cpu_fast();
    LCDC_REG|=LCDCF_BG8000;
    HIDE_WIN;SHOW_BKG;SPRITES_8x8;
    VBK_REG=1;set_bkg_data(0,144,ui_tiles);VBK_REG=0;
    set_sprite_data(240,8,sprite_tiles);
    set_bkg_palette(7,1,ui_palette);set_sprite_palette(0,1,ui_palette);
    for(i=0;i<4;i++){set_sprite_tile(i,240+i);set_sprite_prop(i,0);}
    NR52_REG=0x80;NR50_REG=0x44;NR51_REG=0xff;
    load_game();render_game();
    for(;;) {
        wait_vbl_done();frame++;audio_tick();
        keys=joypad();pressed=keys&~old_keys;
        if((keys&(J_UP|J_DOWN|J_LEFT|J_RIGHT))==(old_keys&(J_UP|J_DOWN|J_LEFT|J_RIGHT))) {
            if(++repeat_clock==24){pressed|=keys&(J_UP|J_DOWN|J_LEFT|J_RIGHT);repeat_clock=16;}
        }else repeat_clock=0;
        old_keys=keys;
        if(ui_mode==RESET_CONFIRM) {
            if((keys&(J_A|J_B))==(J_A|J_B)){if(++reset_hold==120){new_game();sound_cue(2);render_game();}}
            else reset_hold=0;
        }
        if(pressed)input(pressed);
        launch_tick();
        if(ui_mode==PLAY&&game.scene==FLIGHT&&!(frame&1)) {
            if((keys&J_LEFT)&&game.ship_x>8)game.ship_x--;
            if((keys&J_RIGHT)&&game.ship_x<151)game.ship_x++;
            if((keys&J_UP)&&game.ship_y>16)game.ship_y--;
            if((keys&J_DOWN)&&game.ship_y<87)game.ship_y++;
            sprite_position();
            dx=game.ship_x>128?game.ship_x-128:128-game.ship_x;
            dy=game.ship_y>28?game.ship_y-28:28-game.ship_y;
            if(dx<14&&dy<14){sound_cue(2);enter(BEACON);}
        }
    }
}
