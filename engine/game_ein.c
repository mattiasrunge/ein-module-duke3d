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
// game_ein.c -- what the worker asks of the game itself, between its
// turns: whether a menu or the console is up, and its settings written.

#include <emscripten.h>

#include "duke3d.h"
#include "console.h"

// Whether a menu of the game's or its console is up: the worker writes
// the settings as it closes, where they change.
EMSCRIPTEN_KEEPALIVE
int ein_menu(void)
{
	return (ps[myconnectindex].gm & MODE_MENU) != 0 || CONSOLE_IsActive();
}

// Write duke3d.cfg now, as quitting does, so the player's settings are
// kept when the page stops the game without it.
EMSCRIPTEN_KEEPALIVE
void ein_write_config(void)
{
	CONFIG_WriteSetup();
}
