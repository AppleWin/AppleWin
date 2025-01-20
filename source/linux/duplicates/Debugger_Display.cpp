#include "StdAfx.h"
#include "Debug.h"
#include "Keyboard.h"

// NOTE: Keep in sync ConsoleColors_e g_anConsoleColor !
COLORREF g_anConsoleColor[NUM_CONSOLE_COLORS] = {
    // # <Bright Blue Green Red>
    RGB(0, 0, 0),       // 0 0000 K
    RGB(255, 32, 32),   // 1 1001 R
    RGB(0, 255, 0),     // 2 1010 G
    RGB(255, 255, 0),   // 3 1011 Y
    RGB(64, 64, 255),   // 4 1100 B
    RGB(255, 0, 255),   // 5 1101 M Purple/Magenta now used for warnings.
    RGB(0, 255, 255),   // 6 1110 C
    RGB(255, 255, 255), // 7 1111 W
    RGB(255, 128, 0),   // 8 0011 Orange
    RGB(128, 128, 128), // 9 0111 Grey

    RGB(80, 192, 255) // Lite Blue
};

VideoScannerDisplayInfo g_videoScannerDisplayInfo;

char g_aDebuggerVirtualTextScreen[DEBUG_VIRTUAL_TEXT_HEIGHT][DEBUG_VIRTUAL_TEXT_WIDTH];

void DrawConsoleCursor()
{
}

HDC GetDebuggerMemDC(void)
{
    return nullptr;
}

void KeybQueueKeypress(WPARAM /* key */, Keystroke_e /* bASCII */)
{
}

void UpdateDisplay(Update_t /* bUpdate */)
{
}

void StretchBltMemToFrameDC(void)
{
}

void ReleaseDebuggerMemDC(void)
{
}

void ReleaseConsoleFontDC(void)
{
}

HDC GetConsoleFontDC(void)
{
    return nullptr;
}

void DrawConsoleInput()
{
}
