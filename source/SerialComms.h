#pragma once

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

class CSuperSerialCard
{
public:
	CSuperSerialCard();
	~CSuperSerialCard();

	void	CommInitialize(LPBYTE pCxRomPeripheral, UINT uSlot);
	void    CommReset();
	void    CommDestroy();
	void    CommSetSerialPort(HWND,DWORD);
	void    CommUpdate(DWORD);
	DWORD   CommGetSnapshot(SS_IO_Comms* pSS);
	DWORD   CommSetSnapshot(SS_IO_Comms* pSS);

	DWORD	GetSerialPort() { return m_dwSerialPort; }
	void	SetSerialPort(DWORD dwSerialPort) { m_dwSerialPort = dwSerialPort; }

	static BYTE __stdcall SSC_IORead(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nCyclesLeft);
	static BYTE __stdcall SSC_IOWrite(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nCyclesLeft);

private:
	BYTE __stdcall CommCommand(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
	BYTE __stdcall CommControl(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
	BYTE __stdcall CommDipSw(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
	BYTE __stdcall CommReceive(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
	BYTE __stdcall CommStatus(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
	BYTE __stdcall CommTransmit(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);

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

	//

private:
	DWORD	m_dwSerialPort;

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
	DWORD  m_dwCommInactivity;

	//

	CRITICAL_SECTION	m_CriticalSection;	// To guard /g_vRecvBytes/
	BYTE				m_RecvBuffer[uRecvBufferSize];	// NB: More work required if >1 is used
	volatile DWORD		m_vRecvBytes;

	//

	bool m_bTxIrqEnabled;
	bool m_bRxIrqEnabled;

	bool m_bWrittenTx;

	//

	volatile bool m_vbCommIRQ;
	HANDLE m_hCommThread;

	HANDLE m_hCommEvent[COMMEVT_MAX];
	OVERLAPPED m_o;

	BYTE* m_pExpansionRom;
};
