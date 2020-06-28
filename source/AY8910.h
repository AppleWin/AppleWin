#ifndef AY8910_H
#define AY8910_H

#define MAX_8910 4

//-------------------------------------
// MAME interface

void _AYWriteReg(int chip, int r, int v);
//void AY8910_write_ym(int chip, int addr, int data);
void AY8910_reset(int chip);
void AY8910Update(int chip, INT16** buffer, int nNumSamples);

void AY8910_InitAll(int nClock, int nSampleRate);
void AY8910_InitClock(int nClock);
BYTE* AY8910_GetRegsPtr(UINT uChip);

void AY8910UpdateSetCycles();

UINT AY8910_SaveSnapshot(class YamlSaveHelper& yamlSaveHelper, UINT uChip, const std::string& suffix);
UINT AY8910_LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT uChip, const std::string& suffix);

//-------------------------------------
// FUSE stuff

typedef ULONG libspectrum_dword;
typedef UCHAR libspectrum_byte;
typedef SHORT libspectrum_signed_word;

/* max. number of sub-frame AY port writes allowed;
 * given the number of port writes theoretically possible in a
 * 50th I think this should be plenty.
 */
#define AY_CHANGE_MAX		8000

class CAY8910
{
public:
	CAY8910();
	virtual ~CAY8910() {};

	void sound_ay_init( void );
	void sound_init( const char *device );
	void sound_ay_write( int reg, int val, libspectrum_dword now );
	void sound_ay_reset( void );
	void sound_frame( void );
	BYTE* GetAYRegsPtr( void ) { return &sound_ay_registers[0]; }
	static void SetCLK( double CLK ) { m_fCurrentCLK_AY8910 = CLK; }
	void SaveSnapshot(class YamlSaveHelper& yamlSaveHelper, const std::string& suffix);
	bool LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, const std::string& suffix);

private:
	void init( void );
	void sound_end( void );
	void sound_ay_overlay( void );

private:
	/* foo_subcycles are fixed-point with low 16 bits as fractional part.
	 * The other bits count as the chip does.
	 */
	unsigned int ay_tone_tick[3], ay_tone_high[3], ay_noise_tick;
	unsigned int ay_tone_subcycles, ay_env_subcycles;
	unsigned int ay_env_internal_tick, ay_env_tick;
	unsigned int ay_tick_incr;
	unsigned int ay_tone_period[3], ay_noise_period, ay_env_period;

	//static int beeper_last_subpos[2] = { 0, 0 };

	/* Local copy of the AY registers */
	libspectrum_byte sound_ay_registers[16];

	struct ay_change_tag
	{
		libspectrum_dword tstates;
		unsigned short ofs;
		unsigned char reg, val;
	};

	struct ay_change_tag ay_change[ AY_CHANGE_MAX ];
	int ay_change_count;

	// statics from sound_ay_overlay()
	int rng;
	int noise_toggle;
	int env_first, env_rev, env_counter;

	// Vars shared between all AY's
	static double m_fCurrentCLK_AY8910;
};

#endif
