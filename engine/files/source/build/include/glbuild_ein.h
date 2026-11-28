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

// glbuild_ein.h -- the fixed-function OpenGL the renderers use, on WebGL 2 (glbuild_ein.cpp).

#pragma once

#ifdef GLBUILD_EIN

// Points glad's functions at the layer, after gladLoadGLLoader on the current WebGL 2 context.
void glbuild_ein_init(void);

// Draws what is batched: before the frame is shown.
void glbuild_ein_flush(void);

// r_glbatch: whether consecutive draws with the same state are merged into one.
extern int32_t glbuild_ein_batch;

#endif
