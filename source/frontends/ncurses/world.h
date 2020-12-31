#pragma once

#include <string>

int ProcessKeyboard();
void ProcessInput();
void NVideoInitialise(const bool headless);
void NVideoRedrawScreen();
void PaddleInitialise(const std::string & device);

extern double g_relativeSpeed;

extern bool g_stop;
