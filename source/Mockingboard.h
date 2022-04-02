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
	MockingboardCard(UINT slot, SS_CARDTYPE type) : Card(type, slot)
	{
		g_uLastCumulativeCycles = 0;
		g_uLastCumulativeCycles2 = 0;	// TODO: Is this needed?

		for (UINT i = 0; i < NUM_VOICES; i++)
			ppAYVoiceBuffer[NUM_VOICES] = NULL;

		// Construct via placement new, so that it is an array of 'SY6522_AY8910' objects
		g_MB = (SY6522_AY8910*) new BYTE[sizeof(SY6522_AY8910) * NUM_SY6522];
		for (UINT i=0; i< NUM_SY6522; i++)
			new (&g_MB[i]) SY6522_AY8910(m_slot);

		g_nMB_InActiveCycleCount = 0;
		g_bMB_RegAccessedFlag = false;
		g_bMB_Active = false;

		g_bPhasorEnable = (QueryType() == CT_Phasor);
		g_phasorMode = PH_Mockingboard;
		g_PhasorClockScaleFactor = 1;	// for save-state only

		g_cyclesThisAudioFrame = 0;

		g_uLastMBUpdateCycle = 0;
		nNumSamplesError = 0;

		MB_Initialize();
	}

	virtual ~MockingboardCard(void)
	{
		for (UINT i = 0; i < NUM_SY6522; i++)
			g_MB[i].~SY6522_AY8910();
		delete[] (BYTE*) g_MB;
	}

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

	void InitializeForLoadingSnapshot(void);
	void ReinitializeClock(void);
	void MuteControl(bool mute);
	void UpdateCycles(ULONG executedCycles);
	bool IsActive(void);
//	DWORD GetVolume(void);	// Now in MockingboardCardManager
	void SetVolume(DWORD dwVolume, DWORD dwVolumeMax);
	void SetCumulativeCycles(void);
	bool MB_DSInit(void);
	UINT MB_UpdateInternal1(void);
	short** GetVoiceBuffers(void) { return ppAYVoiceBuffer; }
	int GetNumSamplesError(void) { return nNumSamplesError; }
	void SetNumSamplesError(int numSamplesError) { nNumSamplesError = numSamplesError; }
#ifdef _DEBUG
	void CheckCumulativeCycles(void);
	void Get6522IrqDescription(std::string& desc);
#endif

	void UpdateIRQ(void);
	UINT64 GetLastCumulativeCycles(void);
	void UpdateIFR(BYTE nDevice, BYTE clr_mask, BYTE set_mask);
	BYTE GetPCR(BYTE nDevice);

	void GetSnapshot_v1(struct SS_CARD_MOCKINGBOARD_v1* const pSS);

	static std::string GetSnapshotCardName(void);
	static std::string GetSnapshotCardNamePhasor(void);

private:
	enum MockingboardUnitState_e { AY_NOP0, AY_NOP1, AY_INACTIVE, AY_READ, AY_NOP4, AY_NOP5, AY_WRITE, AY_LATCH };

	struct SY6522_AY8910
	{
		SY6522 sy6522;
		AY8913 ay8913[2];				// Phasor has 2x AY per 6522
		SSI263 ssi263;
		BYTE nAY8910Number;
		BYTE nAYCurrentRegister;
		MockingboardUnitState_e state;	// Where a unit is a 6522+AY8910 pair
		MockingboardUnitState_e stateB;	// Phasor: 6522 & 2nd AY8910

		SY6522_AY8910(UINT slot) : sy6522(slot), ssi263(slot)
		{
			nAY8910Number = 0;
			nAYCurrentRegister = 0;
			state = AY_NOP0;
			stateB = AY_NOP0;
			// sy6522 & ssi263 have already been default constructed
		}
	};

	void MB_Initialize(void);
	bool IsAnyTimer1Active(void);
	void AY8910_Write(BYTE nDevice, BYTE nValue, BYTE nAYDevice);
	void WriteToORB(BYTE device);
	void UpdateIFRandIRQ(SY6522_AY8910* pMB, BYTE clr_mask, BYTE set_mask);
	void MB_UpdateInternal(void);
	void MB_Update(void);
	void InitSoundcardType(void);

	void Phasor_SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
	bool Phasor_LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

	static int MB_SyncEventCallback(int id, int /*cycles*/, ULONG uExecutedCycles);
	int MB_SyncEventCallbackInternal(int id, int /*cycles*/, ULONG uExecutedCycles);

	//-------------------------------------
	// MAME interface
	BYTE AYReadReg(int chip, int r);
	void _AYWriteReg(int chip, int r, int v);
	void AY8910_reset(int chip);
	void AY8910Update(int chip, INT16** buffer, int nNumSamples);

	void AY8910_InitAll(int nClock, int nSampleRate);
	void AY8910_InitClock(int nClock);
	BYTE* AY8910_GetRegsPtr(UINT uChip);

	void AY8910UpdateSetCycles();

	UINT AY8910_SaveSnapshot(class YamlSaveHelper& yamlSaveHelper, UINT uChip, const std::string& suffix);
	UINT AY8910_LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT uChip, const std::string& suffix);

	UINT64 g_uLastCumulativeCycles2;	// TODO: Is this needed?
	//-------------------------------------

	static const UINT SY6522_DEVICE_A = 0;
	static const UINT SY6522_DEVICE_B = 1;

	static const UINT NUM_SY6522 = 2;
	static const UINT NUM_AY8913 = 4;	// Phasor has 4, MB has 2
	static const UINT NUM_SSI263 = 2;
	static const UINT NUM_DEVS_PER_MB = NUM_SY6522;
	static const UINT NUM_VOICES_PER_AY8913 = 3;
	static const UINT NUM_VOICES = (NUM_AY8913 * NUM_VOICES_PER_AY8913);

	// Chip offsets from card base:
	static const UINT SY6522A_Offset = 0x00;
	static const UINT SY6522B_Offset = 0x80;
	static const UINT SSI263B_Offset = 0x20;
	static const UINT SSI263A_Offset = 0x40;

	// MB has 2x (1x SY6522 + 1x AY8913), Phasor has 2x (1x SY6522 + 2x AY8913)
	SY6522_AY8910* g_MB;	// NB. In ctor this becomes g_MB[NUM_SY6522]

	static const UINT kNumSyncEvents = NUM_SY6522 * SY6522::kNumTimersPer6522;
	SyncEvent* g_syncEvent[kNumSyncEvents];

	// Timer vars
	static const UINT kTIMERDEVICE_INVALID = -1;
	UINT64 g_uLastCumulativeCycles;

	static const DWORD SAMPLE_RATE = 44100;	// Use a base freq so that DirectX (or sound h/w) doesn't have to up/down-sample

	short* ppAYVoiceBuffer[NUM_VOICES];

	UINT64 g_nMB_InActiveCycleCount;
	bool g_bMB_RegAccessedFlag;
	bool g_bMB_Active;

	//

	bool g_bPhasorEnable;
	PHASOR_MODE g_phasorMode;
	UINT g_PhasorClockScaleFactor;

	//

//	static const unsigned short g_nMB_NumChannels = 2;
//	static const DWORD g_dwDSBufferSize = MAX_SAMPLES * sizeof(short) * g_nMB_NumChannels;
//
//	static const SHORT nWaveDataMin = (SHORT)0x8000;
//	static const SHORT nWaveDataMax = (SHORT)0x7FFF;
//
//	short g_nMixBuffer[g_dwDSBufferSize / sizeof(short)];
//	VOICE MockingboardVoice;

	UINT g_cyclesThisAudioFrame;

	UINT64 g_uLastMBUpdateCycle;
	int nNumSamplesError;
};

#if 0
class PhasorCard : public MockingboardCard
{
public:
	PhasorCard(UINT slot) :
		MockingboardCard(slot)
	{
		// TODO: set Card's type = CT_Phasor
		// or maybe PhasorCard HAS_A MockingboardCard ?
	}
	virtual ~PhasorCard(void);

private:

};
#endif
