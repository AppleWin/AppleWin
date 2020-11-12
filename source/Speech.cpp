#include "StdAfx.h"

#include "Speech.h"

#ifdef USE_SPEECH_API
#include <sapi.h>

CSpeech::~CSpeech(void)
{
	// Don't do this: causes crash on app exit!
	//if (m_cpVoice)
	//	m_cpVoice->Release();
}

bool CSpeech::Init(void)
{
	HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_INPROC, IID_ISpVoice, (LPVOID*)&m_cpVoice);
	return hr == S_OK;
}

void CSpeech::Reset(void)
{
	if (!m_cpVoice)
		return;

	HRESULT hr = m_cpVoice->Speak(NULL, SPF_PURGEBEFORESPEAK, 0);
	_ASSERT(hr == S_OK);
}

void CSpeech::Speak(const char* const pBuffer)
{
	if (!m_cpVoice)
		return;

	size_t uSize;
	errno_t err = mbstowcs_s(&uSize, NULL, 0, pBuffer, 0);
	if (err)
		return;

	WCHAR* pszWTextString = new WCHAR[uSize];
	if (!pszWTextString)
		return;

	err = mbstowcs_s(&uSize, pszWTextString, uSize, pBuffer, uSize);
	if (err)
	{
		delete [] pszWTextString;
		return;
	}

	HRESULT hr = m_cpVoice->Speak(pszWTextString, SPF_ASYNC | SPF_IS_NOT_XML, 0);
	_ASSERT(hr == S_OK);

	hr = m_cpVoice->Resume();
	_ASSERT(hr == S_OK);

	delete [] pszWTextString;
}

#endif // USE_SPEECH_API
