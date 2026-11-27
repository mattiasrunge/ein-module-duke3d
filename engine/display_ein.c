/*
 * display_ein.c -- BUILD's display, keyboard, mouse and timer driver for
 * the module's engine worker, in place of Chocolate Duke3D's SDL one
 * (display.c): no SDL, no window, no DOM and no event loop of its own.
 *
 * The game keeps its blocking loops (the logos, the menus' waits, the
 * bonus screens), so the build runs it under Emscripten's ASYNCIFY: each
 * finished frame (_nextpage) waits for the worker's next tick, which the
 * page asks for at each of its animation frames, and a loop that only
 * polls (_handle_events, _idle) waits for one once a frame's time has
 * gone by. Between ticks the worker hands over keys, already as the PC
 * keyboard's scancodes, the mouse's movement and buttons, and pulls the
 * sound (dsl_ein.c); it takes the frame from ein_screen whenever
 * ein_frames moves on, at the size ein_width and ein_height give.
 *
 * Parts of it follow display.c, written by Ryan C. Gordon for BUILD.
 * "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
 * Ken Silverman's official web site: "http://www.advsys.net/ken"
 * See the included license file "BUILDLIC.TXT" for license info.
 * This file IS NOT A PART OF Ken Silverman's original release
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <emscripten.h>

#include "build.h"
#include "display.h"
#include "fixedPoint_math.h"
#include "engine.h"
#include "draw.h"
#include "cache.h"

int32_t xres, yres, bytesperline, imageSize, maxpages;
uint8_t *frameplace;
uint8_t *frameoffset;
uint8_t *screen, vesachecked;
int32_t buffermode, origbuffermode, linearmode;
uint8_t permanentupdate = 0, vgacompatible;

int32_t total_render_time = 1;
int32_t total_rendered_frames = 0;

// The frame BUILD draws into, one palette index a pixel, and its size.
static uint8_t *pixels = NULL;
static int width = 0;
static int height = 0;

// The frame as the page shows it: 0xAABBGGRR words, R first in memory,
// as a canvas's ImageData has its bytes.
static uint32_t *rgba = NULL;
static uint32_t colours[256];

// The palette last set, as Ken's entries (VBE_setPalette); the engine reads it back.
uint8_t lastPalette[768];

// How many frames have been shown: the worker takes one when it moves on.
static uint32_t frames = 0;

// Keys the worker handed over, as the PC keyboard sends them: a scancode,
// +128 when the key went up, after 0xE0 for an extended key. A tick
// reads them all, so this only needs to hold one tick's worth.
#define KEY_QUEUE_SIZE 256

static uint8_t key_queue[KEY_QUEUE_SIZE];
static unsigned int key_write = 0;
static unsigned int key_read = 0;
static unsigned int lastkey = 0;

// The mouse's movement since the game last asked, and its buttons as
// BUILD wants them (int 33h): 1 left, 2 right, 4 middle, 8 and 16 the
// wheel, which stay pressed until read.
static int mouse_dx = 0;
static int mouse_dy = 0;
static short mouse_buttons = 0;

static uint8_t drawpixel_color = 0;

// When the game last waited for a tick, in milliseconds.
static double last_turn = 0;

// Wait for the worker's next tick: the page's next animation frame, or,
// when the page is hidden, whenever it shows again.
EM_ASYNC_JS(void, ein_wait_frame, (void), {
	await Module.einFrame();
});

// The game has written the file at `path` (files.patch calls this): a
// saved game or duke3d.cfg, for the page to keep.
EM_JS(void, ein_wrote, (const char *path), {
	if (Module.einWrote) Module.einWrote(UTF8ToString(path));
});

// dsl_ein.c's: the sound the worker did not pull, mixed and dropped.
void ein_audio_catch_up(void);

static void take_turn(void)
{
	ein_wait_frame();
	ein_audio_catch_up();
	last_turn = emscripten_get_now();
}

// Keys that went down since the game last finished a frame, by
// scancode, 0x80 above it for an extended key.
static uint8_t fresh[256];

// Hand the game the keys that came since its last tick. A key that went
// down and up again before the game finished a frame would be down for
// no frame at all, so its release, and what came after, wait until the
// game has drawn one with the key down.
static void read_keys(void)
{
	while (key_read != key_write)
	{
		unsigned int at = key_read;
		int extended = key_queue[at] == 0xe0;
		uint8_t code;

		if (extended)
		{
			// The prefix and its key are queued together (ein_key).
			at = (at + 1) % KEY_QUEUE_SIZE;
		}
		code = key_queue[at];
		if ((code & 0x80) && fresh[(code & 0x7f) | (extended ? 0x80 : 0)])
			return;
		if (!(code & 0x80))
			fresh[code | (extended ? 0x80 : 0)] = 1;
		if (extended)
		{
			lastkey = 0xe0;
			keyhandler();
		}
		lastkey = code;
		keyhandler();
		key_read = (at + 1) % KEY_QUEUE_SIZE;
	}
}

void _handle_events(void)
{
	// A loop that only polls would never let the worker answer its
	// tick: after a frame's time it waits for the next one, and the keys
	// that came with it.
	if (emscripten_get_now() - last_turn > 16)
		take_turn();
	read_keys();
}

void _idle(void)
{
	take_turn();
	read_keys();
}

uint8_t _readlastkeyhit(void)
{
	return lastkey;
}

// The page's header has the fullscreen button: the video options' own
// FullScreen toggle, which calls this again, changes nothing.
void _platform_init(int argc, char **argv, const char *title, const char *iconName)
{
	srand((unsigned int) emscripten_get_now());
}

static void present(void)
{
	int i;

	if (!pixels)
		return;
	for (i = 0; i < width * height; i++)
		rgba[i] = colours[pixels[i]];
	frames++;
}

void *_getVideoBase(void)
{
	return pixels;
}

// As display.c's init_new_res_vars: the engine's view of the new mode.
static uint8_t screenalloctype = 255;
static void init_new_res_vars(void)
{
	int i, j;

	setupmouse();

	xdim = xres = width;
	ydim = yres = height;

	numpages = 1;
	bytesperline = width;
	vesachecked = 1;
	vgacompatible = 1;
	linearmode = 1;
	qsetmode = height;
	activepage = visualpage = 0;

	frameoffset = frameplace = pixels;

	if (screen != NULL)
	{
		if (screenalloctype == 0) free((void *) screen);
		if (screenalloctype == 1) suckcache((int32_t *) screen);
		screen = NULL;
	}

	j = ydim * 4 * sizeof(int32_t);
	if (horizlookup)
		free(horizlookup);
	if (horizlookup2)
		free(horizlookup2);
	horizlookup = (int32_t *) malloc(j);
	horizlookup2 = (int32_t *) malloc(j);

	j = 0;
	for (i = 0; i <= ydim; i++)
	{
		ylookup[i] = j;
		j += bytesperline;
	}

	horizycent = ((ydim * 4) >> 1);

	// drawrooms recalculates its aspect.
	oxyaspect = oxdimen = oviewingrange = -1;

	setBytesPerLine(bytesperline);
	setview(0L, 0L, xdim - 1, ydim - 1);
	setbrightness(curbrightness, palette);

	if (searchx < 0)
	{
		searchx = halfxdimen;
		searchy = (ydimen >> 1);
	}
}

static void add_mode(int w, int h)
{
	validmode[validmodecnt] = validmodecnt;
	validmodexdim[validmodecnt] = w;
	validmodeydim[validmodecnt] = h;
	validmodecnt++;
}

// The modes the video options offer: DOS's 320 by 200, which the page
// shows at 4:3 as a VGA monitor did, and square-pixel 4:3 ones up to
// what the renderer's tables hold.
void getvalidvesamodes(void)
{
	static int checked = 0;

	if (checked)
		return;
	checked = 1;
	validmodecnt = 0;
	vidoption = 1;
	add_mode(320, 200);
	add_mode(640, 480);
	add_mode(800, 600);
	if (MAXXDIM >= 1024 && MAXYDIM >= 768)
		add_mode(1024, 768);
}

int32_t _setgamemode(int32_t daxdim, int32_t daydim)
{
	int i, valid = 0;

	getvalidvesamodes();
	for (i = 0; i < validmodecnt; i++)
		if (validmodexdim[i] == daxdim && validmodeydim[i] == daydim)
			valid = 1;
	if (!valid)
	{
		printf("Resolution %dx%d unsupported. Changed to 640x480\n", daxdim, daydim);
		daxdim = 640;
		daydim = 480;
	}

	free(pixels);
	free(rgba);
	width = daxdim;
	height = daydim;
	pixels = calloc(width * height, 1);
	rgba = calloc(width * height, 4);
	init_new_res_vars();

	qsetmode = 200;
	return 0;
}

void setvmode(int mode)
{
}

void screencapture(char *filename)
{
}

void VBE_getPalette(int32_t start, int32_t num, uint8_t *palettebuffer)
{
	uint8_t *p = palettebuffer + (start * 4);
	int i;

	for (i = start; i < start + num; i++)
	{
		*p++ = ((colours[i] >> 16) & 0xff) * 63 / 255;
		*p++ = ((colours[i] >> 8) & 0xff) * 63 / 255;
		*p++ = (colours[i] & 0xff) * 63 / 255;
		*p++ = 0;
	}
}

// Ken's palette entries are blue, green, red and a spare byte, 0 to 63.
void VBE_setPalette(uint8_t *palettebuffer)
{
	uint8_t *p = palettebuffer;
	int i;

	memcpy(lastPalette, palettebuffer, 768);
	for (i = 0; i < 256; i++, p += 4)
		colours[i] = 0xff000000u | ((p[0] * 255 / 63) << 16) | ((p[1] * 255 / 63) << 8) | (p[2] * 255 / 63);
}

// A palette changed without a new frame, as the logos fade.
void VBE_presentPalette(void)
{
	present();
}

void _uninitengine(void)
{
}

void _updateScreenRect(int32_t x, int32_t y, int32_t w, int32_t h)
{
	present();
}

void _nextpage(void)
{
	// The frame drawn saw the keys that went down before it.
	memset(fresh, 0, sizeof(fresh));
	present();
	take_turn();
	read_keys();
	total_rendered_frames++;
}

uint8_t readpixel(uint8_t *location)
{
	return *location;
}

void drawpixel(uint8_t *location, uint8_t pixel)
{
	*location = pixel;
}

void setcolor16(uint8_t col)
{
	drawpixel_color = col;
}

void drawpixel16(int32_t offset)
{
	pixels[offset] = drawpixel_color;
}

void fillscreen16(int32_t offset, int32_t color, int32_t blocksize)
{
	if (!pageoffset)
		offset = (offset << 3) + 640 * 336;
	if (offset < 0)
		offset = 0;
	if (offset + blocksize > width * height)
		blocksize = width * height - offset;
	if (blocksize > 0)
		memset(pixels + offset, color, blocksize);
	_nextpage();
}

// The 2D map view's lines, a pixel at a time; nothing in the game itself
// draws them.
void drawline16(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t color)
{
	int32_t dx = abs(x2 - x1), dy = abs(y2 - y1);
	int32_t sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
	int32_t err = dx - dy, e2;

	if (!pageoffset)
	{
		y1 += 336;
		y2 += 336;
	}
	for (;;)
	{
		if (x1 >= 0 && x1 < width && y1 >= 0 && y1 < height)
			pixels[y1 * width + x1] = color;
		if (x1 == x2 && y1 == y2)
			break;
		e2 = 2 * err;
		if (e2 > -dy) { err -= dy; x1 += sx; }
		if (e2 < dx) { err += dx; y1 += sy; }
	}
}

void clear2dscreen(void)
{
	int rows = qsetmode == 350 ? 350 : (ydim16 <= 336 ? 336 : 480);

	if (rows > height)
		rows = height;
	memset(pixels, 0, width * rows);
}

int setupmouse(void)
{
	mouse_dx = mouse_dy = 0;
	moustat = 1;
	return 1;
}

void readmousexy(short *x, short *y)
{
	if (x)
		*x = mouse_dx << 2;
	if (y)
		*y = mouse_dy << 2;
	mouse_dx = mouse_dy = 0;
}

void readmousebstatus(short *bstatus)
{
	if (bstatus)
		*bstatus = mouse_buttons;
	// The wheel is a click: down until read once.
	mouse_buttons &= ~(8 | 16);
}

void _joystick_init(void)
{
}

void _joystick_deinit(void)
{
}

int _joystick_update(void)
{
	return 0;
}

int _joystick_axis(int axis)
{
	return 0;
}

int _joystick_hat(int hat)
{
	return -1;
}

int _joystick_button(int button)
{
	return 0;
}

// The timer, as display.c's: Ken's, sampled rather than an interrupt.
// totalclock moves on by the ticks gone by whenever the game samples it.
static double timer_start = 0;
static int32_t timerlastsample = 0;
static int timerticspersec = 0;
static void (*usertimercallback)(void) = NULL;

void (*installusertimercallback(void (*callback)(void)))(void)
{
	void (*old)(void) = usertimercallback;

	usertimercallback = callback;
	return old;
}

int inittimer(int tickspersecond)
{
	if (timerticspersec)
		return 0;
	timer_start = emscripten_get_now();
	timerticspersec = tickspersecond;
	timerlastsample = 0;
	usertimercallback = NULL;
	return 0;
}

void uninittimer(void)
{
	timerticspersec = 0;
}

void sampletimer(void)
{
	int32_t n;

	if (!timerticspersec)
		return;
	n = (int32_t) ((emscripten_get_now() - timer_start) * timerticspersec / 1000) - timerlastsample;
	if (n > 0)
	{
		totalclock += n;
		timerlastsample += n;
	}
	if (usertimercallback)
		for (; n > 0; n--)
			usertimercallback();
}

uint32_t getticks(void)
{
	return (uint32_t) emscripten_get_now();
}

int gettimerfreq(void)
{
	return timerticspersec;
}

// A key went down (`pressed` 1) or up (0): `scancode` is the PC
// keyboard's, 0xE0 above it for an extended key (0xE048 the up arrow).
// A full queue drops the key rather than overwrite one not yet read.
EMSCRIPTEN_KEEPALIVE
void ein_key(int pressed, int scancode)
{
	uint8_t bytes[2];
	int count = 0, i;

	if (scancode & 0xff00)
		bytes[count++] = 0xe0;
	bytes[count++] = (scancode & 0x7f) | (pressed ? 0 : 0x80);
	if ((key_write + KEY_QUEUE_SIZE - key_read) % KEY_QUEUE_SIZE + count >= KEY_QUEUE_SIZE)
		return;
	for (i = 0; i < count; i++)
	{
		key_queue[key_write] = bytes[i];
		key_write = (key_write + 1) % KEY_QUEUE_SIZE;
	}
}

// The mouse moved by `dx` and `dy` pixels.
EMSCRIPTEN_KEEPALIVE
void ein_mouse(int dx, int dy)
{
	mouse_dx += dx;
	mouse_dy += dy;
}

// A mouse button went down or up: `bit` is BUILD's, 1 left, 2 right, 4
// middle; 8 and 16 are the wheel up and down, which only go down.
EMSCRIPTEN_KEEPALIVE
void ein_mouse_button(int pressed, int bit)
{
	if (pressed)
		mouse_buttons |= bit;
	else
		mouse_buttons &= ~bit;
}

EMSCRIPTEN_KEEPALIVE
uint32_t *ein_screen(void)
{
	return rgba;
}

EMSCRIPTEN_KEEPALIVE
int ein_width(void)
{
	return width;
}

EMSCRIPTEN_KEEPALIVE
int ein_height(void)
{
	return height;
}

EMSCRIPTEN_KEEPALIVE
uint32_t ein_frames(void)
{
	return frames;
}
