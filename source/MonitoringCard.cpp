#include "StdAfx.h"
#include "MonitoringCard.h"

extern unsigned __int64 g_nCumulativeCycles;

unsigned __int64 CMonitoringCard::startCycles = 0;
unsigned __int64 CMonitoringCard::elapsedCycles = 0;
unsigned __int64 CMonitoringCard::startInterruptCycles = 0;
unsigned __int64 CMonitoringCard::elapsedInterruptCyclesTemp = 0;
unsigned __int64 CMonitoringCard::elapsedInterruptCyclesTotal = 0;
unsigned __int64 CMonitoringCard::elapsedInterruptCyclesLast = 0;
bool CMonitoringCard::isMonitoring = false;

void CMonitoringCard::startMonitoring(bool isInterrupt)
{
	if (!isInterrupt)
	{
		startCycles = g_nCumulativeCycles;
		isMonitoring = true;
		return;
	}
	startInterruptCycles = g_nCumulativeCycles;
	elapsedInterruptCyclesTemp = 0;
}

void CMonitoringCard::stopMonitoring(bool isInterrupt)
{
	if (!isInterrupt)
	{
		elapsedCycles = g_nCumulativeCycles - startCycles;
		isMonitoring = false;
		return;
	}

	// Interrupt
	elapsedInterruptCyclesLast = g_nCumulativeCycles - startInterruptCycles;
	elapsedInterruptCyclesTemp += elapsedInterruptCyclesLast;
	//if (isMonitoring)
	//{
	//	// The interrupt running time must be substracted from the total monitoring
	//	startCycles += elapsedInterruptCyclesLast;
	//}
	elapsedInterruptCyclesTotal = elapsedInterruptCyclesTemp;
}

unsigned __int64 CMonitoringCard::getLastMonitoredCycles()
{
	return elapsedCycles;
}

unsigned __int64 CMonitoringCard::getLastMonitoredInterruptCycles()
{
	return elapsedInterruptCyclesLast;
}

unsigned __int64 CMonitoringCard::getTotalMonitoredInterruptCycles()
{
	return elapsedInterruptCyclesTotal;
}