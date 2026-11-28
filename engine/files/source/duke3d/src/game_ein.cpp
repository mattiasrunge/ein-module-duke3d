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

// game_ein.cpp -- what EiN's engine worker asks of the game itself, between
// its ticks: whether a menu or the console is up and its settings written;
// and a new player's settings, for one who has no eduke32.cfg yet.

#include "duke3d.h"
#include "osd.h"

#include <emscripten.h>

// Whether a menu of the game's or its console is up.
extern "C" EMSCRIPTEN_KEEPALIVE int ein_menu(void)
{
    return (g_player[myconnectindex].ps && (g_player[myconnectindex].ps->gm & MODE_MENU)) || (osd && (osd->flags & OSD_CAPTURE));
}

// Write eduke32.cfg and settings.cfg now, as quitting does, so the player's
// settings are kept when the page stops the game without it.
extern "C" EMSCRIPTEN_KEEPALIVE void ein_write_config(void)
{
    CONFIG_WriteSetup(0);
}

static void G_EinMapKey(int function, char const *key1, char const *key2)
{
    auto &key = ud.config.KeyboardKeys[function];
    int const old1 = key[0], old2 = key[1];

    key[0] = KB_StringToScanCode(key1);
    key[1] = KB_StringToScanCode(key2);
    CONFIG_MapKey(function, key[0], old1, key[1], old2);
}

static void G_EinMapButton(int button, int function)
{
    ud.config.MouseFunctions[button][0] = function;
    CONTROL_MapButton(function, button, 0, controldevice_mouse);
}

// A new player's settings, after CONFIG_SetDefaults: Polymost at 1280 by 720
// (in true colour, filtered trilinearly and anisotropically, widescreen, as
// EiN's build has it for every player), mouse aiming on; EDuke32's modern keys
// with Crouch on C and Fire on Ctrl, as the module's other Duke has them, and
// its mouse: the left button fires, the right opens, the middle walks forward,
// the wheel changes weapon.
void G_EinNewPlayer(void)
{
    ud.setup.xdim = 1280;
    ud.setup.ydim = 720;
    ud.setup.bpp = 32;
    ud.setup.fullscreen = 0;

    g_myAimMode = 1;
    g_player[0].ps->aim_mode = 1;

    G_EinMapKey(gamefunc_Toggle_Crouch, "", "");
    G_EinMapKey(gamefunc_Crouch, "C", "");
    G_EinMapKey(gamefunc_Fire, "LCtrl", "RCtrl");
    G_EinMapKey(gamefunc_Jump, "Space", "/");
    G_EinMapKey(gamefunc_Open, "E", "");
    G_EinMapKey(gamefunc_Quick_Kick, "Q", "");

    // The engine's mouse buttons: 0 left, 1 right, 2 middle, 4 and 5 the wheel up and down.
    G_EinMapButton(0, gamefunc_Fire);
    G_EinMapButton(1, gamefunc_Open);
    G_EinMapButton(2, gamefunc_Move_Forward);
    G_EinMapButton(4, gamefunc_Next_Weapon);
    G_EinMapButton(5, gamefunc_Previous_Weapon);
}
