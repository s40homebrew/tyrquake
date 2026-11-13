/* SPDX-License-Identifier: GPL-2.0-or-later
 * Adapt original Quake drawing to portrait: native 8px text, reflowed menus
 * and a 48px stock HUD with tighter horizontal spacing. Menu/game logic and
 * artwork remain upstream. Temporary dimensions are only used for overlays.
 */
#include "quakedef.h"
#include "common.h"
#include "client.h"
#include "keys.h"
#include "menu.h"
#include "screen.h"
#include "sbar.h"
#include "draw.h"
#include "vid.h"
#include "ui_s40.h"

static byte ui_pixels[320*200];
static viddef_t world_vid;
static int ui_top,ui_rows,lcd_y;

static void begin(int y,int top,int rows) {
    world_vid=vid; lcd_y=y; ui_top=top; ui_rows=rows;
    /* Seed from the existing world. Ceil here and floor in end() make the
     * background round-trip exact, including pixels under transparent art. */
    for(int row=0;row<rows;row++) {
        int sy=qmin((row*3+3)/4,rows*3/4-1);
        const byte *src=vid.buffer+(y+sy)*vid.rowbytes;
        byte *dst=ui_pixels+(top+row)*320;
        for(int x=0;x<320;x++)dst[x]=src[qmin((x*3+3)/4,239)];
    }
    vid.buffer=vid.conbuffer=ui_pixels;
    vid.width=vid.conwidth=vid.rowbytes=vid.conrowbytes=320;
    vid.height=vid.conheight=200;
}
static void end(void) {
    vid=world_vid;
    for(int y=0;y<ui_rows*3/4;y++) {
        const byte *src=ui_pixels+(ui_top+y*4/3)*320;
        byte *dst=vid.buffer+(lcd_y+y)*vid.rowbytes;
        for(int x=0;x<240;x++)dst[x]=src[x*4/3];
    }
    scr_copyeverything=1;
}
extern int m_state;
extern qboolean sb_showscores;
extern const qpic_t *sb_scorebar;

/* Menu state, selection, actions and assets all remain in menu.c. Only its
 * drawing helpers adapt desktop coordinates to the narrower display. */
static int option_page(void) { return m_state>=7 && m_state<=11; }
static void text(int x,int y,const char *s,int gold,int limit) {
    for(int i=0;s[i] && i<limit && x+i*8<=232;i++)
        Draw_Character(x+i*8,y,(unsigned char)s[i]+(gold?128:0));
}
void S40_MenuCharacter(int x,int y,int c) {
    if(option_page()) {
        if(x>=212) { x=136+x-212; y=72+(y-32)*3+10; }
        else { x=8; y=72+(y-32)*3; }
    } else if(m_state==3 || m_state==4) { x=2; y=64+(y-32)*5/2; }
    else if(m_state==13) { x=x>=130?120:8; y=80+(y-48)*3/2; }
    else { x=qmax(0,x-40); y+=52; }
    Draw_Character(x,y,c);
}
void S40_MenuText(int x,int y,const char *s,int gold) {
    if(option_page()) {
        while(*s==' ')s++;
        x=x>=220?176:24; y=72+(y-32)*3;
    } else if(m_state==3 || m_state==4) {
        y=64+(y-32)*5/2;
        text(16,y,s,gold,26);
        if(strlen(s)>26)text(16,y+8,s+26,gold,26);
        return;
    } else if(m_state==13) {
        if(y==32) { text(8,48,!strncmp(s,"Press",5)?"PRESS A KEY; *: CANCEL":"5: CHANGE    *: BACK",gold,28); return; }
        y=80+(y-48)*3/2; x=x<140?8:128+x-140;
        text(x,y,s,gold,x==8?14:13); return;
    } else { x=qmax(8,x-40); y+=52; }
    text(x,y,s,gold,28);
}
static void picture(int x,int y,const qpic_t *p,int w,int h,int transparent,const byte *translation) {
    for(int row=0;row<h;row++)for(int col=0;col<w;col++) {
        if(x+col<0 || x+col>=240 || y+row<0 || y+row>=320)continue;
        byte c=p->data[(row*p->height/h)*p->width+col*p->width/w];
        if(!transparent || c!=255)vid.buffer[(y+row)*vid.rowbytes+x+col]=translation?translation[c]:c;
    }
}
void S40_MenuPicture(int x,int y,const qpic_t *p,int transparent) {
    int w=p->width,h=p->height;
    if(w==32 && h==144) {
        if(m_state==1 || option_page() || m_state==13)return;
        x=0; y=72;
    } else if(m_state==1 && x==72 && y==32) {
        x=24; y=80; w=216; h=p->height*9/10;
    } else if(m_state==1 && x==54) {
        x=4; y=80+(y-32)*9/10;
    } else if((m_state==2 || m_state==5) && y>=32) {
        x=x==54?34:64; y+=56;
    } else if(y==4) { x=(240-w)/2; y=24; }
    else { x-=40; y+=52; }
    if(w>240) {
        w=240; h=p->height*240/p->width; x=0;
    }
    picture(x,y,p,w,h,transparent,NULL);
}
void S40_MenuTranslatedPicture(int x,int y,const qpic_t *p,const byte *translation) {
    picture(x-40,y+52,p,p->width,p->height,1,translation);
}
void S40_DrawHelp(void) {
    static const char *lines[]={"D-PAD: MOVE / TURN","1 / 3: STRAFE","5: FIRE / CONFIRM","0: JUMP","2 / 8: LOOK UP / DOWN","4: CENTRE VIEW","7 / 9: CHANGE WEAPON","#: QUICK-SAVE","6: QUICK-LOAD","*: MENU / BACK"};
    text(64,32,"NOKIA CONTROLS",1,20);
    for(int i=0;i<10;i++)text(24,76+i*20,lines[i],0,26);
}
void S40_DrawMenu(void) {
    if(key_dest!=key_menu)return;
    if(scr_con_current)Draw_ConsoleBackground(vid.height);
    else Draw_FadeScreen();
    M_Draw();
    scr_copyeverything=1;
}
void S40_DrawStatusBar(void) {
    if(scr_con_current==vid.height || cls.state!=ca_active || !sb_lines)return;
    int lines=sb_lines;
    float console=scr_con_current;
    world_vid=vid;
    memset(ui_pixels+152*320,0,48*320);
    vid.buffer=vid.conbuffer=ui_pixels;
    vid.width=vid.conwidth=vid.rowbytes=vid.conrowbytes=320;
    vid.height=vid.conheight=200;
    sb_lines=48; scr_con_current=0;
    Sbar_Changed(); Sbar_Draw();
    sb_lines=lines; scr_con_current=console; vid=world_vid;
    /* Keep the original 48-pixel height. Close the decorative gaps between
     * armor, health and ammo, rather than shrinking the whole bar by 25%. */
    for(int y=0;y<48;y++) {
        const byte *src=ui_pixels+(152+y)*320;
        byte *dst=vid.buffer+(272+y)*vid.rowbytes;
        for(int x=0;x<240;x++) {
            int sx=y<24?x*4/3:(x/80)*112+(x%80)*6/5;
            dst[x]=src[sx];
        }
    }
    if(sb_showscores || cl.stats[STAT_HEALTH]<=0) {
        /* The stock score view has two wide text columns. Stack those same
         * fields and retain the original score-bar texture and 8px font. */
        for(int y=0;y<48;y++) {
            const byte *src=sb_scorebar->data+(y%24)*sb_scorebar->width;
            byte *dst=vid.buffer+(272+y)*vid.rowbytes;
            for(int x=0;x<240;x++)dst[x]=src[160+(x%16)];
        }
        char line[40];
        snprintf(line,sizeof(line),"Monsters: %d / %d",cl.stats[STAT_MONSTERS],cl.stats[STAT_TOTALMONSTERS]);text(8,274,line,0,28);
        snprintf(line,sizeof(line),"Secrets:  %d / %d",cl.stats[STAT_SECRETS],cl.stats[STAT_TOTALSECRETS]);text(8,286,line,0,28);
        snprintf(line,sizeof(line),"Time: %d:%02d",(int)cl.time/60,(int)cl.time%60);text(8,298,line,0,28);
        text(8,310,cl.levelname,0,28);
    } else {
        /* Copy the small ammo digits pixel-for-pixel at an 8px pitch. The
         * inventory pictures may narrow; its text must never become 6px. */
        for(int i=0;i<4;i++)for(int y=0;y<8;y++)
            memcpy(vid.buffer+(272+y)*vid.rowbytes+6+i*36,
                   ui_pixels+(152+y)*320+10+i*48,24);
    }
    scr_copyeverything=1;
}
void S40_DrawIntermission(void) {
    begin(85,0,200); Sbar_IntermissionOverlay(); end();
}
void S40_DrawFinale(void) {
    begin(0,0,200); Sbar_FinaleOverlay(); end();
}
void S40_DrawFPS(unsigned tenths) {
    if(key_dest!=key_game || cls.state!=ca_active)return;
    char line[20]; snprintf(line,sizeof(line),"%u.%u FPS",tenths/10,tenths%10);
    int x=240-8*(int)strlen(line)-4;
    Draw_Fill(x-2,2,242-x,12,0);
    for(int i=0;line[i];i++)Draw_Character(x+i*8,4,line[i]);
}
void S40_CenterMessage(const char *s,int y,int remaining) {
    while(*s && y<=vid.height-8) {
        int n=0,last_space=-1;
        while(s[n] && s[n]!='\n' && n<28) { if(s[n]==' ')last_space=n; n++; }
        if(n==28 && s[n] && s[n]!='\n' && last_space>0)n=last_space;
        int x=(240-n*8)/2;
        for(int i=0;i<n;i++) { if(remaining--<=0)return; Draw_Character(x+i*8,y,s[i]); }
        s+=n;
        if(*s=='\n')s++; else while(*s==' ')s++;
        y+=10;
    }
}
