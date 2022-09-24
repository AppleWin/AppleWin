#pragma once

#define LOG_DISK_ENABLED 1	// Master enable

#define LOG_DISK_TRACKS 0
#define LOG_DISK_MOTOR 0
#define LOG_DISK_PHASES 0
#define LOG_DISK_RW_MODE 0
#define LOG_DISK_ENABLE_DRIVE 0
#define LOG_DISK_NIBBLES_SPIN 0
#define LOG_DISK_NIBBLES_READ 1
#define LOG_DISK_NIBBLES_WRITE 0
#define LOG_DISK_NIBBLES_WRITE_TRACK_GAPS 0	// Gap1, Gap2 & Gap3 info when writing a track
#define LOG_DISK_NIBBLES_USE_RUNTIME_VAR 1
#define LOG_DISK_WOZ_LOADWRITE 0
#define LOG_DISK_WOZ_SHIFTWRITE 0
#define LOG_DISK_WOZ_READTRACK 0
#define LOG_DISK_WOZ_TRACK_SEAM 0

// __VA_ARGS__ not supported on MSVC++ .NET 7.x
#if (LOG_DISK_ENABLED)
	#if !defined(_VC71)
		#define LOG_DISK(format, ...) LOG(format, __VA_ARGS__)
	#else
		#define LOG_DISK	 LogOutput
	#endif
#else
	#if !defined(_VC71)
		#define LOG_DISK(...)
	#else
		#define LOG_DISK(x)
	#endif
#endif
