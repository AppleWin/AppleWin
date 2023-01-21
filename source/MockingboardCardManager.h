#pragma once

#include "Core.h"
#include "SoundCore.h"

class MockingboardCardManager
{
public:
	MockingboardCardManager(void)
	{
		nNumSamplesError = 0;
		dwByteOffset = (DWORD)-1;
		g_cyclesThisAudioFrame = 0;
		m_userVolume = 0;

		// NB. Cmd line has already been processed
		LogFileOutput("MBCardMgr::ctor() g_bDisableDirectSound=%d, g_bDisableDirectSoundMockingboard=%d\n", g_bDisableDirectSound, g_bDisableDirectSoundMockingboard);
	}
	~MockingboardCardManager(void)
	{}

	bool IsMockingboard(UINT slot);
	void ReinitializeClock(void);
	void InitializeForLoadingSnapshot(void);
	void MuteControl(bool mute);
	void SetCumulativeCycles(void);
	void UpdateCycles(ULONG executedCycles);
	bool IsActive(void);
	DWORD GetVolume(void);
	void SetVolume(DWORD volume, DWORD volumeMax);

	void Destroy(void);
	void Reset(const bool powerCycle)
	{
		g_cyclesThisAudioFrame = 0;
	}
	void Update(const ULONG executedCycles);
	void UpdateSoundBuffer(void);

#ifdef _DEBUG
	void CheckCumulativeCycles(void);
	void Get6522IrqDescription(std::string& desc);
#endif

private:
	bool Init(void);
	UINT GenerateAllSoundData(void);
	void MixAllAndCopyToRingBuffer(UINT nNumSamples);

	static const UINT NUM_SY6522 = 2;
	static const UINT NUM_AY8913 = 4;	// Phasor has 4, MB has 2
//	static const UINT NUM_SSI263 = 2;
	static const UINT NUM_DEVS_PER_MB = NUM_SY6522;
	static const UINT NUM_VOICES_PER_AY8913 = 3;
	static const UINT NUM_VOICES = (NUM_AY8913 * NUM_VOICES_PER_AY8913);

	static const unsigned short g_nMB_NumChannels = 2;
	static const DWORD g_dwDSBufferSize = MAX_SAMPLES * sizeof(short) * g_nMB_NumChannels;

	static const SHORT nWaveDataMin = (SHORT)0x8000;
	static const SHORT nWaveDataMax = (SHORT)0x7FFF;

	short g_nMixBuffer[g_dwDSBufferSize / sizeof(short)];
	VOICE MockingboardVoice;

	//

	int nNumSamplesError;
	DWORD dwByteOffset;

	UINT g_cyclesThisAudioFrame;

	DWORD m_userVolume;	// GUI's slide volume
};
