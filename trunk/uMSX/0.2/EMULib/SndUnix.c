/** EMULib Emulation Library *********************************/
/**                                                         **/
/**                        SndUnix.c                        **/
/**                                                         **/
/** This file contains Unix-dependent sound implementation  **/
/** for the emulation library.                              **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1996-2008                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
#include "EMULib.h"
#include "Sound.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
// #include <sys/ioctl.h>
// #include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
// #include <pthread.h>

#ifdef SUN_AUDIO

#include <sys/audioio.h>
#include <sys/conf.h>
#include <stropts.h>

#define AUDIO_CONV(A) (ULAW[0xFF&(128+(A))])

static const unsigned char ULAW[256] =
{
    31,   31,   31,   32,   32,   32,   32,   33,
    33,   33,   33,   34,   34,   34,   34,   35,
    35,   35,   35,   36,   36,   36,   36,   37,
    37,   37,   37,   38,   38,   38,   38,   39,
    39,   39,   39,   40,   40,   40,   40,   41,
    41,   41,   41,   42,   42,   42,   42,   43,
    43,   43,   43,   44,   44,   44,   44,   45,
    45,   45,   45,   46,   46,   46,   46,   47,
    47,   47,   47,   48,   48,   49,   49,   50,
    50,   51,   51,   52,   52,   53,   53,   54,
    54,   55,   55,   56,   56,   57,   57,   58,
    58,   59,   59,   60,   60,   61,   61,   62,
    62,   63,   63,   64,   65,   66,   67,   68,
    69,   70,   71,   72,   73,   74,   75,   76,
    77,   78,   79,   81,   83,   85,   87,   89,
    91,   93,   95,   99,  103,  107,  111,  119,
    255,  247,  239,  235,  231,  227,  223,  221,
    219,  217,  215,  213,  211,  209,  207,  206,
    205,  204,  203,  202,  201,  200,  199,  198,
    219,  217,  215,  213,  211,  209,  207,  206,
    205,  204,  203,  202,  201,  200,  199,  198,
    197,  196,  195,  194,  193,  192,  191,  191,
    190,  190,  189,  189,  188,  188,  187,  187,
    186,  186,  185,  185,  184,  184,  183,  183,
    182,  182,  181,  181,  180,  180,  179,  179,
    178,  178,  177,  177,  176,  176,  175,  175,
    175,  175,  174,  174,  174,  174,  173,  173,
    173,  173,  172,  172,  172,  172,  171,  171,
    171,  171,  170,  170,  170,  170,  169,  169,
    169,  169,  168,  168,  168,  168,  167,  167,
    167,  167,  166,  166,  166,  166,  165,  165,
    165,  165,  164,  164,  164,  164,  163,  163
};

#else /* !SUN_AUDIO */

#ifdef __FreeBSD__
#include <sys/soundcard.h>
#endif

#ifdef __NetBSD__
#include <soundcard.h>
#endif

#ifdef __linux__
#include <sys/soundcard.h>
#endif

#define AUDIO_CONV(A) (128+(A))

#endif /* !SUN_AUDIO */

static int SoundFD     = -1; /* Audio device descriptor      */
static int SndRate     = 0;  /* Audio sampling rate          */
static int SndSize     = 0;  /* SndData[] size               */
static sample *SndData = 0;  /* Audio buffers                */
static int RPtr        = 0;  /* Read pointer into Bufs       */
static int WPtr        = 0;  /* Write pointer into Bufs      */
// static pthread_t Thr   = 0;  /* Audio thread                 */

/** ThrHandler() *********************************************/
/** This is the thread function responsible for sending     **/
/** buffers to the audio device.                            **/
/*************************************************************/
static void *ThrHandler(void *Arg)
{
    int J;

    /* Spin until audio has been trashed */
    for (RPtr=WPtr=0;SndRate&&SndData&&(SoundFD>=0);)
    {
// #ifdef SUN_AUDIO
//     /* Flush output first, don't care about return status. After this
//     ** write next buffer of audio data. This method produces a horrible
//     ** click on each buffer :( Any ideas, how to fix this?
//     */
//     ioctl(SoundFD,AUDIO_DRAIN);
//     write(SoundFD,SndData+RPtr,SND_BUFSIZE);
// #else
        /* We'll block here until next DMA buffer becomes free. It happens
        ** once per SND_BUFSIZE/SndRate seconds.
        */
        write(SoundFD,SndData+RPtr,SND_BUFSIZE);
// #endif

        /* Advance buffer pointer, clearing the buffer */
        for (J=0;J<SND_BUFSIZE;++J) SndData[RPtr++]=AUDIO_CONV(0);
        if (RPtr>=SndSize) RPtr=0;
    }

    return(0);
}

/** InitAudio() **********************************************/
/** Initialize sound. Returns rate (Hz) on success, else 0. **/
/** Rate=0 to skip initialization (will be silent).         **/
/*************************************************************/
unsigned int InitAudio(unsigned int Rate,unsigned int Latency)
{
///Uguru TODO: poner SDL
//   int I,J,K;
//
//   /* Shut down audio, just to be sure */
//   TrashAudio();
//   SndRate = 0;
//   SoundFD = -1;
//   SndSize = 0;
//   SndData = 0;
//   RPtr    = 0;
//   WPtr    = 0;
// //   Thr     = 0;
//
//   /* Have to have at least 8kHz sampling rate and 1ms buffer */
//   if((Rate<8000)||!Latency) return(0);
//
//   /* Compute number of sound buffers */
//   SndSize=(Rate*Latency/1000+SND_BUFSIZE-1)/SND_BUFSIZE;
//
// #ifdef SUN_AUDIO
//
//   /* Open Sun's audio device */
//   if((SoundFD=open("/dev/audio",O_WRONLY|O_NONBLOCK))<0) return(0);
//
//   /*
//   ** Sun's specific initialization should be here...
//   ** We assume, that it's set to 8000Hz u-law mono right now.
//   */
//
// #else /* !SUN_AUDIO */
//
//   /* Open /dev/dsp audio device */
//   if((SoundFD=open("/dev/dsp",O_WRONLY))<0) return(0);
//   /* Set 8-bit sound */
//   J=AFMT_U8;
//   I=ioctl(SoundFD,SNDCTL_DSP_SETFMT,&J)<0;
//   /* Set mono sound */
//   J=0;
//   I|=ioctl(SoundFD,SNDCTL_DSP_STEREO,&J)<0;
//   /* Set sampling rate */
//   I|=ioctl(SoundFD,SNDCTL_DSP_SPEED,&Rate)<0;
//
//   /* Set buffer length and number of buffers */
//   J=K=SND_BITS|(SndSize<<16);
//   I|=ioctl(SoundFD,SNDCTL_DSP_SETFRAGMENT,&J)<0;
//
//   /* Buffer length as n, not 2^n! */
//   if((J&0xFFFF)<=16) J=(J&0xFFFF0000)|(1<<(J&0xFFFF));
//   K=SND_BUFSIZE|(SndSize<<16);
//
//   /* Check audio parameters */
//   I|=(J!=K)&&(((J>>16)<SndSize)||((J&0xFFFF)!=SND_BUFSIZE));
//
//   /* If something went wrong, drop out */
//   if(I) { TrashSound();return(0); }
//
// #endif /* !SUN_AUDIO */
//
//   /* SndSize now means the total buffer size */
//   SndSize*=SND_BUFSIZE;
//
//   /* Allocate audio buffers */
//   SndData=(sample *)malloc(SndSize*sizeof(sample));
//   if(!SndData) { TrashSound();return(0); }
//
//   /* Clear audio buffers */
//   for(J=0;J<SndSize;++J) SndData[J]=AUDIO_CONV(0);
//
//   /* Create audio thread */
//   if(pthread_create(&Thr,0,ThrHandler,0)) { TrashSound();return(0); }
//
//   /* Done, return effective audio rate */
    return(SndRate=Rate);
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

/** GetFreeAudio() *******************************************/
/** Get the amount of free samples in the audio buffer.     **/
/*************************************************************/
unsigned int GetFreeAudio(void)
{
    return(!SndRate? 0:RPtr>=WPtr? RPtr-WPtr:RPtr-WPtr+SndSize);
}

/** WriteAudio() *********************************************/
/** Write up to a given number of samples to audio buffer.  **/
/** Returns the number of samples written.                  **/
/*************************************************************/
unsigned int WriteAudio(sample *Data,unsigned int Length)
{
    unsigned int J;

    /* Require audio to be initialized */
    if (!SndRate) return(0);

    /* Copy audio samples */
    for (J=0;(J<Length)&&(RPtr!=WPtr);++J)
    {
        SndData[WPtr++]=AUDIO_CONV(Data[J]);
        if (WPtr>=SndSize) WPtr=0;
    }

    /* Return number of samples copied */
    return(J);
}

