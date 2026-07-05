#pragma once

bool RiffInitWriteFile(const char* pszFile, unsigned int sample_rate, unsigned int NumChannels);
bool RiffFinishWriteFile();
bool RiffPutSamples(const short* buf, unsigned int uSamples);
