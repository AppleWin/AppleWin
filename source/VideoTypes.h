#pragma once

// Video.h
// TODO: Replace with WinGDI.h / RGBQUAD
struct bgra_t
{
	uint8_t b;
	uint8_t g;
	uint8_t r;
	uint8_t a; // reserved on Win32
};
