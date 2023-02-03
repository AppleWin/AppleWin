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
	bool IsActive(void);
	void SetVolume(DWORD dwVolume, DWORD dwVolumeMax);
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

	void GetSnapshot_v1(struct SS_CARD_MOCKINGBOARD_v1* const pSS);

	static std::string GetSnapshotCardName(void);
	static std::string GetSnapshotCardNamePhasor(void);

private:
	enum MockingboardUnitState_e { AY_NOP0, AY_NOP1, AY_INACTIVE, AY_READ, AY_NOP4, AY_NOP5, AY_WRITE, AY_LATCH };

	struct MB_SUBUNIT
	{
		SY6522 sy6522;
		AY8913 ay8913[2];				// Phasor has 2x AY per 6522
		SSI263 ssi263;
		BYTE nAY8910Number;
		BYTE nAYCurrentRegister;
		MockingboardUnitState_e state;	// Where a unit is a 6522+AY8910 pair
		MockingboardUnitState_e stateB;	// Phasor: 6522 & 2nd AY8910

		MB_SUBUNIT(UINT slot) : sy6522(slot), ssi263(slot)
		{
			nAY8910Number = 0;
			nAYCurrentRegister = 0;
			state = AY_NOP0;
			stateB = AY_NOP0;
			// sy6522 & ssi263 have already been default constructed
		}
	};

	void WriteToORB(BYTE subunit);
	void AY8910_Write(BYTE subunit, BYTE ay, BYTE value);
	void UpdateIFRandIRQ(MB_SUBUNIT* pMB, BYTE clr_mask, BYTE set_mask);

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

	static const DWORD SAMPLE_RATE = 44100;	// Use a base freq so that DirectX (or sound h/w) doesn't have to up/down-sample

	short* m_ppAYVoiceBuffer[NUM_VOICES];

	UINT64 m_inActiveCycleCount;
	bool m_regAccessedFlag;
	bool m_isActive;

	//

	bool m_phasorEnable;
	PHASOR_MODE m_phasorMode;
	UINT m_phasorClockScaleFactor;

	//

	UINT64 m_lastMBUpdateCycle;
	int m_numSamplesError;
};
