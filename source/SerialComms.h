#pragma once
#include <queue>

extern class CSuperSerialCard sg_SSC;

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
	bool	bInterrupts;
} SSC_DIPSW;

#define TEXT_SERIAL_COM TEXT("COM")
#define TEXT_SERIAL_TCP TEXT("TCP")

class CSuperSerialCard
{
public:
	CSuperSerialCard();
	virtual ~CSuperSerialCard();

	void	CommInitialize(LPBYTE pCxRomPeripheral, UINT uSlot);
	void    CommReset();
	void    CommDestroy();
	void    CommSetSerialPort(HWND hWindow, DWORD dwNewSerialPortItem);
	void    CommUpdate(DWORD);
	DWORD   CommGetSnapshot(SS_IO_Comms* pSS);
	DWORD   CommSetSnapshot(SS_IO_Comms* pSS);

	char*	GetSerialPortChoices();
	DWORD	GetSerialPort() { return m_dwSerialPortItem; }	// Drop-down list item
	char*	GetSerialPortName() { return m_ayCurrentSerialPortName; }
	void	SetSerialPortName(const char* pSerialPortName);
	bool	IsActive() { return (m_hCommHandle != INVALID_HANDLE_VALUE) || (m_hCommListenSocket != INVALID_SOCKET); }

	void	CommTcpSerialAccept();
	void	CommTcpSerialReceive();
	void	CommTcpSerialClose();
	void	CommTcpSerialCleanup();

	static BYTE __stdcall SSC_IORead(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nCyclesLeft);
	static BYTE __stdcall SSC_IOWrite(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nCyclesLeft);

private:
	BYTE __stdcall CommCommand(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
	BYTE __stdcall CommControl(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
	BYTE __stdcall CommDipSw(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
	BYTE __stdcall CommReceive(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
	BYTE __stdcall CommStatus(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
	BYTE __stdcall CommTransmit(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);

	void	InternalReset();
	void	GetDIPSW();
	void	SetDIPSWDefaults();
	BYTE	GenerateControl();
	UINT	BaudRateToIndex(UINT uBaudRate);
	void	UpdateCommState();
	BOOL	CheckComm();
	void	CloseComm();
	void	CheckCommEvent(DWORD dwEvtMask);
	static DWORD WINAPI	CommThread(LPVOID lpParameter);
	bool	CommThInit();
	void	CommThUninit();
	UINT	GetNumSerialPortChoices() { return m_vecSerialPortsItems.size(); }
	void	ScanCOMPorts();

	//

public:
	static const UINT SIZEOF_SERIALCHOICE_ITEM = 8*sizeof(char);

private:
	char	m_ayCurrentSerialPortName[SIZEOF_SERIALCHOICE_ITEM];
	DWORD	m_dwSerialPortItem;

	static const UINT SERIALPORTITEM_INVALID_COM_PORT = 0;
	std::vector<UINT> m_vecSerialPortsItems;	// Includes "None" & "TCP" items
	char*	m_aySerialPortChoices;
	UINT	m_uTCPChoiceItemIdx;

	static SSC_DIPSW	m_DIPSWDefault;
	SSC_DIPSW			m_DIPSWCurrent;

	// Derived from DIPSW1
	UINT	m_uBaudRate;

	// Derived from DIPSW2
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
	DWORD  m_dwCommInactivity;

	//

	CRITICAL_SECTION	m_CriticalSection;	// To guard /g_vRecvBytes/
	deque<BYTE>			m_qComSerialBuffer[2];
	volatile UINT		m_vuRxCurrBuffer;	// Written to on COM recv. SSC reads from other one
	deque<BYTE>			m_qTcpSerialBuffer;

	//

	bool m_bTxIrqEnabled;
	bool m_bRxIrqEnabled;

	volatile bool		m_vbTxIrqPending;
	volatile bool		m_vbRxIrqPending;

	bool m_bWrittenTx;

	//

	HANDLE m_hCommThread;

	HANDLE m_hCommEvent[COMMEVT_MAX];
	OVERLAPPED m_o;

	BYTE* m_pExpansionRom;
};
