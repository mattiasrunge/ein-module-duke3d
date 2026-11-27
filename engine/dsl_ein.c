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
// dsl_ein.c -- the sound card for the module's engine worker, in place
// of Chocolate Duke3D's SDL_mixer one (dsl.c). Apogee's mixer
// (multivoc.c) mixes the effects a buffer at a time into a ring of
// buffers, as it did for a Sound Blaster's DMA; here the page plays
// them: at each tick the worker calls ein_audio_render for the time gone
// by since the last, which hands over the rest of the buffer last mixed
// and has the mixer mix the next ones, and hands the samples to the page.
// The music (music_ein.c) is mixed into the same samples. The rate is
// the page's, set by ein_audio_rate before the game starts.

#include <stdint.h>
#include <string.h>

#include <emscripten.h>

#include "dsl.h"
#include "util.h"

extern volatile int MV_MixPage;

// The music's synthesiser, music_ein.c: adds `count` frames of the song
// playing to `out`.
void ein_music_render(int16_t *out, int count, int rate);

static int error_code = DSL_Ok;
static int playing = 0;
static int rate = 48000;

static void (*mix_next)(void);
static volatile char *buffer_start;
// Bytes in each of the mixer's buffers, and how many of them are left to
// play of the one mixed last.
static int buffer_size;
static int remainder;
static int stereo;
static int sixteen_bit;

// When the worker last pulled samples, and up to when the engine has
// mixed them itself since, in milliseconds.
static double last_pull = 0;
static double caught_up = 0;

// The page's sample rate, which the mixer mixes at; before the game starts.
EMSCRIPTEN_KEEPALIVE
void ein_audio_rate(int samplerate)
{
	rate = samplerate;
}

char *DSL_ErrorString(int ErrorNumber)
{
	switch (ErrorNumber)
	{
	case DSL_Warning:
	case DSL_Error:
		return DSL_ErrorString(error_code);
	case DSL_Ok:
		return "Sound driver ok.";
	case DSL_MixerActive:
		return "The sound is already playing.";
	default:
		return "Unknown sound driver error.";
	}
}

int DSL_Init(void)
{
	error_code = DSL_Ok;
	return DSL_Ok;
}

void DSL_Shutdown(void)
{
	DSL_StopPlayback();
}

int DSL_BeginBufferedPlayback(char *BufferStart, int BufferSize, int NumDivisions, unsigned SampleRate, int MixMode, void (*CallBackFunc)(void))
{
	if (playing)
	{
		error_code = DSL_MixerActive;
		return DSL_Error;
	}
	mix_next = CallBackFunc;
	buffer_start = BufferStart;
	buffer_size = BufferSize / NumDivisions;
	remainder = 0;
	stereo = (MixMode & STEREO) != 0;
	sixteen_bit = (MixMode & SIXTEEN_BIT) != 0;
	playing = 1;
	return DSL_Ok;
}

void DSL_StopPlayback(void)
{
	playing = 0;
}

// The mixer mixes at the page's rate, whatever the setup asked for.
unsigned DSL_GetPlaybackRate(void)
{
	return rate;
}

// The sample at byte `at` of the current buffer, as 16-bit signed.
static int16_t sample_at(const volatile char *at)
{
	if (sixteen_bit)
		return *(const volatile int16_t *) at;
	return (int16_t) ((((const volatile uint8_t *) at)[0] - 128) << 8);
}

// Fill `out` with the next `count` frames, interleaved 16-bit stereo:
// the effects, then the music added to them.
static void render(int16_t *out, int count)
{
	int bytes_per_sample = sixteen_bit ? 2 : 1;
	int bytes_per_frame = bytes_per_sample * (stereo ? 2 : 1);
	int i;

	if (!playing)
		memset(out, 0, count * 4);
	else
		for (i = 0; i < count; i++)
		{
			const volatile char *at;

			if (remainder <= 0)
			{
				mix_next();
				remainder = buffer_size;
			}
			at = &buffer_start[MV_MixPage * buffer_size + (buffer_size - remainder)];
			out[i * 2] = sample_at(at);
			out[i * 2 + 1] = stereo ? sample_at(at + bytes_per_sample) : out[i * 2];
			remainder -= bytes_per_frame;
		}
	ein_music_render(out, count, rate);
}

// The worker pulls the next `count` frames into `out`.
EMSCRIPTEN_KEEPALIVE
void ein_audio_render(int16_t *out, int count)
{
	render(out, count);
	last_pull = emscripten_get_now();
}

// While the worker pulls no sound (the page has none), mix the time
// gone by and drop it, so a sound the
// game waits for still ends. display_ein.c calls this on every turn.
void ein_audio_catch_up(void)
{
	static int16_t scratch[1024 * 2];
	double now = emscripten_get_now();
	int frames;

	if (now - last_pull < 250)
		return;
	// Mixed from where the worker stopped, but never more than a second.
	if (caught_up < last_pull)
		caught_up = last_pull;
	if (now - caught_up > 1000)
		caught_up = now - 1000;
	frames = (int) ((now - caught_up) * rate / 1000);
	caught_up += (double) frames * 1000 / rate;
	while (frames > 0)
	{
		int count = frames < 1024 ? frames : 1024;

		render(scratch, count);
		frames -= count;
	}
}
