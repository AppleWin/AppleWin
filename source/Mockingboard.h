#pragma once

#include "6522.h"
#include "AY8910.h"
#include "Card.h"
#include "SoundCore.h"
#include "SSI263.h"
#include "SynchronousEventManager.h"

class MockingboardCard : public Card
{
public:
	MockingboardCard(UINT slot, SS_CARDTYPE type);
	virtual ~MockingboardCard(void);

	virtual void InitializeIO(LPBYTE pCxRomPeripheral);
	virtual void Destroy();
	virtual void Reset(const bool powerCycle);
	virtual void Update(const ULONG executedCycles);
	virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
	virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

	static BYTE __stdcall IORead(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
	static BYTE __stdcall IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
	static BYTE __stdcall PhasorIO(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nExecutedCycles);

	BYTE IOReadInternal(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nExecutedCycles);
	BYTE IOWriteInternal(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
	BYTE PhasorIOInternal(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nExecutedCycles);

	void ReinitializeClock(void);
	void MuteControl(bool mute);
	void UpdateCycles(ULONG executedCycles);
	bool IsActiveToPreventFullSpeed(void);
	void SetVolume(uint32_t dwVolume, uint32_t dwVolumeMax);
	void SetCumulativeCycles(void);
	UINT MB_Update(void);
	short** GetVoiceBuffers(void) { return m_ppAYVoiceBuffer; }
	int GetNumSamplesError(void) { return m_numSamplesError; }
	void SetNumSamplesError(int numSamplesError) { m_numSamplesError = numSamplesError; }
#ifdef _DEBUG
	void CheckCumulativeCycles(void);
	void Get6522IrqDescription(std::string& desc);
#endif

	bool Is6522IRQ(void);
	UINT64 GetLastCumulativeCycles(void);
	void UpdateIFR(BYTE nDevice, BYTE clr_mask, BYTE set_mask);
	BYTE GetPCR(BYTE nDevice);
	bool IsAnyTimer1Active(void);
	void UseBad6522A(void) { m_MBSubUnit[0].sy6522.InitBadState(true); }
	void UseBad6522B(void) { m_MBSubUnit[1].sy6522.InitBadState(true); }
	void SetSocketSSI263(BYTE socket, SSI263Type type) { m_MBSubUnit[socket].ssi263.SetType(type); }
	void SetSocketSC01(SSI263Type type) { m_MBSubUnit[0].ssi263.SetSC01(type); }

	struct DEBUGGER_MB_SUBUNIT
	{
		BYTE regsSY6522[SY6522::SIZE_6522_REGS];
		bool timer1Active;
		bool timer2Active;
		BYTE regsAY8913[NUM_AY8913_PER_SUBUNIT][16];
		BYTE nAYCurrentRegister[NUM_AY8913_PER_SUBUNIT];
		char szState[NUM_AY8913_PER_SUBUNIT][3];	// "--"(INACTIVE), "RD", "WR", "LA"
		bool isAYLatchedAddressValid[NUM_AY8913_PER_SUBUNIT];
		bool is6522Bad;
	};
	struct DEBUGGER_MB_CARD
	{
		SS_CARDTYPE type;
		DEBUGGER_MB_SUBUNIT subUnit[NUM_SUBUNITS_PER_MB];
	};
	void GetSnapshotForDebugger(DEBUGGER_MB_CARD* const pMBForDebugger);

	static std::string GetSnapshotCardName(void);
	static std::string GetSnapshotCardNamePhasor(void);
	static std::string GetSnapshotCardNameMegaAudio(void);
	static std::string GetSnapshotCardNameSDMusic(void);

	static const unsigned short NUM_MB_CHANNELS = 2;
	static const uint32_t SAMPLE_RATE = 44100;	// Use a base freq so that DirectX (or sound h/w) doesn't have to up/down-sample

private:
	enum MockingboardUnitState_e { AY_NOP0, AY_NOP1, AY_INACTIVE, AY_READ, AY_NOP4, AY_NOP5, AY_WRITE, AY_LATCH };

	struct MB_SUBUNIT
	{
		SY6522 sy6522;
		AY8913 ay8913[NUM_AY8913_PER_SUBUNIT];					// Phasor has 2x AY per 6522
		SSI263 ssi263;
		BYTE nAY8910Number;
		BYTE nAYCurrentRegister[NUM_AY8913_PER_SUBUNIT];
		MockingboardUnitState_e state[NUM_AY8913_PER_SUBUNIT];	// AY's PSG function
		bool isAYLatchedAddressValid[NUM_AY8913_PER_SUBUNIT];
		bool isChipSelected[NUM_AY8913_PER_SUBUNIT];
		bool isBusDriven;

		MB_SUBUNIT(UINT slot, SS_CARDTYPE type) : sy6522(slot, type == CT_MegaAudio), ssi263(slot)
		{
			nAY8910Number = 0;
			// sy6522 & ssi263 have already been default constructed
			// Reset() called from MockingboardCard ctor
		}

		void Reset(SS_CARDTYPE type)
		{
			nAYCurrentRegister[0] = nAYCurrentRegister[1] = 0;	// not valid
			state[0] = state[1] = AY_INACTIVE;
			isAYLatchedAddressValid[0] = isAYLatchedAddressValid[1] = false;	// after AY reset
			isChipSelected[0] = type == CT_Phasor ? false : true;	// Only Phasor is false, all other MB variants are true
			isChipSelected[1] = false;
			SetBusState(false);
		}

		void SetBusState(bool state)
		{
			isBusDriven = state;
			sy6522.SetBusBeingDriven(state);
		}
	};

	void WriteToORB(BYTE subunit, BYTE subunitForAY=0);
	void AY8913_Reset(BYTE subunit);
	void AY8913_Write(BYTE subunit, BYTE ay, BYTE value);
	void UpdateIFRandIRQ(MB_SUBUNIT* pMB, BYTE clr_mask, BYTE set_mask);
	void SetPhasorMode(PHASOR_MODE newMode);

	void Phasor_SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
	bool Phasor_LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

	static int MB_SyncEventCallback(int id, int /*cycles*/, ULONG uExecutedCycles);
	int MB_SyncEventCallbackInternal(int id, int /*cycles*/, ULONG uExecutedCycles);

	//-------------------------------------
	// MAME interface
	BYTE AYReadReg(BYTE subunit, BYTE ay, int r);
	void _AYWriteReg(BYTE subunit, BYTE ay, int r, int v);
	void AY8910_reset(BYTE subunit, BYTE ay);
	void AY8910Update(BYTE subunit, BYTE ay, INT16** buffer, int nNumSamples);

	void AY8910_InitAll(int nClock, int nSampleRate);
	void AY8910_InitClock(int nClock);
	BYTE* AY8910_GetRegsPtr(BYTE subunit, BYTE ay);

	void AY8910UpdateSetCycles();

	UINT AY8910_SaveSnapshot(class YamlSaveHelper& yamlSaveHelper, BYTE subunit, BYTE ay, const std::string& suffix);
	UINT AY8910_LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, BYTE subunit, BYTE ay, const std::string& suffix);

	UINT64 m_lastAYUpdateCycle;
	//-------------------------------------

	static const UINT SY6522_DEVICE_A = 0;
	static const UINT SY6522_DEVICE_B = 1;

	static const UINT AY8913_DEVICE_A = 0;
	static const UINT AY8913_DEVICE_B = 1;	// Phasor only

	// Chip offsets from card base:
	static const UINT SY6522A_Offset = 0x00;
	static const UINT SY6522B_Offset = 0x80;
	static const UINT SSI263B_Offset = 0x20;
	static const UINT SSI263A_Offset = 0x40;

	// MB has 2x (1x SY6522 + 1x AY8913), Phasor has 2x (1x SY6522 + 2x AY8913)
	MB_SUBUNIT m_MBSubUnit[NUM_SUBUNITS_PER_MB];

	static const UINT kNumSyncEvents = NUM_SY6522 * SY6522::kNumTimersPer6522;
	SyncEvent* m_syncEvent[kNumSyncEvents];

	UINT64 m_lastCumulativeCycle;

	short* m_ppAYVoiceBuffer[NUM_VOICES];

	UINT64 m_inActiveCycleCount;
	bool m_regAccessedFlag;
	bool m_isActive;

	//

	bool m_isPhasorCard;
	PHASOR_MODE m_phasorMode;
	UINT m_phasorClockScaleFactor;

	//

	UINT64 m_lastMBUpdateCycle;
	int m_numSamplesError;
};
