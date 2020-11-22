#pragma once

int ProcessKeyboard();
void ProcessInput();
void NVideoInitialize();
void NVideoUninitialize();
void VideoRedrawScreen();

void output(const char *fmt, ...);
extern double g_relativeSpeed;
