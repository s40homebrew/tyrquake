/* SPDX-License-Identifier: GPL-2.0-or-later
 * Native Nokia 301 RM-839 02.33 backend for TyrQuake. Java only bootstraps.
 */
#include "firmware_s40.h"
#include "quakedef.h"
#include "common.h"
#include "host.h"
#include "sys.h"
#include "client.h"
#include "cmd.h"
#include "keys.h"
#include "input.h"
#include "vid.h"
#include "d_local.h"
#include "draw.h"
#include "view.h"
#include "ui_s40.h"
#include "sound.h"
#include "s40/audio.h"
#include "libretro.h"
#include <string.h>

uint32_t *s40_result;
static jmp_buf finish_env;
static unsigned start_ms, frames, pump_depth;
static int app_id, finishing;
static uint32_t *lcd;
static void *canvas;
static unsigned fps_start, fps_frames, fps_tenths;
static int held[256];
viddef_t vid;
unsigned short d_8to16table[256];
unsigned d_8to24table[256];
qboolean isDedicated, mouseactive;
bool shutdown_core;
cvar_t _windowed_mouse = {"_windowed_mouse", "0"};
unsigned short ramps[3][256];
void (*VID_SetGammaRamp)(unsigned short ramp[3][256]);

void s40_finish(int status) { longjmp(finish_env, status ? status : 1000); }
void s40_pump(void) {
    if (pump_depth++) { --pump_depth; return; }
    for (unsigned budget=0; budget<32 && fw_msg_count(); ++budget) {
        uintptr_t msg=fw_os_msg_receive();
        if (!msg) continue;
        if (msg>=192 && msg<=441) {
            if (!fw_vm_timer(msg)) fw_CM_Timer_Timeout(msg-192);
            else if (msg-192==73) fw_debug_timer();
        } else if (*(volatile unsigned char *)msg!=1) {
            fw_CM_ComMgr_ProcessPhonetMessage((void *)msg);
        }
        fw_CM_ComMgr_ProcessInternalQueue();
    }
    fw_CM_ComMgr_ProcessInternalQueue();
    fw_CM_ComMgr_ScanISISubscriptions();
    --pump_depth;
}
bool Sys_Error(const char *fmt, ...) {
    va_list ap; va_start(ap,fmt);
    vsnprintf((char *)(s40_result+16),256,fmt,ap); va_end(ap);
    printf("ERROR: %s\n",(char *)(s40_result+16));
    s40_finish(-1);
}
void Sys_Printf(const char *fmt, ...) {
    va_list ap; va_start(ap,fmt); vprintf(fmt,ap); va_end(ap);
}
void Sys_Quit(void) { s40_finish(0); }
double Sys_DoubleTime(void) {
    return (uint32_t)(fw_os_current_time_read_in_ms()-start_ms)*0.001;
}
void Sys_Init(void) {}
void Sys_HighFPPrecision(void) {}
void Sys_LowFPPrecision(void) {}
void Sys_SetFPCW(void) {}
void Sys_MakeCodeWriteable(unsigned long a,unsigned long n) {(void)a;(void)n;}
char *Sys_ConsoleInput(void) { return NULL; }
static bool environment(unsigned cmd,void *data) {
    (void)data;
    if (cmd==RETRO_ENVIRONMENT_SHUTDOWN) Sys_Quit();
    return false;
}
retro_environment_t environ_cb=environment;

static int map_key(int code) {
    int menu=key_dest!=key_game;
    switch (code) {
    case -1: return K_UPARROW;
    case -2: return K_DOWNARROW;
    case -3: return K_LEFTARROW;
    case -4: return K_RIGHTARROW;
    case -5: case '5': return menu ? K_ENTER : K_LCTRL;
    case -6: return K_ENTER;
    case -7: case '*': return K_ESCAPE;
    case '0': return K_SPACE;
    case '1': return ',';
    case '3': return '.';
    case '2': return menu ? K_UPARROW : K_PGUP;
    case '8': return menu ? K_DOWNARROW : K_PGDN;
    case '4': return menu ? K_LEFTARROW : K_END;
    case '7': return '[';
    case '9': return ']';
    default: return 0;
    }
}
void Sys_SendKeyEvents(void) {
    int cat,type,value;
    s40_pump();
    for (unsigned budget=0; budget<64 && fw_event_pop(app_id,&cat,&type,&value); ++budget) {
        if (cat!=1) {
            if (cat==2 && type==4) Sys_Quit();
            continue;
        }
        if (type!=1 && type!=3) continue;
        int code=(int16_t)value, down=type==1, index=code&255;
        s40_result[8]=code; s40_result[9]=type;
        if (!down) {
            if (held[index]) Key_Event(held[index],false);
            held[index]=0; continue;
        }
        if (held[index]) continue;
        if (key_dest==key_game && (code=='#' || code=='6')) {
            Cbuf_AddText(code=='#' ? "save s40quick\n" : "load s40quick\n");
            continue;
        }
        held[index]=map_key(code);
        if (held[index]) Key_Event(held[index],true);
    }
}
void IN_Init(void) {}
void IN_Shutdown(void) {}
void IN_Commands(void) {}
void IN_Move(usercmd_t *cmd) { (void)cmd; }
void IN_ModeChanged(void) {}
void IN_Accumulate(void) {}
void IN_ClearStates(void) {
    for (unsigned i=0;i<256;i++) {
        if (held[i]) Key_Event(held[i],false);
        held[i]=0;
    }
}
void IN_ProcessEvents(void) { Sys_SendKeyEvents(); }

void VID_SetPalette(unsigned char *p) {
    for (unsigned i=0;i<256;i++,p+=3) {
        d_8to24table[i]=0xff000000u | (p[0]<<16) | (p[1]<<8) | p[2];
        d_8to16table[i]=((p[0]&248)<<8)|((p[1]&252)<<3)|(p[2]>>3);
    }
}
void VID_ShiftPalette(unsigned char *p) { VID_SetPalette(p); }
void VID_SetPalette2(unsigned char *p) { VID_SetPalette(p); }
void retro_set_rumble_touch(unsigned intensity,float duration) {(void)intensity;(void)duration;}
void retro_set_rumble_damage(int damage) {(void)damage;}
void VID_Init(unsigned char *palette) {
    if (!*(volatile uintptr_t *)0x60352DF8u) Sys_Error("No foreground surface");
    vid.width=vid.conwidth=240; vid.height=vid.conheight=320;
    vid.rowbytes=vid.conrowbytes=240;
    vid.maxwarpwidth=WARP_WIDTH; vid.maxwarpheight=WARP_HEIGHT;
    vid.aspect=1.0f; vid.numpages=1;
    vid.colormap=host_colormap;
    vid.fullbright=256-LittleLong(*((int *)vid.colormap+2048));
    vid.buffer=vid.conbuffer=calloc(240*320,1);
    d_pzbuffer=calloc(240*320,sizeof(short));
    void *cache=malloc(768*1024);
    lcd=calloc(240*320,4);
    if (!vid.buffer || !d_pzbuffer || !cache || !lcd) Sys_Error("Video allocation failed");
    for(unsigned i=0;i<240*320;i++) lcd[i]=0xff000000u;
    D_InitCaches(cache,768*1024);
    VID_SetPalette(palette);
    int error=0;
    canvas=fw_gdi_wrap_pixels(&error,12,240,320,0x85,240*4,lcd);
    if (error || !canvas) Sys_Error("GDI context failed: %d",error);
    fw_ui_refresh(0,0,240,320,canvas,0,0);
    s40_result[4]=3;
}
void VID_Shutdown(void) {
    if (canvas) { fw_gdi_free_context(canvas); canvas=NULL; }
    /* Tracked allocator reclaims all video and engine memory on exit. */
}
void VID_Update(vrect_t *rects) {
    (void)rects;
    if (!canvas) return;
    unsigned now=fw_os_current_time_read_in_ms();
    fps_frames++;
    if (now-fps_start>=1000) {
        fps_tenths=fps_frames*10000/(now-fps_start);
        fps_frames=0; fps_start=now;
    }
    S40_DrawFPS(fps_tenths);
    /* These allocations never overlap. Keep their addresses in registers:
     * byte stores elsewhere must not make the compiler reload vid.buffer
     * for every output pixel. Four pixels amortize loop/address overhead. */
    const byte *restrict src=vid.buffer;
    uint32_t *restrict dst=lcd;
    const unsigned *restrict palette=d_8to24table;
    for (unsigned i=0;i<240*320;i+=4) {
        dst[i]=palette[src[i]];
        dst[i+1]=palette[src[i+1]];
        dst[i+2]=palette[src[i+2]];
        dst[i+3]=palette[src[i+3]];
    }
    /* Native portrait framebuffer; no resampling, offset or tiled margins. */
    fw_ui_refresh(0,0,240,320,canvas,0,0);
    s40_result[5]=++frames; s40_result[4]=6;
    s40_pump();
}
qboolean VID_IsFullScreen(void) { return true; }
void VID_LockBuffer(void) {}
void VID_UnlockBuffer(void) {}
void D_BeginDirectRect(int x,int y,const byte *p,int w,int h) {(void)x;(void)y;(void)p;(void)w;(void)h;}
void D_EndDirectRect(int x,int y,int w,int h) {(void)x;(void)y;(void)w;(void)h;}

static int find_data(void) {
    static const char *roots[]={"D:\\quake\\","C:\\predefgallery\\quake\\","C:\\quake\\","E:\\quake\\"};
    for(unsigned i=0;i<sizeof(roots)/sizeof(*roots);i++) {
        s40_set_root(roots[i]);
        FILE *f=fopen("id1/pak0.pak","rb"); if (!f) continue;
        char header[4]; size_t n=fread(header,1,4,f); fclose(f);
        if(n==4 && !memcmp(header,"PACK",4)) return 1;
    }
    return 0;
}
static void bindings(void) {
    Key_SetBinding(K_UPARROW,"+forward"); Key_SetBinding(K_DOWNARROW,"+back");
    Key_SetBinding(K_LEFTARROW,"+left"); Key_SetBinding(K_RIGHTARROW,"+right");
    Key_SetBinding(K_LCTRL,"+attack"); Key_SetBinding(K_SPACE,"+jump");
    Key_SetBinding(K_COMMA,"+moveleft"); Key_SetBinding(K_PERIOD,"+moveright");
    Key_SetBinding(K_PGUP,"+lookup"); Key_SetBinding(K_PGDN,"+lookdown");
    Key_SetBinding(K_END,"centerview");
    Key_SetBinding(K_LEFTBRACKET,"impulse 12"); Key_SetBinding(K_RIGHTBRACKET,"impulse 10");
}
__attribute__((visibility("default"))) void native_main(uint32_t *result) {
    s40_result=result; result[0]=0x444f4f4d; result[4]=1;
    start_ms=fw_os_current_time_read_in_ms(); fps_start=start_ms;
    result[6]=fw_os_task_own_id_read(); app_id=fw_app_id(); result[7]=app_id;
    int status=setjmp(finish_env);
    if (!status) {
        if(app_id<=0) Sys_Error("No native application ID");
        result[4]=2; s40_pump();
        if(!find_data()) Sys_Error("Put pak0.pak (and pak1.pak) in D:\\quake\\id1\\");
        s40_set_log(fopen("quake.log","wb"));
        printf("RM839 02.33 native TyrQuake 0.62; root %s\n",s40_get_root());
        quakeparms_t parms={0};
        for(int mb=8;mb>=6;mb-=2) {
            parms.membase=malloc(mb*1024*1024);
            if(parms.membase) { parms.memsize=mb*1024*1024; break; }
        }
        if(!parms.membase) Sys_Error("Cannot allocate 6 MiB Quake heap");
        parms.basedir="."; parms.savedir="./id1";
        const char *argv[]={"quake","-nocdaudio"};
        COM_InitArgv(2,argv); parms.argc=com_argc; parms.argv=com_argv;
        result[4]=4;
        if (!Host_Init(&parms)) Sys_Error("Host_Init failed; see quake.log");
        bindings();
        Cbuf_AddText("viewsize 100\nmenu_main\n");
        result[4]=5;
        unsigned last=fw_os_current_time_read_in_ms();
        for (;;) {
            unsigned now=fw_os_current_time_read_in_ms(), delta=now-last;
            if(delta<33) { s40_pump(); continue; }
            last=now;
            Host_Frame(delta*0.001f);
            if(cls.state==ca_active) {
                S_Update(r_origin,vpn,vright,vup);
                CL_DecayLights();
            } else S_Update(vec3_origin,vec3_origin,vec3_origin,vec3_origin);
        }
    }
    if(!finishing) {
        finishing=1;
        if(status==1000 && host_initialized) Host_Shutdown();
    }
    s40_audio_close();
    VID_Shutdown();
    result[3]=status==1000?0:status;
    result[10]=fw_os_current_time_read_in_ms()-start_ms;
    printf("Exit %d; frames %u; elapsed %u ms\n",status,frames,result[10]);
    s40_cleanup();
}
