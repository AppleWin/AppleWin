// Based on MAME's 6821pia.c
// - by Kyle Kim (Apple in PC)

// 
// From mame.txt (http://www.mame.net/readme.html)
// 
// VI. Reuse of Source Code
// --------------------------
//    This chapter might not apply to specific portions of MAME (e.g. CPU
//    emulators) which bear different copyright notices.
//    The source code cannot be used in a commercial product without the written
//    authorization of the authors. Use in non-commercial products is allowed, and
//    indeed encouraged.  If you use portions of the MAME source code in your
//    program, however, you must make the full source code freely available as
//    well.
//    Usage of the _information_ contained in the source code is free for any use.
//    However, given the amount of time and energy it took to collect this
//    information, if you find new information we would appreciate if you made it
//    freely available as well.
// 

#include "stdafx.h"
#include "6821.h"

// Ctrl-A(B) register bit mask define
/*
			0				1
	bit0	IRQ1_DISABLED	IRQ1_ENABLED
	bit1	C1_HIGH_TO_LOW	C1_LOW_TO_HIGH
	bit2	DDR_SELECTED	OUTPUT_SELECTED	

	bit3	RESET_C2		SET_C2				( C2_OUTPUT & C2_SETMODE )
	bit3	STROBE_C1_RESET	STROBE_E_RESET		( C2_OUTPUT & C2_STROBE_MODE )
	bit4	C2_STROBE_MODE	C2_SETMODE			( C2_OUTPUT )

	bit3	IRQ2_DISABLED	IRQ2_ENABLED		( C2_INPUT )
	bit4	C2_HIGH_TO_LOW	C2_HIGH_TO_LOW		( C2_INPUT )
	bit5	C2_INPUT		C2_OUTPUT
*/
#define PIA_IRQ1	0x80
#define	PIA_IRQ2	0x40
#define SET_IRQ1(c)			c |= PIA_IRQ1;
#define SET_IRQ2(c)			c |= PIA_IRQ2;
#define CLEAR_IRQ1(c)		c &= ~PIA_IRQ1;
#define CLEAR_IRQ2(c)		c &= ~PIA_IRQ2;
#define IRQ1(c)				( c & PIA_IRQ1 )
#define IRQ2(c)				( c & PIA_IRQ2 )

#define IRQ1_ENABLED(c)		 ( c & 0x01 )
#define IRQ1_DISABLED(c)	!( c & 0x01 )
#define C1_LOW_TO_HIGH(c)	 ( c & 0x02 )
#define C1_HIGH_TO_LOW(c)	!( c & 0x02 )
#define OUTPUT_SELECTED(c)	 ( c & 0x04 )
#define DDR_SELECTED(c)		!( c & 0x04 )
#define IRQ2_ENABLED(c)		 ( c & 0x08 )
#define IRQ2_DISABLED(c)	!( c & 0x08 )
#define STROBE_E_RESET(c)	 ( c & 0x08 )
#define STROBE_C1_RESET(c)	!( c & 0x08 )
#define SET_C2(c)			 ( c & 0x08 )
#define RESET_C2(c)			!( c & 0x08 )
#define C2_LOW_TO_HIGH(c)	 ( c & 0x10 )
#define C2_HIGH_TO_LOW(c)	!( c & 0x10 )
#define C2_SET_MODE(c)		 ( c & 0x10 )
#define C2_STROBE_MODE(c)	!( c & 0x10 )
#define C2_OUTPUT(c)		 ( c & 0x20 )
#define C2_INPUT(c)			!( c & 0x20 )

#define PIA_W_CALLBACK(st, val)	\
	if ( st.func ) st.func( this, st.objTo, 0, val )

//////////////////////////////////////////////////////////////////////

C6821::C6821()
{
	Reset();
	m_stOutA.objTo = NULL;
	m_stOutA.func = NULL;
	m_stOutB.objTo = NULL;
	m_stOutB.func = NULL;
	m_stOutCA2.objTo = NULL;
	m_stOutCA2.func = NULL;
	m_stOutCB2.objTo = NULL;
	m_stOutCB2.func = NULL;
	m_stOutIRQA.objTo = NULL;
	m_stOutIRQA.func = NULL;
	m_stOutIRQB.objTo = NULL;
	m_stOutIRQB.func = NULL;
}

C6821::~C6821()
{

}

void C6821::SetListenerA(void *objTo, mem_write_handler func)
{
	m_stOutA.objTo = objTo;
	m_stOutA.func = func;
}

void C6821::SetListenerB(void *objTo, mem_write_handler func)
{
	m_stOutB.objTo = objTo;
	m_stOutB.func = func;
}

void C6821::SetListenerCA2(void *objTo, mem_write_handler func)
{
	m_stOutCA2.objTo = objTo;
	m_stOutCA2.func = func;
}

void C6821::SetListenerCB2(void *objTo, mem_write_handler func)
{
	m_stOutCB2.objTo = objTo;
	m_stOutCB2.func = func;
}

BYTE C6821::Read(BYTE byRS)
{
	BYTE retval = 0;
	byRS &= 3;
	switch ( byRS )
	{
		/******************* port A output/DDR read *******************/
	case PIA_DDRA:
		// read output register
		if ( OUTPUT_SELECTED(m_byCTLA) )
		{
			// combine input and output values
			retval = ( m_byOA & m_byDDRA ) | ( m_byIA & ~m_byDDRA );
			// IRQ flags implicitly cleared by a read
			CLEAR_IRQ1( m_byCTLA );
			CLEAR_IRQ1( m_byCTLB );
			UpdateInterrupts();
			// CA2 is configured as output and in read strobe mode
			if ( C2_OUTPUT(m_byCTLA) && C2_STROBE_MODE(m_byCTLA) )
			{
				// this will cause a transition low; call the output function if we're currently high
				if ( m_byOCA2 )
					PIA_W_CALLBACK( m_stOutCA2, 0 );
				m_byOCA2 = 0;
				
				// if the CA2 strobe is cleared by the E, reset it right away
				if ( STROBE_E_RESET( m_byCTLA ) )
				{
					PIA_W_CALLBACK( m_stOutCA2, 1 );
					m_byOCA2 = 1;
				}
			}
		}
		// read DDR register
		else
		{
			retval = m_byDDRA;
		}
		break;
		
		/******************* port B output/DDR read *******************/
	case PIA_DDRB:
		
		// read output register
		if ( OUTPUT_SELECTED( m_byCTLB ) )
		{
			// combine input and output values
			retval = ( m_byOB & m_byDDRB ) + ( m_byIB & ~m_byDDRB );
			
			// IRQ flags implicitly cleared by a read
			CLEAR_IRQ2( m_byCTLA );
			CLEAR_IRQ2( m_byCTLB );
			UpdateInterrupts();
		}
		/* read DDR register */
		else
		{
			retval = m_byDDRB;
		}
		break;
		
		/******************* port A control read *******************/
	case PIA_CTLA:
		// read control register
		retval = m_byCTLA;
		// when CA2 is an output, IRQA2 = 0, and is not affected by CA2 transitions.
		if ( C2_OUTPUT( m_byCTLA ) )
			retval &= ~PIA_IRQ2;
		break;
		
		/******************* port B control read *******************/
	case PIA_CTLB:
		retval = m_byCTLB;
		// when CB2 is an output, IRQB2 = 0, and is not affected by CB2 transitions.
		if ( C2_OUTPUT( m_byCTLB ) )
			retval &= ~PIA_IRQ2;
		break;
		
	}

	return retval;
}

void C6821::Write(BYTE byRS, BYTE byData)
{
	byRS &= 3;

	switch( byRS )
	{
		/******************* port A output/DDR write *******************/
	case PIA_DDRA:
		
		// write output register
		if ( OUTPUT_SELECTED( m_byCTLA ) )
		{
			// update the output value
			m_byOA = byData;
			
			// send it to the output function
			if ( m_byDDRA )
				PIA_W_CALLBACK( m_stOutA, m_byOA & m_byDDRA );
		}
		
		// write DDR register
		else
		{
			if ( m_byDDRA != byData )
			{
				m_byDDRA = byData;
				
				// send it to the output function
				if ( m_byDDRA )
					PIA_W_CALLBACK( m_stOutA, m_byOA & m_byDDRA );
			}
		}
		break;
		
		/******************* port B output/DDR write *******************/
	case PIA_DDRB:

		// write output register
		if ( OUTPUT_SELECTED( m_byCTLB ) )
		{
			// update the output value
			m_byOB = byData;
			
			// send it to the output function
			if ( m_byDDRB )
				PIA_W_CALLBACK( m_stOutB, m_byOB & m_byDDRB );
			
			// CB2 is configured as output and in write strobe mode
			if ( C2_OUTPUT( m_byCTLB ) && C2_STROBE_MODE( m_byCTLB ) )
			{
				// this will cause a transition low; call the output function if we're currently high
				if ( m_byOCB2 )
					PIA_W_CALLBACK( m_stOutCB2, 0 );
				m_byOCB2 = 0;
					
				// if the CB2 strobe is cleared by the E, reset it right away
				if ( STROBE_E_RESET( m_byCTLB ) )
				{
					PIA_W_CALLBACK( m_stOutCB2, 1 );
					m_byOCB2 = 1;
				}
			}
		}
		// write DDR register
		else
		{
			if ( m_byDDRB != byData )
			{
				m_byDDRB = byData;
				
				// send it to the output function
				if ( m_byDDRB )
					PIA_W_CALLBACK( m_stOutB, m_byOB & m_byDDRB );
			}
		}
		break;
		
		/******************* port A control write *******************/
	case PIA_CTLA:
		// Bit 7 and 6 read only
		byData &= 0x3f;
		
		// CA2 is configured as output and in set/reset mode
		if ( C2_OUTPUT( byData ) )
		{
			// determine the new value
			int temp = SET_C2( byData ) ? 1 : 0;
			
			// if this creates a transition, call the CA2 output function
			if ( m_byOCA2 ^ temp)
				PIA_W_CALLBACK( m_stOutCA2, temp );
				
			// set the new value
			m_byOCA2 = temp;
		}
		
		// update the control register
		m_byCTLA = ( m_byCTLA & ~0x3F ) | byData;
		
		// update externals
		UpdateInterrupts();
		break;
		
		/******************* port B control write *******************/
	case PIA_CTLB:
		
		/* Bit 7 and 6 read only - PD 16/01/00 */
		
		byData &= 0x3f;

		// CB2 is configured as output and in set/reset mode
		if ( C2_OUTPUT( byData ) )
		{
			// determine the new value
			int temp = SET_C2( byData ) ? 1 : 0;
			
			// if this creates a transition, call the CA2 output function
			if ( m_byOCB2 ^ temp)
				PIA_W_CALLBACK( m_stOutCB2, temp );
				
			// set the new value
			m_byOCB2 = temp;
		}
		
		// update the control register
		m_byCTLB = ( m_byCTLB & ~0x3F ) | byData;
		
		// update externals
		UpdateInterrupts();
		break;
	}
	
}

void C6821::Reset()
{
	m_byIA		= 0;
	m_byCA1		= 0;
	m_byICA2	= 0;
	m_byOA		= 0;
	m_byOCA2	= 0;
	m_byDDRA	= 0;
	m_byCTLA	= 0;
	m_byIRQAState	= 0;

	m_byIB		= 0;
	m_byCB1		= 0;
	m_byICB2	= 0;
	m_byOB		= 0;
	m_byOCB2	= 0;
	m_byDDRB	= 0;
	m_byCTLB	= 0;
	m_byIRQBState	= 0;
}

void C6821::UpdateInterrupts()
{
	BYTE byNewState;

	// start with IRQ A
	byNewState = 0;
	if ( ( IRQ1( m_byCTLA ) && IRQ1_ENABLED( m_byCTLA ) ) ||
		 ( IRQ2( m_byCTLA ) && IRQ2_ENABLED( m_byCTLA ) ) )
		 byNewState = 1;

	if ( byNewState != m_byIRQAState )
	{
		m_byIRQAState = byNewState;
		PIA_W_CALLBACK( m_stOutIRQA, m_byIRQAState );
	}

	/* then do IRQ B */
	byNewState = 0;
	if ( ( IRQ1( m_byCTLB ) && IRQ1_ENABLED( m_byCTLB ) ) ||
		 ( IRQ2( m_byCTLB ) && IRQ2_ENABLED( m_byCTLB ) ) )
		 byNewState = 1;

	if ( byNewState != m_byIRQBState )
	{
		m_byIRQBState = byNewState;
		PIA_W_CALLBACK( m_stOutIRQB, m_byIRQBState );
	}
}

void C6821::SetCA1(BYTE byData)
{
	byData = byData ? 1 : 0;

	// the new state has caused a transition
	if ( m_byCA1 ^ byData )
	{
		// handle the active transition
		if ( ( byData && C1_LOW_TO_HIGH( m_byCTLA ) ) ||
			( !byData && C1_HIGH_TO_LOW( m_byCTLA ) ) )
		{
			// mark the IRQ
			SET_IRQ1(m_byCTLA);

			// update externals
			UpdateInterrupts();

			// CA2 is configured as output and in read strobe mode and cleared by a CA1 transition
			if ( C2_OUTPUT( m_byCTLA ) && C2_STROBE_MODE( m_byCTLA ) && STROBE_C1_RESET( m_byCTLA ) )
			{
				// call the CA2 output function
				if ( !m_byOCA2 )
					PIA_W_CALLBACK( m_stOutCA2, 1 );

				// clear CA2
				m_byOCA2 = 1;
			}
		}
	}

	// set the new value for CA1
	m_byCA1 = byData;
}

void C6821::SetCA2(BYTE byData)
{
	byData = byData ? 1 : 0;

	// CA2 is in input mode
	if ( C2_INPUT( m_byCTLA ) )
	{
		// the new state has caused a transition
		if ( m_byICA2 ^ byData )
		{
			// handle the active transition
			if ( ( byData && C2_LOW_TO_HIGH( m_byCTLA ) ) ||
				( !byData && C2_HIGH_TO_LOW( m_byCTLA ) ) )
			{
				// mark the IRQ
				SET_IRQ2( m_byCTLA );

				// update externals
				UpdateInterrupts();
			}
		}
	}

	// set the new value for CA2
	m_byICA2 = byData;
}

void C6821::SetCB1(BYTE byData)
{
	byData = byData ? 1 : 0;

	// the new state has caused a transition
	if ( m_byCB1 ^ byData )
	{
		// handle the active transition
		if ( ( byData && C1_LOW_TO_HIGH( m_byCTLB ) ) ||
			( !byData && C1_HIGH_TO_LOW( m_byCTLB ) ) )
		{
			// mark the IRQ
			SET_IRQ1( m_byCTLB );

			// update externals
			UpdateInterrupts();

			// CB2 is configured as output and in read strobe mode and cleared by a CA1 transition
			if ( C2_OUTPUT( m_byCTLB ) && C2_STROBE_MODE( m_byCTLB ) && STROBE_C1_RESET( m_byCTLB ) )
			{
				// the IRQ1 flag must have also been cleared
				if ( !IRQ1( m_byCTLB ) )
				{
					// call the CB2 output function
					if ( !m_byOCB2 )
						PIA_W_CALLBACK( m_stOutCB2, 1 );

					// clear CB2
					m_byOCB2 = 1;
				}
			}
		}
	}

	// set the new value for CA1
	m_byCB1 = byData;

}

void C6821::SetCB2(BYTE byData)
{
	byData = byData ? 1 : 0;

	// CA2 is in input mode
	if ( C2_INPUT( m_byCTLB ) )
	{
		// the new state has caused a transition
		if ( m_byICB2 ^ byData )
		{
			// handle the active transition
			if ( ( byData && C2_LOW_TO_HIGH( m_byCTLB ) ) ||
				( !byData && C2_HIGH_TO_LOW( m_byCTLB ) ) )
			{
				// mark the IRQ
				SET_IRQ2( m_byCTLB );

				// update externals
				UpdateInterrupts();
			}
		}
	}

	// set the new value for CA2
	m_byICB2 = byData;
}

void C6821::SetPA(BYTE byData)
{
	m_byIA = byData;
}

void C6821::SetPB(BYTE byData)
{
	m_byIB = byData;
}

BYTE C6821::GetPA()
{
	return m_byOA & m_byDDRA;
}

BYTE C6821::GetPB()
{
	return m_byOB & m_byDDRB;
}
