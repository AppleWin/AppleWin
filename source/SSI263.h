#pragma once

#include "Mockingboard.h"	// enum PHASOR_MODE

class SSI263
{
public:
	SSI263(void)
	{
		m_device = -1;	// undefined
		m_cardMode = PH_Mockingboard;
		m_pPhonemeData00 = NULL;

		ResetState();
	}
	~SSI263(void)
	{
		delete [] m_pPhonemeData00;
	}

	void ResetState(void)
	{
		m_currentActivePhoneme = -1;
		m_isVotraxPhoneme = false;
		m_cyclesThisAudioFrame = 0;

		//

		m_lastUpdateCycle = 0;
		m_updateWasFullSpeed = false;

		m_pPhonemeData = NULL;
		m_phonemeLengthRemaining = 0;
		m_phonemeAccurateLengthRemaining = 0;
		m_phonemePlaybackAndDebugger = false;
		m_phonemeCompleteByFullSpeed = false;

		//

		m_numSamplesError = 0;
		m_byteOffset = (DWORD)-1;
		m_currSampleSum = 0;
		m_currNumSamples = 0;
		m_currSampleMod4 = 0;

		//

		m_durationPhoneme = 0;
		m_inflection = 0;
		m_rateInflection = 0;
		m_ctrlArtAmp = 0;
		m_filterFreq = 0;

		m_currentMode = 0;

		//

		m_dbgFirst = true;
		m_dbgStartTime = 0;
	}

	bool DSInit(void);
	void DSUninit(void);

	void Reset(void);
	bool IsPhonemeActive(void) { return m_currentActivePhoneme >= 0; }

	BYTE Read(ULONG nExecutedCycles);
	void Write(BYTE nReg, BYTE nValue);

	void Mute(void);
	void Unmute(void);
	void SetVolume(DWORD dwVolume, DWORD dwVolumeMax);

	void PeriodicUpdate(UINT executedCycles);
	void Update(void);
	void SetSpeechIRQ(void);

	void Votrax_Write(BYTE nValue);
	bool GetVotraxPhoneme(void) { return m_isVotraxPhoneme; }
	void SetVotraxPhoneme(bool value) { m_isVotraxPhoneme = value; }

	void SetCardMode(PHASOR_MODE mode) { m_cardMode = mode; }
	void SetDevice(UINT device) { m_device = device; }

	void SaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
	void LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT device, PHASOR_MODE mode, UINT version);

private:
	void Play(unsigned int nPhoneme);
	void Stop(void);
	void UpdateIRQ(void);
	void UpdateAccurateLength(void);

	static const BYTE m_Votrax2SSI263[/*64*/];

	static const unsigned short m_kNumChannels = 1;
	static const DWORD m_kDSBufferByteSize = MAX_SAMPLES * sizeof(short) * m_kNumChannels;
	short m_mixBufferSSI263[m_kDSBufferByteSize / sizeof(short)];
	VOICE SSI263SingleVoice;

	//

	BYTE m_device;	// SSI263 device# which is generating phoneme-complete IRQ (and only required whilst Mockingboard isn't a class)
	PHASOR_MODE m_cardMode;
	short* m_pPhonemeData00;

	int m_currentActivePhoneme;
	bool m_isVotraxPhoneme;
	UINT m_cyclesThisAudioFrame;

	//

	UINT64 m_lastUpdateCycle;
	bool m_updateWasFullSpeed;

	const short* m_pPhonemeData;
	UINT m_phonemeLengthRemaining;			// length in samples, decremented as space becomes available in the ring-buffer
	UINT m_phonemeAccurateLengthRemaining;	// length in samples, decremented by cycles executed
	bool m_phonemePlaybackAndDebugger;
	bool m_phonemeCompleteByFullSpeed;

	//

	int m_numSamplesError;
	DWORD m_byteOffset;
	int m_currSampleSum;
	int m_currNumSamples;
	UINT m_currSampleMod4;

	// Regs:
	BYTE m_durationPhoneme;
	BYTE m_inflection;		// I10..I3
	BYTE m_rateInflection;
	BYTE m_ctrlArtAmp;
	BYTE m_filterFreq;

	BYTE m_currentMode;		// b7:6=Mode; b0=D7 pin (for IRQ)

	// Debug
	bool m_dbgFirst;
	UINT64 m_dbgStartTime;
};
