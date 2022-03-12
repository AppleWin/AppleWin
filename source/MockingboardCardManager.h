#pragma once

class MockingboardCardManager
{
public:
	MockingboardCardManager(void) {}
	~MockingboardCardManager(void) {}

	void ReinitializeClock(void);
	void InitializeForLoadingSnapshot(void);
	void MuteControl(bool mute);
	void SetCumulativeCycles(void);
#ifdef _DEBUG
	void CheckCumulativeCycles(void);
#endif

private:
	bool IsMockingboard(UINT slot);
};
