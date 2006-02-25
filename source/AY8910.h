#ifndef AY8910_H
#define AY8910_H

#define MAX_8910 4

void _AYWriteReg(int n, int r, int v);
void AY8910_write_ym(int chip, int addr, int data);
void AY8910_reset(int chip);
void AY8910Update(int chip,INT16 **buffer,int length);

void AY8910_InitAll(int nClock, int nSampleRate);
void AY8910_InitClock(int nClock);
BYTE* AY8910_GetRegsPtr(UINT nAyNum);

#endif
