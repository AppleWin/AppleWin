#pragma once

class TMS5220
{
public:
	TMS5220(UINT slot) : m_slot(slot)
	{
		ResetState(true);
	}
	~TMS5220() {}

	void ResetState(const bool powerCycle)
	{
		m_cyclesThisAudioFrame = 0;

		//

		m_lastUpdateCycle = 0;
		m_updateWasFullSpeed = false;

		//

		m_numSamplesError = 0;
		m_byteOffset = (uint32_t)-1;
	}

	void DSUninit();

	void Reset(const bool powerCycle);

	void Mute();
	void Unmute();
	void SetVolume(uint32_t dwVolume, uint32_t dwVolumeMax);

	void PeriodicUpdate(UINT executedCycles);
	void Update();

	uint8_t GetStatus();
	void Write(uint8_t data);

private:
	void Play();
	void Stop();

	UINT64 GetLastCumulativeCycles();

	bool Init();
	bool DSInit();

	static const unsigned short m_kNumChannels = 1;
	static const uint32_t m_kDSBufferByteSize = MAX_SAMPLES * sizeof(short) * m_kNumChannels;
	short m_mixBuffer[m_kDSBufferByteSize / sizeof(short)];
	VOICE m_voice;

	//

	UINT m_slot;

	UINT m_cyclesThisAudioFrame;

	//

	UINT64 m_lastUpdateCycle;
	bool m_updateWasFullSpeed;

	//

	int m_numSamplesError;
	uint32_t m_byteOffset;
};
