#pragma once

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

class AY8913
{
public:
	AY8913(void);
	~AY8913(void) {};

	void sound_ay_init( void );
	void sound_init( const char *device );
	BYTE sound_ay_read( int reg );	// TC
	void sound_ay_write( int reg, int val, libspectrum_dword now );
	void sound_ay_reset( void );
	void sound_frame( void );
	BYTE* GetAYRegsPtr( void ) { return &sound_ay_registers[0]; }
	void SetFramesize(int frameSize) { sound_generator_framesiz = frameSize; }
	void SetSoundBuffers(INT16** buffers) { ppSoundBuffers = buffers; }
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

	// Vars
	libspectrum_signed_word** ppSoundBuffers;	// Used to pass param to sound_ay_overlay()
	int sound_generator_framesiz;
	int sound_generator_freq;
	unsigned int ay_tone_levels[16];

	// Vars shared between all AY's
	static double m_fCurrentCLK_AY8910;
};
