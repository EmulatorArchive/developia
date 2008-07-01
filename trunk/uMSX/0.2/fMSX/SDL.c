/** fMSX: portable MSX emulator ******************************/
/**                                                         **/
/**                         Unix.c                          **/
/**                                                         **/
/** This file contains Unix-dependent subroutines and       **/
/** drivers. It includes screen drivers via Display.h.      **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-2008                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
#include "MSX.h"
#include "Console.h"
#include "EMULib.h"
#include "NetPlay.h"
#include "Sound.h"

#include <SDL/SDL.h>
#include <string.h>
#include <stdio.h>

#include "../../uGUI/menu.h"
#include "../../uGUI/controles.h"
#include "../../uGUI/vkeyb.h"

#define WIDTH       272                   /* Buffer width    */
#define HEIGHT      228                   /* Buffer height   */

/* Press/Release keys in the background KeyState */
#define XKBD_SET(K) XKeyState[Keys[K][0]]&=~Keys[K][1]
#define XKBD_RES(K) XKeyState[Keys[K][0]]|=Keys[K][1]

/* Combination of EFF_* bits */
int UseEffects  = EFF_SCALE|EFF_SAVECPU|EFF_MITSHM|EFF_VARBPP|EFF_SYNC;

int InMenu;                /* 1: In MenuMSX(), ignore keys   */
int UseStatic   = 1;       /* 1: Use static palette          */
int UseZoom     = 2;       /* Zoom factor (1=no zoom)        */
int UseSound    = 44100;   /* Audio sampling frequency (Hz)  */
int FastForward;           /* Fast-forwarded UPeriod backup  */
int SndSwitch;             /* Mask of enabled sound channels */
int SndVolume;             /* Master volume for audio        */
int OldScrMode;            /* fMSX "ScrMode" variable storage*/

const char *Title     = "fMSX Unix 3.5";  /* Program version */

Image NormScreen;          /* Main screen image              */
Image WideScreen;          /* Wide screen image              */
static pixel *WBuf;        /* From Wide.h                    */
static pixel *XBuf/*[WIDTH*HEIGHT*16]*/;        /* From Common.h                  */
static pixel XPal[80];
static pixel BPal[256];
static pixel XPal0;

const char *Disks[2][MAXDISKS+1];         /* Disk names      */
volatile byte XKeyState[20]; /* Temporary KeyState array     */

int SetScrDepth(int Depth);
void HandleKeys(unsigned int Key);
void PutImage(void);

SDL_Surface *screen;
extern SDL_Surface *sur;
int emulating = 0;
char game_image_file[255];
extern word JoyState;
void vkbd_redraw(void);
extern int vkbd_mode;
int        CurrentFrame;
extern int current_fps;

/** InitMachine() ********************************************/
/** Allocate resources needed by machine-dependent code.    **/
/*************************************************************/
int InitMachine(void)
{
    int J;

    /* Initialize variables */
    UseZoom     = UseZoom<1? 1:UseZoom>5? 5:UseZoom;
    InMenu      = 0;
    FastForward = 0;
    OldScrMode  = 0;


    XBuf=malloc(WIDTH*HEIGHT*sizeof(pixel));
    memset(XBuf,0,WIDTH*HEIGHT*sizeof(pixel));

    /* Set all colors to black */
    for (J=0;
            J<80;
            J++) SetColor(J,0,0,0);

    /* Initialize temporary keyboard array */
    memset((void *)XKeyState,0xFF,sizeof(XKeyState));

    /* Initialize sound */
    InitSound(UseSound,150);
    SndSwitch=(1<<MAXCHANNELS)-1;
    SndVolume=255/MAXCHANNELS;
    SetChannels(SndVolume,SndSwitch);

    /* Done */
    return(1);
}

/** TrashMachine() *******************************************/
/** Deallocate all resources taken by InitMachine().        **/
/*************************************************************/
void TrashMachine(void)
{

    TrashAudio();

}







/** PutImage() ***********************************************/
/** Put an image on the screen.                             **/
/*************************************************************/
void PutImage(void)
{
    if (CurrentFrame <= 0) {
        CurrentFrame = emuconfig.Frameskip;

        if (!emuconfig.scale)
        {

            blitScreen(XBuf, WIDTH, HEIGHT, 48, 12);
            if (emuconfig.tvfilter)tvFilter(screen,24, 4,WIDTH, 113);
        }
        else{

            int x_d, x_s;
            int y_s;
            int y;
            short *ptr_dst = screen->pixels +6*screen->pitch;
            short *ptr_src = XBuf;

            for  (y = 0; y < HEIGHT; y++) {
                for (x_d = 0; x_d < screen->w; x_d++) {
                    x_s = 16 + (((x_d << 1) + x_d) >> 2);
                    ptr_dst[x_d] = ptr_src[x_s];
                }
                ptr_src += WIDTH;
                ptr_dst += screen->w;
            }

            if (emuconfig.tvfilter)tvFilter(screen,0,0,320,240);
        }

        if (vkbd_mode)vkbd_redraw();

        if (emuconfig.ShowFps) {
            char buffer[32];
            sprintf(buffer, "FPS %3d", current_fps);
            uPrint(24, 6, buffer);
        }

        SDL_Flip(screen);

    } else if (emuconfig.Frameskip) {
        CurrentFrame--;
    }

    updateCheats();
    synchronize();
    updateFPS();
}

/** Joystick() ***********************************************/
/** Query positions of two joystick connected to ports 0/1. **/
/** Returns 0.0.B2.A2.R2.L2.D2.U2.0.0.B1.A1.R1.L1.D1.U1.    **/
/*************************************************************/
unsigned int Joystick(void)
{
    return(JoyState);
}

void goMenu(void)
{
    SDL_BlitSurface(screen,NULL,sur,NULL);
    SDL_PauseAudio(1);
    run_mainMenu();
    SDL_PauseAudio(0);
    if (!emulating){
        LoadFile(&game_image_file);
        emulating = 1;
        InitCheater();
    }
    blackScreen();
}


/** Keyboard() ***********************************************/
/** Modify keyboard matrix.                                 **/
/*************************************************************/
void Keyboard(void)
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_KEYDOWN:
        {
            switch ((int)event.key.keysym.sym) {

            case ARRIBA:
                if (!vkbd_mode)JoyState|= EmuKeyCode(0);
                else vkbd_actual=vkbd_rect[vkbd_actual].up;
                break;
            case ABAJO:
                if (!vkbd_mode)JoyState|= EmuKeyCode(1);
                else vkbd_actual=vkbd_rect[vkbd_actual].down;
                break;
            case IZQUIERDA:
                if (!vkbd_mode)JoyState|= EmuKeyCode(2);
                else vkbd_actual=vkbd_rect[vkbd_actual].left;
                break;
            case DERECHA:
                if (!vkbd_mode)JoyState|= EmuKeyCode(3);
                else vkbd_actual=vkbd_rect[vkbd_actual].right;
                break;

            case BUTONA:
                if (!vkbd_mode)JoyState|= EmuKeyCode(4);
                else XKBD_SET(vkbd_rect[vkbd_actual].key);
                break;
            case BUTONB:
                if (!vkbd_mode)JoyState|= EmuKeyCode(5);
                break;

            case SDLK_SPACE:
                JoyState|=BTN_FIREL;
                break;

            case SDLK_F1:
                break;

            case SDLK_F2:
                break;

            case SDLK_F3:
                break;

            case SDLK_F4:
                break;

            case VKEYB:
                vkbd_mode^=1;
                break;
            case MENU:
                goMenu();
                break;

            case SDLK_F12:
                exit (0);
                break;
            }

            if (!vkbd_mode)XKBD_SET(event.key.keysym.sym);
        }
        break;

        case SDL_KEYUP:
        {
            switch ((int)event.key.keysym.sym) {

            case ARRIBA:
                if (!vkbd_mode)JoyState&=~EmuKeyCode(0);
                break;
            case ABAJO:
                if (!vkbd_mode)JoyState&=~EmuKeyCode(1);
                break;
            case IZQUIERDA:
                if (!vkbd_mode)JoyState&=~EmuKeyCode(2);
                break;
            case DERECHA:
                if (!vkbd_mode)JoyState&=~EmuKeyCode(3);
                break;

            case BUTONA:
                if (!vkbd_mode)JoyState&=~EmuKeyCode(4);
                else XKBD_RES(vkbd_rect[vkbd_actual].key);
                break;
            case BUTONB:
                if (!vkbd_mode)JoyState&=~EmuKeyCode(5);
                break;

            case SDLK_F1:
                break;

            case SDLK_F2:
                break;

            case SDLK_F3:
                break;

            case SDLK_F4:
                break;

            case SDLK_F5:
                break;

            case SDLK_F12:
                break;
            }

            if (!vkbd_mode) XKBD_RES(event.key.keysym.sym);
        }
        break;

        case SDL_QUIT:
            exit(0);
        }
    }
    memcpy((void *)KeyState,(const void *)XKeyState,sizeof(KeyState));
}


/** Mouse() **************************************************/
/** Query coordinates of a mouse connected to port N.       **/
/** Returns F2.F1.Y.Y.Y.Y.Y.Y.Y.Y.X.X.X.X.X.X.X.X.          **/
/*************************************************************/
unsigned int Mouse(byte N)
{
    unsigned int J;
    int X,Y;

    X = (int)(J&0xFFFF)-(WIDTH-256)/2;
    Y = (int)((J>>16)&0x3FFF)-(HEIGHT-212)/2;

    return(
              ((J>>14)&0x30000)            /* Fire buttons */
              | (X<0? 0:X>255? 255:X)        /* X */
              | ((Y<0? 0:Y>211? 211:Y)<<8)   /* Y */
          );
}

/** SetColor() ***********************************************/
/** Set color N to (R,G,B).                                 **/
/*************************************************************/
void SetColor(byte N,byte R,byte G,byte B)
{
    unsigned int J;

    J=SDL_MapRGB(screen->format,R,G,B);

    if (N) XPal[N]=J;
    else XPal0=J;

}


/** Common.h *************************************************/
/** Display drivers for all possible display depths.        **/
/*************************************************************/
#include "Display.h"
