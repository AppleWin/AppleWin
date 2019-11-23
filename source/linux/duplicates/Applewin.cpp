#include "StdAfx.h"
#include "Memory.h"
#include "Log.h"
#include "Common.h"
#include "CPU.h"

static bool bLogKeyReadDone = false;
static DWORD dwLogKeyReadTickStart;

void LogFileTimeUntilFirstKeyReadReset(void)
{
	if (!g_fh)
		return;

	dwLogKeyReadTickStart = GetTickCount();

	bLogKeyReadDone = false;
}

// Log the time from emulation restart/reboot until the first key read: BIT $C000
// . AZTEC.DSK (DOS 3.3) does prior LDY $C000 reads, but the BIT $C000 is at the "Press any key" message
// . Phasor1.dsk / ProDOS 1.1.1: PC=E797: B1 50: LDA ($50),Y / "Select an Option:" message
// . Rescue Raiders v1.3,v1.5: PC=895: LDA $C000 / boot to intro
void LogFileTimeUntilFirstKeyRead(void)
{
	if (!g_fh || bLogKeyReadDone)
		return;

	if ( (mem[regs.pc-3] != 0x2C)	// AZTEC: bit $c000
		&& !((regs.pc-2) == 0xE797 && mem[regs.pc-2] == 0xB1 && mem[regs.pc-1] == 0x50)	// Phasor1: lda ($50),y
		&& !((regs.pc-3) == 0x0895 && mem[regs.pc-3] == 0xAD)	// Rescue Raiders v1.3,v1.5: lda $c000
		)
		return;

	DWORD dwTime = GetTickCount() - dwLogKeyReadTickStart;

	LogFileOutput("Time from emulation reboot until first $C000 access: %d msec\n", dwTime);

	bLogKeyReadDone = true;
}
