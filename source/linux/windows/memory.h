#pragma once

#include <cstddef>
#include <cstring>
#include <type_traits>
#include "linux/windows/wincompat.h"

#define MEM_COMMIT              0x00001000
#define MEM_RESERVE             0x00002000
#define MEM_RELEASE             0x00008000

#define	PAGE_NOACCESS		0x01
#define	PAGE_READWRITE		0x04

#define MoveMemory(Destination,Source,Length) memmove((Destination),(Source),(Length))
#define FillMemory(Destination,Length,Fill) memset((Destination),(Fill),(Length))
#define EqualMemory(Destination,Source,Length) (!memcmp((Destination),(Source),(Length)))
#define CopyMemory(Destination,Source,Length) memcpy((Destination),(Source),(Length))

template <typename T>
void ZeroMemory(T * dest, const size_t size)
{
  // never zero a C++ complex type requiring a constructor call.
  static_assert(std::is_pod<T>::value || std::is_void<T>::value, "POD required for ZeroMemory()");
  memset(dest, 0, size);
}

LPVOID VirtualAlloc(LPVOID lpAddress, size_t dwSize,
		    DWORD flAllocationType, DWORD flProtect);

BOOL VirtualFree(LPVOID lpAddress, size_t dwSize, DWORD dwFreeType);
