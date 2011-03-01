//
// WS_VIDEO
//

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
