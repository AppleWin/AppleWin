#pragma once

#define LOG_DISK_ENABLED 0	// Master enable

#define LOG_DISK_TRACKS 1
#define LOG_DISK_MOTOR 1
#define LOG_DISK_PHASES 1
#define LOG_DISK_RW_MODE 1
#define LOG_DISK_ENABLE_DRIVE 1
#define LOG_DISK_NIBBLES_SPIN 1
#define LOG_DISK_NIBBLES_READ 1
#define LOG_DISK_NIBBLES_WRITE 1
#define LOG_DISK_NIBBLES_WRITE_TRACK_GAPS 1	// Gap1, Gap2 & Gap3 info when writing a track
#define LOG_DISK_NIBBLES_USE_RUNTIME_VAR 1
#define LOG_DISK_WOZ_LOADWRITE 1
#define LOG_DISK_WOZ_SHIFTWRITE 1

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
