#pragma once

#include "Core.h"
#include "SoundCore.h"

class MockingboardCardManager
{
public:
	MockingboardCardManager(void)
	{
		m_numSamplesError = 0;
		m_byteOffset = (DWORD)-1;
		m_cyclesThisAudioFrame = 0;
		m_userVolume = 0;
		m_outputToRiff = false;

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
	void UpdateIRQ(void);
	bool IsActive(void);
	DWORD GetVolume(void);
	void SetVolume(DWORD volume, DWORD volumeMax);
	void OutputToRiff(void) { m_outputToRiff = true; }

	void Destroy(void);
	void Reset(const bool powerCycle)
	{
		m_cyclesThisAudioFrame = 0;
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

	static const unsigned short NUM_MB_CHANNELS = 2;
	static const DWORD SOUNDBUFFER_SIZE = MAX_SAMPLES * sizeof(short) * NUM_MB_CHANNELS;

	static const SHORT WAVE_DATA_MIN = (SHORT)0x8000;
	static const SHORT WAVE_DATA_MAX = (SHORT)0x7FFF;

	short m_mixBuffer[SOUNDBUFFER_SIZE / sizeof(short)];
	VOICE m_mockingboardVoice;

	//

	int m_numSamplesError;
	DWORD m_byteOffset;
	UINT m_cyclesThisAudioFrame;
	DWORD m_userVolume;	// GUI's slide volume
	bool m_outputToRiff;
};
