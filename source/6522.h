#pragma once

class SY6522
{
public:
	SY6522(void)
	{
		for (UINT i = 0; i < kNumTimersPer6522; i++)
			m_syncEvent[i] = NULL;
		Reset(true);
	}

	~SY6522(void)
	{
	}

	void InitSyncEvents(class SyncEvent* event0, class SyncEvent* event1)
	{
		m_syncEvent[0] = event0;
		m_syncEvent[1] = event1;
	}

	void Reset(const bool powerCycle);

	void StartTimer1(void);
	void StopTimer1(void);
	bool IsTimer1Active(void) { return m_timer1Active; }
	void StopTimer2(void);
	bool IsTimer2Active(void) { return m_timer2Active; }

	void UpdateIFR(BYTE clr_ifr, BYTE set_ifr = 0);

	void UpdateTimer1(USHORT clocks);
	void UpdateTimer2(USHORT clocks);

	enum { rORB = 0, rORA, rDDRB, rDDRA, rT1CL, rT1CH, rT1LL, rT1LH, rT2CL, rT2CH, rSR, rACR, rPCR, rIFR, rIER, rORA_NO_HS, SIZE_6522_REGS };

	BYTE GetReg(BYTE reg)
	{
		switch (reg)
		{
		case rDDRA: return m_regs.DDRA;
		case rORA: return m_regs.ORA;
		case rACR: return m_regs.ACR;
		case rPCR: return m_regs.PCR;
		case rIFR: return m_regs.IFR;
		}
		_ASSERT(0);
		return 0;
	}
	USHORT GetRegT1C(void) { return m_regs.TIMER1_COUNTER.w; }
	USHORT GetRegT2C(void) { return m_regs.TIMER2_COUNTER.w; }
	void GetRegs(BYTE regs[SIZE_6522_REGS]) { memcpy(&regs[0], (BYTE*)&m_regs, SIZE_6522_REGS); }	// For debugger
	void SetRegORA(BYTE reg) { m_regs.ORA = reg; }

	BYTE Read(BYTE nReg);
	void Write(BYTE nReg, BYTE nValue);

	void SaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
	void LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT version);
	void SetTimersActiveFromSnapshot(bool timer1Active, bool timer2Active, UINT version);

	// ACR
	static const BYTE ACR_RUNMODE = 1 << 6;
	static const BYTE ACR_RM_ONESHOT = 0 << 6;
	static const BYTE ACR_RM_FREERUNNING = 1 << 6;

	// IFR & IER:
	static const BYTE IxR_SSI263 = 1 << 1;
	static const BYTE IxR_VOTRAX = 1 << 4;
	static const BYTE IxR_TIMER2 = 1 << 5;
	static const BYTE IxR_TIMER1 = 1 << 6;
	static const BYTE IFR_IRQ = 1 << 7;

	static const UINT kExtraTimerCycles = 2;	// Rockwell, Fig.16: period = N+2 cycles
	static const UINT kNumTimersPer6522 = 2;

private:
	USHORT SetTimerSyncEvent(BYTE reg, USHORT timerLatch);

	USHORT GetTimer1Counter(BYTE reg);
	USHORT GetTimer2Counter(BYTE reg);
	bool IsTimer1Underflowed(BYTE reg);
	bool IsTimer2Underflowed(BYTE reg);

	bool CheckTimerUnderflow(USHORT& counter, int& timerIrqDelay, const USHORT clocks);
	int OnTimer1Underflow(USHORT& counter);

	UINT GetOpcodeCyclesForRead(BYTE reg);
	UINT GetOpcodeCyclesForWrite(BYTE reg);

	void StartTimer2(void);
	void StartTimer1_LoadStateV1(void);

#pragma pack(push)
#pragma pack(1)	// Ensure 'struct Regs' is packed so that GetRegs() can just do a memcpy()

	struct IWORD
	{
		union
		{
			struct
			{
				BYTE l;
				BYTE h;
			};
			USHORT w;
		};
	};

	struct Regs
	{
		BYTE ORB;				// $00 - Port B
		BYTE ORA;				// $01 - Port A (with handshaking)
		BYTE DDRB;				// $02 - Data Direction Register B
		BYTE DDRA;				// $03 - Data Direction Register A
		IWORD TIMER1_COUNTER;	// $04 - Read counter (L) / Write latch (L)
								// $05 - Read / Write & initiate count (H)
		IWORD TIMER1_LATCH;		// $06 - Read / Write & latch (L)
								// $07 - Read / Write & latch (H)
		IWORD TIMER2_COUNTER;	// $08 - Read counter (L) / Write latch (L)
								// $09 - Read counter (H) / Write latch (H)
		BYTE SERIAL_SHIFT;		// $0A
		BYTE ACR;				// $0B - Auxiliary Control Register
		BYTE PCR;				// $0C - Peripheral Control Register
		BYTE IFR;				// $0D - Interrupt Flag Register
		BYTE IER;				// $0E - Interrupt Enable Register
		BYTE ORA_NO_HS;			// $0F - Port A (without handshaking)
		//
		IWORD TIMER2_LATCH;		// Doesn't exist in 6522
	};

#pragma pack(pop)

	Regs m_regs;

	int m_timer1IrqDelay;
	int m_timer2IrqDelay;
	bool m_timer1Active;
	bool m_timer2Active;

	class SyncEvent* m_syncEvent[kNumTimersPer6522];
};
