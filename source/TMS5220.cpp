
#include "StdAfx.h"

#include "CardManager.h"
#include "Core.h"
#include "EchoII.h"
#include "SoundCore.h"
#include "TMS5220.h"

const uint32_t SAMPLE_RATE_TMS5220 = 44100;

//-----------------------------------------------------------------------------

UINT64 TMS5220::GetLastCumulativeCycles()
{
	return dynamic_cast<EchoII&>(GetCardMgr().GetRef(m_slot)).GetLastCumulativeCycles();
}

//-----------------------------------------------------------------------------

void TMS5220::Play()
{
	// TODO
}

void TMS5220::Stop()
{
	if (m_voice.lpDSBvoice && m_voice.bActive)
		DSVoiceStop(&m_voice);
}

//-----------------------------------------------------------------------------

void TMS5220::PeriodicUpdate(UINT executedCycles)
{
	const UINT kCyclesPerAudioFrame = 1000;
	m_cyclesThisAudioFrame += executedCycles;
	if (m_cyclesThisAudioFrame < kCyclesPerAudioFrame)
		return;

	m_cyclesThisAudioFrame %= kCyclesPerAudioFrame;

	Update();
}

//-----------------------------------------------------------------------------

//#define DBG_UPDATE		// NB. This outputs for all active TMS5220 ring-buffers (eg. for mb-audit depends on number of Echo II cards)
//#define DBG_UPDATE_RETURN

// Called by:
// . PeriodicUpdate()
void TMS5220::Update()
{
	//if (!IsPhonemeActive())
	//	return;

	if (!m_voice.lpDSBvoice || !m_voice.bActive)
	{
		if (!DSInit())
			return;
	}

	//UpdateAccurateLength();

	if (g_bFullSpeed)
	{
		// TODO

		//m_updateWasFullSpeed = true;
		//return;
	}

	//

	const bool nowNormalSpeed = m_updateWasFullSpeed;	// Just transitioned from full-speed to normal speed
	m_updateWasFullSpeed = false;

	// NB. next call to this function: nowNormalSpeed = false
	if (nowNormalSpeed)
		m_byteOffset = (uint32_t)-1;	// ...which resets m_numSamplesError below

	//-------------

	DWORD dwCurrentPlayCursor, dwCurrentWriteCursor;
	HRESULT hr = m_voice.lpDSBvoice->GetCurrentPosition(&dwCurrentPlayCursor, &dwCurrentWriteCursor);
	if (FAILED(hr))
	{
		LogOutput("TMS5220::Update() early return: GetCurrentPosition() failed\n");
		return;
	}

	bool prefillBufferOnInit = false;

	if (m_byteOffset == (uint32_t)-1)
	{
		// First time in this func (or transitioned from full-speed to normal speed, or a ring-buffer reset)
#ifdef DBG_UPDATE
		double fTicksSecs = (double)GetTickCount() / 1000.0;
		LogOutput("%010.3f: [SSUpdtInit%1d]PC=%08X, WC=%08X, Diff=%08X, Off=%08X xxx\n",
			fTicksSecs, m_device, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor - dwCurrentPlayCursor, m_byteOffset);
#endif
		m_byteOffset = dwCurrentWriteCursor;
		m_numSamplesError = 0;
		prefillBufferOnInit = true;
	}
	else
	{
		// Check that our offset isn't between Play & Write positions
		if (SoundCore_ValidateAndAlignWriteOffset(m_byteOffset, dwCurrentPlayCursor, dwCurrentWriteCursor))
		{
#ifdef DBG_UPDATE
			double fTicksSecs = (double)GetTickCount() / 1000.0;
			const char* tag = (dwCurrentWriteCursor > dwCurrentPlayCursor) ? "xxx" : "XXX";
			LogOutput("%010.3f: [SSUpdt%1d]    PC=%08X, WC=%08X, Diff=%08X, Off=%08X %s\n",
				fTicksSecs, m_device, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor - dwCurrentPlayCursor, m_byteOffset, tag);
#endif
			m_numSamplesError = 0;
		}
	}

	//-------------

	const UINT kMinBytesInBuffer = m_kDSBufferByteSize / 4;	// 25% full
	int nNumSamples = 0;
	double updateInterval = 0.0;

	if (prefillBufferOnInit)
	{
		// Just prefill first 25% of buffer with zeros:
		// . so we have a quarter buffer of silence/lag before the real sample data begins.
		// . NB. this is fine, since it's the steady state; and it's likely that no actual data will ever occur during this initial time.
		// This means that the '1st phoneme playback time' (in cycles) will be a bit longer for subsequent times.

		m_lastUpdateCycle = GetLastCumulativeCycles();

		nNumSamples = kMinBytesInBuffer / sizeof(short);
		memset(&m_mixBuffer[0], 0, nNumSamples);
	}
	else
	{
		// For small timer periods, wait for a period of 500cy before updating DirectSound ring-buffer.
		// NB. A timer period of less than 24cy will yield nNumSamplesPerPeriod=0.
		const double kMinimumUpdateInterval = 500.0;	// Arbitary (500 cycles = 21 samples)
		const double kMaximumUpdateInterval = (double)(0xFFFF + 2);	// Max 6522 timer interval (1372 samples)

		_ASSERT(GetLastCumulativeCycles() >= m_lastUpdateCycle);
		updateInterval = (double)(GetLastCumulativeCycles() - m_lastUpdateCycle);
		if (updateInterval < kMinimumUpdateInterval)
		{
#ifdef DBG_UPDATE_RETURN
			LogOutput("TMS5220::Update() early return: updateInterval < kMinimumUpdateInterval\n");
#endif
			return;
		}
		if (updateInterval > kMaximumUpdateInterval)
			updateInterval = kMaximumUpdateInterval;

		m_lastUpdateCycle = GetLastCumulativeCycles();

		const double nIrqFreq = g_fCurrentCLK6502 / updateInterval + 0.5;			// Round-up
		const int nNumSamplesPerPeriod = (int)((double)(SAMPLE_RATE_TMS5220) / nIrqFreq);	// Eg. For 60Hz this is 735

		nNumSamples = nNumSamplesPerPeriod + m_numSamplesError;						// Apply correction
		if (nNumSamples <= 0)
			nNumSamples = 0;
		if (nNumSamples > 2 * nNumSamplesPerPeriod)
			nNumSamples = 2 * nNumSamplesPerPeriod;

		if (nNumSamples > m_kDSBufferByteSize / sizeof(short))
			nNumSamples = m_kDSBufferByteSize / sizeof(short);	// Clamp to prevent buffer overflow

		//		if (nNumSamples)
		//		{ /* Generate new sample data - ie. could merge from all the TMS5220 sources */ }

				//

		int nBytesRemaining = m_byteOffset - dwCurrentPlayCursor;
		if (nBytesRemaining < 0)
			nBytesRemaining += m_kDSBufferByteSize;

		// Calc correction factor so that play-buffer doesn't under/overflow
		const int nErrorInc = SoundCore_GetErrorInc();
		if (nBytesRemaining < kMinBytesInBuffer)
			m_numSamplesError += nErrorInc;				// < 0.25 of buffer remaining
		else if (nBytesRemaining > m_kDSBufferByteSize / 2)
			m_numSamplesError -= nErrorInc;				// > 0.50 of buffer remaining
		else
			m_numSamplesError = 0;						// Acceptable amount of data in buffer
	}

#if defined(DBG_UPDATE)
	double fTicksSecs = (double)GetTickCount() / 1000.0;
	LogOutput("%010.3f: [SSUpdt%1d]    PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X, NSE=%08X, Interval=%f\n", fTicksSecs, m_device, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor - dwCurrentPlayCursor, m_byteOffset, nNumSamples, m_numSamplesError, updateInterval);
#endif

	if (nNumSamples == 0)
	{
		if (m_numSamplesError)
		{
			// Reset ring-buffer if we've had a major interruption, eg. F7 (enter debugger), F8 (configure), F11/12 (save-state), Pause, etc
			// - this can cause Apple II SSI263 detection code to fail (when either timing one or a sequence of phonemes)
			// When the AppleWin code restarts and reads the ring-buffer position it'll be at a random point, and maybe nearly full (>50% full)
			// - so the code waits until it drains (nNumSamples=0 each time)
			// - but it takes a large number of calls to this func to drain to an acceptable level
			m_byteOffset = (uint32_t)-1;
#if defined(DBG_UPDATE)
			double fTicksSecs = (double)GetTickCount() / 1000.0;
			LogOutput("%010.3f: [SSUpdt%1d]    Reset ring-buffer\n", fTicksSecs, m_device);
#endif
		}
#ifdef DBG_UPDATE_RETURN
		LogOutput("SSI263::Update() early return: nNumSamples == 0\n");
#endif
		return;
	}

	//-------------


}

//-----------------------------------------------------------------------------

// Cf. void MockingboardCardManager::UpdateSoundBuffer()
bool TMS5220::DSInit()
{
	if (!m_voice.lpDSBvoice)
	{
		if (g_bDisableDirectSound || g_bDisableDirectSoundMockingboard)
			return false;

		if (!Init())
			return false;
	}

	if (!m_voice.bActive)
	{
		bool bRes = DSZeroVoiceBuffer(&m_voice, m_kDSBufferByteSize);	// ... and Play()
		LogFileOutput("SSI263: DSZeroVoiceBuffer(), res=%d\n", bRes ? 1 : 0);
		if (!bRes)
			return false;
	}

	return true;
}

// Cf. bool MockingboardCardManager::Init()
bool TMS5220::Init()
{
	if (!DSAvailable())
		return false;

	HRESULT hr = DSGetSoundBuffer(&m_voice, m_kDSBufferByteSize, SAMPLE_RATE_TMS5220, m_kNumChannels, "TMS5220");
	LogFileOutput("SSI263: DSGetSoundBuffer(), hr=0x%08X\n", hr);
	if (FAILED(hr))
	{
		LogFileOutput("SSI263: DSGetSoundBuffer failed (%08X)\n", hr);
		return false;
	}

	// Don't DirectSoundBuffer::Play() via DSZeroVoiceBuffer() - instead wait until this TMS5220 is actually first used (like SSI263)
	// . different to Speaker & Mockingboard ring buffers

	hr = m_voice.lpDSBvoice->SetVolume(m_voice.nVolume);
	LogFileOutput("SSI263::DSInit: SetVolume(), hr=0x%08X\n", hr);

	return true;
}

void TMS5220::DSUninit()
{
	Stop();
	DSReleaseSoundBuffer(&m_voice);
}

//-----------------------------------------------------------------------------

void TMS5220::Reset(const bool powerCycle)
{
}

void TMS5220::Mute()
{
	if (m_voice.bActive && !m_voice.bMute)
	{
		m_voice.lpDSBvoice->SetVolume(DSBVOLUME_MIN);
		m_voice.bMute = true;
	}
}

void TMS5220::Unmute()
{
	if (m_voice.bActive && m_voice.bMute)
	{
		m_voice.lpDSBvoice->SetVolume(m_voice.nVolume);
		m_voice.bMute = false;
	}
}

void TMS5220::SetVolume(uint32_t dwVolume, uint32_t dwVolumeMax)
{
	m_voice.dwUserVolume = dwVolume;

	m_voice.nVolume = NewVolume(dwVolume, dwVolumeMax);

	if (m_voice.bActive && !m_voice.bMute)
		m_voice.lpDSBvoice->SetVolume(m_voice.nVolume);
}

uint8_t TMS5220::GetStatus()
{
	return 0;
}

void TMS5220::Write(uint8_t data)
{

}
