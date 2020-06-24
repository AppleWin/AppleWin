#pragma once

void LoadConfiguration(void);
void CheckCpu();
void SetWindowTitle();

void getScreenData(uint8_t * & data, int & width, int & height, int & sx, int & sy, int & sw, int & sh);

extern int g_nAltCharSetOffset; // alternate character set
