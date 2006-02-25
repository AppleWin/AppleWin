#pragma once

#define  TRACKS      35
#define  IMAGETYPES  7
#define  NIBBLES     6656

BOOL    ImageBoot (HIMAGE);
void    ImageClose (HIMAGE);
void    ImageDestroy ();
void    ImageInitialize ();
int     ImageOpen (LPCTSTR,HIMAGE *,BOOL *,BOOL);
void    ImageReadTrack (HIMAGE,int,int,LPBYTE,int *);
void    ImageWriteTrack (HIMAGE,int,int,LPBYTE,int);
