#pragma once

int ProcessKeyboard();
void ProcessInput();
void NVideoInitialize();
void VideoUninitialize();
void VideoRedrawScreen();

void output(const char *fmt, ...);
extern double g_relativeSpeed;
