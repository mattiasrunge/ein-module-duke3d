/*
 * conio.h -- the PC's I/O ports for Apogee's AdLib MIDI driver
 * (al_midi.c) and sequencer (midi.c) in the EiN build: the FM chip's
 * ports are music_ein.c's OPL3 emulator, and nothing else is there to
 * read.
 */
#ifndef EIN_DOS_H
#define EIN_DOS_H

void ein_outp(int port, int value);

#define outp(port, value) ein_outp((port), (value))
#define inp(port) (0)

// No interrupts to hold off: the music runs between the game's turns.
#define _enable() ((void) 0)
#define _disable() ((void) 0)

// Watcom's, which Apogee's code takes from here.
#ifndef max
#define max(x, y) (((x) > (y)) ? (x) : (y))
#endif
#ifndef min
#define min(x, y) (((x) < (y)) ? (x) : (y))
#endif

#endif
