/*
 * blaster.h -- the Sound Blaster Apogee's AdLib MIDI driver (al_midi.c)
 * asks about, in the EiN build: a Sound Blaster 16, whose OPL3 is at
 * 0x220 and 0x222, the chip music_ein.c emulates.
 */
#ifndef EIN_BLASTER_H
#define EIN_BLASTER_H

enum BLASTER_ERRORS
{
	BLASTER_Warning = -2,
	BLASTER_Error = -1,
	BLASTER_Ok = 0
};

enum BLASTER_Types
{
	SB = 1,
	SBPro = 2,
	SB20 = 3,
	SBPro2 = 4,
	SB16 = 6
};

typedef struct
{
	unsigned Address;
	unsigned Type;
	unsigned Interrupt;
	unsigned Dma8;
	unsigned Dma16;
	unsigned Midi;
	unsigned Emu;
} BLASTER_CONFIG;

static int BLASTER_GetCardSettings(BLASTER_CONFIG *Config)
{
	Config->Address = 0x220;
	Config->Type = SB16;
	return BLASTER_Ok;
}

static int BLASTER_GetEnv(BLASTER_CONFIG *Config)
{
	return BLASTER_GetCardSettings(Config);
}

#endif
