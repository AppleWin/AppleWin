#pragma once

#include "Mockingboard.h"	// enum PHASOR_MODE

class SSI263
{
public:
	SSI263(void)
	{
		g_nSSI263Device = 0;
		g_nCurrentActivePhoneme = -1;
		g_bVotraxPhoneme = false;
		g_cyclesThisAudioFrameSSI263 = 0;

		memset(&SSI263SingleVoice, 0, sizeof(SSI263SingleVoice));

		cardMode = PH_Mockingboard;
	}
	~SSI263(void){}

	bool DSInit(void);
	void DSUninit(void);

	void Play(unsigned int nPhoneme);
	void Stop(void);
	void Reset(void);
	bool IsPhonemeActive(void) { return g_nCurrentActivePhoneme >= 0; }

	BYTE Read(BYTE nDevice, ULONG nExecutedCycles);
	void Write(BYTE nDevice, BYTE nReg, BYTE nValue);

	void Mute(void);
	void Unmute(void);

	void PeriodicUpdate(UINT executedCycles);
	void Update(void);
	void SetSpeechIRQ(BYTE nDevice);

	void Votrax_Write(BYTE nDevice, BYTE nValue);

	void SaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
	void LoadSnapshot(class YamlLoadHelper& yamlLoadHelper);

private:
	void UpdateIRQ(void);

	static const BYTE Votrax2SSI263[/*64*/];

	BYTE g_nSSI263Device;	// SSI263 device# which is generating phoneme-complete IRQ
	int g_nCurrentActivePhoneme;
	bool g_bVotraxPhoneme;

	static const unsigned short g_nSSI263_NumChannels = 1;
	static const DWORD g_dwDSSSI263BufferSize = MAX_SAMPLES * sizeof(short) * g_nSSI263_NumChannels;
	short g_nMixBufferSSI263[g_dwDSSSI263BufferSize / sizeof(short)];
	VOICE SSI263SingleVoice;

	UINT g_cyclesThisAudioFrameSSI263;

	PHASOR_MODE cardMode;

	// Regs:
	BYTE m_durationPhoneme;
	BYTE m_inflection;		// I10..I3
	BYTE m_rateInflection;
	BYTE m_ctrlArtAmp;
	BYTE m_filterFreq;

	BYTE m_currentMode;		// b7:6=Mode; b0=D7 pin (for IRQ)
};
