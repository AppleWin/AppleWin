#pragma once

int RiffInitWriteFile(const char* pszFile, unsigned int sample_rate, unsigned int NumChannels);
int RiffFinishWriteFile();
int RiffPutSamples(const short* buf, unsigned int uSamples);
