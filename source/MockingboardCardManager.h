#pragma once

class MockingboardCardManager
{
public:
	MockingboardCardManager(void)
	{
		m_userVolume = 0;
	}
	~MockingboardCardManager(void) {}

	void ReinitializeClock(void);
	void InitializeForLoadingSnapshot(void);
	void MuteControl(bool mute);
	void SetCumulativeCycles(void);
	void UpdateCycles(ULONG executedCycles);
	bool IsActive(void);
	DWORD GetVolume(void);
	void SetVolume(DWORD volume, DWORD volumeMax);
#ifdef _DEBUG
	void CheckCumulativeCycles(void);
	void Get6522IrqDescription(std::string& desc);
#endif

private:
	bool IsMockingboard(UINT slot);
	DWORD m_userVolume;	// GUI's slide volume
};
