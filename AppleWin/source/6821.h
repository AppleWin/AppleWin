// Motorola MC6821 PIA

typedef void (*mem_write_handler) (void* objFrom, void* objTo, int nAddr, BYTE byData);

typedef struct _STWriteHandler
{
	void* objTo;
	mem_write_handler func;
} STWriteHandler;

//

#define	PIA_DDRA	0
#define	PIA_CTLA	1
#define	PIA_DDRB	2
#define	PIA_CTLB	3

class C6821
{
public:
	C6821();
	virtual ~C6821();

	BYTE GetPB();
	BYTE GetPA();
	void SetPB(BYTE byData);
	void SetPA(BYTE byData);
	void SetCA1( BYTE byData );
	void SetCA2( BYTE byData );
	void SetCB1( BYTE byData );
	void SetCB2( BYTE byData );
	void Reset();
	BYTE Read( BYTE byRS );
	void Write( BYTE byRS, BYTE byData );

	void UpdateInterrupts();

	void SetListenerA( void *objTo, mem_write_handler func );
	void SetListenerB( void *objTo, mem_write_handler func );
	void SetListenerCA2( void *objTo, mem_write_handler func );
	void SetListenerCB2( void *objTo, mem_write_handler func );

protected:
	BYTE	m_byIA;
	BYTE	m_byCA1;
	BYTE	m_byICA2;
	BYTE	m_byOA;
	BYTE	m_byOCA2;
	BYTE	m_byDDRA;
	BYTE	m_byCTLA;
	BYTE	m_byIRQAState;

	BYTE	m_byIB;
	BYTE	m_byCB1;
	BYTE	m_byICB2;
	BYTE	m_byOB;
	BYTE	m_byOCB2;
	BYTE	m_byDDRB;
	BYTE	m_byCTLB;
	BYTE	m_byIRQBState;

	STWriteHandler m_stOutA;
	STWriteHandler m_stOutB;
	STWriteHandler m_stOutCA2;
	STWriteHandler m_stOutCB2;
	STWriteHandler m_stOutIRQA;
	STWriteHandler m_stOutIRQB;
};
