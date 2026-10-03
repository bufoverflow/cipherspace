#pragma bank 1
#include "game.h"
#include "cinema.h"

uint8_t bgtiles[360], bgattrs[360];
/* Shape, position and button badges carry meaning alongside the colors. */
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
static uint8_t ink=UI_STANDARD, colored_screen, sprite_tiles_reserved;

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

static uint8_t badge_width(const char *key) {
    if((key[0]=='A'||key[0]=='B')&&!key[1])return 1;
    if(!strcmp(key,"A+B"))return 3;
    return (uint8_t)strlen(key)+2;
}

static void badge(uint8_t x,uint8_t y,const char *key,const char *action) {
    uint8_t c,prior=ink;ink=UI_STANDARD;
    if((key[0]=='A'||key[0]=='B')&&!key[1]){
        tile(x,y,key[0]=='A'?65:66);print(x+2,y,action,0);ink=prior;return;
    }
    if(!strcmp(key,"A+B")){
        tile(x,y,65);print(x+1,y,"+",0);tile(x+2,y,66);
        print(x+4,y,action,0);ink=prior;return;
    }
    tile(x++,y,64);
    while(*key&&x<20){c=*key++;tile(x++,y,c>=32&&c<96?c-32+64:64);}
    tile(x++,y,64);print(x+1,y,action,0);ink=prior;
}

static void action(uint8_t y,const char *key,const char *text) {
    uint8_t n=badge_width(key)+(uint8_t)strlen(text)+1;
    badge(n<20?(20-n)/2:0,y,key,text);
}

static void footer(uint8_t y,const char *text) {
    /* World captions pass only familiar button names followed by an action. */
    if(!strncmp(text,"A ",2))action(y,"A",text+2);
    else if(!strncmp(text,"B ",2))action(y,"B",text+2);
    else if(!strncmp(text,"SELECT: ",8))action(y,"SELECT",text+8);
    else if(!strncmp(text,"START: ",7))action(y,"START",text+7);
    else center(y,text);
}

static void say(const char *a,const char *b,const char *foot) {
    /* Keep the cabin and its empty seat visible above a compact caption. */
    ink=UI_STANDARD;panel(0,12,20,4,UI_STANDARD);center(13,a);center(14,b);
    fill(0,16,20,2,UI_STANDARD);footer(17,foot);
}

static void flight_note(const char *a,const char *b,const char *foot) {
    /* Keep all 96 sky pixels available to the rocket, including older saves. */
    ink=UI_STANDARD;panel(0,12,20,4,UI_STANDARD);
    center(13,a);center(14,b);fill(0,16,20,2,UI_STANDARD);footer(17,foot);
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

static void ticker(const char *text) {
    uint8_t x,n=(uint8_t)strlen(text),span;
    uint16_t t;
    if(n<=20){ticker_offset=0;center(0,text);return;}
    span=n-20;t=scene_ticks%(360u+45u*span);
    ticker_offset=t<180?0:(t-180)/45+1;
    if(ticker_offset>span)ticker_offset=span;
    for(x=0;x<20;x++){
        tile(x,0,text[ticker_offset+x]-32);
    }
}

static void connector(uint8_t x,uint8_t y) {
    static uint8_t loaded;
    static const uint8_t arrow_tiles[32]={
        1,1, 1,1, 1,1, 1,1, 15,15, 7,7, 3,3, 0,0,
        128,128, 128,128, 128,128, 128,128, 240,240, 224,224, 192,192, 0,0
    };
    /* IDs192–207 lie between the two buffered sets of enlarged glyphs. */
    if(!loaded){VBK_REG=1;set_bkg_data(192,2,arrow_tiles);VBK_REG=0;loaded=1;}
    tile(x,y,192);tile(x+1,y,193);
}

static void puzzle(void) {
    uint8_t p=puzzle_id(),i,x,l,a,active,done=game.solved&(1u<<p);
    static const char *const titles[3]={"MATCH THE SHAPES TO OPEN THE DOOR","MATCH THE SHAPES TO FIND HER","SEND A HELLO WITH SHAPES"};
    colored_screen=1;ink=UI_STANDARD;ticker(titles[p]);
    if(wrong){ink=UI_ERROR;tile(0,1,143);print(2,1,"THAT DOESN'T MATCH",0);}
    panel(0,2,20,3,UI_KEY);ink=UI_KEY;
    if(p==0){for(i=0;i<4;i++)pair(2+i*4,3,i);}
    else if(p==1){pair(4,3,CIRCLE);pair(8,3,STAR);pair(12,3,DIAMOND);}
    else{pair(6,3,PLUS);pair(10,3,HEART);}
    for(i=0;i<lengths[p];i++) {
        x=(20-lengths[p]*4)/2+i*4;l=words[p][i];a=game.answers[p][l];
        active=i==slot;
        if(done||active)panel(x,5,4,7,done?UI_GOOD:UI_ANSWER);
        ink=done?UI_GOOD:active?UI_ANSWER:UI_NEUTRAL;
        big(x+1,6,144+i*4,p==2?letters[l]:l,p==2,0);
        if(active&&!done)connector(x+1,8);
        ink=done?UI_GOOD:active?(wrong?UI_ERROR:UI_ANSWER):UI_NEUTRAL;
        big(x+1,9,160+i*4,a==255?'?':(p==2?a:letters[a]),a==255||p!=2,0);
    }
    if(done) {
        /* The completed word gets a short quiet beat before the scene moves. */
        ink=UI_GOOD;center(14,"!");
    } else {
        for(i=0;i<option_counts[p];i++){
            x=(20-option_counts[p]*4)/2+i*4;l=options[p][i];
            if(i==choice)panel(x,12,4,4,UI_FOCUS);
            ink=i==choice?UI_FOCUS:UI_STANDARD;
            big(x+1,13,176+i*4,p==2?l:letters[l],p!=2,0);
        }
        ink=UI_STANDARD;badge(1,16,"A","USE");badge(10,16,"B","UNDO");
        action(17,"SELECT","HELP");
    }
}

static void words_page(void) {
    uint8_t i;
    static const char *const names[6]={"KEY","PILOT","BEACON","CRATER","ANTENNA","SOUTH POLE"};
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
        case 5:
            center(5,"THE MOON'S");center(7,"SOUTH END.");
            ink=UI_NEUTRAL;big(9,9,144,CIRCLE,0,0);
            ink=UI_KEY|64;connector(9,11);break;
    }
    ink=UI_STANDARD;center(15,"<   WORDS   >");action(17,"B","BACK");
}

static void help_page(void) {
    uint8_t p=puzzle_id(),l;
    center(0,"LET'S TRY TOGETHER");panel(0,2,20,12,UI_NEUTRAL);ink=UI_NEUTRAL;
    if(p!=255) {
        l=words[p][0];
        if(hint_step==0){center(4,"LOOK AT YOUR KEY.");center(6,p==2?"FIND THE LETTER.":"FIND ITS SHAPE.");center(9,"PICK ITS PARTNER.");action(11,"A","USE IT");}
        else if(hint_step==1){center(4,"ONE SHAPE MEANS");center(6,"ONE LETTER.");center(9,"UP/DOWN PICKS");center(11,"A BOX TO CHANGE.");}
        else{
            center(4,"HERE IS ONE PAIR.");ink=UI_KEY;
            big(6,7,144,l,0,0);print(9,7,"=",0);big(11,7,148,letters[l],1,0);
            ink=UI_NEUTRAL;center(11,"TRY THE NEXT ONE!");
        }
    }else if(game.scene==FLIGHT){center(5,"FLY TO THE ROUND");center(7,"MOON AT THE TOP.");center(10,"LET GO TO STOP.");}
    else if(game.scene==MOON_APPROACH){center(4,"FLY TO THE GLOW.");center(6,"AT THE SOUTH POLE.");center(9,"ARROWS STEER.");action(11,"A","LAND");}
    else if(game.scene==MOON_WALK){center(4,"WALK TO THE GLOW.");center(6,"LEFT AND RIGHT");center(8,"MOVE YOUR FEET.");action(11,"A","MEET");}
    else{center(5,"LOOK FOR A GLOW.");action(8,"A","LOOK");}
    ink=UI_STANDARD;action(15,"A",hint_step<2?"MORE HELP":"TRY IT");action(17,"B","BACK");
}

static void menus(void) {
    char caption[21];colored_screen=1;ink=UI_STANDARD;
    if(ui_mode==WORD_HELP){words_page();return;}
    if(ui_mode==PUZZLE_HELP){help_page();return;}
    if(ui_mode==HELP_MENU){
        center(0,"YOUR HELPER");panel(0,2,20,12,UI_NEUTRAL);ink=UI_NEUTRAL;
        print(4,4,"WORD HELP",selection==0);print(4,7,"PUZZLE HELP",selection==1);print(4,10,"BACK",selection==2);
        print(2,4+selection*3,">",0);ink=UI_STANDARD;action(15,"A","CHOOSE");center(17,"ASK ANY TIME!");
    }else if(ui_mode==PAUSE_MENU){
        center(1,"TAKE A BREAK");strcpy(caption,"PILOT: ");strcat(caption,game.name);center(3,caption);
        panel(0,5,20,10,UI_NEUTRAL);ink=UI_NEUTRAL;action(6,"A","KEEP PLAYING");action(9,"B",game.sound?"SOUND: ON":"SOUND: OFF");action(12,"SELECT","TITLE");
        ink=UI_STANDARD;center(17,"YOUR TRIP IS SAVED.");
    }else if(ui_mode==RESET_CONFIRM){
        center(1,"START A NEW TRIP?");panel(0,3,20,11,UI_NEUTRAL);ink=UI_NEUTRAL;
        center(5,"THIS STARTS OVER.");action(8,"A+B","HOLD TO RESET");center(10,"FOR TWO SECONDS.");ink=UI_STANDARD;action(17,"B","BACK");
    }
}

static void name_setup(void) {
    uint8_t i,x,y;
    char letter[2];letter[1]=0;colored_screen=1;ink=UI_STANDARD;
    center(0,"YOUR NAME");
    panel(0,2,20,4,UI_ANSWER);
    ink=UI_ANSWER;for(i=0;i<8;i++)big(2+i*2,3,144+i*4,i<name_length?game.name[i]:'_',1,0);
    if(name_error){ink=UI_ERROR;center(1,name_error==1?"ADD A LETTER FIRST":"FULL! PRESS START.");}
    for(i=0;i<26;i++){
        x=2+(i%7)*2;y=7+(i/7)*2;ink=UI_STANDARD;
        letter[0]='A'+i;print(x,y,letter,0);
    }
    x=2+(name_choice%7)*2;y=7+(name_choice/7)*2;
    panel(x-1,y-1,3,3,UI_FOCUS);ink=UI_FOCUS;letter[0]='A'+name_choice;print(x,y,letter,0);
    ink=UI_STANDARD;badge(1,16,"A","ADD");badge(10,16,"B","ERASE");action(17,"START","DONE");
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
    if(launch_count==10){large_letter(4,5,'1');large_letter(10,5,'0');}
    else if(launch_count){large_letter(7,5,'0'+launch_count);}
    else{large_letter(4,5,'G');large_letter(10,5,'O');}
    ink=UI_STANDARD;center(15,launch_count?"HERE WE GO...":"TO THE MOON!");
}

static void note_symbols(uint8_t p) {
    uint8_t i;ink=UI_STANDARD;
    for(i=0;i<lengths[p];i++)big(4+i*3,5,144+i*4,words[p][i],0,0);
}

static void cinema_effects(void) {
    static uint8_t loaded;
    static const uint8_t effects[224]={
        0,0,1,0,1,0,6,0,6,0,24,0,24,0,96,0,
        0,0,128,0,128,0,96,0,96,0,24,0,24,0,6,0,
        96,0,24,0,24,0,6,0,6,0,1,0,1,0,0,0,
        6,0,24,0,24,0,96,0,96,0,128,0,128,0,0,0,
        7,0,24,0,32,0,64,0,64,0,128,0,128,0,128,0,
        224,0,24,0,4,0,2,0,2,0,1,0,1,0,1,0,
        128,0,128,0,128,0,64,0,64,0,32,0,24,0,7,0,
        1,0,1,0,1,0,2,0,2,0,4,0,24,0,224,0,
        255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,
        128,0,128,0,128,0,128,0,128,0,128,0,128,0,128,0,
        1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,
        24,24,24,24,24,24,24,24,24,24,24,24,0,0,0,0,
        60,126,60,126,24,60,24,60,0,24,0,24,0,0,0,0
    };
    if(!loaded){VBK_REG=1;set_bkg_data(194,14,effects);VBK_REG=0;loaded=1;}
}

static void moving_crystal(uint8_t x) {
    ink=UI_STANDARD;
    tile(x,4,194);tile(x+1,4,195);tile(x,5,196);tile(x+1,5,197);
}

static void beacon_ring(uint8_t x,uint8_t y,uint8_t radius) {
    uint8_t i,w=2+radius*2;ink=UI_STANDARD;x-=radius;y-=radius;
    tile(x,y,198);tile(x+w-1,y,199);tile(x,y+w-1,200);tile(x+w-1,y+w-1,201);
    for(i=1;i<w-1;i++){
        tile(x+i,y,202);tile(x+i,y+w-1,203);
        tile(x,y+i,204);tile(x+w-1,y+i,205);
    }
}

static void camera_shift(int8_t dx,int8_t dy) {
    static uint8_t source_tiles[240],source_attrs[240];
    uint8_t x,y;int8_t sx,sy;uint16_t dest,source;
    memcpy(source_tiles,bgtiles,240);memcpy(source_attrs,bgattrs,240);
    for(y=0;y<12;y++)for(x=0;x<20;x++){
        sx=(int8_t)x-dx;sy=(int8_t)y-dy;
        if(sx<0)sx=0;if(sx>19)sx=19;if(sy<0)sy=0;if(sy>11)sy=11;
        dest=(uint16_t)y*20+x;source=(uint16_t)sy*20+sx;
        bgtiles[dest]=source_tiles[source];bgattrs[dest]=source_attrs[source];
    }
}

static void speech(uint8_t x,uint8_t y,uint8_t w,const char *text,uint8_t tail) {
    panel(x,y,w,3,UI_STANDARD);ink=UI_STANDARD;
    print(x+(w-(uint8_t)strlen(text))/2,y+1,text,0);
    tile(x+tail,y+3,'V'-32);
}

static void name_patch(void) {
    static uint8_t bits[16];
    uint8_t n=(uint8_t)strlen(game.name),cols,width,left,first,tx,ty,x,y,px,py;
    uint8_t color,letter,column,glyph,logical;
    if(!n)return;if(n>8)n=8;
    cols=(6*n+15)/8;width=cols*8;left=(21-cols)/2;
    first=(width-(6*n-1))/2;ink=UI_STANDARD;
    for(ty=0;ty<2;ty++)for(tx=0;tx<cols;tx++){
        memset(bits,0,sizeof(bits));
        for(y=0;y<8;y++)for(x=0;x<8;x++){
            px=tx*8+x;py=ty*8+y;
            /* Tan corner pixels blend into the suit; gold dots form stitches. */
            color=((px<2||px>=width-2)&&(py<2||py>=14))?2:0;
            if(((py==1||py==14)&&px>=3&&px<width-3&&!(px&1))||
               ((px==1||px==width-2)&&py>=3&&py<13&&!(py&1)))color=2;
            if(py>=4&&py<11&&px>=first&&px<first+6*n-1){
                letter=(px-first)/6;column=(px-first)%6;
                glyph=font_bits[((uint16_t)game.name[letter]-32)*8+py-4];
                if(column<5&&(glyph&(64u>>column)))color=3;
            }
            if(color&1)bits[y*2]|=128u>>x;
            if(color&2)bits[y*2+1]|=128u>>x;
        }
        logical=144+ty*cols+tx;load_ui_tile(logical,bits);tile(left+tx,8+ty,logical);
    }
}

static void prepare_book(void) {
    static uint8_t bits[16];
    uint8_t closed=game.scene==EMPTY_SEAT,cols=closed?2:3,tx,ty,x,y,px,py,color,depth;
    if(!closed&&!book_turn)return;
    sprite_tiles_reserved=closed?4:9;depth=4+(uint16_t)book_turn*20/24;
    for(ty=0;ty<cols;ty++)for(tx=0;tx<cols;tx++){
        memset(bits,0,sizeof(bits));
        for(y=0;y<8;y++)for(x=0;x<8;x++){
            px=tx*8+x;py=ty*8+y;color=0;
            if(closed){
                if(px>=1&&px<=14&&py>=2&&py<=14){
                    color=1;
                    if(px<=3||px==14||py==2)color=2;
                    if(py>=12&&px>=4)color=py==13?1:3;
                    if((px==8&&py>=5&&py<=9)||(py==7&&px>=6&&px<=10))color=3;
                }
            }else if(46-px-py<depth)color=46-px-py==depth-1?2:3;
            if(color&1)bits[y*2]|=128u>>x;
            if(color&2)bits[y*2+1]|=128u>>x;
        }
        load_ui_tile(144+ty*cols+tx,bits);
    }
}

static void book_sprites(void) {
    uint8_t i=0,row,col,which,x,y,r,w;
    if(game.scene==EMPTY_SEAT){
        for(row=0;row<2;row++)for(col=0;col<2;col++){
            set_sprite_tile(i,144+row*2+col+render_tile_offset);set_sprite_prop(i,8);
            move_sprite(i++,32+col*8,80+row*8);
        }
        return;
    }
    /* Transparent sprite edges keep every illustration pixel behind the page. */
    for(row=0;row<12;row++){
        which=!row?135:row==11?137:141;
        set_sprite_tile(i,which);set_sprite_prop(i,8);move_sprite(i++,11,16+row*8);
        which=!row?136:row==11?138:142;
        set_sprite_tile(i,which);set_sprite_prop(i,8);move_sprite(i++,158,16+row*8);
    }
    /* Four transparent corners locate the subject without hiding its face. */
    x=!game.card?13:game.card==1?11:3;y=!game.card?8:game.card==1?5:2;
    r=((anim_frame/3)&1)+(game.card==1?1:0);w=2+r*2;x-=r;y-=r;
    for(row=0;row<2;row++)for(col=0;col<2;col++){
        set_sprite_tile(i,198+row*2+col);set_sprite_prop(i,8);
        move_sprite(i++,8+(x+col*(w-1))*8,16+(y+row*(w-1))*8);
    }
    if(book_turn)for(row=0;row<3;row++)for(col=0;col<3;col++){
        x=book_direction?136+col*8:16-col*8;
        set_sprite_tile(i,144+row*3+col+render_tile_offset);
        set_sprite_prop(i,8|(book_direction?0:32));move_sprite(i++,x+8,88+row*8);
    }
}

static void cinema_caption(const char *a,const char *b,const char *key,const char *text) {
    ink=UI_STANDARD;fill(0,12,20,6,UI_STANDARD);
    if(*a||*b){panel(0,12,20,4,UI_STANDARD);center(13,a);center(14,b);}
    if(*key)action(17,key,text);
}

void render_cinema(void) BANKED {
    uint8_t phase,i;int8_t shift;colored_screen=0;ink=UI_STANDARD;cinema_effects();
    switch(game.scene) {
        case PILOT_CARD:
            load_cinema(CIN_PILOT);
            name_patch();
            cinema_caption("READY TO EXPLORE?","","A","LET'S GO");break;
        case DOOR_NOTE:
            load_cinema(game.card||scene_ticks>=60?CIN_DOOR_CLOSE:CIN_DOOR_WIDE);
            if(game.card||scene_ticks>=60)note_symbols(0);
            cinema_caption("A NOTE ON THE DOOR","",game.card?"A":"", "DECODE");break;
        case HATCH_OPEN:
            load_cinema(game.card||scene_ticks>=45?CIN_HATCH_OPEN:CIN_HATCH_HALF);
            cinema_caption(game.card?"LET'S PEEK INSIDE.":"","",game.card?"A":"","PEEK INSIDE");break;
        case REPAIR:
            load_cinema(game.repaired?CIN_CRYSTAL_IN:CIN_CRYSTAL_OUT);
            if(!game.repaired)moving_crystal(7);
            cinema_caption("THIS PART FELL OUT.","IT FITS HERE!","A",game.repaired?"LOOK INSIDE":"PUT IT BACK");break;
        case CRYSTAL_REPAIR:
            load_cinema(scene_ticks<72?CIN_CRYSTAL_OUT:CIN_CRYSTAL_IN);
            if(scene_ticks<72)moving_crystal(7+scene_ticks/12);
            cinema_caption("","","","");break;
        case SHIP_WAKE:
            load_cinema(game.card||scene_ticks>=96||(scene_ticks/24&1)?CIN_SHIP_BRIGHT:CIN_SHIP_DIM);
            if(!game.card&&scene_ticks>=96){ink=UI_STANDARD;tile(anim_frame&1?4:15,anim_frame&2?3:5,131);}
            cinema_caption(game.card?"THE SHIP IS AWAKE!":"","",game.card?"A":"","LOOK INSIDE");break;
        case EMPTY_SEAT:
            load_cinema(CIN_EMPTY_COCKPIT);prepare_book();
            if(!game.card)cinema_caption("AN EMPTY SEAT!","",scene_ticks>=90?"A":"","TAKE A LOOK");
            else cinema_caption("SHOULD WE FLY?","LET'S CHECK THIS.",scene_ticks>=90?"A":"","OPEN BOOK");break;
        case OWNER_LOG:
            load_cinema(game.card?CIN_MOON_LOG:CIN_MOON_APPROACH);prepare_book();
            if(!game.card){
                /* The same marked location becomes the landing target. */
                cinema_caption("HER MOON VISIT","SOUTH POLE","","");
            }else if(game.card==1){
                /* Keep the broken ship inside a pulsing warning frame. */
                ink=UI_STANDARD;if(anim_frame&2)tile(12,1,143);
                cinema_caption("SHIP TROUBLE!","SHE GOT OUT.","","");
            }else{
                /* A separate signal locates the pilot's escape pod. */
                ink=UI_STANDARD;if(anim_frame&2)tile(6,2,134);
                cinema_caption("SHE IS SAFE.","SHE NEEDS A RIDE.","","");
            }
            ink=UI_STANDARD;print(1,12,"LOG BOOK",0);
            tile(16,12,'1'+game.card-32);print(17,12,"/3",0);
            if(!book_turn){
                if(game.card){badge(1,17,"A",game.card==2?"READ NOTE":"TURN PAGE");badge(13,17,"B","BACK");}
                else action(17,"A","TURN PAGE");
            }
            break;
        case MOON_NOTE:
            load_cinema(game.card||scene_ticks>=60?CIN_MOON_NOTE:CIN_MOON_LOG);
            if(game.card||scene_ticks>=60)note_symbols(1);
            cinema_caption("WHERE IS SHE?","",game.card?"A":"","DECODE");break;
        case LAUNCH:
            load_cinema(game.card?CIN_SEATED:CIN_SHIP_BRIGHT);
            if(!game.card)cinema_caption("TO THE MOON!","","A","TAKE THE SEAT");
            else cinema_caption("READY TO FLY?","","A","LIFT OFF!");
            break;
        case BOARDING:
            load_cinema(scene_ticks<48?CIN_BOARDING:CIN_SEATED);
            if(scene_ticks>=48&&(anim_frame&1)){ink=UI_STANDARD;tile(13,8,128);}
            cinema_caption("","","","");break;
        case ASCENT:
            phase=scene_ticks<120?CIN_LAUNCH_GROUND:scene_ticks<240?CIN_LAUNCH_RISE:scene_ticks<360?CIN_LAUNCH_CLOUD:CIN_LAUNCH_SPACE;
            load_cinema(phase);
            if(scene_ticks<120){shift=anim_frame&1?1:0;camera_shift(shift,0);}
            else if(scene_ticks<240){shift=-(int8_t)((scene_ticks-120)/32);camera_shift(0,shift);}
            else if(scene_ticks<360){shift=(anim_frame&3)-1;camera_shift(shift,-1);}
            else{shift=-(int8_t)((scene_ticks-360)/40);camera_shift(0,shift);}
            ink=UI_STANDARD;
            if(scene_ticks<120){tile(9+(anim_frame&1),10,207);if(anim_frame&2)tile(9+(anim_frame&1),11,207);}
            else for(i=0;i<4;i++)tile(2+i*5,(anim_frame+i*3)%11,206);
            cinema_caption("","","","");break;
        case MOON_APPROACH:
            load_cinema(CIN_MOON_APPROACH);
            if((anim_frame&3)<2){ink=UI_STANDARD;tile(14,9,131);}
            cinema_caption("SOUTH POLE",nav_ready?"READY TO LAND":"ARROWS STEER",nav_ready?"A":"","LAND");break;
        case MOON_WALK:
            load_cinema(CIN_MOON_SURFACE);
            beacon_ring(15,7,(anim_frame/2)%3);
            cinema_caption("SOUTH POLE",nav_ready?"":" > FOLLOW THE LIGHT",nav_ready?"A":"","MEET");break;
        case FIRST_CONTACT:
            load_cinema(game.card?CIN_ALIEN_CONFUSED:CIN_ALIEN_WAIT);
            if(game.card==0){
                if((anim_frame&7)<3){ink=UI_STANDARD;tile(16,2,131);}
                cinema_caption("","","A","SAY HELLO");
            }
            else if(game.card==1){
                speech(1,0,9,"HELLO!",5);speech(12,1,7,"?",1);
                cinema_caption("","","A","TRY HI");
            }
            else{
                speech(3,0,6,"HI!",3);panel(11,1,8,4,UI_STANDARD);ink=UI_STANDARD;
                big(12,2,144,PLUS,0,0);big(16,2,148,HEART,0,0);tile(13,5,'V'-32);
                cinema_caption("","","A","LOOK");
            }
            break;
        case ALIEN_REPLY:
            load_cinema(CIN_ALIEN_CODE);ink=UI_STANDARD;
            print(9,8,"HI",0);print(5,9,"H=",0);big(7,9,144,PLUS,0,0);
            print(10,9,"I=",0);big(12,9,148,HEART,0,0);
            cinema_caption("LET'S TRY HER WAY.","","A","SEND A HELLO");break;
        case FRIENDSHIP:
            load_cinema(CIN_ALIEN_HAPPY);
            if(!game.card){ink=UI_STANDARD;tile((anim_frame&1)?7:11,(anim_frame&2)?2:3,134);}
            cinema_caption(game.card?"A NEW FRIEND!":"","",game.card?"A":"","NEXT");break;
        default:break;
    }
}

static void world(void) {
    char caption[21];colored_screen=0;ink=UI_STANDARD;
    switch(game.scene) {
        case TITLE:
            load_scene(0);center(1,"CIPHERSPACE");
            if(has_save&&game.name[0]){strcpy(caption,"PILOT ");strcat(caption,game.name);center(3,caption);}
            say("CHAPTER 1","THE EMPTY SHIP",has_save?"":"A PLAY");
            if(has_save){badge(0,17,"A","CONTINUE");badge(13,17,"B","NEW");}break;
        case NAME_SETUP:name_setup();break;
        case YARD:
            load_scene(0);strcpy(caption,game.name);strcat(caption,"'S BACKYARD");
            if(!game.card)say(caption,"A SHIP LANDED!","A LOOK CLOSER");
            else say("A NOTE ON THE DOOR","LET'S READ IT.","A READ THE NOTE");
            break;
        case REPAIR:case EMPTY_SEAT:render_cinema();break;
        case LAUNCH:
            if(game.card>=2){countdown();break;}
            render_cinema();break;
        case FLIGHT:
            load_cinema(CIN_FLIGHT);flight_note("STEER TO THE MOON.","LET GO TO STOP.","SELECT: HELP");break;
        case BEACON:
            load_flight();flight_note("FOLLOW THE GLOW.","IT SHOWS THE WAY.","A CONTINUE");
            break;
        case LANDING:
            load_scene(3);label("THE MOON");
            if(!game.card)say("YOU MADE IT!","A LIGHT IS NEAR!","A LOOK AROUND");
            else say("A DIP IN THE MOON.","IT IS A CRATER!","A CONTINUE");
            break;
        case ENDING:
            load_cinema(CIN_ALIEN_HAPPY);say("CHAPTER 1 COMPLETE","NEXT: THE WAY HOME","A TITLE");break;
        default:if(game.scene>=PILOT_CARD&&game.scene<=FRIENDSHIP)render_cinema();break;
    }
}

static void signed_art_map(void) {
    static uint8_t used[48],aliases[128];
    uint8_t id,free_slot=0;
    uint16_t i;
    memset(used,0,sizeof(used));memset(aliases,255,sizeof(aliases));
    for(i=0;i<sprite_tiles_reserved;i++)used[i]=1;
    /* Only UI glyphs reserve these logical slots. Art has its own numbering. */
    for(i=0;i<360;i++){
        id=bgtiles[i];
        if((bgattrs[i]&8)&&id>=144&&id<192)used[id-144]=1;
    }
    for(i=0;i<240;i++){
        id=bgtiles[i];
        if(bgattrs[i]&8){
            if(id<128){
                if(aliases[id]==255){
                    while(free_slot<48&&used[free_slot])free_slot++;
                    /* A future overfull overlay must not read art as letters. */
                    if(free_slot==48){bgtiles[i]=143;continue;}
                    aliases[id]=144+free_slot;
                    used[free_slot++]=1;
                    load_alias(aliases[id],id);
                }
                bgtiles[i]=aliases[id];
            }
        }else if(id>=120){
            /* Alternate art occupies 120 signed tiles in each VRAM bank. */
            bgtiles[i]=id-120;bgattrs[i]|=8;
        }
    }
}

void render_game(void) BANKED {
    static uint8_t page_tiles[2][360],page_attrs[2][360],page_valid[2];
    uint8_t next_page=(LCDC_REG&LCDCF_BG9C00)?0:1,row,ui_only,show_ship,show_book;
    uint16_t i,offset;
    uint8_t *next_map=(uint8_t *)(next_page?0x9c00u:0x9800u);

    /* Build the next screen away from the visible tilemap. Dynamic glyphs
       have matching hidden copies, so changing one never tears the old view. */
    pending_art_signed=0;sprite_tiles_reserved=0;render_tile_offset=next_page?64:0;
    memset(bgtiles,0,360);memset(bgattrs,15,360);
    if(ui_mode!=PLAY)menus();
    else if(puzzle_id()!=255)puzzle();
    else world();
    if(pending_art_signed)signed_art_map();
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
    show_ship=ui_mode==PLAY&&(game.scene==FLIGHT||game.scene==BEACON||game.scene==MOON_APPROACH||game.scene==MOON_WALK);
    show_book=ui_mode==PLAY&&(game.scene==OWNER_LOG||game.scene==EMPTY_SEAT);
    /* Keep VBlank from copying a partly rebuilt sprite list. The scanline
       interrupt must stay active while the old illustration is visible. */
    DISABLE_OAM_DMA;
    for(row=0;row<40;row++)hide_sprite(row);
    if(show_book)book_sprites();else if(show_ship)sprite_position();

    /* Commit palette, map and sprite visibility together during VBlank.
       The LCD is enabled once after boot, and stays enabled for all input. */
    if(LCDC_REG&LCDCF_ON)wait_vbl_done();
    ENABLE_OAM_DMA;refresh_OAM();
    display_art_signed=pending_art_signed;visible_art=!ui_only;
    if(display_art_signed)LCDC_REG&=~LCDCF_BG8000;else LCDC_REG|=LCDCF_BG8000;
    if(ui_only)set_bkg_palette(0,8,ui_screen_palettes);
    else{set_bkg_palette(0,7,render_art_palettes);set_bkg_palette(7,1,ui_palette);}
    if(next_page)LCDC_REG|=LCDCF_BG9C00;else LCDC_REG&=~LCDCF_BG9C00;
    if(show_ship||show_book)SHOW_SPRITES;else HIDE_SPRITES;
    SHOW_BKG;
    if(!(LCDC_REG&LCDCF_ON))DISPLAY_ON;
}
