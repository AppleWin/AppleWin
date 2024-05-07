#pragma once

#include "MockingboardDefs.h"

class SSI263
{
public:
	SSI263(UINT slot) : m_slot(slot)
	{
		m_device = -1;	// undefined
		m_cardMode = PH_Mockingboard;
		m_pPhonemeData00 = NULL;

		ResetState(true);
	}
	~SSI263(void)
	{
		delete [] m_pPhonemeData00;
	}

	void ResetState(const bool powerCycle)
	{
		m_currentActivePhoneme = -1;
		m_isVotraxPhoneme = false;
		m_votraxPhoneme = 0;
		m_cyclesThisAudioFrame = 0;

		//

		m_lastUpdateCycle = 0;
		m_updateWasFullSpeed = false;

		m_pPhonemeData = NULL;
		m_phonemeLengthRemaining = 0;
		m_phonemeAccurateLengthRemaining = 0;
		m_phonemePlaybackAndDebugger = false;
		m_phonemeCompleteByFullSpeed = false;
		m_phonemeLeadoutLength = 0;

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
		m_ctrlArtAmp = powerCycle ? CONTROL_MASK : 0;	// Chip power-on, so CTL=1 (power-down / standby)
		m_filterFreq = powerCycle ? FILTER_FREQ_SILENCE : 0;	// Empirically observed at chip power-on (GH#175)

		m_currentMode.mode = 0;

		//

		m_dbgFirst = true;
		m_dbgStartTime = 0;
	}

	void SetDevice(UINT device) { m_device = device; }
	void SetCardMode(PHASOR_MODE mode);

	bool DSInit(void);
	void DSUninit(void);

	void Reset(const bool powerCycle);
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

	void SaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
	void LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, PHASOR_MODE mode, UINT version);

private:
	void Play(unsigned int nPhoneme);
	void Stop(void);
	void UpdateIRQ(void);
	void UpdateAccurateLength(void);

	UINT64 GetLastCumulativeCycles(void);
	void UpdateIFR(BYTE nDevice, BYTE clr_mask, BYTE set_mask);
	BYTE GetPCR(BYTE nDevice);

	static const BYTE m_Votrax2SSI263[/*64*/];

	static const unsigned short m_kNumChannels = 1;
	static const DWORD m_kDSBufferByteSize = MAX_SAMPLES * sizeof(short) * m_kNumChannels;
	short m_mixBufferSSI263[m_kDSBufferByteSize / sizeof(short)];
	VOICE SSI263SingleVoice;

	//

	static const BYTE CONTROL_MASK = 0x80;
	static const BYTE FILTER_FREQ_SILENCE = 0xFF;

	UINT m_slot;
	BYTE m_device;	// SSI263 device# which is generating phoneme-complete IRQ (and only required whilst Mockingboard isn't a class)
	PHASOR_MODE m_cardMode;
	short* m_pPhonemeData00;

	int m_currentActivePhoneme;
	bool m_isVotraxPhoneme;
	BYTE m_votraxPhoneme;
	UINT m_cyclesThisAudioFrame;

	//

	UINT64 m_lastUpdateCycle;
	bool m_updateWasFullSpeed;

	const short* m_pPhonemeData;
	UINT m_phonemeLengthRemaining;			// length in samples, decremented as space becomes available in the ring-buffer
	UINT m_phonemeAccurateLengthRemaining;	// length in samples, decremented by cycles executed
	bool m_phonemePlaybackAndDebugger;
	bool m_phonemeCompleteByFullSpeed;
	UINT m_phonemeLeadoutLength;			// length in samples, decremented after \m_phonemeLengthRemaining\ goes to zero. Delay until phoneme repeats

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

	union
	{
		struct
		{
			BYTE function : 2;		// b7:6 = function
			BYTE enableInts : 1;	// b5 = enable A/!R (ie. interrupts)
			BYTE reserved : 4;
			BYTE D7 : 1;			// b0=D7 pin (for IRQ)
		};
		BYTE mode;
	} m_currentMode;

	// Debug
	bool m_dbgFirst;
	UINT64 m_dbgStartTime;
};
