/*
AppleWin : An Apple //e emulator for Windows

Advanced Logic Systems "The CP/M Card"

A 6 MHz Z80B with its own 64K of DRAM, the card CP/M Plus 3.01 for the
Apple II was sold with. Modelled from the card's schematic ("CP/M CARD" rev A,
05-04-83) and photos of a board; the MAME device, the photos, the schematic
and the disks are at https://github.com/mildstorm/TheCPMCard

. No firmware ROM: the 6502 loads the system from disk.
. Every write to $Cnxx toggles bus ownership (U22), from the 6502 or from the
  Z80 through DMA; the 6502 is held while the Z80 has the bus. AppleWin runs
  one CPU at a time (SetActiveCpu), as for the Z-80 SoftCard. On the card the
  Z80 only waits (/WAIT) when it reaches for Apple memory, so it could run on
  from its own RAM while the 6502 has the bus; CP/M Plus 3.01C never does
  (it gives the bus back from Apple memory, and the Z80 comes out of reset
  fetching Apple $9000), so not running it then changes nothing it can see.
. Bank latch (U27): D0 of every Z80 OUT, port not decoded. Bank 0 (reset):
  Z80 $0000-$EFFF is Apple memory through the U26 page PROM; bank 1: card
  DRAM. $F000-$FFFF is card DRAM in both. An IN returns D0 = Apple /IRQ, the
  rest pulled high.
. U26 is not dumped: Apple = Z80 + $9000 with the $Cxxx and $Fxxx pages
  swapped, which is what the 3.01C BIOS uses. The PROM sees A15 inverted.
. Real-time clock: Apple PHI1 / 2^14 (U1, ~62.3 Hz) sets a tick; /INT is
  tick OR Apple /IRQ; the acknowledge clears the tick and puts $F7 on the
  bus (RST 30h in IM 0). The BIOS clock handler counts 60 ticks a second.
. Clock: 12 MHz / 2 = 6 MHz in bank 1, every opcode fetch half a T-state
  longer (U6); in bank 0 the divider only runs while PHI0 is high: 3 MHz.

The boot loader finds the card by writing $Cn00 for slots 7 down to 1, and so
does Microsoft's CP/M for the Z-80 SoftCard: the two cannot share a machine.
*/

#include "StdAfx.h"

#include "ALSCPMCard.h"
#include "CPU.h"
#include "Memory.h"
#include "YamlHelper.h"
#include "Z80VICE/z80.h"

ALSCPMCard* ALSCPMCard::s_card = NULL;

static const double kZ80ClockHz = 12000000.0 / 2.0;	// Y1 12 MHz, U22 divides by two
static const UINT kTickDivider = 1 << 14;			// U1 4020 Q14
static const BYTE kIntaVector = 0xF7;				// U13 pulls D3 low: RST 30h

ALSCPMCard::ALSCPMCard(UINT slot) :
	Card(CT_ALSCPM, slot),
	m_bank(0),
	m_tickAcked(0),
	m_now(0)
{
	memset(m_ram, 0, sizeof(m_ram));

	// U26: Apple page = Z80 page + $90, the $Cx and $Fx pages swapped; the
	// index has bit 7 flipped, as the PROM sees A15 through an inverter.
	for (UINT page = 0; page < 0x100; page++)
	{
		BYTE apple = (BYTE)(page + 0x90);
		if ((apple & 0xF0) == 0xC0)
			apple = (apple & 0x0F) | 0xF0;
		else if ((apple & 0xF0) == 0xF0)
			apple = (apple & 0x0F) | 0xC0;
		m_prom[page ^ 0x80] = apple;
	}

	static Z80Board board;
	board.read = Z80Read;
	board.write = Z80Write;
	board.in = Z80In;
	board.out = Z80Out;
	board.intLine = Z80IntLine;
	board.intAck = Z80IntAck;
	board.halfSpeed = Z80HalfSpeed;
	board.m1Stretch = Z80M1Stretch;
	board.tStatesPer6502Cycle = kZ80ClockHz / CLK_6502_NTSC;

	s_card = this;
	Z80_SetBoard(&board);
}

ALSCPMCard::~ALSCPMCard()
{
	if (s_card == this)
	{
		s_card = NULL;
		Z80_SetBoard(NULL);	// back to the SoftCard's map for the shared Z80
	}
}

void ALSCPMCard::Reset(const bool powerCycle)
{
	// Apple /RESET resets the Z80 (CpuReset) and clears U22 and the bank
	// latch; the DRAM keeps its contents unless the power went.
	m_bank = 0;
	m_tickAcked = g_nCumulativeCycles / kTickDivider;
	if (powerCycle)
		memset(m_ram, 0, sizeof(m_ram));
}

void ALSCPMCard::InitializeIO(LPBYTE pCxRomPeripheral)
{
	// No ROM: $Cnxx reads float.
	memset(pCxRomPeripheral + (m_slot << 8), 0xFF, APPLE_SLOT_SIZE);
	RegisterIoHandler(m_slot, IO_Null, IO_Null, IO_Null, &ALSCPMCard::IOWrite, this, NULL);
}

BYTE __stdcall ALSCPMCard::IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles)
{
	const UINT slot = (addr >> 8) & 0x7;

	// Any write to $Cnxx: U22 toggles, the 6502 or the Z80 gets the bus.
	if ((addr & 0xFF00) == (0xC000 + (slot << 8)))
		SetActiveCpu(GetActiveCpu() == CPU_Z80 ? GetMainCpu() : CPU_Z80);

	return IO_Null(pc, addr, bWrite, value, nExecutedCycles);
}

//===========================================================================

BYTE ALSCPMCard::Z80Read(WORD addr, ULONG cycles)
{
	ALSCPMCard& card = *s_card;
	if (card.IsLocal(addr))
		return card.m_ram[addr];

	const WORD apple = card.ToApple(addr);
	if ((apple & 0xF000) == 0xC000)
		return IORead[(apple >> 4) & 0xFF](regs.pc, apple, 0, 0, cycles);
	return CpuRead(apple, cycles);
}

void ALSCPMCard::Z80Write(WORD addr, BYTE value, ULONG cycles)
{
	ALSCPMCard& card = *s_card;
	if (card.IsLocal(addr))
	{
		card.m_ram[addr] = value;
		return;
	}

	// A write to this card's own $Cnxx is decoded by the Apple as /IOSEL and
	// comes back as IOWrite, which hands the bus back.
	CpuWrite(card.ToApple(addr), value, cycles);
}

BYTE ALSCPMCard::Z80In(WORD port)
{
	// Any port: U13 puts Apple /IRQ on D0, RP3 pulls the rest high.
	return IsIrqAsserted() ? 0xFE : 0xFF;
}

void ALSCPMCard::Z80Out(WORD port, BYTE value)
{
	// Any port: U27 latches D0.
	s_card->m_bank = value & 0x01;
}

bool ALSCPMCard::Z80IntLine(ULONG cycles)
{
	ALSCPMCard& card = *s_card;
	card.m_now = g_nCumulativeCycles + cycles;
	// U27 is set on each rising edge of U1's Q14 and cleared by the acknowledge.
	const bool tick = card.m_now / kTickDivider > card.m_tickAcked;
	return tick || IsIrqAsserted();
}

BYTE ALSCPMCard::Z80IntAck()
{
	ALSCPMCard& card = *s_card;
	card.m_tickAcked = card.m_now / kTickDivider;
	return kIntaVector;
}

bool ALSCPMCard::Z80HalfSpeed()
{
	return s_card->m_bank == 0;
}

bool ALSCPMCard::Z80M1Stretch()
{
	return s_card->m_bank != 0;
}

//===========================================================================

#define SS_YAML_KEY_ACTIVE "Active"
#define SS_YAML_KEY_BANK "Bank"
#define SS_YAML_KEY_TICK_ACKED "Tick Acknowledged"
#define SS_YAML_KEY_REGISTERS "Z80"
#define SS_YAML_KEY_MEMORY "Memory"

const std::string& ALSCPMCard::GetSnapshotCardName()
{
	static const std::string name("ALS CP/M Card");
	return name;
}

void ALSCPMCard::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Slot slot(yamlSaveHelper, GetSnapshotCardName(), m_slot, 1);
	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);

	yamlSaveHelper.Save("%s: %d\n", SS_YAML_KEY_ACTIVE, GetActiveCpu() == CPU_Z80 ? 1 : 0);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_BANK, m_bank);
	yamlSaveHelper.SaveHexUint64(SS_YAML_KEY_TICK_ACKED, m_tickAcked);
	{
		YamlSaveHelper::Label regs(yamlSaveHelper, "%s:\n", SS_YAML_KEY_REGISTERS);
		Z80_SaveRegisters(yamlSaveHelper);
	}
	{
		YamlSaveHelper::Label mem(yamlSaveHelper, "%s:\n", SS_YAML_KEY_MEMORY);
		yamlSaveHelper.SaveMemory(m_ram, sizeof(m_ram));
	}
}

bool ALSCPMCard::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (version != 1)
		Card::ThrowErrorInvalidVersion(CT_ALSCPM, version);

	m_bank = yamlLoadHelper.LoadUint(SS_YAML_KEY_BANK) & 0x01;
	m_tickAcked = yamlLoadHelper.LoadUint64(SS_YAML_KEY_TICK_ACKED);

	if (!yamlLoadHelper.GetSubMap(SS_YAML_KEY_REGISTERS))
		throw std::runtime_error("Card: Expected key: " SS_YAML_KEY_REGISTERS);
	Z80_LoadRegisters(yamlLoadHelper);
	yamlLoadHelper.PopMap();

	if (!yamlLoadHelper.GetSubMap(SS_YAML_KEY_MEMORY))
		throw std::runtime_error("Card: Expected key: " SS_YAML_KEY_MEMORY);
	yamlLoadHelper.LoadMemory(m_ram, sizeof(m_ram));
	yamlLoadHelper.PopMap();

	if (yamlLoadHelper.LoadUint(SS_YAML_KEY_ACTIVE))
		SetActiveCpu(CPU_Z80);

	return true;
}
