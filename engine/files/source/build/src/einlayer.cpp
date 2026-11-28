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

// einlayer.cpp -- the platform layer for EiN's engine worker, in place of
// sdlayer.cpp: no SDL window, no SDL events and no DOM. SDL2 is linked for
// its timer and spinlocks only.
//
// The game keeps its own loops, so it runs under ASYNCIFY: each finished
// frame (videoShowFrame) waits for the worker's next tick, and a loop that
// polls without drawing waits for one at each handleevents, so every turn
// of a loop costs one tick and no more. Between ticks the worker hands over
// keys as the PC keyboard's scancodes, typed characters, the mouse's
// movement, buttons and wheel, which wait in a queue for the game's next
// handleevents; it pulls the sound (driver_ein.cpp) and takes the frame from
// its OffscreenCanvas (Module.canvas) whenever ein_frames moves on.
//
// The frame is drawn with WebGL 2, through the fixed-function layer
// glbuild_ein.cpp: at 32 bpp by Polymost, at 8 bpp by the classic renderer,
// whose frame glsurface puts on the screen. The canvas takes the video mode's
// size.

#include <emscripten.h>
#include <emscripten/html5.h>

#include "a.h"
#include "build.h"
#include "cache1d.h"
#include "compat.h"
#include "build_cpuid.h"
#include "engine_priv.h"
#include "osd.h"
#include "palette.h"
#include "renderlayer.h"
#include "communityapi.h"

#ifdef USE_OPENGL
# include "glad/glad.h"
# include "glbuild.h"
# include "glsurface.h"
# include "glbuild_ein.h"
#endif

extern "C" void *emscripten_GetProcAddress(const char *name);
void ein_audio_catch_up(void);

int32_t startwin_open(void) { return 0; }
int32_t startwin_close(void) { return 0; }
int32_t startwin_puts(const char *s) { UNREFERENCED_PARAMETER(s); return 0; }
int32_t startwin_idle(void *s) { UNREFERENCED_PARAMETER(s); return 0; }
int32_t startwin_settitle(const char *s) { UNREFERENCED_PARAMETER(s); return 0; }
int32_t startwin_run(void) { return 0; }
bool startwin_isopen(void) { return false; }

int32_t inputchecked = 0;

char quitevent = 0, appactive = 1, novideo = 0;

int32_t maxrefreshfreq = 0;
int32_t xres = -1, yres = -1, bpp = 0, fullscreen = 0, bytesperline;
// The rate of the worker's ticks, as measured (einTurn).
double refreshfreq = 60.0;
intptr_t frameplace = 0;
int32_t lockcount = 0;
char modechange = 1;
char offscreenrendering = 0;
char videomodereset = 0;
int32_t nofog = 0;
#ifdef USE_OPENGL
char nogl = 0;
#endif

bool g_ImGuiCaptureInput = false;
bool g_ImGuiFrameActive = false;
uint8_t g_ImGuiCapturedDevices;

static int vsync_renderlayer;

// The WebGL 2 context, made once on the worker's canvas: a canvas has one
// WebGL context, which every later video mode keeps.
static EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context;

// How many frames have been shown: the worker takes one when it moves on.
static uint32_t frames;

// Whether videoShowFrame has waited for a tick since handleevents last ran.
static bool turnTaken;

// Wait for the worker's next tick: the page's next animation frame, or,
// while the page is hidden, whenever it shows again.
EM_ASYNC_JS(void, ein_wait_frame, (void), {
    await Module.einFrame();
});

// The game has written the file at `path`: eduke32.cfg, settings.cfg or a
// saved game, for the page to keep.
EM_JS(void, ein_wrote, (const char *path), {
    if (Module.einWrote) Module.einWrote(UTF8ToString(path));
});

// platform/emscripten/ein-library.js: Module.canvas as "!canvas".
extern "C" void ein_canvas(void);

int32_t wm_msgbox(const char *name, const char *fmt, ...)
{
    char *buf = (char *)Balloca(MSGBOX_PRINTF_MAX);
    va_list va;

    va_start(va, fmt);
    Bvsnprintf(buf, MSGBOX_PRINTF_MAX, fmt, va);
    va_end(va);
    buf[MSGBOX_PRINTF_MAX-1] = 0;

    LOG_F(WARNING, "%s: %s", name, buf);
    return 0;
}

// There is no one to ask: the answer is no.
int32_t wm_ynbox(const char *name, const char *fmt, ...)
{
    char *buf = (char *)Balloca(MSGBOX_PRINTF_MAX);
    va_list va;

    va_start(va, fmt);
    Bvsnprintf(buf, MSGBOX_PRINTF_MAX, fmt, va);
    va_end(va);
    buf[MSGBOX_PRINTF_MAX-1] = 0;

    LOG_F(WARNING, "%s: %s (no)", name, buf);
    return 0;
}

void wm_setapptitle(const char *name)
{
    if (name != apptitle)
        Bstrncpyz(apptitle, name, sizeof(apptitle));
}

int main(int argc, char *argv[])
{
    engineSetupAllocator();
    engineSetupLogging(argc, argv);
    return app_main(argc, argv);
}

int32_t initsystem(void)
{
    sysReadCPUID();
    timerInit(CLOCKTICKSPERSECOND);

    frameplace = 0;
    lockcount = 0;
    g_numdisplays = 1;

    return 0;
}

void uninitsystem(void)
{
    uninitinput();
    timerUninit();
}

void system_getcvars(void)
{
    vsync = videoSetVsync(vsync);
}

int debugprintf(const char *f, ...)
{
    UNREFERENCED_PARAMETER(f);
    return 0;
}


//
// Input
//

// What the worker handed over since the game last read it, in the order it came.
enum
{
    EIN_KEY,
    EIN_CHAR,
    EIN_BUTTON,
    EIN_WHEEL,
};

struct einevent_t
{
    uint8_t type;
    uint8_t pressed;
    uint16_t value;
};

#define EIN_QUEUE_SIZE 256

static einevent_t eventQueue[EIN_QUEUE_SIZE];
static unsigned eventRead, eventWrite;

// The mouse's movement since the game last read the queue.
static vec2_t mouseMoved;

// Keys that went down since the last tick, by the engine's numbering: a key
// that goes down and up again within one would be down for no frame at all,
// so its release, and what came after, wait for the next tick.
static uint8_t fresh[NUMKEYS];

// Keys held, by the engine's numbering, before the OSD and the remap table.
static uint8_t held[NUMKEYS];

static void einQueue(int type, int pressed, int value)
{
    if (((eventWrite + 1) % EIN_QUEUE_SIZE) == eventRead)
        return;
    eventQueue[eventWrite] = { (uint8_t)type, (uint8_t)!!pressed, (uint16_t)value };
    eventWrite = (eventWrite + 1) % EIN_QUEUE_SIZE;
}

// The PC keyboard's set 1 scancode, 0xE000 above it for an extended key, as
// the engine numbers keys: the same, 0x80 above it for an extended one.
static int einKeyCode(int scancode)
{
    return (scancode & 0x7f) | ((scancode & 0xff00) ? 0x80 : 0);
}

// The names the menus show for keys, by the engine's numbering, as SDL's.
static struct { uint8_t code; char const *name; } const keyNames[] = {
    { 0x01, "Escape" }, { 0x02, "1" }, { 0x03, "2" }, { 0x04, "3" }, { 0x05, "4" }, { 0x06, "5" },
    { 0x07, "6" }, { 0x08, "7" }, { 0x09, "8" }, { 0x0a, "9" }, { 0x0b, "0" }, { 0x0c, "-" }, { 0x0d, "=" },
    { 0x0e, "Backspace" }, { 0x0f, "Tab" }, { 0x10, "Q" }, { 0x11, "W" }, { 0x12, "E" }, { 0x13, "R" },
    { 0x14, "T" }, { 0x15, "Y" }, { 0x16, "U" }, { 0x17, "I" }, { 0x18, "O" }, { 0x19, "P" }, { 0x1a, "[" },
    { 0x1b, "]" }, { 0x1c, "Return" }, { 0x1d, "Left Ctrl" }, { 0x1e, "A" }, { 0x1f, "S" }, { 0x20, "D" },
    { 0x21, "F" }, { 0x22, "G" }, { 0x23, "H" }, { 0x24, "J" }, { 0x25, "K" }, { 0x26, "L" }, { 0x27, ";" },
    { 0x28, "'" }, { 0x29, "`" }, { 0x2a, "Left Shift" }, { 0x2b, "\\" }, { 0x2c, "Z" }, { 0x2d, "X" },
    { 0x2e, "C" }, { 0x2f, "V" }, { 0x30, "B" }, { 0x31, "N" }, { 0x32, "M" }, { 0x33, "," }, { 0x34, "." },
    { 0x35, "/" }, { 0x36, "Right Shift" }, { 0x37, "Keypad *" }, { 0x38, "Left Alt" }, { 0x39, "Space" },
    { 0x3a, "CapsLock" }, { 0x3b, "F1" }, { 0x3c, "F2" }, { 0x3d, "F3" }, { 0x3e, "F4" }, { 0x3f, "F5" },
    { 0x40, "F6" }, { 0x41, "F7" }, { 0x42, "F8" }, { 0x43, "F9" }, { 0x44, "F10" }, { 0x45, "Numlock" },
    { 0x46, "ScrollLock" }, { 0x47, "Keypad 7" }, { 0x48, "Keypad 8" }, { 0x49, "Keypad 9" },
    { 0x4a, "Keypad -" }, { 0x4b, "Keypad 4" }, { 0x4c, "Keypad 5" }, { 0x4d, "Keypad 6" },
    { 0x4e, "Keypad +" }, { 0x4f, "Keypad 1" }, { 0x50, "Keypad 2" }, { 0x51, "Keypad 3" },
    { 0x52, "Keypad 0" }, { 0x53, "Keypad ." }, { 0x54, "SysReq" }, { 0x56, "<" }, { 0x57, "F11" },
    { 0x58, "F12" }, { 0x59, "Pause" }, { 0x9c, "Keypad Enter" }, { 0x9d, "Right Ctrl" },
    { 0xb5, "Keypad /" }, { 0xb8, "Right Alt" }, { 0xc7, "Home" }, { 0xc8, "Up" }, { 0xc9, "PageUp" },
    { 0xcb, "Left" }, { 0xcd, "Right" }, { 0xcf, "End" }, { 0xd0, "Down" }, { 0xd1, "PageDown" },
    { 0xd2, "Insert" }, { 0xd3, "Delete" }, { 0xdb, "Left GUI" }, { 0xdc, "Right GUI" }, { 0xdd, "Menu" },
};

void joyScanDevices(void)
{
    inputdevices &= ~DEV_JOYSTICK;
}

int32_t initinput(void (*hotplugCallback)(void) /*= NULL*/)
{
    UNREFERENCED_PARAMETER(hotplugCallback);

    inputdevices = DEV_KEYBOARD | DEV_MOUSE;
    g_mouseGrabbed = false;

    memset(g_keyNameTable, 0, sizeof(g_keyNameTable));
    for (auto const &key : keyNames)
        Bstrncpyz(g_keyNameTable[key.code], key.name, sizeof(g_keyNameTable[0]));

    return 0;
}

void uninitinput(void)
{
    mouseUninit();
}

const char *joyGetName(int32_t what, int32_t num)
{
    UNREFERENCED_PARAMETER(what);
    UNREFERENCED_PARAMETER(num);
    return NULL;
}

void mouseInit(void)
{
    mouseGrabInput(g_mouseEnabled = g_mouseLockedToWindow);
}

void mouseUninit(void)
{
    mouseGrabInput(0);
    g_mouseEnabled = 0;
}

// The page locks the pointer: the mouse always moves the game by how far it went.
void mouseGrabInput(bool grab)
{
    if (grab != g_mouseGrabbed)
        g_mousePos.x = g_mousePos.y = 0;
    g_mouseGrabbed = grab;
}

void mouseLockToWindow(char a)
{
    if (!(a & 2))
    {
        mouseGrabInput(a);
        g_mouseLockedToWindow = g_mouseGrabbed;
    }
}

// A key the worker handed over, as SDL_KEYDOWN and SDL_KEYUP are handled:
// Return, Escape, Backspace, Tab and Ctrl with a letter type a character too.
static void einHandleKey(int code, int pressed)
{
    int const ctrlAlone = held[0x1d] && !held[0x9d] && !held[0x2a] && !held[0x36] && !held[0x38] && !held[0xb8];

    held[code] = pressed;

    if (pressed && !keyBufferFull())
    {
        char typed = 0;

        switch (code)
        {
            case 0x1c: case 0x9c: typed = '\r'; break;
            case 0x01: typed = 27; break;
            case 0x0e: typed = '\b'; break;
            case 0x0f: typed = '\t'; break;
            default:
                if (ctrlAlone && code < 0x80 && g_keyAsciiTable[code] >= 'a' && g_keyAsciiTable[code] <= 'z')
                    typed = g_keyAsciiTable[code] - 'a' + 1;
                break;
        }

        if (typed && OSD_HandleChar(typed))
            keyBufferInsert(typed);
    }

    int j = OSD_HandleScanCode(code, pressed);

    if (j <= 0)
    {
        // The console's key went down: every key goes up.
        if (j == -1)
        {
            for (j = 0; j < NUMKEYS; ++j)
            {
                if (keyGetState(j) && keypresscallback)
                    keypresscallback(j, 0);
                keySetState(j, 0);
            }
        }
        return;
    }

    if (pressed)
    {
        if (!keyGetState(code) && keypresscallback)
            keypresscallback(code, 1);
        keySetState(code, 1);
    }
    else
    {
        // Pause goes up as soon as it goes down: the game sees it pressed.
        if (code == 0x59)
            return;
        keySetState(code, 0);
        if (keypresscallback)
            keypresscallback(code, 0);
    }
}

// A typed character, as SDL_TEXTINPUT: the console takes it first, then the
// game's key buffer, which the menus' text fields read.
static void einHandleChar(int code)
{
    if (code < 32 || code > 126 || code == g_keyAsciiTable[OSD_OSDKey()] || keyBufferFull())
        return;
    if (OSD_HandleChar(code))
        keyBufferInsert(code);
}

// A mouse button by the browser's numbering (MouseEvent.button: 0 left,
// 1 middle, 2 right, 3 back, 4 forward), as the engine's bits have them.
static void einHandleButton(int button, int pressed)
{
    static int8_t const bits[] = { 0, 2, 1, 3, 6 };

    if ((unsigned)button >= ARRAY_SIZE(bits))
        return;

    int const j = bits[button];

    if (j == 0)
        g_mouseClickState = pressed ? MOUSE_PRESSED : MOUSE_RELEASED;

    if (pressed)
        g_mouseBits |= (1 << j);
    else
        g_mouseBits &= ~(1 << j);

    if (g_mouseCallback)
        g_mouseCallback(j + 1, pressed);
}

// A notch of the wheel: its buttons go down now and up at the next
// handleevents after the game has read the input.
static void einHandleWheel(int up)
{
    g_mouseBits |= up ? 16 : 32;
    if (g_mouseCallback)
        g_mouseCallback(up ? 5 : 6, 1);
}

// Hand the game what came since it last asked.
static void einReadEvents(void)
{
    if (g_mouseGrabbed && appactive)
    {
        g_mousePos.x += mouseMoved.x;
        g_mousePos.y += mouseMoved.y;
    }
    mouseMoved = {};

    while (eventRead != eventWrite)
    {
        einevent_t const &ev = eventQueue[eventRead];

        switch (ev.type)
        {
            case EIN_KEY:
                if (!ev.pressed && fresh[ev.value])
                    return;
                if (ev.pressed)
                    fresh[ev.value] = 1;
                einHandleKey(ev.value, ev.pressed);
                break;
            case EIN_CHAR: einHandleChar(ev.value); break;
            case EIN_BUTTON: einHandleButton(ev.value, ev.pressed); break;
            case EIN_WHEEL: einHandleWheel(ev.value); break;
        }

        eventRead = (eventRead + 1) % EIN_QUEUE_SIZE;
    }
}

// How many ticks the game has waited for.
static uint32_t ticks;

static void einTurn(void)
{
    static double lastTick;

    ein_wait_frame();
    ticks++;

    double const now = emscripten_get_now(), interval = now - lastTick;
    lastTick = now;
    if (interval > 0 && interval < 250)
        refreshfreq = clamp(refreshfreq * 0.9 + 100.0 / interval, 24.0, 1000.0);

    ein_audio_catch_up();
    memset(fresh, 0, sizeof(fresh));
}

// Whether a loop that throttles itself is due a frame (engineFPSLimit): one
// that waits at each turn at every tick, one that works between them, as
// the loading screen does, once a tick's time has gone by.
bool einFrameDue(void)
{
    static uint32_t lastTicks;
    static double lastTime;
    double const now = emscripten_get_now();

    if (ticks == lastTicks && now - lastTime < 1000.0 / refreshfreq)
        return false;

    lastTicks = ticks;
    lastTime = now;
    return true;
}

int32_t handleevents_peekkeys(void)
{
    for (unsigned i = eventRead; i != eventWrite; i = (i + 1) % EIN_QUEUE_SIZE)
        if (eventQueue[i].type == EIN_KEY && eventQueue[i].pressed)
            return 1;
    return 0;
}

static int32_t einHandleEvents(bool wait)
{
    if (inputchecked && g_mouseEnabled)
    {
        if (g_mouseCallback)
        {
            if (g_mouseBits & 16)
                g_mouseCallback(5, 0);
            if (g_mouseBits & 32)
                g_mouseCallback(6, 0);
        }

        OSD_HandleWheel();
        g_mouseBits &= ~(16 | 32);
    }

    // A loop that only polls waits for a tick each turn; one that drew has waited already.
    if (wait)
    {
        if (!turnTaken)
            einTurn();
        turnTaken = false;
    }

    einReadEvents();

    inputchecked = 0;
    timerUpdateClock();
    communityapiRunCallbacks();

    return 0;
}

int32_t handleevents(void)
{
    return einHandleEvents(true);
}

// The input since the game last read it, without waiting: the game reads
// its input (CONTROL_GetInput) inside a turn of a loop, whose handleevents
// or frame waits.
int32_t handleevents_input(void)
{
    return einHandleEvents(false);
}

// A key went down (`pressed` 1) or up (0): `scancode` is the PC keyboard's
// set 1 one, 0xE000 above it for an extended key (0xE048 the up arrow).
extern "C" EMSCRIPTEN_KEEPALIVE void ein_key(int pressed, int scancode)
{
    einQueue(EIN_KEY, pressed, einKeyCode(scancode));
}

// A character typed, as Unicode: the console's and the menus' text.
extern "C" EMSCRIPTEN_KEEPALIVE void ein_char(int unicode)
{
    if (unicode > 0 && unicode < 128)
        einQueue(EIN_CHAR, 1, unicode);
}

// The mouse moved by `dx` and `dy` pixels.
extern "C" EMSCRIPTEN_KEEPALIVE void ein_mouse(int dx, int dy)
{
    mouseMoved.x += dx;
    mouseMoved.y += dy;
}

// A mouse button went down or up, by MouseEvent.button.
extern "C" EMSCRIPTEN_KEEPALIVE void ein_mouse_button(int pressed, int button)
{
    einQueue(EIN_BUTTON, pressed, button);
}

// The wheel turned a notch, up (1) or down (0).
extern "C" EMSCRIPTEN_KEEPALIVE void ein_wheel(int up)
{
    einQueue(EIN_WHEEL, 1, !!up);
}

// How many frames have been shown.
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t ein_frames(void)
{
    return frames;
}


//
// Video
//

static char modeschecked;

static int sortmodes(const void *a_, const void *b_)
{
    auto a = (const struct validmode_t *)b_;
    auto b = (const struct validmode_t *)a_;

    int x;

    if ((x = a->fs   - b->fs)   != 0) return x;
    if ((x = a->bpp  - b->bpp)  != 0) return x;
    if ((x = a->xdim - b->xdim) != 0) return x;
    if ((x = a->ydim - b->ydim) != 0) return x;

    return 0;
}

// The modes the video options offer, all in a window, which the page
// shows: 4:3, 16:9 and 16:10 up to 1920 by 1200, at 8 bpp for the classic
// renderer and 32 for Polymost.
void videoGetModes(int display)
{
    static vec2_t const modes[] = {
        { 640, 480 },   { 800, 600 },   { 1024, 768 },  { 1280, 720 },  { 1280, 800 },
        { 1366, 768 },  { 1600, 900 },  { 1680, 1050 }, { 1920, 1080 }, { 1920, 1200 },
    };

    UNREFERENCED_PARAMETER(display);

    if (modeschecked)
        return;

    validmodecnt = 0;

    for (auto const &mode : modes)
    {
        SDL_ADDMODE(mode.x, mode.y, 8, 0);
#ifdef USE_OPENGL
        if (!nogl)
            SDL_ADDMODE(mode.x, mode.y, 32, 0);
#endif
    }

    qsort((void *)validmode, validmodecnt, sizeof(struct validmode_t), &sortmodes);

    modeschecked = 1;
}

// As sdlayer's, with every mode windowed.
int32_t videoCheckMode(int32_t *x, int32_t *y, int32_t c, int32_t fs, int32_t forced)
{
    int32_t i, nearest = -1, dx, dy, odx = 9999, ody = 9999;

    UNREFERENCED_PARAMETER(fs);
    UNREFERENCED_PARAMETER(forced);

    videoGetModes();

    if (c > 8 && nogl)
        return -1;

    *x = clamp(*x & ~1, 320, MAXXDIM);
    *y = clamp(*y & ~1, 200, MAXYDIM);

    for (i = 0; i < validmodecnt; i++)
    {
        if (validmode[i].bpp != c)
            continue;

        dx = klabs(validmode[i].xdim - *x);
        dy = klabs(validmode[i].ydim - *y);

        if (!(dx | dy))
        {
            nearest = i;
            break;
        }

        if ((dx <= odx) && (dy <= ody))
        {
            nearest = i;
            odx = dx;
            ody = dy;
        }
    }

    if (nearest < 0)
        return -1;

    *x = validmode[nearest].xdim;
    *y = validmode[nearest].ydim;

    return nearest;
}

char const *videoGetDisplayName(int display)
{
    UNREFERENCED_PARAMETER(display);
    return "EiN";
}

void engineBeginImGuiFrame(void) {}
void engineEndImGuiInput(void) { keyFlushChars(); keyFlushScans(); }
void engineBeginImGuiInput(void) { keyFlushChars(); keyFlushScans(); }

int32_t videoSetVsync(int32_t newSync)
{
    vsync_renderlayer = newSync;
    return newSync;
}

#ifdef USE_OPENGL
// The context on the worker's canvas and the GL functions from it, once.
static int einCreateContext(void)
{
    EmscriptenWebGLContextAttributes attributes;

    emscripten_webgl_init_context_attributes(&attributes);
    attributes.majorVersion = 2;
    attributes.minorVersion = 0;
    attributes.alpha = EM_FALSE;
    attributes.depth = EM_TRUE;
    attributes.stencil = EM_TRUE;
    attributes.antialias = EM_FALSE;
    attributes.premultipliedAlpha = EM_FALSE;
    attributes.preserveDrawingBuffer = EM_FALSE;
    attributes.powerPreference = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;

    ein_canvas();
    context = emscripten_webgl_create_context("!canvas", &attributes);
    if (context <= 0)
    {
        LOG_F(ERROR, "This browser has no WebGL 2 in a worker.");
        context = 0;
        return -1;
    }

    emscripten_webgl_make_context_current(context);

    gladLoadGLLoader(emscripten_GetProcAddress);
    glbuild_ein_init();

    LOG_F(INFO, "WebGL context: GL_VERSION \"%s\", glad %d.%d", (char const *)glGetString(GL_VERSION), GLVersion.major, GLVersion.minor);

    if (GLVersion.major < 2)
    {
        LOG_F(ERROR, "Video driver does not support OpenGL version 2 or greater; all OpenGL modes are unavailable.");
        return -1;
    }

    return 0;
}
#endif

int32_t videoSetMode(int32_t x, int32_t y, int32_t c, int32_t fs)
{
    UNREFERENCED_PARAMETER(fs);

    if (x == xres && y == yres && c == bpp && !videomodereset)
        return 0;

    if (videoCheckMode(&x, &y, c, 0, 0) < 0)
        return -1;

    int const regrab = g_mouseGrabbed;

    if (regrab)
        mouseGrabInput(0);

    while (lockcount)
        videoEndDrawing();

#ifdef USE_OPENGL
    if (!context && !nogl && einCreateContext())
        nogl = 1;

    if (nogl)
    {
        LOG_F(ERROR, "Unable to set video mode %dx%d: no WebGL 2.", x, y);
        return -1;
    }

    if (bpp == 8)
        glsurface_destroy();
#endif

    VLOG_F(LOG_GFX, "Setting video mode %dx%d (%d-bpp).", x, y, c);

    emscripten_set_canvas_element_size("!canvas", x, y);

    xres = x;
    yres = y;
    bpp = c;
    fullscreen = 0;
    numpages = c > 8 ? 2 : 1;
    frameplace = 0;
    lockcount = 0;
    modechange = 1;
    videomodereset = 0;

#ifdef USE_OPENGL
    glsurface_destroy();
    glShadeModel(GL_SMOOTH);
    glClearColor(0, 0, 0, 1.0);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glViewport(0, 0, xres, yres);
    fill_glinfo();
#endif

    // Gamma, contrast and saturation are the renderer's colour correction.
    gammabrightness = 1;

    videoFadePalette(palfadergb.r, palfadergb.g, palfadergb.b, palfadedelta);

    if (regrab)
        mouseGrabInput(g_mouseLockedToWindow);

    return 0;
}

void videoResetMode(void)
{
    videomodereset = 1;
    modeschecked = 0;
}

#ifdef DEBUG_FRAME_LOCKING
uint32_t begindrawing_line[BEGINDRAWING_SIZE];
const char *begindrawing_file[BEGINDRAWING_SIZE];
void begindrawing_real(void)
#else
void videoBeginDrawing(void)
#endif
{
    if (bpp > 8)
    {
        if (offscreenrendering) return;
        frameplace = 0;
        bytesperline = 0;
        modechange = 0;
        return;
    }

    if (lockcount++ > 0)
        return;

    static intptr_t backupFrameplace = 0;

    if (inpreparemirror)
    {
        if (offscreenrendering)
        {
            if (!backupFrameplace)
                backupFrameplace = frameplace;
            else if (frameplace != (intptr_t)mirrorBuffer &&
                     frameplace != backupFrameplace)
                return;
        }

        frameplace = (intptr_t)mirrorBuffer;

        if (offscreenrendering)
            return;
    }
    else if (offscreenrendering)
    {
        if (backupFrameplace)
        {
            frameplace = backupFrameplace;
            backupFrameplace = 0;
        }
        return;
    }
    else
    {
#ifdef USE_OPENGL
        frameplace = (intptr_t)glsurface_getBuffer();
#endif
    }

    if (modechange)
    {
        bytesperline = xdim;
        calc_ylookup(bytesperline, ydim);
        modechange = 0;
    }
}

void videoEndDrawing(void)
{
    if (bpp > 8)
    {
        if (!offscreenrendering) frameplace = 0;
        return;
    }

    if (!frameplace) return;
    if (lockcount > 1) { lockcount--; return; }
    if (!offscreenrendering) frameplace = 0;
    if (lockcount == 0) return;
    lockcount = 0;
}

// The frame is done: the worker takes it from the canvas once the game waits for its next tick.
void videoShowFrame(int32_t w)
{
#ifdef USE_OPENGL
    if (nogl)
        return;

    if (bpp > 8)
    {
        if (palfadedelta)
            fullscreen_tint_gl(palfadergb.r, palfadergb.g, palfadergb.b, palfadedelta);
    }
    else
    {
        if (lockcount)
        {
            LOG_F(WARNING, "Frame was still locked at a depth of %d when videoShowFrame() was called!", lockcount);
            while (lockcount) videoEndDrawing();
        }
        glsurface_blitBuffer();
    }

    glbuild_ein_flush();
    frames++;
    einTurn();
    turnTaken = true;

    if (w != -1)
    {
        if (bpp > 8)
            polymost_resetState();
        else
            glsurface_refresh();
    }
#else
    UNREFERENCED_PARAMETER(w);
#endif
}

int32_t videoUpdatePalette(int32_t start, int32_t num)
{
    UNREFERENCED_PARAMETER(start);
    UNREFERENCED_PARAMETER(num);

    if (bpp > 8)
        return 0;

#ifdef USE_OPENGL
    if (!nogl)
        glsurface_setPalette(curpalettefaded);
#endif

    return 0;
}

int32_t videoSetGamma(void)
{
#ifdef USE_OPENGL
    if (!nogl)
    {
        g_glColorCorrection = { g_videoGamma, g_videoContrast, g_videoSaturation, 0.f };

        if (videoGetRenderMode() == REND_POLYMOST)
            polymost_setColorCorrection(g_glColorCorrection);
    }
#endif
    return 0;
}
