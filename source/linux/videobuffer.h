#pragma once

#include <cstdint>

// calls VideoResetState();
// and
// initialises g_pFramebufferbits as a simple malloc'ed buffer
void VideoBufferInitialize();
void VideoBufferDestroy();

void getScreenData(uint8_t * & data, int & width, int & height, int & sx, int & sy, int & sw, int & sh);
