#pragma once

#include "6522.h"
#include "Card.h"
#include "SoundCore.h"
#include "SSI263.h"

class MockingboardCard : public Card
{
public:
	MockingboardCard(UINT slot) :
		Card(CT_MockingboardC, slot)
	{
		g_uLastCumulativeCycles = 0;

		for (UINT i = 0; i < NUM_VOICES; i++)
			ppAYVoiceBuffer[NUM_VOICES] = NULL;

		g_nMB_InActiveCycleCount = 0;
		g_bMB_RegAccessedFlag = false;
		g_bMB_Active = false;

		g_bMBAvailable = false;

		g_bPhasorEnable = false;
		g_phasorMode = PH_Mockingboard;
		g_PhasorClockScaleFactor = 1;	// for save-state only

		g_cyclesThisAudioFrame = 0;

		g_uLastMBUpdateCycle = 0;
	}
	virtual ~MockingboardCard(void);

	virtual void InitializeIO(LPBYTE pCxRomPeripheral);
	virtual void Init(void);
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

	void Get6522IrqDescription(std::string& desc);
	void MB_UpdateIRQ(void);
	void MB_Initialize(void);
	void MB_InitializeForLoadingSnapshot(void);
	void MB_Reinitialize(void);
	void MB_Destroy(void);
	void MB_Mute(void);
	void MB_Unmute(void);
#ifdef _DEBUG
	void MB_CheckCumulativeCycles(void);
#endif
	void MB_SetCumulativeCycles(void);
	void MB_PeriodicUpdate(UINT executedCycles);
	void MB_UpdateCycles(ULONG uExecutedCycles);
	bool MB_IsActive(void);
	DWORD MB_GetVolume(void);
	void MB_SetVolume(DWORD dwVolume, DWORD dwVolumeMax);

	UINT64 MB_GetLastCumulativeCycles(void);
	void MB_UpdateIFR(BYTE nDevice, BYTE clr_mask, BYTE set_mask);
	BYTE MB_GetPCR(BYTE nDevice);

	void MB_GetSnapshot_v1(struct SS_CARD_MOCKINGBOARD_v1* const pSS, const DWORD dwSlot);

	std::string MB_GetSnapshotCardName(void);
	std::string Phasor_GetSnapshotCardName(void);

private:
	enum MockingboardUnitState_e { AY_NOP0, AY_NOP1, AY_INACTIVE, AY_READ, AY_NOP4, AY_NOP5, AY_WRITE, AY_LATCH };

	struct SY6522_AY8910
	{
		SY6522 sy6522;
		SSI263 ssi263;
		BYTE nAY8910Number;
		BYTE nAYCurrentRegister;
		MockingboardUnitState_e state;	// Where a unit is a 6522+AY8910 pair
		MockingboardUnitState_e stateB;	// Phasor: 6522 & 2nd AY8910

		SY6522_AY8910(void)
		{
			nAY8910Number = 0;
			nAYCurrentRegister = 0;
			state = AY_NOP0;
			stateB = AY_NOP0;
			// r6522 & ssi263 have already been default constructed
		}
	};

	bool IsAnyTimer1Active(void);
	void AY8910_Write(BYTE nDevice, BYTE nValue, BYTE nAYDevice);
	void WriteToORB(BYTE device);
	void UpdateIFRandIRQ(SY6522_AY8910* pMB, BYTE clr_mask, BYTE set_mask);
	void MB_UpdateInternal(void);
	void MB_Update(void);
	bool MB_DSInit(void);
	void MB_DSUninit(void);
	void InitSoundcardType(void);

	void Phasor_SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
	bool Phasor_LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

	static int MB_SyncEventCallback(int id, int /*cycles*/, ULONG uExecutedCycles);
	int MB_SyncEventCallbackInternal(int id, int /*cycles*/, ULONG uExecutedCycles);

	static const UINT SY6522_DEVICE_A = 0;
	static const UINT SY6522_DEVICE_B = 1;

	static const UINT NUM_MB = 2;
	static const UINT NUM_DEVS_PER_MB = 2;
	static const UINT NUM_AY8910 = (NUM_MB * NUM_DEVS_PER_MB);
	static const UINT NUM_SY6522 = NUM_AY8910;
	static const UINT NUM_VOICES_PER_AY8910 = 3;
	static const UINT NUM_VOICES = (NUM_AY8910 * NUM_VOICES_PER_AY8910);


	// Chip offsets from card base.
	static const UINT SY6522A_Offset = 0x00;
	static const UINT SY6522B_Offset = 0x80;
	static const UINT SSI263B_Offset = 0x20;
	static const UINT SSI263A_Offset = 0x40;

	// Support 2 MB's, each with 2x SY6522/AY8910 pairs.
	SY6522_AY8910 g_MB[NUM_AY8910];	// FIXME - only 2 AYs in a MB-C

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

	bool g_bMBAvailable;

	//

	bool g_bPhasorEnable;
	PHASOR_MODE g_phasorMode;
	UINT g_PhasorClockScaleFactor;

	//-------------------------------------

	static const unsigned short g_nMB_NumChannels = 2;
	static const DWORD g_dwDSBufferSize = MAX_SAMPLES * sizeof(short) * g_nMB_NumChannels;

	static const SHORT nWaveDataMin = (SHORT)0x8000;
	static const SHORT nWaveDataMax = (SHORT)0x7FFF;

	short g_nMixBuffer[g_dwDSBufferSize / sizeof(short)];
	VOICE MockingboardVoice;

	UINT g_cyclesThisAudioFrame;

	UINT64 g_uLastMBUpdateCycle;
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

void	MB_Initialize();
void	MB_Reinitialize();
void	MB_Destroy();
void    MB_Reset(const bool powerCycle);
void	MB_InitializeForLoadingSnapshot(void);
//void    MB_InitializeIO(LPBYTE pCxRomPeripheral, UINT uSlot4, UINT uSlot5);	-> virtual InitializeIO()
void    MB_Mute();
void    MB_Unmute();
#ifdef _DEBUG
void    MB_CheckCumulativeCycles();	// DEBUG
#endif
void    MB_SetCumulativeCycles();
//void    MB_PeriodicUpdate(UINT executedCycles);	-> virtual Update()
void    MB_CheckIRQ();
void    MB_UpdateCycles(ULONG uExecutedCycles);
//SS_CARDTYPE MB_GetSoundcardType();	// removed call from PageSound.cpp
bool    MB_IsActive();
DWORD   MB_GetVolume();
void    MB_SetVolume(DWORD dwVolume, DWORD dwVolumeMax);
void MB_Get6522IrqDescription(std::string& desc);

void MB_UpdateIRQ(void);										// called by 6522
UINT64 MB_GetLastCumulativeCycles(void);						// called by SSI263
void MB_UpdateIFR(BYTE nDevice, BYTE clr_mask, BYTE set_mask);	// called by SSI263
BYTE MB_GetPCR(BYTE nDevice);									// called by SSI263

void    MB_GetSnapshot_v1(struct SS_CARD_MOCKINGBOARD_v1* const pSS, const DWORD dwSlot);	// For debugger
std::string MB_GetSnapshotCardName(void);
void    MB_SaveSnapshot(class YamlSaveHelper& yamlSaveHelper, const UINT uSlot);
bool    MB_LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version);

std::string Phasor_GetSnapshotCardName(void);
void Phasor_SaveSnapshot(class YamlSaveHelper& yamlSaveHelper, const UINT uSlot);
bool Phasor_LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version);
