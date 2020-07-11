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

// NTSC_Charset.h
typedef unsigned char(*csbits_t)[256][8];

// RGB_Monitor.h
const UINT kNumBaseColors = 16;
typedef bgra_t(*baseColors_t)[kNumBaseColors];
