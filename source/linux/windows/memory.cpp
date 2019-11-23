#include "linux/windows/memory.h"
#include <cstdlib>

LPVOID VirtualAlloc(LPVOID lpAddress, size_t dwSize,
		    DWORD flAllocationType, DWORD flProtect) {
  /* just malloc and alles? 0_0 */
  void* mymemory = realloc(lpAddress, dwSize);
  if (flAllocationType & MEM_COMMIT)
    ZeroMemory(mymemory, dwSize); // original VirtualAlloc does this (if..)
  return mymemory;
}

BOOL VirtualFree(LPVOID lpAddress, size_t dwSize,
		 DWORD dwFreeType) {
  free(lpAddress);
  return TRUE;
}
