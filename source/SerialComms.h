#pragma once

#include "Card.h"

enum {COMMEVT_WAIT=0, COMMEVT_ACK, COMMEVT_TERM, COMMEVT_MAX};
enum eFWMODE {FWMODE_CIC=0, FWMODE_SIC_P8, FWMODE_PPC, FWMODE_SIC_P8A};	// NB. CIC = SSC

typedef struct
{
	//DIPSW1
	UINT	uBaudRate;
	eFWMODE	eFirmwareMode;

	//DIPSW2
	UINT	uStopBits;
	UINT	uByteSize;
	UINT	uParity;
	bool	bLinefeed;
	bool	bInterrupts;	// NB. Can't be read from s/w
} SSC_DIPSW;

#define TEXT_SERIAL_COM TEXT("COM")
#define TEXT_SERIAL_TCP TEXT("TCP")

class CSuperSerialCard : public Card
{
public:
	CSuperSerialCard(UINT slot);
	virtual ~CSuperSerialCard();
	virtual void Update(const ULONG nExecutedCycles) {}
	virtual void InitializeIO(LPBYTE pCxRomPeripheral);
	virtual void Reset(const bool powerCycle);
	virtual void Destroy() {}
	static const std::string& GetSnapshotCardName(void);
	virtual void	SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
	virtual bool	LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

	void    CommSetSerialPort(DWORD dwNewSerialPortItem);

	std::string const& GetSerialPortChoices();
	DWORD	GetSerialPort() { return m_dwSerialPortItem; }	// Drop-down list item
	const std::string& GetSerialPortName() { return m_currentSerialPortName; }
	bool	IsActive() { return (m_hCommHandle != INVALID_HANDLE_VALUE) || (m_hCommListenSocket != INVALID_SOCKET); }
	void	SupportDCD(bool bEnable) { m_bCfgSupportDCD = bEnable; }	// Status

	void	CommTcpSerialAccept();
	void	CommTcpSerialReceive();
	void	CommTcpSerialClose();
	void	CommTcpSerialCleanup();

	static BYTE __stdcall SSC_IORead(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nExecutedCycles);
	static BYTE __stdcall SSC_IOWrite(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nExecutedCycles);

private:
	BYTE __stdcall CommCommand(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
	BYTE __stdcall CommControl(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
	BYTE __stdcall CommDipSw(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
	BYTE __stdcall CommReceive(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
	BYTE __stdcall CommStatus(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
	BYTE __stdcall CommTransmit(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
	BYTE __stdcall CommProgramReset(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);

	void	InternalReset();
	void	UpdateCommandAndControlRegs(BYTE command, BYTE control);
	void	UpdateCommandReg(BYTE command);
	void	UpdateControlReg(BYTE control);
	void	GetDIPSW();
	void	SetDIPSWDefaults();
	UINT	BaudRateToIndex(UINT uBaudRate);
	void	UpdateCommState();
	void	TransmitDone(void);
	bool	CheckComm();
	void	CloseComm();
	void	CheckCommEvent(DWORD dwEvtMask);
	static DWORD WINAPI	CommThread(LPVOID lpParameter);
	bool	CommThInit();
	void	CommThUninit();
	UINT	GetNumSerialPortChoices() { return m_vecSerialPortsItems.size(); }
	void	ScanCOMPorts();
	void	SetSerialPortName(const char* pSerialPortName);
	void	SetRegistrySerialPortName(void);
	void	SaveSnapshotDIPSW(class YamlSaveHelper& yamlSaveHelper, std::string key, SSC_DIPSW& dipsw);
	void	LoadSnapshotDIPSW(class YamlLoadHelper& yamlLoadHelper, std::string key, SSC_DIPSW& dipsw);

	//

private:
	std::string m_currentSerialPortName;
	DWORD	m_dwSerialPortItem;

	static const UINT SERIALPORTITEM_INVALID_COM_PORT = 0;
	std::vector<UINT> m_vecSerialPortsItems;	// Includes "None" & "TCP" items
	std::string m_strSerialPortChoices;
	UINT	m_uTCPChoiceItemIdx;

	static SSC_DIPSW	m_DIPSWDefault;
	SSC_DIPSW			m_DIPSWCurrent;

	static const UINT m_kDefaultBaudRate = CBR_9600;
	UINT	m_uBaudRate;
	UINT	m_uStopBits;
	UINT	m_uByteSize;
	UINT	m_uParity;

	// SSC Registers
	BYTE   m_uControlByte;
	BYTE   m_uCommandByte;

	//

	HANDLE m_hCommHandle;
	SOCKET m_hCommListenSocket;
	SOCKET m_hCommAcceptSocket;

	//

	CRITICAL_SECTION	m_CriticalSection;	// To guard /m_vuRxCurrBuffer/ and /m_vbTxEmpty/
	std::deque<BYTE>	m_qComSerialBuffer[2];
	volatile UINT		m_vuRxCurrBuffer;	// Written to on COM recv. SSC reads from other one
	std::deque<BYTE>	m_qTcpSerialBuffer;

	//

	bool m_bTxIrqEnabled;
	bool m_bRxIrqEnabled;

	volatile bool m_vbTxIrqPending;
	volatile bool m_vbRxIrqPending;
	volatile bool m_vbTxEmpty;

	//

	HANDLE m_hCommThread;

	HANDLE m_hCommEvent[COMMEVT_MAX];
	OVERLAPPED m_o;

	BYTE* m_pExpansionRom;

	bool m_bCfgSupportDCD;
	UINT m_uDTR;

	static const DWORD m_kDefaultModemStatus = 0;	// MS_RLSD_OFF(=DCD_OFF), MS_DSR_OFF, MS_CTS_OFF
	volatile DWORD m_dwModemStatus;	// Updated by CommThread when any of RLSD|DSR|CTS changes / Read by main thread - CommStatus()& CommDipSw()

	UINT m_uRTS;
};
