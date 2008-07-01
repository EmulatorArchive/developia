/** EMULib Emulation Library *********************************/
/**                                                         **/
/**                          SndPsp.c                       **/
/**                                                         **/
/** This file contains standard sound generation routines   **/
/** for Playstation Portable console                        **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1996-2008                 **/
/**               Akop Karapetyan 2008                      **/
/**               Vincent van Dam 2002                      **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

#include <string.h>
#include <SDL/SDL.h>

#include "../../uGUI/menu.h"

#if defined(FMSX) && defined(ALTSOUND)
#include "fmopl.h"
#include "emu2149.h"
#include "emu2212.h"
#include "emu2413.h"
#endif

// #include "audio.h"
#include "Sound.h"

#define MAX_AUDIO_RB 8192*5

signed short ringbuf[MAX_AUDIO_RB];
volatile unsigned int ringbuffer_read = 0;
volatile unsigned int ringbuffer_write = 0;

// static void MixAudio(PspMonoSample *buffer, unsigned int length);
static void AudioCallback(void *buf, unsigned int *length, void *userdata);

#if defined(FMSX) && defined(ALTSOUND)
#define MSX_CLK 3579545

static OPLL   *opll;
static FM_OPL *fm_opl;
static PSG    *psg;
static SCC    *scc;

int UseStereo    = 0;     /* Stereo sound (1=enable)         */
int Use2413 = 0;     /* MSX-MUSIC emulation (1=enable)  */
int Use8950 = 0;     /* MSX-AUDIO emulation (1=enable)  */
int SndBufSize   = 512;   /* Buffer size for audio emulation */
float FactorPSG  = 3.00;  /* Volume percentage of PSG        */
float FactorSCC  = 3.00;  /* Volume percentage of SCC        */
float Factor2413 = 3.00;  /* Volume percentage of MSX-MUSIC  */
float Factor8950 = 2.25;  /* Volume percentage of MSX-AUDIO  */
static int Verbose = 1;   /* Verbose messaging flag          */
#else
static sample SndData[SND_BUFSIZE];
static int MixBuffer[SND_BUFSIZE];
static int WPtr = 0;
#endif

static int SndRate     = 0;  /* Audio sampling rate          */

/** DSPLoop() ************************************************/
/** Main loop of the sound server.                          **/
/*************************************************************/
static void DSPCallBack(void* unused, unsigned char *stream, int len)
{
    register int   J;
    register INT16 P,O,A,R1,R2,S;

    if (emuconfig.Sound) {
        for (J=0;J<len;J+=4)
        {
            P=PSG_calc(psg);
            S=SCC_calc(scc);
            O=Use2413? OPLL_calc(opll): 0;
            A=Use8950? Y8950UpdateOne(fm_opl): 0;
            if (UseStereo) {
                R1=P*FactorPSG+A*Factor8950+S*FactorSCC;
                R2=O*Factor2413+S*FactorSCC;
            } else {
                R1=R2=P*FactorPSG+O*Factor2413+A*Factor8950+S*FactorSCC;
            }
            stream[J+0]=R2&0x00FF;
            stream[J+1]=R2>>8;
            stream[J+2]=R1&0x00FF;
            stream[J+3]=R1>>8;
        }
    }
// else {
//     memset(stream, 0, len);
//   }
}

/** OpenSoundDevice() ****************************************/
/** Open sound device with a given level of sound quality.  **/
/** Returns 0 if failed or sound quality (Mode).            **/
/*************************************************************/
static int OpenSoundDevice(int Rate)
{
    /* Open the audio device */
    SDL_AudioSpec *desired, *obtained;

    /* Allocate a desired SDL_AudioSpec */
    desired=(SDL_AudioSpec*)malloc(sizeof(SDL_AudioSpec));

    /* Allocate space for the obtained SDL_AudioSpec */
    obtained=(SDL_AudioSpec*)malloc(sizeof(SDL_AudioSpec));

    /* set audio parameters */
    desired->freq=Rate;
    desired->format=AUDIO_S16LSB; /* 16-bit signed audio */
    desired->samples=SndBufSize;  /* size audio buffer */
    desired->channels=2;

    /* Our callback function */
    desired->callback=DSPCallBack;
    desired->userdata=NULL;

    /* Open the audio device */
    if (SDL_OpenAudio(desired, obtained)<0) return 0;

    Rate=obtained->freq;

    /* Adjust buffer size to the obtained buffer size */
    SndBufSize=obtained->samples;
    if (Verbose) printf("OK\n  Obtained buffer of %d bytes...", SndBufSize);

    /* Start playing */
    SDL_PauseAudio(0);

    /* Free memory */
    free(obtained);
    free(desired);

    if (Verbose) puts("OK");
    return(Rate);
}

/** InitSound() **********************************************/
/** Initialize DSP. Returns Rate on success, 0 otherwise.   **/
/** Mode is 0 to skip initialization (will be silent).      **/
/*************************************************************/
int InitSound(int Rate,int Verb)
{
    Verbose=Verb;
    if (Rate<=0) return(0);
    if (Rate<8192) Rate=44100;

    /* MSX-MUSIC emulation */
    OPLL_init(MSX_CLK,Rate);
    opll=OPLL_new();
    OPLL_reset(opll);
    OPLL_reset_patch(opll,0);

    /* MSX-AUDIO emulation */
    fm_opl=OPLCreate(OPL_TYPE_Y8950,MSX_CLK,Rate,256);
    OPLResetChip(fm_opl);

    /* PSG/SCC emulation */
    PSG_init(MSX_CLK,Rate);
    psg=PSG_new();
    PSG_reset(psg);
    PSG_setVolumeMode(psg,2);

    SCC_init(MSX_CLK,Rate);
    scc=SCC_new();
    SCC_reset(scc);

    /* Open sound device */
    if (Verbose) printf("OK\n  Opening sound device...");
    if (!(Rate=OpenSoundDevice(Rate))) return(0);

    return Rate;
}

/** ResetSound() *********************************************/
/** Reset sound chips.                                      **/
/*************************************************************/
void ResetSound()
{
    PSG_reset(psg);
    SCC_reset(scc);
    OPLL_reset(opll);
    OPLResetChip(fm_opl);
}

/** TrashAudio() *********************************************/
/** Free resources allocated by InitAudio().                **/
/*************************************************************/
void TrashAudio(void)
{
///Uguru TODO: poner SDL
//   /* Sound off */
//   SndRate = 0;
//
//   /* Wait for audio thread to finish */
//   if(Thr) pthread_join(Thr,0);
//
//   /* If audio was initialized... */
//   if(SoundFD>=0)
//   {
// #ifndef SUN_AUDIO
//     ioctl(SoundFD,SNDCTL_DSP_RESET);
// #endif
//     close(SoundFD);
//   }
//
//   /* If buffers were allocated... */
//   if(SndData) free(SndData);
//
//   /* Sound trashed */
//   SoundFD = -1;
//   SndData = 0;
//   SndSize = 0;
//   RPtr    = 0;
//   WPtr    = 0;
//   Thr     = 0;
}


/** TrashSound() *********************************************/
/** Shut DSP down.                                          **/
/*************************************************************/
void TrashSound(void)
{
    SDL_CloseAudio();

    /* clean up msxmusic */
    OPLL_close();
    OPLL_delete(opll);

    /* clean up msxaudio */
    OPLDestroy(fm_opl);

    /* clean up psg/scc */
    PSG_delete(psg);
    SCC_delete(scc);
}

/* wrapper functions to actual sound emulation */
void WriteOPLL (int R,int V) {
    OPLL_writeReg(opll,R,V);
}
void WriteAUDIO(int R,int V) {
    OPLWrite(fm_opl,R,V);
}
void Write2212 (int R,int V) {
    SCC_write(scc,R,V);
}
void WritePSG  (int R,int V) {
    PSG_writeReg(psg,R,V);
}
int  ReadAUDIO (int R)       {
    return OPLRead(fm_opl,R);
}
int  ReadPSG   (int R)       {
    return PSG_readReg(psg,R);
}

