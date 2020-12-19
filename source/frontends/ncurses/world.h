#pragma once

int ProcessKeyboard();
void ProcessInput();
void NVideoInitialize(const bool headless);
void VideoRedrawScreen();

extern double g_relativeSpeed;

extern bool g_stop;
