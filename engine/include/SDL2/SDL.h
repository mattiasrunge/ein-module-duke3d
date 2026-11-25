/*
 * SDL.h -- the little of SDL the game code itself names, for the EiN
 * build, which has no SDL: the reverb's lock, which a single thread does
 * not need, and the mouse's capture, which is the page's.
 */
#ifndef EIN_SDL_SHIM_H
#define EIN_SDL_SHIM_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int SDL_mutex;
typedef enum { SDL_FALSE = 0, SDL_TRUE = 1 } SDL_bool;
typedef struct SDL_Surface SDL_Surface;

#define SDL_CreateMutex() ((SDL_mutex *) 0)
#define SDL_DestroyMutex(mutex) ((void) (mutex))
#define SDL_mutexP(mutex) ((void) (mutex))
#define SDL_mutexV(mutex) ((void) (mutex))

#define SDL_GetRelativeMouseMode() SDL_TRUE
#define SDL_SetRelativeMouseMode(mode) ((void) (mode))

#define SDL_Quit() ((void) 0)

#endif
