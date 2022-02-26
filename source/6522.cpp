/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2021, Tom Charlesworth, Michael Pohoreski, Nick Westgate

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Rockwell 6522 VIA emulation
 *
 * Author: Various
 */

#include "StdAfx.h"

#include "6522.h"
#include "Core.h"
#include "CPU.h"
#include "Memory.h"
#include "SynchronousEventManager.h"
#include "YamlHelper.h"

void SY6522::Reset(const bool powerCycle)
{
	if (powerCycle)
	{
		memset(&m_regs, 0, sizeof(Regs));
		m_regs.TIMER1_LATCH.w = 0xffff;	// Some random value (but pick $ffff so it's deterministic)
										// . NB. if it's too small (< ~$0007) then MB detection routines will fail!
	}

	CpuCreateCriticalSection();	// Reset() called by SY6522 global ctor, so explicitly create CPU's CriticalSection
	Write(rACR, 0x00);	// ACR = 0x00: T1 one-shot mode
	Write(rIFR, 0x7f);	// IFR = 0x7F: de-assert any IRQs
	Write(rIER, 0x7f);	// IER = 0x7F: disable all IRQs

	StopTimer1();
	StopTimer2();

	m_timer1IrqDelay = m_timer2IrqDelay = 0;
}

//---------------------------------------------------------------------------

void SY6522::StartTimer1(void)
{
	m_timer1Active = true;
}

// The assumption was that timer1 was only active if IER.TIMER1=1
// . Not true, since IFR can be polled (with IER.TIMER1=0)
void SY6522::StartTimer1_LoadStateV1(void)
{
	if ((m_regs.IER & IxR_TIMER1) == 0x00)
		return;

	m_timer1Active = true;
}

void SY6522::StopTimer1(void)
{
	m_timer1Active = false;
}

//-----------------------------------------------------------------------------

void SY6522::StartTimer2(void)
{
	m_timer2Active = true;

}

void SY6522::StopTimer2(void)
{
	m_timer2Active = false;
}

//-----------------------------------------------------------------------------

// Insert a new synchronous event whenever the 6522 timer's counter is written.
// . NB. it doesn't matter if the timer's interrupt enable (IER) is set or not
//   - the state of IER is only important when the counter underflows - see: MB_SyncEventCallback()
USHORT SY6522::SetTimerSyncEvent(BYTE reg, USHORT timerLatch)
{
	_ASSERT(reg == rT1CH || reg == rT2CH);
	SyncEvent* syncEvent = reg == rT1CH ? m_syncEvent[0] : m_syncEvent[1];

	// NB. This TIMER adjustment value gets subtracted when this current opcode completes, so no need to persist to save-state
	const UINT opcodeCycleAdjust = GetOpcodeCyclesForWrite(reg);

	if (syncEvent->m_active)
		g_SynchronousEventMgr.Remove(syncEvent->m_id);

	syncEvent->SetCycles(timerLatch + kExtraTimerCycles + opcodeCycleAdjust);
	g_SynchronousEventMgr.Insert(syncEvent);

	// It doesn't matter if this overflows (ie. >0xFFFF), since on completion of current opcode it'll be corrected
	return (USHORT)(timerLatch + opcodeCycleAdjust);
}

//-----------------------------------------------------------------------------

extern void MB_UpdateIRQ(void);

void SY6522::UpdateIFR(BYTE clr_ifr, BYTE set_ifr /*= 0*/)
{
	m_regs.IFR &= ~clr_ifr;
	m_regs.IFR |= set_ifr;

	if (m_regs.IFR & m_regs.IER & 0x7F)
		m_regs.IFR |= IFR_IRQ;
	else
		m_regs.IFR &= ~IFR_IRQ;

	MB_UpdateIRQ();
}

//-----------------------------------------------------------------------------

void SY6522::Write(BYTE nReg, BYTE nValue)
{
	switch (nReg)
	{
	case 0x00:	// ORB
		m_regs.ORB = nValue & m_regs.DDRB;
		break;
	case 0x01:	// ORA
		m_regs.ORA = nValue & m_regs.DDRA;
		break;
	case 0x02:	// DDRB
		m_regs.DDRB = nValue;
		break;
	case 0x03:	// DDRA
		m_regs.DDRA = nValue;
		break;
	case 0x04:	// TIMER1L_COUNTER
	case 0x06:	// TIMER1L_LATCH
		m_regs.TIMER1_LATCH.l = nValue;
		break;
	case 0x05:	// TIMER1H_COUNTER
	{
		UpdateIFR(IxR_TIMER1);				// Clear Timer1 Interrupt Flag
		m_regs.TIMER1_LATCH.h = nValue;
		m_regs.TIMER1_COUNTER.w = SetTimerSyncEvent(nReg, m_regs.TIMER1_LATCH.w);
		StartTimer1();
	}
	break;
	case 0x07:	// TIMER1H_LATCH
		UpdateIFR(IxR_TIMER1);				// Clear Timer1 Interrupt Flag
		m_regs.TIMER1_LATCH.h = nValue;
		break;
	case 0x08:	// TIMER2L
		m_regs.TIMER2_LATCH.l = nValue;
		break;
	case 0x09:	// TIMER2H
	{
		UpdateIFR(IxR_TIMER2);				// Clear Timer2 Interrupt Flag
		m_regs.TIMER2_LATCH.h = nValue;		// NB. Real 6522 doesn't have TIMER2_LATCH.h
		m_regs.TIMER2_COUNTER.w = SetTimerSyncEvent(nReg, m_regs.TIMER2_LATCH.w);
		StartTimer2();
	}
	break;
	case 0x0a:	// SERIAL_SHIFT
		break;
	case 0x0b:	// ACR
		m_regs.ACR = nValue;
		break;
	case 0x0c:	// PCR -  Used for Speech chip only
		m_regs.PCR = nValue;
		break;
	case 0x0d:	// IFR
		// - Clear those bits which are set in the lower 7 bits.
		// - Can't clear bit 7 directly.
		UpdateIFR(nValue);
		break;
	case 0x0e:	// IER
		if (!(nValue & 0x80))
		{
			// Clear those bits which are set in the lower 7 bits.
			nValue ^= 0x7F;
			m_regs.IER &= nValue;
		}
		else
		{
			// Set those bits which are set in the lower 7 bits.
			nValue &= 0x7F;
			m_regs.IER |= nValue;
		}
		UpdateIFR(0);
		break;
	case 0x0f:	// ORA_NO_HS
		break;
	}
}

//-----------------------------------------------------------------------------

void SY6522::UpdateTimer1(USHORT clocks)
{
	if (CheckTimerUnderflow(m_regs.TIMER1_COUNTER.w, m_timer1IrqDelay, clocks))
		m_timer1IrqDelay = OnTimer1Underflow(m_regs.TIMER1_COUNTER.w);
}

void SY6522::UpdateTimer2(USHORT clocks)
{
	// No TIMER2 latch so "after timing out, the counter will continue to decrement"
	CheckTimerUnderflow(m_regs.TIMER2_COUNTER.w, m_timer2IrqDelay, clocks);
}

//-----------------------------------------------------------------------------

bool SY6522::CheckTimerUnderflow(USHORT& counter, int& timerIrqDelay, const USHORT clocks)
{
	if (clocks == 0)
		return false;

	int oldTimer = counter;
	int timer = counter;
	timer -= clocks;
	counter = (USHORT)timer;

	bool timerIrq = false;

	if (timerIrqDelay)	// Deal with any previous counter underflow which didn't yet result in an IRQ
	{
		_ASSERT(timerIrqDelay == 1);
		timerIrqDelay = 0;
		timerIrq = true;
		// if LATCH is very small then could underflow for every opcode...
	}

	if (oldTimer >= 0 && timer < 0)	// Underflow occurs for 0x0000 -> 0xFFFF
	{
		if (timer <= -2)				// TIMER = 0xFFFE (or less)
			timerIrq = true;
		else							// TIMER = 0xFFFF
			timerIrqDelay = 1;			// ...so 1 cycle until IRQ
	}

	return timerIrq;
}

int SY6522::OnTimer1Underflow(USHORT& counter)
{
	int timer = (int)(short)(counter);
	while (timer < -1)
		timer += (m_regs.TIMER1_LATCH.w + kExtraTimerCycles);	// GH#651: account for underflowed cycles / GH#652: account for extra 2 cycles
	counter = (USHORT)timer;
	return (timer == -1) ? 1 : 0;		// timer1IrqDelay
}

//-----------------------------------------------------------------------------

USHORT SY6522::GetTimer1Counter(BYTE reg)
{
	USHORT counter = m_regs.TIMER1_COUNTER.w;	// NB. don't update the real T1C
	int timerIrqDelay = m_timer1IrqDelay;		// NB. don't update the real timer1IrqDelay
	const UINT opcodeCycleAdjust = GetOpcodeCyclesForRead(reg) - 1;	// to compensate for the 4/5/6 cycle read opcode
	if (CheckTimerUnderflow(counter, timerIrqDelay, opcodeCycleAdjust))
		OnTimer1Underflow(counter);
	return counter;
}

USHORT SY6522::GetTimer2Counter(BYTE reg)
{
	const UINT opcodeCycleAdjust = GetOpcodeCyclesForRead(reg) - 1;	// to compensate for the 4/5/6 cycle read opcode
	return m_regs.TIMER2_COUNTER.w - opcodeCycleAdjust;
}

bool SY6522::IsTimer1Underflowed(BYTE reg)
{
	USHORT counter = m_regs.TIMER1_COUNTER.w;	// NB. don't update the real T1C
	int timerIrqDelay = m_timer1IrqDelay;		// NB. don't update the real timer1IrqDelay
	const UINT opcodeCycleAdjust = GetOpcodeCyclesForRead(reg);	// to compensate for the 4/5/6 cycle read opcode
	return CheckTimerUnderflow(counter, timerIrqDelay, opcodeCycleAdjust);
}

bool SY6522::IsTimer2Underflowed(BYTE reg)
{
	USHORT counter = m_regs.TIMER2_COUNTER.w;	// NB. don't update the real T2C
	int timerIrqDelay = m_timer2IrqDelay;		// NB. don't update the real timer2IrqDelay
	const UINT opcodeCycleAdjust = GetOpcodeCyclesForRead(reg);	// to compensate for the 4/5/6 cycle read opcode
	return CheckTimerUnderflow(counter, timerIrqDelay, opcodeCycleAdjust);
}

//-----------------------------------------------------------------------------

BYTE SY6522::Read(BYTE nReg)
{
	BYTE nValue = 0x00;

	switch (nReg)
	{
	case 0x00:	// ORB
		nValue = m_regs.ORB;
		break;
	case 0x01:	// ORA
		nValue = m_regs.ORA;
		break;
	case 0x02:	// DDRB
		nValue = m_regs.DDRB;
		break;
	case 0x03:	// DDRA
		nValue = m_regs.DDRA;
		break;
	case 0x04:	// TIMER1L_COUNTER
		// NB. GH#701 (T1C:=0xFFFF, LDA T1C_L[4cy], A==0xFC)
		nValue = GetTimer1Counter(nReg) & 0xff;
		UpdateIFR(IxR_TIMER1);
		break;
	case 0x05:	// TIMER1H_COUNTER
		nValue = GetTimer1Counter(nReg) >> 8;
		break;
	case 0x06:	// TIMER1L_LATCH
		nValue = m_regs.TIMER1_LATCH.l;
		break;
	case 0x07:	// TIMER1H_LATCH
		nValue = m_regs.TIMER1_LATCH.h;
		break;
	case 0x08:	// TIMER2L
		nValue = GetTimer2Counter(nReg) & 0xff;
		UpdateIFR(IxR_TIMER2);
		break;
	case 0x09:	// TIMER2H
		nValue = GetTimer2Counter(nReg) >> 8;
		break;
	case 0x0a:	// SERIAL_SHIFT
		break;
	case 0x0b:	// ACR
		nValue = m_regs.ACR;
		break;
	case 0x0c:	// PCR
		nValue = m_regs.PCR;
		break;
	case 0x0d:	// IFR
		nValue = m_regs.IFR;
		if (m_timer1Active && IsTimer1Underflowed(nReg))
			nValue |= IxR_TIMER1;
		if (m_timer2Active && IsTimer2Underflowed(nReg))
			nValue |= IxR_TIMER2;
		break;
	case 0x0e:	// IER
		nValue = 0x80 | m_regs.IER;	// GH#567
		break;
	case 0x0f:	// ORA_NO_HS
		nValue = m_regs.ORA;
		break;
	}

	return nValue;
}

//-----------------------------------------------------------------------------

// TODO: RMW opcodes: dec,inc,asl,lsr,rol,ror (abs16 & abs16,x) + 65C02 trb,tsb (abs16)
UINT SY6522::GetOpcodeCyclesForRead(BYTE reg)
{
	UINT opcodeCycles = 0;
	BYTE opcode = 0;
	bool abs16 = false;
	bool abs16x = false;
	bool abs16y = false;
	bool indx = false;
	bool indy = false;

	const BYTE opcodeMinus3 = mem[(::regs.pc - 3) & 0xffff];
	const BYTE opcodeMinus2 = mem[(::regs.pc - 2) & 0xffff];

	if (((opcodeMinus2 & 0x0f) == 0x01) && ((opcodeMinus2 & 0x10) == 0x00))	// ora (zp,x), and (zp,x), ..., sbc (zp,x)
	{
		// NB. this is for read, so don't need to exclude 0x81 / sta (zp,x)
		opcodeCycles = 6;
		opcode = opcodeMinus2;
		indx = true;
	}
	else if (((opcodeMinus2 & 0x0f) == 0x01) && ((opcodeMinus2 & 0x10) == 0x10))	// ora (zp),y, and (zp),y, ..., sbc (zp),y
	{
		// NB. this is for read, so don't need to exclude 0x91 / sta (zp),y
		opcodeCycles = 5;
		opcode = opcodeMinus2;
		indy = true;
	}
	else if (((opcodeMinus2 & 0x0f) == 0x02) && ((opcodeMinus2 & 0x10) == 0x10) && GetMainCpu() == CPU_65C02)	// ora (zp), and (zp), ..., sbc (zp) : 65C02-only
	{
		// NB. this is for read, so don't need to exclude 0x92 / sta (zp)
		opcodeCycles = 5;
		opcode = opcodeMinus2;
	}
	else
	{
		if ((((opcodeMinus3 & 0x0f) == 0x0D) && ((opcodeMinus3 & 0x10) == 0x00)) ||	// ora abs16, and abs16, ..., sbc abs16
			(opcodeMinus3 == 0x2C) ||			// bit abs16
			(opcodeMinus3 == 0xAC) ||			// ldy abs16
			(opcodeMinus3 == 0xAE) ||			// ldx abs16
			(opcodeMinus3 == 0xCC) ||			// cpy abs16
			(opcodeMinus3 == 0xEC))			// cpx abs16
		{
		}
		else if ((opcodeMinus3 == 0xBC) ||			// ldy abs16,x
			((opcodeMinus3 == 0x3C) && GetMainCpu() == CPU_65C02))		// bit abs16,x : 65C02-only
		{
			abs16x = true;
		}
		else if ((opcodeMinus3 == 0xBE))			// ldx abs16,y
		{
			abs16y = true;
		}
		else if ((opcodeMinus3 & 0x10) == 0x10)
		{
			if ((opcodeMinus3 & 0x0f) == 0x0D)		// ora abs16,x, and abs16,x, ..., sbc abs16,x
				abs16x = true;
			else if ((opcodeMinus3 & 0x0f) == 0x09) // ora abs16,y, and abs16,y, ..., sbc abs16,y
				abs16y = true;
		}
		else
		{
			_ASSERT(0);
			opcodeCycles = 0;
			return 0;
		}

		opcodeCycles = 4;
		opcode = opcodeMinus3;
		abs16 = true;
	}

	//

	WORD addr16 = 0;

	if (!abs16)
	{
		BYTE zp = mem[(::regs.pc - 1) & 0xffff];
		if (indx) zp += ::regs.x;
		addr16 = (mem[zp] | (mem[(zp + 1) & 0xff] << 8));
		if (indy) addr16 += ::regs.y;
	}
	else
	{
		addr16 = mem[(::regs.pc - 2) & 0xffff] | (mem[(::regs.pc - 1) & 0xffff] << 8);
		if (abs16y) addr16 += ::regs.y;
		if (abs16x) addr16 += ::regs.x;
	}

	// Check we've reverse looked-up the 6502 opcode correctly
	if ((addr16 & 0xF80F) != (0xC000 + reg))
	{
		_ASSERT(0);
		return 0;
	}

	return opcodeCycles;
}

// TODO: RMW opcodes: dec,inc,asl,lsr,rol,ror (abs16 & abs16,x) + 65C02 trb,tsb (abs16)
UINT SY6522::GetOpcodeCyclesForWrite(BYTE reg)
{
	UINT opcodeCycles = 0;
	BYTE opcode = 0;
	bool abs16 = false;

	const BYTE opcodeMinus3 = mem[(::regs.pc - 3) & 0xffff];
	const BYTE opcodeMinus2 = mem[(::regs.pc - 2) & 0xffff];

	if ((opcodeMinus3 == 0x8C) ||		// sty abs16
		(opcodeMinus3 == 0x8D) ||		// sta abs16
		(opcodeMinus3 == 0x8E))		// stx abs16
	{	// Eg. FT demos: CHIP, MADEF, MAD2
		opcodeCycles = 4;
		opcode = opcodeMinus3;
		abs16 = true;
	}
	else if ((opcodeMinus3 == 0x99) ||	// sta abs16,y
		(opcodeMinus3 == 0x9D))	// sta abs16,x
	{	// Eg. Paleotronic microTracker demo
		opcodeCycles = 5;
		opcode = opcodeMinus3;
		abs16 = true;
	}
	else if (opcodeMinus2 == 0x81)		// sta (zp,x)
	{
		opcodeCycles = 6;
		opcode = opcodeMinus2;
	}
	else if (opcodeMinus2 == 0x91)		// sta (zp),y
	{	// Eg. FT demos: OMT, PLS
		opcodeCycles = 6;
		opcode = opcodeMinus2;
	}
	else if (opcodeMinus2 == 0x92 && GetMainCpu() == CPU_65C02)		// sta (zp) : 65C02-only
	{
		opcodeCycles = 5;
		opcode = opcodeMinus2;
	}
	else if (opcodeMinus3 == 0x9C && GetMainCpu() == CPU_65C02)		// stz abs16 : 65C02-only
	{
		opcodeCycles = 4;
		opcode = opcodeMinus3;
		abs16 = true;
	}
	else if (opcodeMinus3 == 0x9E && GetMainCpu() == CPU_65C02)		// stz abs16,x : 65C02-only
	{
		opcodeCycles = 5;
		opcode = opcodeMinus3;
		abs16 = true;
	}
	else
	{
		_ASSERT(0);
		opcodeCycles = 0;
		return 0;
	}

	//

	WORD addr16 = 0;

	if (!abs16)
	{
		BYTE zp = mem[(::regs.pc - 1) & 0xffff];
		if (opcode == 0x81) zp += ::regs.x;
		addr16 = (mem[zp] | (mem[(zp + 1) & 0xff] << 8));
		if (opcode == 0x91) addr16 += ::regs.y;
	}
	else
	{
		addr16 = mem[(::regs.pc - 2) & 0xffff] | (mem[(::regs.pc - 1) & 0xffff] << 8);
		if (opcode == 0x99) addr16 += ::regs.y;
		if (opcode == 0x9D || opcode == 0x9E) addr16 += ::regs.x;
	}

	// Check we've reverse looked-up the 6502 opcode correctly
	if ((addr16 & 0xF80F) != (0xC000 + reg))
	{
		_ASSERT(0);
		return 0;
	}

	return opcodeCycles;
}

//=============================================================================

#define SS_YAML_KEY_SY6522 "SY6522"
// NB. No version - this is determined by the parent "Mockingboard C" or "Phasor" unit

#define SS_YAML_KEY_SY6522_REG_ORB "ORB"
#define SS_YAML_KEY_SY6522_REG_ORA "ORA"
#define SS_YAML_KEY_SY6522_REG_DDRB "DDRB"
#define SS_YAML_KEY_SY6522_REG_DDRA "DDRA"
#define SS_YAML_KEY_SY6522_REG_T1_COUNTER "Timer1 Counter"
#define SS_YAML_KEY_SY6522_REG_T1_LATCH "Timer1 Latch"
#define SS_YAML_KEY_SY6522_REG_T2_COUNTER "Timer2 Counter"
#define SS_YAML_KEY_SY6522_REG_T2_LATCH "Timer2 Latch"
#define SS_YAML_KEY_SY6522_REG_SERIAL_SHIFT "Serial Shift"
#define SS_YAML_KEY_SY6522_REG_ACR "ACR"
#define SS_YAML_KEY_SY6522_REG_PCR "PCR"
#define SS_YAML_KEY_SY6522_REG_IFR "IFR"
#define SS_YAML_KEY_SY6522_REG_IER "IER"

#define SS_YAML_KEY_SY6522_TIMER1_IRQ_DELAY "Timer1 IRQ Delay"
#define SS_YAML_KEY_SY6522_TIMER2_IRQ_DELAY "Timer2 IRQ Delay"
#define SS_YAML_KEY_SY6522_TIMER1_ACTIVE "Timer1 Active"
#define SS_YAML_KEY_SY6522_TIMER2_ACTIVE "Timer2 Active"

void SY6522::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Label label(yamlSaveHelper, "%s:\n", SS_YAML_KEY_SY6522);

	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SY6522_REG_ORB, m_regs.ORB);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SY6522_REG_ORA, m_regs.ORA);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SY6522_REG_DDRB, m_regs.DDRB);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SY6522_REG_DDRA, m_regs.DDRA);
	yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_SY6522_REG_T1_COUNTER, m_regs.TIMER1_COUNTER.w);
	yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_SY6522_REG_T1_LATCH, m_regs.TIMER1_LATCH.w);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_SY6522_TIMER1_IRQ_DELAY, m_timer1IrqDelay);	// v4
	yamlSaveHelper.SaveBool(SS_YAML_KEY_SY6522_TIMER1_ACTIVE, m_timer1Active);		// v8
	yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_SY6522_REG_T2_COUNTER, m_regs.TIMER2_COUNTER.w);
	yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_SY6522_REG_T2_LATCH, m_regs.TIMER2_LATCH.w);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_SY6522_TIMER2_IRQ_DELAY, m_timer2IrqDelay);	// v4
	yamlSaveHelper.SaveBool(SS_YAML_KEY_SY6522_TIMER2_ACTIVE, m_timer2Active);		// v8
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SY6522_REG_SERIAL_SHIFT, m_regs.SERIAL_SHIFT);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SY6522_REG_ACR, m_regs.ACR);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SY6522_REG_PCR, m_regs.PCR);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SY6522_REG_IFR, m_regs.IFR);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SY6522_REG_IER, m_regs.IER);
	// NB. No need to write ORA_NO_HS, since same data as ORA, just without handshake
}

void SY6522::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (!yamlLoadHelper.GetSubMap(SS_YAML_KEY_SY6522))
		throw std::runtime_error("Card: Expected key: " SS_YAML_KEY_SY6522);

	m_regs.ORB = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_ORB);
	m_regs.ORA = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_ORA);
	m_regs.DDRB = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_DDRB);
	m_regs.DDRA = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_DDRA);
	m_regs.TIMER1_COUNTER.w = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_T1_COUNTER);
	m_regs.TIMER1_LATCH.w = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_T1_LATCH);
	m_regs.TIMER2_COUNTER.w = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_T2_COUNTER);
	m_regs.TIMER2_LATCH.w = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_T2_LATCH);
	m_regs.SERIAL_SHIFT = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_SERIAL_SHIFT);
	m_regs.ACR = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_ACR);
	m_regs.PCR = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_PCR);
	m_regs.IFR = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_IFR);
	m_regs.IER = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_IER);
	m_regs.ORA_NO_HS = 0;	// Not saved

	m_timer1IrqDelay = m_timer2IrqDelay = 0;

	if (version >= 4)
	{
		m_timer1IrqDelay = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_TIMER1_IRQ_DELAY);
		m_timer2IrqDelay = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_TIMER2_IRQ_DELAY);
	}

	if (version < 7)
	{
		// Assume t1_latch was never written to (so had the old default of 0x0000) - this now results in failure of Mockingboard detection!
		if (m_regs.TIMER1_LATCH.w == 0x0000)
			m_regs.TIMER1_LATCH.w = 0xFFFF;		// Allow Mockingboard detection to succeed
	}

	if (version >= 8)
	{
		bool timer1Active = yamlLoadHelper.LoadBool(SS_YAML_KEY_SY6522_TIMER1_ACTIVE);
		bool timer2Active = yamlLoadHelper.LoadBool(SS_YAML_KEY_SY6522_TIMER2_ACTIVE);
		SetTimersActiveFromSnapshot(timer1Active, timer2Active, version);
	}

	yamlLoadHelper.PopMap();
}

void SY6522::SetTimersActiveFromSnapshot(bool timer1Active, bool timer2Active, UINT version)
{
	m_timer1Active = timer1Active;
	m_timer2Active = timer2Active;

	//

	if (version == 1)
	{
		StartTimer1_LoadStateV1();	// Attempt to start timer
	}
	else	// version >= 2
	{
		if (IsTimer1Active())
			StartTimer1();			// Attempt to start timer
	}

	if (IsTimer1Active())
	{
		SyncEvent* syncEvent = m_syncEvent[0];
		syncEvent->SetCycles(GetRegT1C() + kExtraTimerCycles);	// NB. use COUNTER, not LATCH
		g_SynchronousEventMgr.Insert(syncEvent);
	}
	if (IsTimer2Active())
	{
		SyncEvent* syncEvent = m_syncEvent[1];
		syncEvent->SetCycles(GetRegT2C() + kExtraTimerCycles);	// NB. use COUNTER, not LATCH
		g_SynchronousEventMgr.Insert(syncEvent);
	}
}
