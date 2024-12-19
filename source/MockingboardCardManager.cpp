/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2022, Tom Charlesworth, Michael Pohoreski, Nick Westgate

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Mockingboard/Phasor Card Manager
 *
 * Author: Various
 *
 */

#include "StdAfx.h"

#include "MockingboardCardManager.h"
#include "Core.h"
#include "CardManager.h"
#include "CPU.h"
#include "MockingboardDefs.h"
#include "Riff.h"

//#define DBG_MB_UPDATE

bool MockingboardCardManager::IsMockingboard(UINT slot)
{
	SS_CARDTYPE type = GetCardMgr().QuerySlot(slot);
	return type == CT_MockingboardC || type == CT_Phasor || IsMockingboardExtraCardType(slot);
}

bool MockingboardCardManager::IsMockingboardExtraCardType(UINT slot)
{
	SS_CARDTYPE type = GetCardMgr().QuerySlot(slot);
	return type == CT_MegaAudio || type == CT_SDMusic;
}

void MockingboardCardManager::ReinitializeClock(void)
{
	for (UINT i = SLOT0; i < NUM_SLOTS; i++)
	{
		if (IsMockingboard(i))
			dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(i)).ReinitializeClock();
	}
}

void MockingboardCardManager::InitializeForLoadingSnapshot(void)
{
	if (g_bDisableDirectSound || g_bDisableDirectSoundMockingboard)
		return;

	if (!m_mockingboardVoice.lpDSBvoice)
		return;

	DSVoiceStop(&m_mockingboardVoice);	// Reason: 'MB voice is playing' then loading a save-state where 'no MB present' (GH#609)
}

void MockingboardCardManager::MuteControl(bool mute)
{
	for (UINT i = SLOT0; i < NUM_SLOTS; i++)
	{
		if (IsMockingboard(i))
			dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(i)).MuteControl(mute);
	}

	if (mute)
	{
		if (m_mockingboardVoice.bActive && !m_mockingboardVoice.bMute)
		{
			m_mockingboardVoice.lpDSBvoice->SetVolume(DSBVOLUME_MIN);
			m_mockingboardVoice.bMute = true;
		}
	}
	else
	{
		if (m_mockingboardVoice.bActive && m_mockingboardVoice.bMute)
		{
			m_mockingboardVoice.lpDSBvoice->SetVolume(m_mockingboardVoice.nVolume);
			m_mockingboardVoice.bMute = false;
		}
	}
}

void MockingboardCardManager::SetCumulativeCycles(void)
{
	for (UINT i = SLOT0; i < NUM_SLOTS; i++)
	{
		if (IsMockingboard(i))
			dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(i)).SetCumulativeCycles();
	}
}

void MockingboardCardManager::UpdateCycles(ULONG executedCycles)
{
	for (UINT i = SLOT0; i < NUM_SLOTS; i++)
	{
		if (IsMockingboard(i))
			dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(i)).UpdateCycles(executedCycles);
	}
}

// Called from class SY6522
void MockingboardCardManager::UpdateIRQ(void)
{
	bool irq = false;
	for (UINT i = SLOT0; i < NUM_SLOTS; i++)
	{
		if (IsMockingboard(i))
			irq |= dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(i)).Is6522IRQ();
	}

	if (irq)
		CpuIrqAssert(IS_6522);
	else
		CpuIrqDeassert(IS_6522);
}

bool MockingboardCardManager::IsActiveToPreventFullSpeed(void)
{
	if (!m_mockingboardVoice.bActive)
		return false;

	for (UINT i = SLOT0; i < NUM_SLOTS; i++)
	{
		if (IsMockingboard(i))
			if (dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(i)).IsActiveToPreventFullSpeed())
				return true;	// if any card is true then the condition for active is true
	}

	return false;
}

uint32_t MockingboardCardManager::GetVolume(void)
{
	return m_userVolume;
}

void MockingboardCardManager::SetVolume(uint32_t volume, uint32_t volumeMax)
{
	m_userVolume = volume;

	m_mockingboardVoice.dwUserVolume = volume;
	m_mockingboardVoice.nVolume = NewVolume(volume, volumeMax);

	if (m_mockingboardVoice.bActive && !m_mockingboardVoice.bMute)
		m_mockingboardVoice.lpDSBvoice->SetVolume(m_mockingboardVoice.nVolume);

	for (UINT i = SLOT0; i < NUM_SLOTS; i++)
	{
		if (IsMockingboard(i))
			dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(i)).SetVolume(volume, volumeMax);
	}
}

bool MockingboardCardManager::GetEnableExtraCardTypes(void)
{
	// Scan slots for any extra card types
	// . eg. maybe started a new AppleWin (with empty cmd line), but with Registry's slot 4 = CT_MegaAudio
	// Otherwise, Config->Sound will show slot as "Unavailable"
	for (UINT i = SLOT0; i < NUM_SLOTS; i++)
	{
		if (IsMockingboardExtraCardType(i))
			return true;
	}

	return m_enableExtraCardTypes;
}

#ifdef _DEBUG
void MockingboardCardManager::CheckCumulativeCycles(void)
{
	for (UINT i = SLOT0; i < NUM_SLOTS; i++)
	{
		if (IsMockingboard(i))
			dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(i)).CheckCumulativeCycles();
	}
}

void MockingboardCardManager::Get6522IrqDescription(std::string& desc)
{
	for (UINT i = SLOT0; i < NUM_SLOTS; i++)
	{
		if (IsMockingboard(i))
			dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(i)).Get6522IrqDescription(desc);
	}
}
#endif

// Called by CardManager::Destroy()
void MockingboardCardManager::Destroy(void)
{
	// NB. All cards (including any Mockingboard cards) have just been destroyed by CardManager

	if (m_mockingboardVoice.lpDSBvoice && m_mockingboardVoice.bActive)
		DSVoiceStop(&m_mockingboardVoice);

	DSReleaseSoundBuffer(&m_mockingboardVoice);
}

// Called by ContinueExecution() at the end of every execution period (~1000 cycles or ~3 cycles when MODE_STEPPING)
// NB. Required for FT's TEST LAB #1 player
void MockingboardCardManager::Update(const ULONG executedCycles)
{
	// NB. CardManager has just called each card's Update()

	bool active = false;
	bool present = false;
	for (UINT i = SLOT0; i < NUM_SLOTS; i++)
	{
		if (IsMockingboard(i))
		{
			active |= dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(i)).IsAnyTimer1Active();
			present = true;
		}
	}

	if (!present || active)
		return;

	// No 6522 TIMER1's are active, so periodically update AY8913's here...

	const UINT kCyclesPerAudioFrame = 1000;
	m_cyclesThisAudioFrame += executedCycles;
	if (m_cyclesThisAudioFrame < kCyclesPerAudioFrame)
		return;

	m_cyclesThisAudioFrame %= kCyclesPerAudioFrame;

	UpdateSoundBuffer();
}

// Called by:
// . MB_SyncEventCallback() on a TIMER1 (not TIMER2) underflow - when IsAnyTimer1Active() == true
// . Update()                                                  - when IsAnyTimer1Active() == false
void MockingboardCardManager::UpdateSoundBuffer(void)
{
#ifdef LOG_PERF_TIMINGS
	extern UINT64 g_timeMB_NoTimer;
	extern UINT64 g_timeMB_Timer;
	PerfMarker perfMarker(!IsAnyTimer1Active() ? g_timeMB_NoTimer : g_timeMB_Timer);
#endif

	if (!m_mockingboardVoice.lpDSBvoice)
	{
		if (g_bDisableDirectSound || g_bDisableDirectSoundMockingboard)
			return;

		if (!Init())
			return;
	}

	if (!m_mockingboardVoice.bActive)
	{
		// Sound buffer may have been stopped by MB_InitializeForLoadingSnapshot().
		// NB. DSZeroVoiceBuffer() also zeros the sound buffer, so it's better than directly calling IDirectSoundBuffer::Play():
		// - without zeroing, then the previous sound buffer can be heard for a fraction of a second
		// - eg. when doing Mockingboard playback, then loading a save-state which is also doing Mockingboard playback
		DSZeroVoiceBuffer(&m_mockingboardVoice, SOUNDBUFFER_SIZE);	// ... and Play()
	}

	UINT numSamples = GenerateAllSoundData();
	if (numSamples)
		MixAllAndCopyToRingBuffer(numSamples);
}

bool MockingboardCardManager::Init(void)
{
	if (!g_bDSAvailable)
		return false;

	HRESULT hr = DSGetSoundBuffer(&m_mockingboardVoice, DSBCAPS_CTRLVOLUME, SOUNDBUFFER_SIZE, MockingboardCard::SAMPLE_RATE, MockingboardCard::NUM_MB_CHANNELS, "MB");
	LogFileOutput("MBCardMgr: DSGetSoundBuffer(), hr=0x%08X\n", hr);
	if (FAILED(hr))
	{
		LogFileOutput("MBCardMgr: DSGetSoundBuffer failed (%08X)\n", hr);
		return false;
	}

	bool bRes = DSZeroVoiceBuffer(&m_mockingboardVoice, SOUNDBUFFER_SIZE);	// ... and Play()
	LogFileOutput("MBCardMgr: DSZeroVoiceBuffer(), res=%d\n", bRes ? 1 : 0);
	if (!bRes)
		return false;

	m_mockingboardVoice.bActive = true;

	// Volume might've been setup from value in Registry
	if (!m_mockingboardVoice.nVolume)
		m_mockingboardVoice.nVolume = DSBVOLUME_MAX;

	hr = m_mockingboardVoice.lpDSBvoice->SetVolume(m_mockingboardVoice.nVolume);
	LogFileOutput("MBCardMgr: SetVolume(), hr=0x%08X\n", hr);
	return true;
}

UINT MockingboardCardManager::GenerateAllSoundData(void)
{
	UINT nNumSamples = 0;

	for (UINT slot = SLOT0; slot < NUM_SLOTS; slot++)
	{
		if (!IsMockingboard(slot))
			continue;

		MockingboardCard& MB = dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(slot));

		MB.SetNumSamplesError(m_numSamplesError);
		nNumSamples = MB.MB_Update();
		m_numSamplesError = MB.GetNumSamplesError();
	}

	//

	DWORD dwCurrentPlayCursor, dwCurrentWriteCursor;
	HRESULT hr = m_mockingboardVoice.lpDSBvoice->GetCurrentPosition(&dwCurrentPlayCursor, &dwCurrentWriteCursor);
	if (FAILED(hr))
		return 0;

	if (m_byteOffset == (uint32_t)-1)
	{
		// First time in this func

		m_byteOffset = dwCurrentWriteCursor;
	}
	else
	{
		// Check that our offset isn't between Play & Write positions

		if (dwCurrentWriteCursor > dwCurrentPlayCursor)
		{
			// |-----PxxxxxW-----|
			if ((m_byteOffset > dwCurrentPlayCursor) && (m_byteOffset < dwCurrentWriteCursor))
			{
#ifdef DBG_MB_UPDATE
				double fTicksSecs = (double)GetTickCount() / 1000.0;
				LogOutput("%010.3f: [MBUpdt]    PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X xxx\n", fTicksSecs, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor - dwCurrentPlayCursor, dwByteOffset, nNumSamples);
#endif
				m_byteOffset = dwCurrentWriteCursor;
				m_numSamplesError = 0;
			}
		}
		else
		{
			// |xxW----------Pxxx|
			if ((m_byteOffset > dwCurrentPlayCursor) || (m_byteOffset < dwCurrentWriteCursor))
			{
#ifdef DBG_MB_UPDATE
				double fTicksSecs = (double)GetTickCount() / 1000.0;
				LogOutput("%010.3f: [MBUpdt]    PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X XXX\n", fTicksSecs, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor - dwCurrentPlayCursor, dwByteOffset, nNumSamples);
#endif
				m_byteOffset = dwCurrentWriteCursor;
				m_numSamplesError = 0;
			}
		}
	}

	int nBytesRemaining = m_byteOffset - dwCurrentPlayCursor;
	if (nBytesRemaining < 0)
		nBytesRemaining += SOUNDBUFFER_SIZE;

	// Calc correction factor so that play-buffer doesn't under/overflow
	const int nErrorInc = SoundCore_GetErrorInc();
	if (nBytesRemaining < SOUNDBUFFER_SIZE / 4)
		m_numSamplesError += nErrorInc;				// < 0.25 of buffer remaining
	else if (nBytesRemaining > SOUNDBUFFER_SIZE / 2)
		m_numSamplesError -= nErrorInc;				// > 0.50 of buffer remaining
	else
		m_numSamplesError = 0;						// Acceptable amount of data in buffer

#ifdef DBG_MB_UPDATE
	double fTicksSecs = (double)GetTickCount() / 1000.0;
	LogOutput("%010.3f: [MBUpdt]    PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X, NSE=%08X, Interval=%f\n", fTicksSecs, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor - dwCurrentPlayCursor, dwByteOffset, nNumSamples, nNumSamplesError, updateInterval);
#endif

	return nNumSamples;
}

void MockingboardCardManager::MixAllAndCopyToRingBuffer(UINT nNumSamples)
{
//	const double fAttenuation = g_bPhasorEnable ? 2.0 / 3.0 : 1.0;
	const double fAttenuation = true ? 2.0 / 3.0 : 1.0;

	short** slotAYVoiceBuffers[NUM_SLOTS] = {0};

	for (UINT slot = SLOT0; slot < NUM_SLOTS; slot++)
	{
		if (IsMockingboard(slot))
			slotAYVoiceBuffers[slot] = dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(slot)).GetVoiceBuffers();
	}

	for (UINT i = 0; i < nNumSamples; i++)
	{
		// Mockingboard stereo (all voices on an AY8910 wire-or'ed together)
		// L = Address.b7=0, R = Address.b7=1
		int nDataL = 0, nDataR = 0;

		for (UINT slot = SLOT0; slot < NUM_SLOTS; slot++)
		{
			if (!slotAYVoiceBuffers[slot])
				continue;

			for (UINT j = 0; j < NUM_VOICES_PER_AY8913; j++)
			{
				short** ppAYVoiceBuffer = slotAYVoiceBuffers[slot];

				// Regular MB-C AY's
				nDataL += (int)((double)ppAYVoiceBuffer[0 * NUM_VOICES_PER_AY8913 + j][i] * fAttenuation);
				nDataR += (int)((double)ppAYVoiceBuffer[2 * NUM_VOICES_PER_AY8913 + j][i] * fAttenuation);

				// Extra Phasor AY's
				nDataL += (int)((double)ppAYVoiceBuffer[1 * NUM_VOICES_PER_AY8913 + j][i] * fAttenuation);
				nDataR += (int)((double)ppAYVoiceBuffer[3 * NUM_VOICES_PER_AY8913 + j][i] * fAttenuation);
			}
		}

		// Cap the superpositioned output
		if (nDataL < WAVE_DATA_MIN)
			nDataL = WAVE_DATA_MIN;
		else if (nDataL > WAVE_DATA_MAX)
			nDataL = WAVE_DATA_MAX;

		if (nDataR < WAVE_DATA_MIN)
			nDataR = WAVE_DATA_MIN;
		else if (nDataR > WAVE_DATA_MAX)
			nDataR = WAVE_DATA_MAX;

		m_mixBuffer[i * MockingboardCard::NUM_MB_CHANNELS + 0] = (short)nDataL;	// L
		m_mixBuffer[i * MockingboardCard::NUM_MB_CHANNELS + 1] = (short)nDataR;	// R
	}

	//

	DWORD dwDSLockedBufferSize0, dwDSLockedBufferSize1;
	SHORT* pDSLockedBuffer0, * pDSLockedBuffer1;

	HRESULT hr = DSGetLock(m_mockingboardVoice.lpDSBvoice,
		m_byteOffset, (uint32_t)nNumSamples * sizeof(short) * MockingboardCard::NUM_MB_CHANNELS,
		&pDSLockedBuffer0, &dwDSLockedBufferSize0,
		&pDSLockedBuffer1, &dwDSLockedBufferSize1);
	if (FAILED(hr))
		return;

	memcpy(pDSLockedBuffer0, &m_mixBuffer[0], dwDSLockedBufferSize0);
	if (pDSLockedBuffer1)
		memcpy(pDSLockedBuffer1, &m_mixBuffer[dwDSLockedBufferSize0 / sizeof(short)], dwDSLockedBufferSize1);

	// Commit sound buffer
	hr = m_mockingboardVoice.lpDSBvoice->Unlock((void*)pDSLockedBuffer0, dwDSLockedBufferSize0,
		(void*)pDSLockedBuffer1, dwDSLockedBufferSize1);

	m_byteOffset = (m_byteOffset + (uint32_t)nNumSamples * sizeof(short) * MockingboardCard::NUM_MB_CHANNELS) % SOUNDBUFFER_SIZE;

	if (m_outputToRiff)
		RiffPutSamples(&m_mixBuffer[0], nNumSamples);
}
