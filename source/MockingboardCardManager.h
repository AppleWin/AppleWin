#pragma once

#include "Core.h"
#include "SoundCore.h"
#include "Mockingboard.h"

class MockingboardCardManager
{
public:
	MockingboardCardManager()
	{
		m_numSamplesError = 0;
		m_byteOffset = (uint32_t)-1;
		m_cyclesThisAudioFrame = 0;
		m_userVolume = 0;
		m_outputToRiff = false;
		m_enableExtraCardTypes = false;

		// NB. Cmd line has already been processed
		LogFileOutput("MBCardMgr::ctor() g_bDisableDirectSound=%d, g_bDisableDirectSoundMockingboard=%d\n", g_bDisableDirectSound, g_bDisableDirectSoundMockingboard);
	}
	~MockingboardCardManager()
	{}

	bool IsMockingboard(UINT slot);
	void ReinitializeClock();
	void InitializeForLoadingSnapshot();
	void MuteControl(bool mute);
	void SetCumulativeCycles();
	void UpdateCycles(ULONG executedCycles);
	void UpdateIRQ();
	bool IsActiveToPreventFullSpeed();
	uint32_t GetVolume();
	void SetVolume(uint32_t volume, uint32_t volumeMax);
	void OutputToRiff() { m_outputToRiff = true; }
	void SetEnableExtraCardTypes(bool enable) { m_enableExtraCardTypes = enable; }
	bool GetEnableExtraCardTypes();

	void Destroy();
	void Reset(const bool powerCycle)
	{
		m_cyclesThisAudioFrame = 0;
	}
	void Update(const ULONG executedCycles);
	void UpdateSoundBuffer();

#ifdef _DEBUG
	void CheckCumulativeCycles();
	void Get6522IrqDescription(std::string& desc);
#endif

private:
	bool Init();
	UINT GenerateAllSoundData();
	void MixAllAndCopyToRingBuffer(UINT nNumSamples);
	bool IsMockingboardExtraCardType(UINT slot);

	static const uint32_t SOUNDBUFFER_SIZE = MAX_SAMPLES * sizeof(short) * MockingboardCard::NUM_MB_CHANNELS;

	static const SHORT WAVE_DATA_MIN = (SHORT)0x8000;
	static const SHORT WAVE_DATA_MAX = (SHORT)0x7FFF;

	short m_mixBuffer[SOUNDBUFFER_SIZE / sizeof(short)];
	VOICE m_mockingboardVoice;

	//

	int m_numSamplesError;
	uint32_t m_byteOffset;
	UINT m_cyclesThisAudioFrame;
	uint32_t m_userVolume;	// GUI's slide volume
	bool m_outputToRiff;
	bool m_enableExtraCardTypes;
};
