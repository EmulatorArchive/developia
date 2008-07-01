/** EMULib Emulation Library *********************************/
/**                                                         **/
/**                          Image.c                        **/
/**                                                         **/
/** This file contains non-essential functions that operate **/
/** on images.                                              **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1996-2008                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
#include "EMULib.h"

/** Merge2() *************************************************/
/** Merges pixels A and B with bias X.                      **/
/*************************************************************/
static __inline pixel Merge2(pixel A,pixel B,unsigned int X)
{
    unsigned int DA,DB;

    // Do not interpolate if values are the same
    if (A==B) return A;

    // Reduce 16 bit fraction to 5 bits
    DB = (X>>11)&0x001F;
    DA = 0x20 - DB;

#if defined(BPP24) || defined(BPP32)
    X  = (DA*(A&GMASK)+DB*(B&GMASK))>>5;
    DA*= A&(RMASK|BMASK);
    DB*= B&(RMASK|BMASK);
    DA = (DA+DB)>>5;
    return((DA&(RMASK|BMASK))|(X&GMASK));
#else
    DA*= (A&(RMASK|BMASK)) | ((A&GMASK)<<16);
    DB*= (B&(RMASK|BMASK)) | ((B&GMASK)<<16);
    DA = (DA+DB)>>5;
    return((DA&(RMASK|BMASK))|((DA>>16)&GMASK));
#endif
}

/** Merge4() *************************************************/
/** Merges pixels A,B,C,D with biases X,Y.                  **/
/*************************************************************/
static __inline pixel Merge4(pixel A,pixel B,pixel C,pixel D,unsigned int X,unsigned int Y)
{
    unsigned int DA,DB,DC,DD;

    X  = (X>>11)&0x001F;
    Y  = (Y>>11)&0x001F;
    DD = (X*Y)>>5;
    DA = 0x20 + DD - X - Y;
    DB = X - DD;
    DC = Y - DD;

#if defined(BPP24) || defined(BPP32)
    X  = (DA*(A&GMASK)+DB*(B&GMASK)+DC*(C&GMASK)+DD*(D&GMASK))>>5;
    DA*= A&(RMASK|BMASK);
    DB*= B&(RMASK|BMASK);
    DC*= C&(RMASK|BMASK);
    DD*= D&(RMASK|BMASK);
    DD = (DA+DB+DC+DD)>>5;
    return((DD&(RMASK|BMASK))|(X&GMASK));
#else
    DA*= (A&(RMASK|BMASK)) | ((A&GMASK)<<16);
    DB*= (B&(RMASK|BMASK)) | ((B&GMASK)<<16);
    DC*= (C&(RMASK|BMASK)) | ((C&GMASK)<<16);
    DD*= (D&(RMASK|BMASK)) | ((D&GMASK)<<16);
    DD = (DA+DB+DC+DD)>>5;
    return((DD&(RMASK|BMASK))|((DD>>16)&GMASK));
#endif
}

/** GetColor() ***********************************************/
/** Return pixel corresponding to the given <R,G,B> value.  **/
/** This only works for non-palletized modes.               **/
/*************************************************************/
pixel GetColor(unsigned char R,unsigned char G,unsigned char B)
{
    return(PIXEL(R,G,B));
}

/** ClearImage() *********************************************/
/** Clear image with a given color.                         **/
/*************************************************************/
void ClearImage(Image *Img,int Color)
{
    pixel *P;
    int X,Y;

    for (P=(pixel *)Img->Data,Y=Img->H;Y;--Y,P+=Img->L)
        for (X=0;X<Img->W;++X) P[X]=Color;
}

/** IMGCopy() ************************************************/
/** Copy one image into another.                            **/
/*************************************************************/
void IMGCopy(Image *Dst,int DX,int DY,Image *Src,int SX,int SY,int W,int H,int TColor)
{
    pixel *S,*D;
    int X;

    if (DX<0) {
        W+=DX;
        SX-=DX;
        DX=0;
    }
    if (DY<0) {
        H+=DY;
        SY-=DY;
        DY=0;
    }
    if (SX<0) {
        W+=SX;
        DX-=SX;
        SX=0;
    } else if (SX+W>Src->W) W=Src->W-SX;
    if (SY<0) {
        H+=SY;
        DY-=SY;
        SY=0;
    } else if (SY+H>Src->H) H=Src->H-SY;
    if (DX+W>Dst->W) W=Dst->W-DX;
    if (DY+H>Dst->H) H=Dst->H-DY;

    if ((W>0)&&(H>0))
    {
        S = (pixel *)Src->Data+Src->L*SY+SX;
        D = (pixel *)Dst->Data+Dst->L*DY+DX;
        if (TColor<0)
            for (;H;--H,S+=Src->L,D+=Dst->L)
                for (X=0;X<W;++X) D[X]=S[X];
        else
            for (;H;--H,S+=Src->L,D+=Dst->L)
                for (X=0;X<W;++X)
                    if (S[X]!=TColor) D[X]=S[X];
    }
}

/** IMGDrawRect()/IMGFillRect() ******************************/
/** Draw filled/unfilled rectangle in a given image.        **/
/*************************************************************/
void IMGDrawRect(Image *Img,int X,int Y,int W,int H,int Color)
{
    pixel *P;
    int J;

    if (X<0) {
        W+=X;
        X=0;
    } else if (X+W>Img->W) W=Img->W-X;
    if (Y<0) {
        H+=Y;
        Y=0;
    } else if (Y+H>Img->H) H=Img->H-Y;

    if ((W>0)&&(H>0))
    {
        for (P=(pixel *)Img->Data+Img->L*Y+X,J=0;J<W;++J) P[J]=Color;
        for (H-=2,P+=Img->L;H;--H,P+=Img->L) P[0]=P[W-1]=Color;
        for (J=0;J<W;++J) P[J]=Color;
    }
}

void IMGFillRect(Image *Img,int X,int Y,int W,int H,int Color)
{
    pixel *P;

    if (X<0) {
        W+=X;
        X=0;
    } else if (X+W>Img->W) W=Img->W-X;
    if (Y<0) {
        H+=Y;
        Y=0;
    } else if (Y+H>Img->H) H=Img->H-Y;

    if ((W>0)&&(H>0))
        for (P=(pixel *)Img->Data+Img->L*Y+X;H;--H,P+=Img->L)
            for (X=0;X<W;++X) P[X]=Color;
}

/** IMGPrint() ***********************************************/
/** Print text in a given image.                            **/
/*************************************************************/
/*** @@@ NOT YET
void IMGPrint(Image *Img,const char *S,int X,int Y,pixel FG,pixel BG)
{
  const unsigned char *C;
  pixel *P;
  int I,J,K;

  X = X<0? 0:X>Img->W-8? Img->W-8:X;
  Y = Y<0? 0:Y>Img->H-8? Img->H-8:Y;

  for(K=X;*S;S++)
    switch(*S)
    {
      case '\n':
        K=X;Y+=8;
        if(Y>Img->H-8) Y=0;
        break;
      default:
        P=(pixel *)Img->Data+Img->L*Y+K;
        for(C=CurFont+(*S<<3),J=8;J;P+=Img->L,++C,--J)
          for(I=0;I<8;++I) P[I]=*C&(0x80>>I)? FG:BG;
        K+=8;
        if(X>Img->W-8)
        {
          K=0;Y+=8;
          if(Y>Img->H-8) Y=0;
        }
        break;
    }
}
***/

/** ScaleImage() *********************************************/
/** Copy Src into Dst, scaling as needed                    **/
/*************************************************************/
void ScaleImage(Image *Dst,const Image *Src,int X,int Y,int W,int H)
{
    register pixel *DP,*SP,*S;
    register unsigned int DX,DY;

    /* Check arguments, compute start pointer */
    if (W<0) {
        X+=W;
        W=-W;
    }
    if (H<0) {
        Y+=H;
        H=-H;
    }
    X  = X<0? 0:X>Src->W? Src->W:X;
    Y  = Y<0? 0:Y>Src->H? Src->H:Y;
    W  = X+W>Src->W? Src->W-X:W;
    H  = Y+H>Src->H? Src->H-Y:H;
    if (!W||!H) return;
    SP = (pixel *)Src->Data+Y*Src->L+X;
    DP = (pixel *)Dst->Data;
    W  = W<<16;
    H  = H<<16;
    DX = (W+Dst->W-1)/Dst->W;
    DY = (H+Dst->H-1)/Dst->H;

    for (Y=0;Y<H;Y+=DY)
    {
        S=SP+(Y>>16)*Src->L;
        for (X=0;X<W;X+=DX) *DP++=S[X>>16];
        DP+=Dst->L-Dst->W;
    }
}

/** TelevizeImage() ******************************************/
/** Create televizion effect on the image.                  **/
/*************************************************************/
void TelevizeImage(Image *Img,int X,int Y,int W,int H)
{
#if defined(BPP32) || defined(BPP24) || defined(BPP16)
    register pixel *P,*T;

    /* Check arguments, compute start pointer */
    if (W<0) {
        X+=W;
        W=-W;
    }
    if (H<0) {
        Y+=H;
        H=-H;
    }
    X = X<0? 0:X>Img->W? Img->W:X;
    Y = Y<0? 0:Y>Img->H? Img->H:Y;
    W = X+W>Img->W? Img->W-X:W;
    H = Y+H>Img->H? Img->H-Y:H;
    if (!W||!H) return;
    T = (pixel *)Img->Data+Y*Img->L+X;

#if defined(BPP32) || defined(BPP24)
    for (P=T,Y=H;Y>0;Y-=2,P+=Img->L<<1)
        for (X=0;X<W;++X) P[X]-=(P[X]>>4)&0x000F0F0F;
    for (P=T+Img->L,Y=H-1;Y>0;Y-=2,P+=Img->L<<1)
        for (X=0;X<W;++X) P[X]+=(~P[X]>>4)&0x000F0F0F;
#elif defined(UNIX) || defined(S60) || defined(UIQ)
    /* Unix/X11 and Symbian S60/UIQ use 16bit color */
    for (P=T,Y=H;Y>0;Y-=2,P+=Img->L<<1)
        for (X=0;X<W;++X) P[X]-=(P[X]>>3)&0x18E3;
    for (P=T+Img->L,Y=H-1;Y>0;Y-=2,P+=Img->L<<1)
        for (X=0;X<W;++X) P[X]+=(~P[X]>>3)&0x18E3;
#else
    /* Other platforms use 15bit color */
    for (P=T,Y=H;Y>0;Y-=2,P+=Img->L<<1)
        for (X=0;X<W;++X) P[X]-=(P[X]>>3)&0x0C63;//0x0421;//0x0C63;
    for (P=T+Img->L,Y=H-1;Y>0;Y-=2,P+=Img->L<<1)
        for (X=0;X<W;++X) P[X]+=(~P[X]>>3)&0x0C63;//0x0421;//0x0C63;
#endif /* !S60 && !UIQ */
#endif /* BPP32 || BPP24 || BPP16 */
}

/** SoftenImage() ********************************************/
/** Uses softening algorithm to interpolate image Src into  **/
/** a bigger image Dst.                                     **/
/*************************************************************/
void SoftenImage(Image *Dst,const Image *Src,int X,int Y,int W,int H)
{
#ifdef BPP8
    /* Things look real nasty in BPP8, better just scale */
    ScaleImage(Dst,Src,X,Y,W,H);
#else
    register unsigned int DX,DY;
    register const pixel *S,*SP;
    register pixel *DP,*D;

    /* Check arguments, compute start pointer */
    if (W<0) {
        X+=W;
        W=-W;
    }
    if (H<0) {
        Y+=H;
        H=-H;
    }
    X  = X<0? 0:X>Src->W? Src->W:X;
    Y  = Y<0? 0:Y>Src->H? Src->H:Y;
    W  = X+W>Src->W-3? Src->W-3-X:W;
    H  = Y+H>Src->H-3? Src->H-3-Y:H;
    // Source image must be at least 4x4 pixels
    if ((W<=0)||(H<=0)) return;
    S  = (pixel *)Src->Data+Y*Src->L+Src->L+X+1;

    // Convert to 16:16 fixed point values
    W  = (W-2)<<16;
    DX = (W-0x10001+Dst->W)/Dst->W;
    H  = (H-2)<<16;
    DY = (H-0x10001+Dst->H)/Dst->H;

    for (Y=0x10000,DP=D=(pixel *)Dst->Data;Y<H;Y+=DY,DP=(D+=Dst->L))
    {
        unsigned int A,B,C,D,X1,X2,Y1,Y2,F1,F2;

        // Compute new source pointer
        SP = S+(Y>>16)*Src->L;
        // Get fractional part of Y
        Y1 = Y&0xFFFF;
        Y2 = 0x10000-Y1;

        for (X=0x10000;X<W;SP-=X>>16,X+=DX,*DP++=X2)
        {
            // SP points to [A]:
            //
            //   E F
            // G[A]B I
            // H C D J
            //   K L

            // Get next 2x2 core pixels
            SP+= X>>16;
            A  = SP[0];
            B  = SP[1];
            C  = SP[Src->L];
            D  = SP[Src->L+1];

            // If all 4x4 pixels are the same...
            if (A==B && C==D && A==C)
            {
                // Do not interpolate
                X2 = A;
            }
            // If diagonal line "\"...
            else if (A==D && B!=C)
            {
                X1 = X&0xFFFF;
                F1 = (X1>>1) + (0x10000>>2);
                F2 = (Y1>>1) + (0x10000>>2);

                if (Y1<=F1 && A==SP[Src->L+2] && A!=SP[-Src->L])
                    X2 = Merge2(A,B,F1-Y1);
                else if (Y1>=F1 && A==SP[-1] && A!=SP[(Src->L<<1)+1])
                    X2 = Merge2(A,C,Y1-F1);
                else if (X1>=F2 && A==SP[-Src->L] && A!=SP[Src->L+2])
                    X2 = Merge2(A,B,X1-F2);
                else if (X1<=F2 && A==SP[(Src->L<<1)+1] && A!=SP[-1])
                    X2 = Merge2(A,C,F2-X1);
                else if (Y1>=X1)
                    X2 = Merge2(A,C,Y1-X1);
                else
                    X2 = Merge2(A,B,X1-Y1);
            }
            // If diagonal line "/"...
            else if (B==C && A!=D)
            {
                X1 = X&0xFFFF;
                X2 = 0x10000-X1;
                F1 = (X1>>1) + (0x10000>>2);
                F2 = (Y1>>1) + (0x10000>>2);

                if (Y2>=F1 && B==SP[Src->L-1] && B!=SP[-Src->L+1])
                    X2 = Merge2(B,A,Y2-F1);
                else if (Y2<=F1 && B==SP[2] && B!=SP[Src->L<<1])
                    X2 = Merge2(B,D,F1-Y2);
                else if (X2>=F2 && B==SP[-Src->L+1] && B!=SP[Src->L-1])
                    X2 = Merge2(B,A,X2-F2);
                else if (X2<=F2 && B==SP[Src->L<<1] && B!=SP[2])
                    X2 = Merge2(B,D,F2-X2);
                else if (Y2>=X1)
                    X2 = Merge2(B,A,Y2-X1);
                else
                    X2 = Merge2(B,D,X1-Y2);
            }
            // No diagonal lines detected...
            else
            {
                X2 = Merge4(A,B,C,D,X&0xFFFF,Y1);
            }
        }
    }
#endif /* !BPP8 */
}
