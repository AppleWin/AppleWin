#pragma once

/*
 * mc6821.h - MC6821 emulation for the 1571 disk drives with DD3.
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

typedef void (*mem_write_handler) (void* objTo, BYTE byData);

typedef struct
{
	void* objTo;
	mem_write_handler func;
} STWriteHandler;

struct mc6821_s {
	/* MC6821 register.  */
	BYTE pra;
	BYTE ddra;
	BYTE cra;
	BYTE prb;
	BYTE ddrb;
	BYTE crb;

	/* Drive structure */
//    struct drive_s *drive;
};
typedef struct mc6821_s mc6821_t;


class C6821
{
public:
	C6821()
	{
		mc6821_reset();
		m_stOutA.objTo = NULL;
		m_stOutA.func = NULL;
		m_stOutB.objTo = NULL;
		m_stOutB.func = NULL;
	};

	~C6821() {};

	// AppleWin:TC
	void SetPA(BYTE byData) { m_byIA = byData; }
	void SetPB(BYTE byData) { m_byIB = byData; }
	void Reset() { mc6821_reset(); }
	BYTE Read( BYTE byRS ) { return mc6821_read_internal(byRS); }
	void Write( BYTE byRS, BYTE byData ) { mc6821_store_internal(byRS, byData); }

	void SetListenerA(void *objTo, mem_write_handler func)
	{
		m_stOutA.objTo = objTo;
		m_stOutA.func = func;
	}

	void SetListenerB(void *objTo, mem_write_handler func)
	{
		m_stOutB.objTo = objTo;
		m_stOutB.func = func;
	}
	void Get6821(mc6821_t& r6821, BYTE& byIA, BYTE& byIB)
	{
		r6821 = mc6821[0];
		byIA = m_byIA;
		byIB = m_byIB;
	}
	void Set6821(const mc6821_t& r6821, const BYTE byIA, const BYTE byIB)
	{
		mc6821[0] = r6821;
		m_byIA = byIA;
		m_byIB = byIB;
	}
	// AppleWin:TC END

private:
	/* Signal values (for signaling edges on the control lines)  */
	#define MC6821_SIG_CA1 0
	#define MC6821_SIG_CA2 1
	#define MC6821_SIG_CB1 2
	#define MC6821_SIG_CB2 3

	//struct drive_s;

	//struct drive_context_s;
	void mc6821_init(/*struct drive_context_s *drv*/);
	void mc6821_reset(/*struct drive_context_s *drv*/);
	//extern void mc6821_mem_init(struct drive_context_s *drv, unsigned int type);

	void mc6821_set_signal(/*struct drive_context_s *drive_context,*/ int line);

private:
	// AppleWin:TC
	void mc6821_write_pra(BYTE byte, unsigned int dnr=0);
	void mc6821_write_ddra(BYTE byte, unsigned int dnr=0);
	BYTE mc6821_read_pra(unsigned int dnr=0);
	void mc6821_write_prb(BYTE byte, unsigned int dnr=0);
	void mc6821_write_ddrb(BYTE byte, unsigned int dnr=0);
	BYTE mc6821_read_prb(unsigned int dnr=0);
	void mc6821_write_cra(BYTE byte, unsigned int dnr=0);
	void mc6821_write_crb(BYTE byte, unsigned int dnr=0);
	void mc6821_store_internal(WORD addr, BYTE byte, unsigned int dnr=0);
	BYTE mc6821_read_internal(WORD addr, unsigned int dnr=0);
	void mc6821_reset_internal(unsigned int dnr=0);
	// AppleWin:TC END

private:
	// AppleWin:TC
	STWriteHandler m_stOutA;
	STWriteHandler m_stOutB;
	BYTE	m_byIA;	// InputA from 285
	BYTE	m_byIB;	// InputB from 285
	// AppleWin:TC END

	/* mc6821 structure.  */
	#define DRIVE_NUM 1		// AppleWin:TC
	mc6821_t mc6821[DRIVE_NUM];
};
