#pragma once

int ProcessKeyboard();
void ProcessInput();
void NVideoInitialize(const bool headless);
void NVideoRedrawScreen();

extern double g_relativeSpeed;

extern bool g_stop;
