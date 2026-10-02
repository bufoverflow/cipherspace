#pragma bank 1
#include "game.h"

uint8_t bgtiles[360],bgattrs[360];

static void tile(uint8_t x,uint8_t y,uint8_t value) {
    if(x<20&&y<18){bgtiles[(uint16_t)y*20+x]=value;bgattrs[(uint16_t)y*20+x]=15;}
}

static void print(uint8_t x,uint8_t y,const char *s,uint8_t inv) {
    uint8_t c;
    while(*s&&x<20){c=*s++;if(c>='a'&&c<='z')c-=32;tile(x++,y,c>=32&&c<96?c-32+(inv?64:0):0);}
}

static void center(uint8_t y,const char *s) {
    /* Every centered line is limited to 20 cells. */
    print((20-(uint8_t)strlen(s))/2,y,s,0);
}

static void rule(uint8_t y) {uint8_t x;for(x=0;x<20;x++)tile(x,y,'-'-32);}

static void label(const char *s) {print(0,0,s,1);}

static void say(const char *a,const char *b,const char *c,const char *foot) {
    rule(12);print(1,13,a,0);print(1,14,b,0);print(1,15,c,0);print(1,17,foot,0);
}

static void big(uint8_t x,uint8_t y,uint8_t base,uint8_t symbol,uint8_t letter,uint8_t inverse) {
    load_big(base,symbol,letter,inverse);
    tile(x,y,base);tile(x+1,y,base+1);tile(x,y+1,base+2);tile(x+1,y+1,base+3);
}

static void pair(uint8_t x,uint8_t y,uint8_t symbol) {
    char l[2];l[0]=letters[symbol];l[1]=0;
    tile(x,y,128+symbol);print(x+1,y,"=",0);print(x+2,y,l,0);
}

static void puzzle(void) {
    uint8_t p=puzzle_id(),i,x,l,a,done=game.solved&(1u<<p);
    static const char *const titles[3]={"THE FIRST NOTE","WHERE IS SHE?","SEND A HELLO?"};
    center(0,titles[p]);rule(1);print(1,2,"KEY",0);print(14,2,p==2?"SEND":"READ",0);
    if(p==0){for(i=0;i<4;i++)pair(2+i*4,3,i);}
    else if(p==1){pair(4,3,CIRCLE);pair(8,3,STAR);pair(12,3,DIAMOND);}
    else{pair(6,3,PLUS);pair(10,3,HEART);}
    for(i=0;i<lengths[p];i++) {
        x=(20-lengths[p]*4)/2+i*4;l=words[p][i];a=game.answers[p][l];
        if(i==slot&&!done)print(x,4,"--",0);
        big(x,5,144+i*4,p==2?letters[l]:l,p==2,0);
        print(x,7," V",0);
        big(x,8,160+i*4,a==255?'?':(p==2?a:letters[a]),a==255||p!=2,i==slot&&!done);
    }
    if(done) {
        if(p==0){center(12,"YOU OPENED IT!");center(14,"LET'S LOOK INSIDE.");}
        else if(p==1){center(12,"SHE IS ON THE MOON!");center(14,"LET'S GO FIND HER!");}
        else{center(12,"A MESSAGE CAME BACK!");center(14,"SOMEONE IS WAITING.");}
        center(17,"A CONTINUE");
    } else {
        for(i=0;i<option_counts[p];i++){
            x=(20-option_counts[p]*3)/2+i*3;l=options[p][i];
            big(x,12,176+i*4,p==2?l:letters[l],p!=2,i==choice);
        }
        center(15,wrong?"CHECK THE KEY.":p==2&&!can_undo()?"A CHOOSE  B LATER":"A CHOOSE  B UNDO");
        center(16,"ARROWS PICK");
        center(17,"SELECT FOR HELP");
    }
}

static void words_page(void) {
    uint8_t i;
    static const char *const names[5]={"KEY","PILOT","BEACON","CRATER","ANTENNA"};
    center(0,"WORDS TO EXPLORE");rule(1);center(3,names[word_index]);
    switch(word_index) {
        case 0:
            center(5,"THIS GUIDE SHOWS");center(6,"THE SWAPS.");
            for(i=0;i<4;i++)pair(2+i*4,9,i);
            for(i=4;i<7;i++)pair(4+(i-4)*4,11,i);
            break;
        case 1:center(6,"A PILOT FLIES");center(7,"THE SHIP.");center(10,"YOU ARE THE PILOT!");break;
        case 2:center(5,"IT SENDS A MESSAGE:");center(7,"HERE I AM!");center(10,"IT HELPS US FIND");center(11,"A PLACE.");break;
        case 3:center(6,"A ROUND DIP IN");center(7,"THE GROUND IS");center(8,"A CRATER.");center(11,"SEE ONE ON THE MOON!");break;
        case 4:center(6,"THIS PART HELPS US");center(7,"SEND AND GET");center(8,"MESSAGES.");break;
    }
    center(15,"LEFT / RIGHT: WORD");center(17,"B BACK");
}

static void help_page(void) {
    uint8_t p=puzzle_id(),l;
    center(0,"PUZZLE HELP");rule(1);
    if(p!=255) {
        l=words[p][0];
        if(hint_step==0){center(5,"LOOK AT THE KEY.");center(7,p==2?"FIND THE LETTER.":"FIND THE SAME SHAPE.");}
        else if(hint_step==1){center(5,"ONE SHAPE STANDS");center(6,"FOR ONE LETTER.");center(9,"LOOK AT THE PAIRS.");}
        else{
            center(4,"TRY THIS PAIR.");
            big(6,7,144,l,0,0);print(9,7,"=",0);big(11,7,148,letters[l],1,0);
            center(11,"NOW TRY THE OTHERS.");
        }
    }else if(game.scene==FLIGHT){center(5,"FLY TO THE ROUND");center(6,"MOON AT THE TOP.");center(9,"LET GO TO STOP.");}
    else{center(5,"LOOK FOR A LIT PART.");center(7,"PRESS A TO LOOK.");}
    center(15,hint_step<2?"A MORE":"A TRY IT");center(17,"B BACK");
}

static void menus(void) {
    char caption[21];
    if(ui_mode==WORD_HELP){words_page();return;}
    if(ui_mode==PUZZLE_HELP){help_page();return;}
    if(ui_mode==HELP_MENU){
        center(0,"YOUR GUIDE");rule(1);
        print(3,5,"WORD HELP",selection==0);print(3,8,"PUZZLE HELP",selection==1);print(3,11,"BACK",selection==2);
        print(1,5+selection*3,">",0);center(15,"UP / DOWN THEN A");center(17,"HELP IS ALWAYS FREE");
    }else if(ui_mode==PAUSE_MENU){
        center(2,"PAUSED");rule(4);center(7,"A KEEP PLAYING");center(10,game.sound?"B SOUND: ON":"B SOUND: OFF");center(13,"SELECT: TITLE");center(16,"YOUR TRIP IS SAVED.");
        strcpy(caption,"PILOT: ");strcat(caption,game.name);center(3,caption);
    }else if(ui_mode==RESET_CONFIRM){
        center(2,"START A NEW TRIP?");center(5,"THIS STARTS OVER.");center(8,"HOLD A+B TO RESET");center(10,"FOR TWO SECONDS.");center(15,"B BACK");
    }
}

static void name_setup(void) {
    uint8_t i,x,y;
    char letter[2];letter[1]=0;
    center(0,"YOUR PILOT");center(2,"WHAT IS YOUR NAME?");
    for(i=0;i<8;i++)big(2+i*2,4,144+i*4,i<name_length?game.name[i]:'_',1,i==name_length);
    center(6,name_error==1?"ADD A LETTER FIRST":name_error==2?"8 LETTERS FIT.":"CHOOSE 1-8 LETTERS");
    for(i=0;i<28;i++){
        x=2+(i%7)*2;y=8+(i/7)*2;
        if(i<26){letter[0]='A'+i;print(x,y,letter,i==name_choice);}
        else print(x,y,i==26?"<":"DONE",i==name_choice);
    }
    center(16,name_choice==26?"A ERASE  B ERASE":name_choice==27?"A DONE  B ERASE":"A ADD  B ERASE");center(17,"START: DONE");
}

static void world(void) {
    char caption[21];
    switch(game.scene) {
        case TITLE:
            load_scene(0);center(2,"CIPHERSPACE");
            if(has_save&&game.name[0]){strcpy(caption,"PILOT ");strcat(caption,game.name);center(4,caption);}
            say("CHAPTER 1","THE EMPTY SHIP",has_save?"B NEW TRIP":"YOUR ADVENTURE",has_save?"A CONTINUE":"A PLAY");break;
        case NAME_SETUP:name_setup();break;
        case YARD:
            load_scene(0);strcpy(caption,game.name);strcat(caption,"'S BACKYARD");label(caption);
            if(!game.card)say("SOMETHING LANDED","IN THE BACKYARD!","","A LOOK CLOSER");
            else say("THE SHIP IS QUIET.","SHAPES HIDE A WORD.","LET'S READ IT!","A READ THE NOTE");
            break;
        case REPAIR:
            load_scene(game.repaired?1:0);
            if(game.repaired)say("THE SHIP IS AWAKE!","LOOK AT IT GLOW!","","A LOOK INSIDE");
            else say("A STONE FELL OUT!","IT FITS RIGHT HERE.","","A PUT IT BACK");
            break;
        case EMPTY_SEAT:
            load_scene(2);
            if(!game.card)say("THE SEAT IS EMPTY.","WHO FLIES THE SHIP?","","A LOOK AT THE NOTE");
            else if(game.card==1)say("SHE IS SAFE.","SHE NEEDS A RIDE.","","A CONTINUE");
            else say("WHERE IS SHE?","THERE IS A MESSAGE!","","A READ IT");
            break;
        case LAUNCH:
            load_scene(game.card?2:1);
            if(!game.card)say("THE SHIP CAN FLY!","LET'S GO FIND HER.","","A TAKE THE SEAT");
            else say("YOU ARE THE PILOT!","ARROWS MOVE.","LET GO TO STOP.","A LIFT OFF");
            break;
        case FLIGHT:
            load_flight();label("TO THE MOON");say("STEER TO THE MOON.","THE SHIP WILL LAND","WHEN YOU GET CLOSE.","SELECT FOR HELP");break;
        case BEACON:
            load_flight();
            if(!game.card)say("IT SENDS A MESSAGE:","HERE I AM!","","A CONTINUE");
            else if(game.card==1)say("THAT IS A BEACON.","IT HELPS US FIND","A PLACE.","A CONTINUE");
            else say("FOLLOW THE GLOWING","DOT.","SEND A HELLO FIRST?","A WRITE A MESSAGE");
            break;
        case LANDING:
            load_scene(3);label("THE MOON");
            if(!game.card)say("YOU MADE IT!","A LIGHT IS WAITING.","","A LOOK AROUND");
            else say("THIS ROUND DIP","IS A CRATER.","SOMEONE IS NEARBY!","A CONTINUE");
            break;
        case ENDING:
            load_scene(3);say("YOU FIXED THE SHIP.","YOU FOUND THE MOON!","CHAPTER 1 COMPLETE","A TITLE");break;
        default:break;
    }
}

void render_game(void) BANKED {
    if(LCDC_REG&LCDCF_ON)wait_vbl_done();DISPLAY_OFF;HIDE_SPRITES;
    memset(bgtiles,0,360);memset(bgattrs,15,360);
    if(ui_mode!=PLAY)menus();
    else if(puzzle_id()!=255)puzzle();
    else world();
    set_bkg_palette(7,1,ui_palette);
    VBK_REG=0;set_bkg_tiles(0,0,20,18,bgtiles);
    VBK_REG=1;set_bkg_tiles(0,0,20,18,bgattrs);VBK_REG=0;
    if(ui_mode==PLAY&&(game.scene==FLIGHT||game.scene==BEACON)){sprite_position();SHOW_SPRITES;}
    SHOW_BKG;DISPLAY_ON;
}
