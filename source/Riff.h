#pragma once

int RiffInitWriteFile(char* pszFile, unsigned int sample_rate, unsigned int NumChannels);
int RiffFinishWriteFile();
int RiffPutSamples(short* buf, unsigned int uSamples);
