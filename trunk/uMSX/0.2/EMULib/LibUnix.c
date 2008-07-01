/** EMULib Emulation Library *********************************/
/**                                                         **/
/**                        LibUnix.c                        **/
/**                                                         **/
/** This file contains Unix-dependent implementation        **/
/** parts of the emulation library.                         **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1996-2008                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
#include "EMULib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

extern int MasterSwitch; /* Switches to turn channels on/off */
extern int MasterVolume; /* Master volume                    */

static volatile int TimerReady = 0;   /* 1: Sync timer ready */
static volatile unsigned int JoyState = 0; /* Joystick state */
static volatile unsigned int LastKey  = 0; /* Last key prsd  */
static volatile unsigned int KeyModes = 0; /* SHIFT/CTRL/ALT */

static int Effects    = EFF_SCALE|EFF_SAVECPU; /* EFF_* bits */
static int TimerON    = 0; /* 1: sync timer is running       */
static Display *Dsp   = 0; /* X11 display                    */
static Screen *Scr    = 0; /* X11 screen                     */
static Window Wnd     = 0; /* X11 window                     */
static Colormap CMap;      /* X11 color map                  */
static Image OutImg;       /* Scaled output image buffer     */
static const char *AppTitle; /* Current window title         */
static int XSize,YSize;    /* Current window dimensions      */

/** TimerHandler() *******************************************/
/** The main timer handler used by SetSyncTimer().          **/
/*************************************************************/
static void TimerHandler(int Arg)
{
    /* Mark sync timer as "ready" */
    TimerReady=1;
    /* Repeat signal next time */
    signal(Arg,TimerHandler);
}

/** InitUnix() ***********************************************/
/** Initialize Unix/X11 resources and set initial window    **/
/** title and dimensions.                                   **/
/*************************************************************/
int InitUnix(const char *Title,int Width,int Height)
{
    /* Initialize variables */
    AppTitle    = Title;
    XSize       = Width;
    YSize       = Height;
    TimerON     = 0;
    TimerReady  = 0;
    JoyState    = 0;
    LastKey     = 0;
    KeyModes    = 0;
    Wnd         = 0;
    Dsp         = 0;
    Scr         = 0;
    CMap        = 0;

    /* No output image yet */
    OutImg.XImg            = 0;
    OutImg.SHMInfo.shmaddr = 0;

    /* Open X11 display */
    if (!(Dsp=XOpenDisplay(0))) return(0);

    /* Get default screen and color map */
    Scr  = DefaultScreenOfDisplay(Dsp);
    CMap = DefaultColormapOfScreen(Scr);

    /* Done */
    return(1);
}

/** TrashUnix() **********************************************/
/** Free resources allocated in InitUnix()                  **/
/*************************************************************/
void TrashUnix(void)
{
    /* Remove sync timer */
    SetSyncTimer(0);
    /* Shut down audio */
    TrashAudio();
    /* Free output image buffer */
    FreeImage(&OutImg);

    /* If X11 display open... */
    if (Dsp)
    {
        /* Close the window */
        if (Wnd) {
            XDestroyWindow(Dsp,Wnd);
            Wnd=0;
        }
        /* Done with display */
        XCloseDisplay(Dsp);
        /* Display now closed */
        Dsp=0;
    }
}

/** ShowVideo() **********************************************/
/** Show "active" image at the actual screen or window.     **/
/*************************************************************/
int ShowVideo(void)
{
    /* Must have active video image, X11 display */
    if (!Dsp||!VideoImg||!VideoImg->Data) return(0);

    /* If no window yet... */
    if (!Wnd)
    {
        /* Create new window */
        Wnd=X11Window(AppTitle? AppTitle:"EMULib",XSize,YSize);
        if (!Wnd) return(0);
    }

    /* Allocate image buffer if none */
    if (!OutImg.Data&&!NewImage(&OutImg,XSize,YSize)) return(0);

    /* Wait for sync timer if requested */
    if (Effects&EFF_SYNC) WaitSyncTimer();

    /* Wait for all X11 requests to complete, to avoid flicker */
    XSync(Dsp,False);

    /* If not scaling or post-processing image, avoid extra work */
    if (!(Effects&(EFF_SOFTEN|EFF_SCALE|EFF_TVLINES)))
    {
#ifdef MITSHM
        if (VideoImg->Attrs&EFF_MITSHM)
            XShmPutImage(Dsp,Wnd,DefaultGCOfScreen(Scr),VideoImg->XImg,VideoX,VideoY,(XSize-VideoW)>>1,(YSize-VideoH)>>1,VideoW,VideoH,False);
        else
#endif
            XPutImage(Dsp,Wnd,DefaultGCOfScreen(Scr),VideoImg->XImg,VideoX,VideoY,(XSize-VideoW)>>1,(YSize-VideoH)>>1,VideoW,VideoH);
        return;
    }

    /* Scale video buffer into OutImg */
    if (Effects&EFF_SOFTEN) SoftenImage(&OutImg,VideoImg,VideoX,VideoY,VideoW,VideoH);
    else                   ScaleImage(&OutImg,VideoImg,VideoX,VideoY,VideoW,VideoH);

    /* Apply TV scanlines if requested */
    if (Effects&EFF_TVLINES) TelevizeImage(&OutImg,0,0,OutImg.W,OutImg.H);

    /* Copy image to the window, either using SHM or not */
#ifdef MITSHM
    if (VideoImg->Attrs&EFF_MITSHM)
        XShmPutImage(Dsp,Wnd,DefaultGCOfScreen(Scr),OutImg.XImg,0,0,0,0,XSize,YSize,False);
    else
#endif
        XPutImage(Dsp,Wnd,DefaultGCOfScreen(Scr),OutImg.XImg,0,0,0,0,XSize,YSize);
}

/** GetJoystick() ********************************************/
/** Get the state of joypad buttons (1="pressed"). Refer to **/
/** the BTN_* #defines for the button mappings.             **/
/*************************************************************/
unsigned int GetJoystick(void)
{
    X11ProcessEvents();
    return(JoyState|KeyModes);
}

/** GetMouse() ***********************************************/
/** Get mouse position and button states in the following   **/
/** format: RMB.LMB.Y[29-16].X[15-0].                       **/
/*************************************************************/
unsigned int GetMouse(void)
{
    int X,Y,J,Mask;
    Window W;

    /* Need to have a display and a window */
    if (!Dsp||!Wnd) return(0);
    /* Query mouse pointer */
    if (!XQueryPointer(Dsp,Wnd,&W,&W,&J,&J,&X,&Y,&Mask)) return(0);

    /* If scaling video... */
    if (Effects&(EFF_SOFTEN|EFF_SCALE|EFF_TVLINES))
    {
        /* Scale mouse position */
        X = VideoW*(X<0? 0:X>=XSize? XSize-1:X)/XSize;
        Y = VideoH*(Y<0? 0:Y>=YSize? YSize-1:Y)/YSize;
    }
    else
    {
        /* Translate mouse position */
        X-= ((XSize-VideoW)>>1);
        Y-= ((YSize-VideoH)>>1);
        X = X<0? 0:X>=XSize? XSize-1:X;
        Y = Y<0? 0:Y>=YSize? YSize-1:Y;
    }

    /* Return mouse position and buttons */
    return(
              (X&0xFFFF)
              | ((Y&0x3FFF)<<16)
              | (Mask&Button1Mask? 0x40000000:0)
              | (Mask&Button3Mask? 0x80000000:0)
          );
}

/** GetKey() *************************************************/
/** Get currently pressed key or 0 if none pressed. Returns **/
/** CON_* definitions for arrows and special keys.          **/
/*************************************************************/
unsigned int GetKey(void)
{
    unsigned int J;

    X11ProcessEvents();
    J=LastKey;
    LastKey=0;
    return(J);
}

/** WaitKey() ************************************************/
/** Wait for a key to be pressed. Returns CON_* definitions **/
/** for arrows and special keys.                            **/
/*************************************************************/
unsigned int WaitKey(void)
{
    unsigned int J;

    /* Swallow current keypress */
    GetKey();
    /* Wait in 100ms increments for a new keypress */
    while (!(J=GetKey())&&VideoImg) usleep(100000);
    /* Return key code */
    return(J);
}

/** WaitSyncTimer() ******************************************/
/** Wait for the timer to become ready.                     **/
/*************************************************************/
void WaitSyncTimer(void)
{
    /* Wait in 1ms increments until timer becomes ready */
    for (TimerReady=0;!TimerReady&&TimerON&&VideoImg;) usleep(1000);
}

/** SyncTimerReady() *****************************************/
/** Return 1 if sync timer ready, 0 otherwise.              **/
/*************************************************************/
int SyncTimerReady(void)
{
    /* Return whether timer is ready or not */
    return(TimerReady||!TimerON||!VideoImg);
}

/** SetSyncTimer() *******************************************/
/** Set synchronization timer to a given frequency in Hz.   **/
/*************************************************************/
int SetSyncTimer(int Hz)
{
    struct itimerval TimerValue;

    /* Compute and set timer period */
    TimerValue.it_interval.tv_sec  =
        TimerValue.it_value.tv_sec     = 0;
    TimerValue.it_interval.tv_usec =
        TimerValue.it_value.tv_usec    = Hz? 1000000L/Hz:0;

    /* Set timer */
    if (setitimer(ITIMER_REAL,&TimerValue,NULL)) return(0);

    /* Set timer signal */
    signal(SIGALRM,Hz? TimerHandler:SIG_DFL);

    /* Done */
    TimerON=Hz;
    return(1);
}

/** ChangeDir() **********************************************/
/** This function is a wrapper for chdir().                 **/
/*************************************************************/
int ChangeDir(const char *Name) {
    return(chdir(Name));
}

/** NewImage() ***********************************************/
/** Create a new image of the given size. Returns pointer   **/
/** to the image data on success, 0 on failure.             **/
/*************************************************************/
pixel *NewImage(Image *Img,int Width,int Height)
{
    XVisualInfo VInfo;
    int Depth,J,I;

    /* Set data fields to ther defaults */
    Img->Data  = 0;
    Img->W     = 0;
    Img->H     = 0;
    Img->L     = 0;
    Img->D     = 0;
    Img->Attrs = 0;

    /* Need to initalize library first */
    if (!Dsp) return(0);

    /* Image depth we are going to use */
    Depth = Effects&EFF_VARBPP? DefaultDepthOfScreen(Scr):(sizeof(pixel)<<3);

    /* Get appropriate Visual for this depth */
    I=XScreenNumberOfScreen(Scr);
    for (J=7;J>=0;J--)
        if (XMatchVisualInfo(Dsp,I,Depth,J,&VInfo)) break;
    if (J<0) return(0);

#ifdef MITSHM
    if (Effects&EFF_MITSHM)
    {
        /* Create shared XImage */
        Img->XImg = XShmCreateImage(Dsp,VInfo.visual,Depth,ZPixmap,0,&Img->SHMInfo,Width,Height);
        if (!Img->XImg) return(0);

        /* Get ID for shared segment */
        Img->SHMInfo.shmid = shmget(IPC_PRIVATE,Img->XImg->bytes_per_line*Img->XImg->height,IPC_CREAT|0777);
        if (Img->SHMInfo.shmid==-1) return(0);

        /* Attach to shared segment by ID */
        Img->XImg->data = Img->SHMInfo.shmaddr = shmat(Img->SHMInfo.shmid,0,0);
        if (!Img->XImg->data) {
            shmctl(Img->SHMInfo.shmid,IPC_RMID,0);
            return(0);
        }

        /* Can write into shared segment */
        Img->SHMInfo.readOnly = False;

        /* Attach segment to X display and make sure it is done */
        J=XShmAttach(Dsp,&Img->SHMInfo);
        XSync(Dsp,False);

        /* We do not need an ID any longer */
        shmctl(Img->SHMInfo.shmid,IPC_RMID,0);

        /* If attachment failed, break out */
        if (!J) return(0);
    }
    else
#endif
    {
        /* Create normal XImage */
        Img->XImg = XCreateImage(Dsp,VInfo.visual,Depth,ZPixmap,0,0,Width,Height,Depth,0);
        if (!Img->XImg) return(0);

        /* Allocate data */
        Img->XImg->data = (char *)malloc(Img->XImg->bytes_per_line*Img->XImg->height);
        if (!Img->XImg->data) return(0);
    }

    /* Done */
    Depth      = Depth==24? 32:Depth;
    Img->Data  = (pixel *)Img->XImg->data;
    Img->W     = Img->XImg->width;
    Img->H     = Img->XImg->height;
    Img->L     = Img->XImg->bytes_per_line/(Depth>>3);
    Img->D     = Depth;
    Img->Attrs = Effects&(EFF_MITSHM|EFF_VARBPP);
    return(Img->Data);
}

/** FreeImage() **********************************************/
/** Free previously allocated image.                        **/
/*************************************************************/
void FreeImage(Image *Img)
{
    /* Need to initalize library first */
    if (!Dsp) return;

#ifdef MITSHM
    /* Detach shared memory segment */
    if ((Img->Attrs&EFF_MITSHM)&&Img->SHMInfo.shmaddr)
    {
        XShmDetach(Dsp,&Img->SHMInfo);
        shmdt(Img->SHMInfo.shmaddr);
    }
    Img->SHMInfo.shmaddr = 0;
#endif

    /* Get rid of the image */
    if (Img->XImg) {
        XDestroyImage(Img->XImg);
        Img->XImg=0;
    }

    /* Image freed */
    Img->Data = 0;
    Img->W    = 0;
    Img->H    = 0;
    Img->L    = 0;
}

/** CropImage() **********************************************/
/** Create a subimage Dst of the image Src. Returns Dst.    **/
/*************************************************************/
Image *CropImage(Image *Dst,const Image *Src,int X,int Y,int W,int H)
{
    Dst->Data    = (pixel *)((char *)Src->Data+(Src->L*Y+X)*(Src->D<<3));
    Dst->Cropped = 1;
    Dst->W       = W;
    Dst->H       = H;
    Dst->L       = Src->L;
    Dst->D       = Src->D;
    Dst->XImg    = 0;
    Dst->Attrs   = 0;
    return(Dst);
}

/** X11SetEffects() ******************************************/
/** Set visual effects applied to video in ShowVideo().     **/
/*************************************************************/
void X11SetEffects(int NewEffects)
{
    /* Set new effects */
    Effects=NewEffects;
}

/** X11ProcessEvents() ***************************************/
/** Process X11 event messages.                             **/
/*************************************************************/
void X11ProcessEvents(void)
{
    XEvent E;
    int J;

    /* Need to have display and a window */
    if (!Dsp||!Wnd) return;

    /* Check for keypresses/keyreleases */
    while (XCheckWindowEvent(Dsp,Wnd,KeyPressMask|KeyReleaseMask,&E))
    {
        /* Get key code */
        J=XLookupKeysym((XKeyEvent *)&E,0);

        /* If key pressed... */
        if (E.type==KeyPress)
        {
            /* Process ASCII keys */
            if ((J>=' ')&&(J<0x7F)) LastKey=toupper(J);

            /* Special keys pressed... */
            switch (J)
            {
            case XK_Left:
                JoyState|=BTN_LEFT;
                LastKey=CON_LEFT;
                break;
            case XK_Right:
                JoyState|=BTN_RIGHT;
                LastKey=CON_RIGHT;
                break;
            case XK_Up:
                JoyState|=BTN_UP;
                LastKey=CON_UP;
                break;
            case XK_Down:
                JoyState|=BTN_DOWN;
                LastKey=CON_DOWN;
                break;
            case XK_Shift_L:
            case XK_Shift_R:
                KeyModes|=CON_SHIFT;
                break;
            case XK_Alt_L:
            case XK_Alt_R:
                KeyModes|=CON_ALT;
                break;
            case XK_Control_L:
            case XK_Control_R:
                KeyModes|=CON_CONTROL;
                break;
            case XK_Caps_Lock:
                KeyModes|=CON_CAPS;
                break;
            case XK_Escape:
                JoyState|=BTN_EXIT;
                LastKey=CON_EXIT;
                break;
            case XK_Tab:
                JoyState|=BTN_SELECT;
                break;
            case XK_Return:
                JoyState|=BTN_START;
                LastKey=CON_OK;
                break;
            case XK_BackSpace:
                LastKey=8;
                break;

            case 'q':
            case 'e':
            case 't':
            case 'u':
            case 'o':
                JoyState|=BTN_FIREL;
                break;
            case 'w':
            case 'r':
            case 'y':
            case 'i':
            case 'p':
                JoyState|=BTN_FIRER;
                break;
            case 'a':
            case 's':
            case 'd':
            case 'f':
            case 'g':
            case 'h':
            case 'j':
            case 'k':
            case 'l':
            case ' ':
                JoyState|=BTN_FIREA;
                break;
            case 'z':
            case 'x':
            case 'c':
            case 'v':
            case 'b':
            case 'n':
            case 'm':
                JoyState|=BTN_FIREB;
                break;

            case XK_Page_Up:
                if (KeyModes&CON_ALT)
                {
                    /* Volume up */
                    SetChannels(MasterVolume<247? MasterVolume+8:255,MasterSwitch);
                    /* Key swallowed */
                    J=0;
                }
                break;

            case XK_Page_Down:
                if (KeyModes&CON_ALT)
                {
                    /* Volume down */
                    SetChannels(MasterVolume>8? MasterVolume-8:0,MasterSwitch);
                    /* Key swallowed */
                    J=0;
                }
                break;
            }

            /* Call user handler */
            if (J&&KeyHandler) (*KeyHandler)(J|KeyModes);
        }

        /* If key released... */
        if (E.type==KeyRelease)
        {
            /* Special keys released... */
            switch (J)
            {
            case XK_Left:
                JoyState&=~BTN_LEFT;
                break;
            case XK_Right:
                JoyState&=~BTN_RIGHT;
                break;
            case XK_Up:
                JoyState&=~BTN_UP;
                break;
            case XK_Down:
                JoyState&=~BTN_DOWN;
                break;
            case XK_Shift_L:
            case XK_Shift_R:
                KeyModes&=~CON_SHIFT;
                break;
            case XK_Alt_L:
            case XK_Alt_R:
                KeyModes&=~CON_ALT;
                break;
            case XK_Control_L:
            case XK_Control_R:
                KeyModes&=~CON_CONTROL;
                break;
            case XK_Caps_Lock:
                KeyModes&=~CON_CAPS;
                break;
            case XK_Escape:
                JoyState&=~BTN_EXIT;
                break;
            case XK_Tab:
                JoyState&=~BTN_SELECT;
                break;
            case XK_Return:
                JoyState&=~BTN_START;
                break;

            case 'q':
            case 'e':
            case 't':
            case 'u':
            case 'o':
                JoyState&=~BTN_FIREL;
                break;
            case 'w':
            case 'r':
            case 'y':
            case 'i':
            case 'p':
                JoyState&=~BTN_FIRER;
                break;
            case 'a':
            case 's':
            case 'd':
            case 'f':
            case 'g':
            case 'h':
            case 'j':
            case 'k':
            case 'l':
            case ' ':
                JoyState&=~BTN_FIREA;
                break;
            case 'z':
            case 'x':
            case 'c':
            case 'v':
            case 'b':
            case 'n':
            case 'm':
                JoyState&=~BTN_FIREB;
                break;
            }

            /* Call user handler */
            if (J&&KeyHandler) (*KeyHandler)(J|CON_RELEASE|KeyModes);
        }
    }

    /* Check for focus change events */
    for (E.type=0;XCheckWindowEvent(Dsp,Wnd,FocusChangeMask,&E););
    /* If saving CPU and focus is out... */
    if ((Effects&EFF_SAVECPU)&&(E.type==FocusOut))
    {
        J=MasterSwitch;
        SetChannels(MasterVolume,0);
        /* Wait for focus-in event */
        do
            while (!XCheckWindowEvent(Dsp,Wnd,FocusChangeMask,&E)&&VideoImg) sleep(1);
        while ((E.type!=FocusIn)&&VideoImg);
        SetChannels(MasterVolume,J);
    }

    /* If window has been resized, remove current output buffer */
    for (E.type=0;XCheckWindowEvent(Dsp,Wnd,StructureNotifyMask,&E););
    if ((E.type==ConfigureNotify)&&!E.xconfigure.send_event)
        if ((XSize!=E.xconfigure.width)||(YSize!=E.xconfigure.height))
        {
            FreeImage(&OutImg);
            XSize=E.xconfigure.width;
            YSize=E.xconfigure.height;
        }
}

/** X11GetColor **********************************************/
/** Get pixel for the current screen depth based on the RGB **/
/** values.                                                 **/
/*************************************************************/
unsigned int X11GetColor(unsigned char R,unsigned char G,unsigned char B)
{
    int J;

    /* If using constant BPP, just return a pixel */
    if (!Dsp||!(Effects&EFF_VARBPP)) return(PIXEL(R,G,B));

    /* If variable BPP, compute pixel based on the screen depth */
    J=DefaultDepthOfScreen(Scr);
    return(
              J<=8?  (((7*(R)/255)<<5)|((7*(G)/255)<<2)|(3*(B)/255))
              : J<=16? (((31*(R)/255)<<11)|((63*(G)/255)<<5)|(31*(B)/255))
              : J<=32? (((R)<<16)|((G)<<8)|B)
              : 0
          );
}

/** X11Window() **********************************************/
/** Open a window of a given size with a given title.       **/
/*************************************************************/
Window X11Window(const char *Title,int Width,int Height)
{
    XSetWindowAttributes Attrs;
    XVisualInfo VInfo;
    XClassHint ClassHint;
    XSizeHints Hints;
    XWMHints WMHints;
    Window Wnd;
    char *P;
    int Q;

    /* Need to initalize library first */
    if (!Dsp) return(0);

    /* Set necessary attributes */
    Attrs.event_mask =
        FocusChangeMask|KeyPressMask|KeyReleaseMask|StructureNotifyMask;
    Attrs.background_pixel=BlackPixelOfScreen(Scr);
    Attrs.backing_store=Always;

    /* Create a window */
    Wnd=XCreateWindow
        (
            Dsp,RootWindowOfScreen(Scr),0,0,Width,Height,1,
            CopyFromParent,CopyFromParent,CopyFromParent,
            CWBackPixel|CWEventMask|CWBackingStore,&Attrs
        );
    if (!Wnd) return(0);

    /* Set application class hint */
    if (ARGC&&ARGV)
    {
        P=strrchr(ARGV[0],'/');
        ClassHint.res_name  = P? P+1:ARGV[0];
        ClassHint.res_class = P? P+1:ARGV[0];
        XSetClassHint(Dsp,Wnd,&ClassHint);
        XSetCommand(Dsp,Wnd,ARGV,ARGC);
    }

    /* Set hints */
    Q=sizeof(long);
    Hints.flags       = PSize|PMinSize|PMaxSize|PResizeInc;
    Hints.min_width   = ((Width/4)/Q)*Q;
    Hints.max_width   = ((Width*4)/Q)*Q;
    Hints.base_width  = (Width/Q)*Q;
    Hints.width_inc   = Q;
    Hints.min_height  = ((Height/4)/Q)*Q;
    Hints.max_height  = ((Height*4)/Q)*Q;
    Hints.base_height = (Height/Q)*Q;
    Hints.height_inc  = Q;
    WMHints.input     = True;
    WMHints.flags     = InputHint;

    if (ARGC&&ARGV)
    {
        WMHints.window_group=Wnd;
        WMHints.flags|=WindowGroupHint;
    }

    /* Set hints, title, size */
    XSetWMHints(Dsp,Wnd,&WMHints);
    XSetWMNormalHints(Dsp,Wnd,&Hints);
    XStoreName(Dsp,Wnd,Title);

    /* Do additional housekeeping and return */
    XMapRaised(Dsp,Wnd);
    XClearWindow(Dsp,Wnd);

    /* Done */
    return(Wnd);
}
