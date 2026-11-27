/*
Copyright (C) 2026 The ein-module-duke3d authors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

*/
// music_ein.c -- the music for the module's engine worker, as Duke
// Nukem 3D played it on a Sound Blaster 16: the songs' MIDI through
// Apogee's own sequencer (midi.c) and AdLib driver (al_midi.c), with the
// game's timbre bank, into an OPL3, which Nuked OPL3 emulates. The sequencer was a timer
// task; here its task runs on the samples' clock as dsl_ein.c pulls the
// music, and the chip's output is added to the effects.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "music.h"
#include "dsl.h"
#include "filesystem.h"
#include "al/al_midi.h"
#include "al/midi.h"
#include "al/task_man.h"
#include "opl/opl3.h"

int MUSIC_ErrorCode = MUSIC_Ok;

// SoundBlaster in Apogee's sndcards.h, which the game's own, shorter one
// of the same name keeps out of this file.
#define SOUND_BLASTER 0

// midi.c's EMIDI plays the tracks meant for the card this names.
int MUSIC_SoundDevice = SOUND_BLASTER;

volatile int TS_InInterrupt = 0;

static opl3_chip chip;
// The register the next write to a bank's data port goes to.
static uint16_t chip_register = 0;

// The song playing, as the game's file was; midi.c reads it as it plays.
static unsigned char *song = NULL;

// The sequencer's task, and the output frames until it runs next, in
// 1/65536ths.
static task *sequencer = NULL;
static int64_t until_tick = 0;

static int initialised = 0;

// The chip's output beside the effects: Nuked OPL3 plays the songs about
// 12 dB under Apogee's mixer, where a Sound Blaster 16's mixer had them
// about level.
#define MUSIC_GAIN 4

// The chip's half- and quarter-sine waves carry a steady offset, which
// the sound card's output capacitor took off: a high-pass at about 5 Hz
// does the same here, on each side's last input and output.
static double dc_in[2], dc_out[2];

static midifuncs fm = {
	.NoteOff = AL_NoteOff,
	.NoteOn = AL_NoteOn,
	.ControlChange = AL_ControlChange,
	.ProgramChange = AL_ProgramChange,
	.PitchBend = AL_SetPitchBend,
};

// The Sound Blaster 16's FM ports, base 0x220: register then data, at
// +0 and +1 for the first bank and +2 and +3 for the second.
void ein_outp(int port, int value)
{
	switch (port & 3)
	{
	case 0:
		chip_register = value & 0xff;
		break;
	case 1:
		OPL3_WriteRegBuffered(&chip, chip_register, value);
		break;
	case 2:
		chip_register = 0x100 | (value & 0xff);
		break;
	case 3:
		OPL3_WriteRegBuffered(&chip, chip_register, value);
		break;
	}
}

// The task manager, for midi.c's one task: it runs at `rate` a second.
task *TS_ScheduleTask(void (*Function)(task *), int rate, int priority, void *data)
{
	task *scheduled = calloc(1, sizeof(task));

	scheduled->TaskService = Function;
	scheduled->rate = rate;
	scheduled->priority = priority;
	scheduled->data = data;
	sequencer = scheduled;
	until_tick = 0;
	return scheduled;
}

int TS_Terminate(task *ptr)
{
	if (ptr == sequencer)
		sequencer = NULL;
	free(ptr);
	return TASK_Ok;
}

void TS_Dispatch(void)
{
}

void TS_SetTaskRate(task *Task, int rate)
{
	Task->rate = rate;
}

void TS_Shutdown(void)
{
}

char *MUSIC_ErrorString(int ErrorNumber)
{
	return ErrorNumber == MUSIC_Ok ? "Music ok." : "Music error.";
}

int MUSIC_Init(int SoundCard, int Address)
{
	int i;

	for (i = 0; i < 128; i++)
		MIDI_PatchMap[i] = i;
	// The chip runs at the page's rate, which dsl_ein.c has by now.
	OPL3_Reset(&chip, DSL_GetPlaybackRate());
	AL_Init(SOUND_BLASTER);
	MIDI_SetMidiFuncs(&fm);
	initialised = 1;
	return MUSIC_Ok;
}

int MUSIC_Shutdown(void)
{
	MUSIC_StopSong();
	AL_Shutdown();
	initialised = 0;
	return MUSIC_Ok;
}

void MUSIC_SetMaxFMMidiChannel(int channel)
{
	AL_SetMaxMidiChannel(channel);
}

void MUSIC_SetVolume(int volume)
{
	MIDI_SetVolume(volume);
}

void MUSIC_SetMidiChannelVolume(int channel, int volume)
{
	MIDI_SetUserChannelVolume(channel, volume);
}

void MUSIC_ResetMidiChannelVolumes(void)
{
	MIDI_ResetUserChannelVolume();
}

int MUSIC_GetVolume(void)
{
	return MIDI_GetVolume();
}

void MUSIC_SetLoopFlag(int loopflag)
{
	MIDI_SetLoopFlag(loopflag);
}

int MUSIC_SongPlaying(void)
{
	return MIDI_SongPlaying();
}

void MUSIC_Continue(void)
{
	MIDI_ContinueSong();
}

void MUSIC_Pause(void)
{
	MIDI_PauseSong();
}

int MUSIC_StopSong(void)
{
	MIDI_StopSong();
	free(song);
	song = NULL;
	return MUSIC_Ok;
}

// The game names a song by its file, in the GRP or beside it.
int MUSIC_PlaySong(char *songFilename, int loopflag)
{
	int32_t fd, size;

	if (!initialised)
		return MUSIC_Error;
	MUSIC_StopSong();
	fd = kopen4load(songFilename, 0);
	if (fd == -1)
	{
		printf("The music '%s' cannot be found in the GRP or the filesystem.\n", songFilename);
		return MUSIC_Error;
	}
	size = kfilelength(fd);
	song = malloc(size);
	kread(fd, song, size);
	kclose(fd);
	if (MIDI_PlaySong(song, loopflag) != MIDI_Ok)
	{
		printf("The music '%s' is not a MIDI file the sequencer plays.\n", songFilename);
		free(song);
		song = NULL;
		return MUSIC_Error;
	}
	return MUSIC_Ok;
}

void PlayMusic(char *fileName)
{
	MUSIC_PlaySong(fileName, MUSIC_LoopSong);
}

void MUSIC_SetContext(int context)
{
	MIDI_SetContext(context);
}

int MUSIC_GetContext(void)
{
	return MIDI_GetContext();
}

void MUSIC_SetSongTick(uint32_t PositionInTicks)
{
	MIDI_SetSongTick(PositionInTicks);
}

void MUSIC_SetSongTime(uint32_t milliseconds)
{
	MIDI_SetSongTime(milliseconds);
}

void MUSIC_SetSongPosition(int measure, int beat, int tick)
{
	MIDI_SetSongPosition(measure, beat, tick);
}

void MUSIC_GetSongPosition(songposition *pos)
{
	MIDI_GetSongPosition(pos);
}

void MUSIC_GetSongLength(songposition *pos)
{
	MIDI_GetSongLength(pos);
}

// The game never fades; a fade is the volume at once.
int MUSIC_FadeVolume(int tovolume, int milliseconds)
{
	MIDI_SetVolume(tovolume);
	return MUSIC_Ok;
}

int MUSIC_FadeActive(void)
{
	return 0;
}

void MUSIC_StopFade(void)
{
}

void MUSIC_RerouteMidiChannel(int channel, int cdecl (*function)(int event, int c1, int c2))
{
	MIDI_RerouteMidiChannel(channel, function);
}

void MUSIC_RegisterTimbreBank(unsigned char *timbres)
{
	AL_RegisterTimbreBank(timbres);
}

// Add `count` frames of the music to `out`, interleaved 16-bit stereo at
// `rate`, the page's: the chip's output, with the sequencer's task run
// whenever its time comes.
void ein_music_render(int16_t *out, int count, int rate)
{
	const double pole = 1 - 2 * 3.14159265358979 * 5 / rate;
	int16_t frame[2];
	int i, side, sum;

	if (!initialised)
		return;
	for (i = 0; i < count; i++)
	{
		while (sequencer && sequencer->rate > 0 && until_tick <= 0)
		{
			sequencer->TaskService(sequencer);
			if (sequencer)
				until_tick += ((int64_t) rate << 16) / sequencer->rate;
		}
		until_tick -= 1 << 16;
		OPL3_GenerateResampled(&chip, frame);
		for (side = 0; side < 2; side++)
		{
			dc_out[side] = frame[side] - dc_in[side] + pole * dc_out[side];
			dc_in[side] = frame[side];
			sum = out[i * 2 + side] + (int) (dc_out[side] * MUSIC_GAIN);
			out[i * 2 + side] = sum > 32767 ? 32767 : sum < -32768 ? -32768 : sum;
		}
	}
}
