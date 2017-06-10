#pragma once

int ProcessKeyboard();
void ProcessInput();
void VideoInitialize();
void VideoUninitialize();
void VideoRedrawScreen();

void output(const char *fmt, ...);
extern double g_relativeSpeed;
