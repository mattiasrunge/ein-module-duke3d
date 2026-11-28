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

// driver_ein.cpp -- MultiVoc's sound card for EiN's engine worker, in place
// of driver_sdl.cpp, by its names, so the driver table and FX_Init take it
// as SDL's. It never plays on its own: the worker pulls the sound with
// ein_audio_render, which has the mixer mix as many of its buffers as the
// frames asked for take and keeps what is left of the last for the next
// pull. The music, OPL3 or the other MIDI drivers, is mixed into the same
// buffers by the mixer. The rate is the page's, set by ein_audio_rate before
// the game starts; the sound is 16-bit stereo.

#include "driver_sdl.h"

#include "compat.h"
#include "multivoc.h"

#include <emscripten.h>

enum
{
    EINErr_Error   = -1,
    EINErr_Ok      = 0,
    EINErr_Uninitialised,
};

static int ErrorCode = EINErr_Ok;
static int Initialised;
static int Playing;

static char *MixBuffer;
static int MixBufferSize;
static int MixBufferCount;
static int MixBufferCurrent;
static int MixBufferUsed;
static void (*MixCallBack)(void);

static int Rate = 48000;

// When the worker last pulled sound, and up to when the engine has mixed and
// dropped it itself since, in milliseconds.
static double lastPull;
static double caughtUp;

char SDLAudioDriverName[16];

// The page's sample rate, which the mixer mixes at; before the game starts.
extern "C" EMSCRIPTEN_KEEPALIVE void ein_audio_rate(int rate)
{
    Rate = rate;
}

// Fill `out` with the next `frames` frames: the rest of the buffer mixed
// last, then as many new ones as it takes.
static void render(int16_t *out, int frames)
{
    auto ptr = (char *)out;
    int remaining = frames * 4;

    if (!Playing || !MixBuffer || !MixCallBack)
    {
        memset(out, 0, remaining);
        return;
    }

    while (remaining > 0)
    {
        if (MixBufferUsed == MixBufferSize)
        {
            MixCallBack();
            MixBufferUsed = 0;

            if (++MixBufferCurrent >= MixBufferCount)
                MixBufferCurrent -= MixBufferCount;
        }

        int len = min(remaining, MixBufferSize - MixBufferUsed);

        memcpy(ptr, MixBuffer + (MixBufferCurrent * MixBufferSize) + MixBufferUsed, len);
        ptr += len;
        MixBufferUsed += len;
        remaining -= len;
    }
}

// The worker pulls the next `frames` frames into `at`, interleaved 16-bit stereo.
extern "C" EMSCRIPTEN_KEEPALIVE void ein_audio_render(int16_t *at, int frames)
{
    render(at, frames);
    lastPull = emscripten_get_now();
}

// While the worker pulls no sound (the page has none), mix the time gone by
// and drop it, so a sound the game waits for still ends; at each tick.
void ein_audio_catch_up(void)
{
    static int16_t scratch[1024 * 2];
    double const now = emscripten_get_now();

    if (now - lastPull < 250)
        return;

    // From where the worker stopped, but never more than a second.
    if (caughtUp < lastPull)
        caughtUp = lastPull;
    if (now - caughtUp > 1000)
        caughtUp = now - 1000;

    int frames = (int)((now - caughtUp) * Rate / 1000);
    caughtUp += (double)frames * 1000 / Rate;

    while (frames > 0)
    {
        int const count = min(frames, 1024);
        render(scratch, count);
        frames -= count;
    }
}

int SDLDrv_GetError(void) { return ErrorCode; }

const char *SDLDrv_ErrorString(int ErrorNumber)
{
    switch (ErrorNumber)
    {
        case EINErr_Error:         return SDLDrv_ErrorString(ErrorCode);
        case EINErr_Ok:            return "EiN audio ok.";
        case EINErr_Uninitialised: return "EiN audio uninitialized.";
        default:                   return "Unknown EiN audio error code.";
    }
}

void SDLDrv_PCM_PrintDrivers(void)
{
    LOG_F(INFO, "Available audio drivers:");
    LOG_F(INFO, "ein");
}

int SDLDrv_PCM_CheckDriverName(char const *dev) { return !Bstrcasecmp(dev, "ein"); }

char const *SDLDrv_PCM_GetDriverName(void) { return "ein"; }

int SDLDrv_PCM_Init(int *mixrate, int *numchannels, void *initdata)
{
    UNREFERENCED_PARAMETER(initdata);

    *mixrate = Rate;
    *numchannels = 2;
    lastPull = caughtUp = emscripten_get_now();
    Initialised = 1;

    VLOG_F(LOG_ASS, "Using the EiN page's audio at %d Hz", Rate);

    return EINErr_Ok;
}

void SDLDrv_PCM_Shutdown(void)
{
    Initialised = 0;
    Playing = 0;
}

int SDLDrv_PCM_BeginPlayback(char *BufferStart, int BufferSize, int NumDivisions, void (*CallBackFunc)(void))
{
    if (!Initialised)
    {
        ErrorCode = EINErr_Uninitialised;
        return EINErr_Error;
    }

    MixBuffer = BufferStart;
    MixBufferSize = BufferSize;
    MixBufferCount = NumDivisions;
    MixBufferCurrent = 0;
    MixBufferUsed = 0;
    MixCallBack = CallBackFunc;

    // prime the buffer
    MixCallBack();

    Playing = 1;

    return EINErr_Ok;
}

void SDLDrv_PCM_StopPlayback(void)
{
    Playing = 0;
}

// The mixer runs only when the worker pulls, never beside the game.
void SDLDrv_PCM_Lock(void) {}
void SDLDrv_PCM_Unlock(void) {}
