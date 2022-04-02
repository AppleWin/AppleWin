#pragma once

#include "SoundCore.h"

class MockingboardCardManager
{
public:
	MockingboardCardManager(void)
	{
		m_userVolume = 0;
		nNumSamplesError = 0;
		dwByteOffset = (DWORD)-1;
	}
	~MockingboardCardManager(void)
	{}

	bool IsMockingboard(UINT slot);
	void InitializePostWindowCreate(void);
	void ReinitializeClock(void);
	void InitializeForLoadingSnapshot(void);
	void MuteControl(bool mute);
	void SetCumulativeCycles(void);
	void UpdateCycles(ULONG executedCycles);
	bool IsActive(void);
	DWORD GetVolume(void);
	void SetVolume(DWORD volume, DWORD volumeMax);

	void Destroy(void);
	void Update(void);

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

	DWORD m_userVolume;	// GUI's slide volume
};
