#pragma once

#include "Card.h"

// Advanced Logic Systems "The CP/M Card": a 6 MHz Z80B with its own 64K of
// DRAM, sold with CP/M Plus 3.01. See ALSCPMCard.cpp.
class ALSCPMCard : public Card
{
public:
	ALSCPMCard(UINT slot);
	virtual ~ALSCPMCard();

	virtual void Destroy() {}
	virtual void Reset(const bool powerCycle);
	virtual void Update(const ULONG nExecutedCycles) {}

	virtual void InitializeIO(LPBYTE pCxRomPeripheral);

	static BYTE __stdcall IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);

	static const std::string& GetSnapshotCardName();
	virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
	virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

private:
	// The Z80's board (see Z80Board in Z80VICE/z80.h). There is one Z80, so
	// these reach the card through s_card.
	static BYTE Z80Read(WORD addr, ULONG cycles);
	static void Z80Write(WORD addr, BYTE value, ULONG cycles);
	static BYTE Z80In(WORD port);
	static void Z80Out(WORD port, BYTE value);
	static bool Z80IntLine(ULONG cycles);
	static BYTE Z80IntAck();
	static bool Z80HalfSpeed();
	static bool Z80M1Stretch();

	bool IsLocal(WORD addr) const { return m_bank != 0 || addr >= 0xF000; }
	WORD ToApple(WORD addr) const { return (WORD)((m_prom[(addr >> 8) ^ 0x80] << 8) | (addr & 0xFF)); }

	static ALSCPMCard* s_card;

	BYTE m_ram[0x10000];	// card DRAM (U2-U5, U7-U10)
	BYTE m_prom[0x100];		// U26, the half its pin 19 selects
	BYTE m_bank;			// U27 bank latch
	UINT64 m_tickAcked;		// U1 periods: the tick flip-flop was last cleared in this one
	UINT64 m_now;			// 6502 cycle of the last /INT sample
};
