#pragma once

// calls VideoResetState();
// and
// initialises g_pFramebufferbits as a simple malloc'ed buffer
void VideoBufferInitialize();
void VideoBufferDestroy();
