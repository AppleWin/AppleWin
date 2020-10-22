#include "StdAfx.h"
#include "Common.h"
#include "Card.h"


class CMonitoringCard : public DummyCard
{
private:
	static unsigned __int64 startCycles;
	static unsigned __int64 elapsedCycles;
	static unsigned __int64 startInterruptCycles;
	static unsigned __int64 elapsedInterruptCyclesTemp;
	static unsigned __int64 elapsedInterruptCyclesTotal;
	static unsigned __int64 elapsedInterruptCyclesLast;
	static bool isMonitoring;

public:
	static void startMonitoring(bool isInterrupt = false);
	static void stopMonitoring(bool isInterrput = false);
	static unsigned __int64 getLastMonitoredCycles();
	static unsigned __int64 getLastMonitoredInterruptCycles();
	static unsigned __int64 getTotalMonitoredInterruptCycles();
};