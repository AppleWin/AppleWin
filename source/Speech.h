#pragma once

struct ISpVoice;

class CSpeech
{
public:
	CSpeech() :
		m_cpVoice(NULL)
	{
	}
	virtual ~CSpeech();

	bool Init();
	void Reset();
	bool IsEnabled() { return m_cpVoice != NULL; }
	void Speak(const char* const pBuffer);

private:
	ISpVoice* m_cpVoice;
};
