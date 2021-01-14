#pragma once

#include <memory>

class NFrame;

int ProcessKeyboard(const std::shared_ptr<NFrame> & frame);
void SetCtrlCHandler(const bool headless);

extern double g_relativeSpeed;

extern bool g_stop;
