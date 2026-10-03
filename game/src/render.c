#pragma bank 1
#include "game.h"

uint8_t bgtiles[360], bgattrs[360];
/* Each area has both a label and a distinct color; color is never the only cue. */
enum { UI_KEY=8, UI_FOCUS, UI_ANSWER, UI_ERROR, UI_GOOD, UI_BORDER, UI_NEUTRAL, UI_STANDARD };
const uint16_t ui_screen_palettes[32] = {
    RGB(2,8,9), RGB(6,15,14), RGB(11,24,20), RGB(19,31,25),
    RGB(31,25,11), RGB(27,19,7), RGB(9,10,10), RGB(2,4,8),
    RGB(3,9,17), RGB(8,15,24), RGB(14,22,29), RGB(24,29,31),
    RGB(13,3,6), RGB(20,7,9), RGB(28,15,16), RGB(31,26,24),
    RGB(2,9,7), RGB(8,19,12), RGB(17,28,19), RGB(25,31,25),
    RGB(2,4,8), RGB(5,9,14), RGB(9,15,22), RGB(15,23,27),
    RGB(4,7,12), RGB(8,12,18), RGB(14,20,24), RGB(27,29,30),
    RGB(2,4,8), RGB(13,27,23), RGB(30,24,13), RGB(31,31,29)
};
static uint8_t ink=UI_STANDARD, colored_screen;

static void tile(uint8_t x,uint8_t y,uint8_t value) {
    if(x<20&&y<18){bgtiles[(uint16_t)y*20+x]=value;bgattrs[(uint16_t)y*20+x]=ink;}
}

static void print(uint8_t x,uint8_t y,const char *s,uint8_t selected) {
    uint8_t c,prior=ink;
    if(selected&&colored_screen)ink=UI_FOCUS;
    while(*s&&x<20){
        c=*s++;if(c>='a'&&c<='z')c-=32;
        tile(x++,y,c>=32&&c<96?c-32+((selected&&!colored_screen)?64:0):'?' -32);
    }
    ink=prior;
}

static void center(uint8_t y,const char *s) {
    uint8_t n=(uint8_t)strlen(s);
    print(n<20?(20-n)/2:0,y,s,0);
}

static void fill(uint8_t x,uint8_t y,uint8_t w,uint8_t h,uint8_t color) {
    uint8_t i,j,prior=ink;ink=color;
    for(j=0;j<h;j++)for(i=0;i<w;i++)tile(x+i,y+j,0);
    ink=prior;
}

static void panel(uint8_t x,uint8_t y,uint8_t w,uint8_t h,uint8_t color) {
    uint8_t i,prior=ink;fill(x,y,w,h,color);ink=color;
    tile(x,y,135);tile(x+w-1,y,136);tile(x,y+h-1,137);tile(x+w-1,y+h-1,138);
    for(i=1;i<w-1;i++){tile(x+i,y,139);tile(x+i,y+h-1,140);}
    for(i=1;i<h-1;i++){tile(x,y+i,141);tile(x+w-1,y+i,142);}
    ink=prior;
}

static void rule(uint8_t y) {uint8_t x;for(x=0;x<20;x++)tile(x,y,139);}

static void label(const char *s) {ink=UI_STANDARD;print(1,0,s,0);}

static void say(const char *a,const char *b,const char *foot) {
    /* Two short lines with a full blank row between them, inside a story callout. */
    ink=UI_STANDARD;tile(3,9,'/'-32);tile(4,9,'\\'-32);
    panel(0,10,20,6,UI_STANDARD);center(11,a);center(13,b);
    fill(0,16,20,2,UI_STANDARD);print(1,17,foot,0);
}

static void flight_note(const char *a,const char *b,const char *foot) {
    /* Keep all 96 sky pixels available to the rocket, including older saves. */
    ink=UI_STANDARD;panel(0,12,20,5,UI_STANDARD);
    center(13,a);center(15,b);fill(0,17,20,1,UI_STANDARD);print(1,17,foot,0);
}

static void big(uint8_t x,uint8_t y,uint8_t base,uint8_t symbol,uint8_t letter,uint8_t selected) {
    uint8_t prior=ink;
    load_big(base,symbol,letter,0);
    if(selected)ink=UI_FOCUS;
    tile(x,y,base);tile(x+1,y,base+1);tile(x,y+1,base+2);tile(x+1,y+1,base+3);
    ink=prior;
}

static void pair(uint8_t x,uint8_t y,uint8_t symbol) {
    char l[2];l[0]=letters[symbol];l[1]=0;
    tile(x,y,128+symbol);print(x+1,y,"=",0);print(x+2,y,l,0);
}

static void puzzle(void) {
    uint8_t p=puzzle_id(),i,x,l,a,done=game.solved&(1u<<p);
    static const char *const titles[3]={"OPEN THE HATCH","FIND OUR FRIEND","SAY HELLO"};
    colored_screen=1;ink=UI_STANDARD;center(0,titles[p]);
    panel(0,1,20,4,UI_KEY);ink=UI_KEY;print(6,1," YOUR KEY ",0);
    if(p==0){for(i=0;i<4;i++)pair(2+i*4,3,i);}
    else if(p==1){pair(4,3,CIRCLE);pair(8,3,STAR);pair(12,3,DIAMOND);}
    else{pair(6,3,PLUS);pair(10,3,HEART);}
    ink=UI_STANDARD;center(5,p==2?"SEND THIS WORD":"SECRET NOTE");
    for(i=0;i<lengths[p];i++) {
        x=(20-lengths[p]*4)/2+i*4;l=words[p][i];a=game.answers[p][l];
        ink=UI_STANDARD;big(x,6,144+i*4,p==2?letters[l]:l,p==2,0);
        ink=done?UI_GOOD:(i==slot?(wrong?UI_ERROR:UI_ANSWER):UI_NEUTRAL);
        big(x,9,160+i*4,a==255?'?':(p==2?a:letters[a]),a==255||p!=2,0);
        if(i==slot&&!done&&!wrong)print(x,11,"^^",0);
    }
    ink=UI_ANSWER;center(8,p==2?"YOUR MESSAGE":"YOUR WORD");
    if(done) {
        panel(0,12,20,4,UI_GOOD);ink=UI_GOOD;
        center(13,p==0?"YOU OPENED IT!":p==1?"SHE IS ON THE MOON":"YOUR HELLO ARRIVED");
        center(14,p==0?"LET'S PEEK INSIDE.":p==1?"LET'S GO FIND HER!":"SOMEONE IS WAITING");
        ink=UI_STANDARD;center(17,"A CONTINUE");
    } else {
        if(wrong){fill(0,11,20,1,UI_ERROR);ink=UI_ERROR;tile(0,11,143);print(2,11,"THAT DOESN'T MATCH",0);}
        ink=UI_NEUTRAL;center(12,"LEFT/RIGHT: PICK");ink=UI_STANDARD;
        for(i=0;i<option_counts[p];i++){
            x=(20-option_counts[p]*4)/2+i*4;l=options[p][i];
            big(x,13,176+i*4,p==2?l:letters[l],p!=2,i==choice);
        }
        ink=UI_STANDARD;center(16,p==2&&!can_undo()?"A USE    B LATER":"A USE     B UNDO");
        center(17,"SELECT: HELP");
    }
}

static void words_page(void) {
    uint8_t i;
    static const char *const names[5]={"KEY","PILOT","BEACON","CRATER","ANTENNA"};
    center(0,"WORD EXPLORER");panel(0,2,20,12,UI_NEUTRAL);ink=UI_NEUTRAL;center(3,names[word_index]);
    switch(word_index) {
        case 0:
            center(5,"A KEY IS A GUIDE.");center(7,"IT MATCHES SHAPES");center(8,"TO LETTERS.");
            ink=UI_KEY;for(i=0;i<4;i++)pair(2+i*4,10,i);
            for(i=4;i<7;i++)pair(4+(i-4)*4,12,i);break;
        case 1:center(6,"A PILOT FLIES");center(8,"THE SHIP.");center(11,"THAT'S YOU!");break;
        case 2:center(5,"IT SAYS:");center(7,"HERE I AM!");center(10,"IT HELPS US FIND");center(12,"A PLACE.");break;
        case 3:center(5,"A ROUND DIP IN");center(7,"THE GROUND IS");center(9,"A CRATER.");center(12,"THE MOON HAS LOTS!");break;
        case 4:center(5,"THIS PART HELPS US");center(7,"SEND AND GET");center(9,"MESSAGES.");break;
    }
    ink=UI_STANDARD;center(15,"LEFT/RIGHT: WORD");center(17,"B BACK");
}

static void help_page(void) {
    uint8_t p=puzzle_id(),l;
    center(0,"LET'S TRY TOGETHER");panel(0,2,20,12,UI_NEUTRAL);ink=UI_NEUTRAL;
    if(p!=255) {
        l=words[p][0];
        if(hint_step==0){center(4,"LOOK AT YOUR KEY.");center(6,p==2?"FIND THE LETTER.":"FIND ITS SHAPE.");center(9,"PICK ITS PARTNER.");center(11,"PRESS A TO USE IT.");}
        else if(hint_step==1){center(4,"ONE SHAPE MEANS");center(6,"ONE LETTER.");center(9,"UP/DOWN PICKS");center(11,"A BOX TO CHANGE.");}
        else{
            center(4,"HERE IS ONE PAIR.");ink=UI_KEY;
            big(6,7,144,l,0,0);print(9,7,"=",0);big(11,7,148,letters[l],1,0);
            ink=UI_NEUTRAL;center(11,"TRY THE NEXT ONE!");
        }
    }else if(game.scene==FLIGHT){center(5,"FLY TO THE ROUND");center(7,"MOON AT THE TOP.");center(10,"LET GO TO STOP.");}
    else{center(5,"LOOK FOR A GLOW.");center(8,"PRESS A TO LOOK.");}
    ink=UI_STANDARD;center(15,hint_step<2?"A MORE HELP":"A TRY IT");center(17,"B BACK");
}

static void menus(void) {
    char caption[21];colored_screen=1;ink=UI_STANDARD;
    if(ui_mode==WORD_HELP){words_page();return;}
    if(ui_mode==PUZZLE_HELP){help_page();return;}
    if(ui_mode==HELP_MENU){
        center(0,"YOUR HELPER");panel(0,2,20,12,UI_NEUTRAL);ink=UI_NEUTRAL;
        print(4,4,"WORD HELP",selection==0);print(4,7,"PUZZLE HELP",selection==1);print(4,10,"BACK",selection==2);
        print(2,4+selection*3,">",0);ink=UI_STANDARD;center(15,"UP/DOWN THEN A");center(17,"ASK ANY TIME!");
    }else if(ui_mode==PAUSE_MENU){
        center(1,"TAKE A BREAK");strcpy(caption,"PILOT: ");strcat(caption,game.name);center(3,caption);
        panel(0,5,20,10,UI_NEUTRAL);ink=UI_NEUTRAL;center(6,"A KEEP PLAYING");center(9,game.sound?"B SOUND: ON":"B SOUND: OFF");center(12,"SELECT: TITLE");
        ink=UI_STANDARD;center(17,"YOUR TRIP IS SAVED.");
    }else if(ui_mode==RESET_CONFIRM){
        center(1,"START A NEW TRIP?");panel(0,3,20,11,UI_NEUTRAL);ink=UI_NEUTRAL;
        center(5,"THIS STARTS OVER.");center(8,"HOLD A+B TO RESET");center(10,"FOR TWO SECONDS.");ink=UI_STANDARD;center(17,"B BACK");
    }
}

static void name_setup(void) {
    uint8_t i,x,y;
    char letter[2];letter[1]=0;colored_screen=1;ink=UI_STANDARD;
    center(0,"YOUR PILOT NAME");
    ink=UI_ANSWER;for(i=0;i<8;i++)big(2+i*2,2,144+i*4,i<name_length?game.name[i]:'_',1,i==name_length);
    ink=name_error?UI_ERROR:UI_NEUTRAL;
    center(5,name_error==1?"ADD A LETTER FIRST":name_error==2?"FULL! PRESS START.":"PICK A LETTER");
    if(name_error)tile(0,5,143);
    for(i=0;i<28;i++){
        x=2+(i%7)*2;y=7+(i/7)*2;ink=UI_STANDARD;
        if(i<26){letter[0]='A'+i;print(x,y,letter,i==name_choice);}
        else print(x,y,i==26?"<":"DONE",i==name_choice);
    }
    ink=UI_STANDARD;center(15,name_choice==26?"A ERASE   B ERASE":name_choice==27?"A DONE    B ERASE":"A ADD     B ERASE");center(17,"START: ALL DONE!");
}

static void large_letter(uint8_t x,uint8_t y,uint8_t c) {
    uint8_t row,col,bits;
    for(row=0;row<7;row++){
        bits=font_bits[((uint16_t)c-32)*8+row];
        for(col=0;col<5;col++)if(bits&(64u>>col))tile(x+col,y+row,64);
    }
}

static void countdown(void) {
    colored_screen=1;ink=UI_STANDARD;center(1,"READY FOR LIFTOFF!");
    ink=UI_KEY;tile(2,4,131);tile(17,5,131);tile(3,12,131);tile(16,12,131);
    ink=UI_KEY;
    if(launch_count){large_letter(7,5,'0'+launch_count);}
    else{large_letter(4,5,'G');large_letter(10,5,'O');}
    ink=UI_STANDARD;center(15,launch_count?"HERE WE GO...":"TO THE MOON!");
}

static void world(void) {
    char caption[21];colored_screen=0;ink=UI_STANDARD;
    switch(game.scene) {
        case TITLE:
            load_scene(0);center(1,"CIPHERSPACE");
            if(has_save&&game.name[0]){strcpy(caption,"PILOT ");strcat(caption,game.name);center(3,caption);}
            say("CHAPTER 1","THE EMPTY SHIP",has_save?"A CONTINUE  B NEW":"A PLAY");break;
        case NAME_SETUP:name_setup();break;
        case YARD:
            load_scene(0);strcpy(caption,game.name);strcat(caption,"'S BACKYARD");label(caption);
            if(!game.card)say("WHAT WAS THAT?","A SHIP LANDED!","A LOOK CLOSER");
            else say("A NOTE ON THE DOOR","LET'S READ IT.","A READ THE NOTE");
            break;
        case REPAIR:
            load_scene(game.repaired?1:0);
            if(game.repaired)say("YOU DID IT!","THE SHIP IS AWAKE!","A LOOK INSIDE");
            else say("A STONE FELL OUT!","IT FITS HERE!","A PUT IT BACK");
            break;
        case EMPTY_SEAT:
            load_scene(2);
            if(!game.card)say("AN EMPTY SEAT!","WHO FLIES THE SHIP?","A LOOK AT THE NOTE");
            else if(game.card==1)say("SHE IS SAFE.","SHE NEEDS A RIDE.","A CONTINUE");
            else say("WHERE IS SHE?","A NOTE CAN HELP!","A READ IT");
            break;
        case LAUNCH:
            if(game.card>=2){countdown();break;}
            load_scene(game.card?2:1);
            if(!game.card)say("THE SHIP CAN FLY!","LET'S GO FIND HER.","A TAKE THE SEAT");
            else say("YOU ARE THE PILOT!","ARROWS STEER.","A LIFT OFF");
            break;
        case FLIGHT:
            load_flight();label("TO THE MOON");flight_note("STEER TO THE MOON.","LET GO TO STOP.","SELECT: HELP");break;
        case BEACON:
            load_flight();
            if(!game.card)flight_note("A LIGHT IS SAYING:","HERE I AM!","A CONTINUE");
            else if(game.card==1)flight_note("IT IS A BEACON.","IT SHOWS THE WAY.","A CONTINUE");
            else flight_note("FOLLOW THE GLOW.","SEND HER A HELLO!","A WRITE A MESSAGE");
            break;
        case LANDING:
            load_scene(3);label("THE MOON");
            if(!game.card)say("YOU MADE IT!","A LIGHT IS NEAR!","A LOOK AROUND");
            else say("A DIP IN THE MOON.","IT IS A CRATER!","A CONTINUE");
            break;
        case ENDING:
            load_scene(3);say("HELLO, MOON!","CHAPTER 1 COMPLETE","A TITLE");break;
        default:break;
    }
}

void render_game(void) BANKED {
    static uint8_t page_tiles[2][360],page_attrs[2][360],page_valid[2];
    uint8_t next_page=(LCDC_REG&LCDCF_BG9C00)?0:1,row,ui_only,show_ship;
    uint16_t i,offset;
    uint8_t *next_map=(uint8_t *)(next_page?0x9c00u:0x9800u);

    /* Build the next screen away from the visible tilemap. Dynamic glyphs
       have matching hidden copies, so changing one never tears the old view. */
    render_tile_offset=next_page?64:0;
    memset(bgtiles,0,360);memset(bgattrs,15,360);
    if(ui_mode!=PLAY)menus();
    else if(puzzle_id()!=255)puzzle();
    else world();
    for(i=0;i<360;i++){
        if((bgattrs[i]&8)&&bgtiles[i]>=144&&bgtiles[i]<192)bgtiles[i]+=render_tile_offset;
    }

    /* GBDK's set_tiles waits for VRAM access and targets an explicit map.
       Cache each page separately so cursor-only updates write few rows. */
    VBK_REG=0;
    for(row=0;row<18;row++){
        offset=(uint16_t)row*20;
        if(!page_valid[next_page]||memcmp(page_tiles[next_page]+offset,bgtiles+offset,20)){
            set_tiles(0,row,20,1,next_map,bgtiles+offset);
            memcpy(page_tiles[next_page]+offset,bgtiles+offset,20);
        }
    }
    VBK_REG=1;
    for(row=0;row<18;row++){
        offset=(uint16_t)row*20;
        if(!page_valid[next_page]||memcmp(page_attrs[next_page]+offset,bgattrs+offset,20)){
            set_tiles(0,row,20,1,next_map,bgattrs+offset);
            memcpy(page_attrs[next_page]+offset,bgattrs+offset,20);
        }
    }
    VBK_REG=0;page_valid[next_page]=1;
    ui_only=ui_mode!=PLAY||puzzle_id()!=255||game.scene==NAME_SETUP||(game.scene==LAUNCH&&game.card>=2);
    show_ship=ui_mode==PLAY&&(game.scene==FLIGHT||game.scene==BEACON);
    if(show_ship)sprite_position();

    /* Commit palette, map and sprite visibility together during VBlank.
       The LCD is enabled once after boot, and stays enabled for all input. */
    if(LCDC_REG&LCDCF_ON)wait_vbl_done();
    if(ui_only)set_bkg_palette(0,8,ui_screen_palettes);
    else{set_bkg_palette(0,7,render_art_palettes);set_bkg_palette(7,1,ui_palette);}
    if(next_page)LCDC_REG|=LCDCF_BG9C00;else LCDC_REG&=~LCDCF_BG9C00;
    if(show_ship)SHOW_SPRITES;else HIDE_SPRITES;
    SHOW_BKG;
    if(!(LCDC_REG&LCDCF_ON))DISPLAY_ON;
}
