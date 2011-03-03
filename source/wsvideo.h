/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 2010-2011, William S Simms

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef WS_VIDEO
#define WS_VIDEO

struct wsVideoDirtyRect {
  unsigned int ulx, uly, lrx, lry;
};

extern struct wsVideoDirtyRect wsVideoNewDirtyRect;
extern struct wsVideoDirtyRect wsVideoAllDirtyRect;

extern unsigned char wsTouched[32768];
extern unsigned char * wsLines[384];

//extern unsigned wsFlashidx;
//extern unsigned wsFlashmask;

extern int wsTextPage;
extern int wsHiresPage;
extern int wsVideoMixed;
extern int wsVideoCharSet;

void wsVideoInit();
void wsVideoInitModel(int);
void wsUpdateVideoText(long);
void wsUpdateVideoDblText(long);

void wsUpdateVideoLores(long);
void wsUpdateVideo7MLores(long);
void wsUpdateVideoDblLores(long);

void wsUpdateVideoHires(long);
void wsUpdateVideoHires0(long);
void wsUpdateVideoDblHires(long);

extern void (* wsVideoText)(long);
extern void (* wsVideoUpdate)(long);

extern unsigned char wsVideoByte(unsigned long);
extern void wsVideoRefresh();
extern int wsVideoIsVbl();

extern void wsVideoStyle(int,int);

#endif // #ifndef WS_VIDEO
