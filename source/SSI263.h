#pragma once

#include "Mockingboard.h"	// enum PHASOR_MODE

class SSI263
{
public:
	SSI263(void)
	{
		m_device = -1;	// undefined
		g_nCurrentActivePhoneme = -1;
		g_bVotraxPhoneme = false;
		g_cyclesThisAudioFrameSSI263 = 0;

		memset(&SSI263SingleVoice, 0, sizeof(SSI263SingleVoice));

		m_cardMode = PH_Mockingboard;

		//

		dbgFirst = true;
		dbgSTime = 0;

		g_uLastSSI263UpdateCycle = 0;
		g_ssi263UpdateWasFullSpeed = false;

		g_pPhonemeData = NULL;
		g_uPhonemeLength = 0;

		g_pPhonemeData00 = NULL;

		//

		nNumSamplesError = 0;
		dwByteOffset = (DWORD)-1;
	}
	~SSI263(void)
	{
		delete [] g_pPhonemeData00;
	}

	bool DSInit(void);
	void DSUninit(void);

	void Stop(void);
	void Reset(void);
	bool IsPhonemeActive(void) { return g_nCurrentActivePhoneme >= 0; }

	BYTE Read(ULONG nExecutedCycles);
	void Write(BYTE nReg, BYTE nValue);

	void Mute(void);
	void Unmute(void);
	void SetVolume(DWORD dwVolume, DWORD dwVolumeMax);

	void PeriodicUpdate(UINT executedCycles);
	void Update(void);
	void SetSpeechIRQ(void);

	void Votrax_Write(BYTE nValue);
	bool GetVotraxPhoneme(void) { return g_bVotraxPhoneme; }
	void SetVotraxPhoneme(bool value) { g_bVotraxPhoneme = value; }

	void SetCardMode(PHASOR_MODE mode) { m_cardMode = mode; }
	void SetDevice(UINT device) { m_device = device; }

	void SaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
	void LoadSnapshot(class YamlLoadHelper& yamlLoadHelper);

private:
	void Play(unsigned int nPhoneme);
	void UpdateIRQ(void);

	static const BYTE Votrax2SSI263[/*64*/];

	BYTE m_device;	// SSI263 device# which is generating phoneme-complete IRQ (and only required whilst Mockingboard isn't a class)
	int g_nCurrentActivePhoneme;
	bool g_bVotraxPhoneme;

	static const unsigned short g_nSSI263_NumChannels = 1;
	static const DWORD g_dwDSSSI263BufferSize = MAX_SAMPLES * sizeof(short) * g_nSSI263_NumChannels;
	short g_nMixBufferSSI263[g_dwDSSSI263BufferSize / sizeof(short)];
	VOICE SSI263SingleVoice;

	UINT g_cyclesThisAudioFrameSSI263;

	PHASOR_MODE m_cardMode;

	//
	bool dbgFirst;
	UINT64 dbgSTime;

	UINT64 g_uLastSSI263UpdateCycle;
	bool g_ssi263UpdateWasFullSpeed;

	const short* g_pPhonemeData;
	UINT g_uPhonemeLength;	// length in samples

	short* g_pPhonemeData00;

	//
	int nNumSamplesError;
	DWORD dwByteOffset;

	// Regs:
	BYTE m_durationPhoneme;
	BYTE m_inflection;		// I10..I3
	BYTE m_rateInflection;
	BYTE m_ctrlArtAmp;
	BYTE m_filterFreq;

	BYTE m_currentMode;		// b7:6=Mode; b0=D7 pin (for IRQ)
};
