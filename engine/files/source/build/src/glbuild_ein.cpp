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

// glbuild_ein.cpp -- the fixed-function OpenGL that Polymost, the 2D drawing, the
// models and the classic renderer's glsurface use, implemented on WebGL 2.
//
// glbuild_ein_init() points glad's function pointers at the functions here, so the
// renderers' calls stay what they are. The layer keeps:
//  - matrix stacks (modelview, projection, a texture matrix per unit) and the
//    current colour and texture coordinates;
//  - client arrays (glVertexPointer, glTexCoordPointer, glColorPointer) read from
//    CPU memory or from a buffer given GL_STREAM_DRAW data, which stays on the CPU;
//  - immediate mode (glBegin/glEnd), GL_QUADS, GL_QUAD_STRIP and GL_POLYGON as
//    triangles;
//  - linear fog and the alpha test, GL_TEXTURE_2D and the other fixed-function
//    capabilities as state of its own.
//
// A program that has the attribute a_vertex (the polymost1 shaders, and the built-in
// one used while no program is bound) takes these as attributes a_vertex,
// a_texcoord0, a_color, a_texcoord3, a_texcoord4, a_fog (the fog's end and
// 1 / (end - start), the same for a whole draw) and uniforms u_modelView,
// u_projection, u_textureMatrix0/3/4, u_fogColor and u_alphaRef (a fragment is
// discarded at or below it). Other programs (glsurface's) draw with their own
// generic attributes, on the default vertex array object.
//
// To spend few WebGL calls a frame, the GL state the renderers set (capabilities,
// blending, depth, bound textures and their parameters, programs' uniforms) reaches
// WebGL only when a draw needs it and only where it differs from what WebGL has, and
// draws from CPU data are merged: their vertices go, as triangles (or lines or
// points) with their colour and fog, into one batch, which is drawn with one upload
// into a streaming buffer and one glDrawArrays when the state it was made with
// changes, or when glbuild_ein_flush() is called (r_glbatch 0 draws each at once).

#ifdef GLBUILD_EIN

#include <emscripten.h>

#include "compat.h"
#include "glad/glad.h"
#include "glbuild.h"
#include "glbuild_ein.h"

#include <cstddef>
#include <vector>

int32_t glbuild_ein_batch = 1;

// WebGL 2 itself, called directly rather than through glad's pointers.
extern "C" {
void emscripten_glActiveTexture(GLenum);
void emscripten_glAttachShader(GLuint, GLuint);
void emscripten_glBindAttribLocation(GLuint, GLuint, const GLchar *);
void emscripten_glBindBuffer(GLenum, GLuint);
void emscripten_glBindSampler(GLuint, GLuint);
void emscripten_glBindTexture(GLenum, GLuint);
void emscripten_glBindVertexArray(GLuint);
void emscripten_glBlendEquationSeparate(GLenum, GLenum);
void emscripten_glBlendFuncSeparate(GLenum, GLenum, GLenum, GLenum);
void emscripten_glBufferData(GLenum, GLsizeiptr, const void *, GLenum);
void emscripten_glBufferSubData(GLenum, GLintptr, GLsizeiptr, const void *);
void emscripten_glClear(GLbitfield);
void emscripten_glClearColor(GLfloat, GLfloat, GLfloat, GLfloat);
void emscripten_glColorMask(GLboolean, GLboolean, GLboolean, GLboolean);
void emscripten_glCompileShader(GLuint);
void emscripten_glCompressedTexImage2D(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *);
GLuint emscripten_glCreateProgram(void);
GLuint emscripten_glCreateShader(GLenum);
void emscripten_glCullFace(GLenum);
void emscripten_glDeleteBuffers(GLsizei, const GLuint *);
void emscripten_glDeleteProgram(GLuint);
void emscripten_glDeleteShader(GLuint);
void emscripten_glDeleteTextures(GLsizei, const GLuint *);
void emscripten_glDepthFunc(GLenum);
void emscripten_glDepthMask(GLboolean);
void emscripten_glDepthRangef(GLfloat, GLfloat);
void emscripten_glDisable(GLenum);
void emscripten_glDisableVertexAttribArray(GLuint);
void emscripten_glDrawArrays(GLenum, GLint, GLsizei);
void emscripten_glDrawElements(GLenum, GLsizei, GLenum, const void *);
void emscripten_glEnable(GLenum);
void emscripten_glEnableVertexAttribArray(GLuint);
void emscripten_glFinish(void);
void emscripten_glFlush(void);
void emscripten_glFrontFace(GLenum);
void emscripten_glGenBuffers(GLsizei, GLuint *);
void emscripten_glGenVertexArrays(GLsizei, GLuint *);
GLint emscripten_glGetAttribLocation(GLuint, const GLchar *);
void emscripten_glGetFloatv(GLenum, GLfloat *);
void emscripten_glGetIntegerv(GLenum, GLint *);
void emscripten_glGetProgramInfoLog(GLuint, GLsizei, GLsizei *, GLchar *);
void emscripten_glGetProgramiv(GLuint, GLenum, GLint *);
void emscripten_glGetShaderInfoLog(GLuint, GLsizei, GLsizei *, GLchar *);
void emscripten_glGetShaderiv(GLuint, GLenum, GLint *);
GLint emscripten_glGetUniformLocation(GLuint, const GLchar *);
void emscripten_glHint(GLenum, GLenum);
void emscripten_glLineWidth(GLfloat);
void emscripten_glLinkProgram(GLuint);
void emscripten_glPolygonOffset(GLfloat, GLfloat);
void emscripten_glReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *);
void emscripten_glScissor(GLint, GLint, GLsizei, GLsizei);
void emscripten_glShaderSource(GLuint, GLsizei, const GLchar *const *, const GLint *);
void emscripten_glStencilFunc(GLenum, GLint, GLuint);
void emscripten_glStencilOp(GLenum, GLenum, GLenum);
void emscripten_glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
void emscripten_glTexParameterf(GLenum, GLenum, GLfloat);
void emscripten_glTexParameteri(GLenum, GLenum, GLint);
void emscripten_glTexSubImage2D(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *);
void emscripten_glUniform1f(GLint, GLfloat);
void emscripten_glUniform1fv(GLint, GLsizei, const GLfloat *);
void emscripten_glUniform1i(GLint, GLint);
void emscripten_glUniform2f(GLint, GLfloat, GLfloat);
void emscripten_glUniform2fv(GLint, GLsizei, const GLfloat *);
void emscripten_glUniform3f(GLint, GLfloat, GLfloat, GLfloat);
void emscripten_glUniform4f(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
void emscripten_glUniform4fv(GLint, GLsizei, const GLfloat *);
void emscripten_glUniformMatrix4fv(GLint, GLsizei, GLboolean, const GLfloat *);
void emscripten_glUseProgram(GLuint);
void emscripten_glVertexAttrib4fv(GLuint, const GLfloat *);
void emscripten_glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
void emscripten_glViewport(GLint, GLint, GLsizei, GLsizei);
}

// Warnings go to the console directly: nothing here may reach a function the game's
// ASYNCIFY build would have to unwind through.
EM_JS(void, glein_warn, (const char *what), { console.warn('[gl] ' + UTF8ToString(what)); });

#define GLEIN_WARN_ONCE(text) do { static bool warned_; if (!warned_) { warned_ = true; glein_warn(text); } } while (0)

enum { GLEIN_UNITS = 8, GLEIN_STACK = 16 };

// Attribute locations of the layer's programs.
enum { A_VERTEX, A_TEXCOORD0, A_COLOR, A_TEXCOORD3, A_TEXCOORD4, A_FOG, A_COUNT };
static char const *const glein_attribNames[A_COUNT] = { "a_vertex", "a_texcoord0", "a_color", "a_texcoord3", "a_texcoord4", "a_fog" };

// Uniforms the layer feeds.
enum { U_MODELVIEW, U_PROJECTION, U_TEXMAT0, U_TEXMAT3, U_TEXMAT4, U_FOGCOLOR, U_ALPHAREF, U_TEXTURING, U_FOG, U_COUNT };
static char const *const glein_uniformNames[U_COUNT] = { "u_modelView", "u_projection", "u_textureMatrix0", "u_textureMatrix3",
                                                         "u_textureMatrix4", "u_fogColor", "u_alphaRef", "u_ffTexturing", "u_ffFog" };
static int const glein_uniformSize[U_COUNT] = { 16, 16, 16, 16, 16, 4, 1, 1, 1 };

////////// state the renderers set //////////

struct glein_stack { float m[GLEIN_STACK][16]; int top; };

static glein_stack glein_modelview, glein_projection, glein_texmat[GLEIN_UNITS];
static GLenum glein_matrixMode = GL_MODELVIEW;
static int glein_activeUnit, glein_clientUnit;

struct glein_array
{
    bool enabled;
    GLint size;
    GLenum type;
    GLsizei stride;
    uint8_t const *pointer;
    GLuint buffer;
};

static glein_array glein_vertexArray, glein_colorArray, glein_texArray[GLEIN_UNITS];

static float glein_color[4] = { 1.f, 1.f, 1.f, 1.f };
static float glein_texCoord[GLEIN_UNITS][4];

static float glein_fogStart, glein_fogEnd = 1.f, glein_fogColor[4];
static bool glein_fogOn, glein_alphaTest;
static GLenum glein_alphaFunc = GL_ALWAYS;
static float glein_alphaRefValue;
static bool glein_texture2D[GLEIN_UNITS];

////////// WebGL state: what the renderers want and what WebGL has //////////

enum { CAP_BLEND = 1, CAP_CULL_FACE = 2, CAP_DEPTH_TEST = 4, CAP_SCISSOR_TEST = 8, CAP_STENCIL_TEST = 16, CAP_POLYGON_OFFSET_FILL = 32 };

struct glein_raster
{
    uint32_t caps;
    GLenum blend[4], blendEquation[2];
    GLenum depthFunc, cullFace, frontFace;
    GLboolean depthMask, colorMask[4];
    GLenum stencilFunc;
    GLint stencilRef;
    GLuint stencilMask;
    GLenum stencilOp[3];
    GLint scissor[4], viewport[4];
    GLfloat clearColor[4], polygonOffset[2], depthRange[2], lineWidth;
    GLuint texture[GLEIN_UNITS];
};

static glein_raster glein_want, glein_have;
static bool glein_rasterDirty;

// Texture parameters, applied when the texture is drawn with.
enum { TP_WRAP_S, TP_WRAP_T, TP_MIN_FILTER, TP_MAG_FILTER, TP_BASE_LEVEL, TP_MAX_LEVEL, TP_ANISOTROPY, TP_COUNT };
static GLenum const glein_texParamNames[TP_COUNT] = { GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_TEXTURE_MIN_FILTER, GL_TEXTURE_MAG_FILTER,
                                                      GL_TEXTURE_BASE_LEVEL, GL_TEXTURE_MAX_LEVEL, GL_TEXTURE_MAX_ANISOTROPY_EXT };

struct glein_texture
{
    float want[TP_COUNT], have[TP_COUNT];
    bool dirty;
};

static std::vector<glein_texture *> glein_textures;
static int glein_texturesDirty;

static int glein_realActiveUnit;
static GLuint glein_realProgram, glein_realVao, glein_realArrayBuffer, glein_realElementBuffer[2];

////////// buffers //////////

// A buffer given GL_STREAM_DRAW data stays on the CPU: draws read its vertices from
// there. Others are WebGL buffers.
struct glein_buffer
{
    bool client;
    std::vector<uint8_t> data;
    GLsizeiptr size;
    uint8_t *staging;
    GLintptr mapOffset;
    GLsizeiptr mapLength;
};

static std::vector<glein_buffer *> glein_buffers;
static GLuint glein_arrayBuffer, glein_elementBuffer;

////////// programs //////////

// A uniform's value as the renderer set it, and as WebGL has it.
struct glein_uniform
{
    uint8_t kind, sentKind;
    bool dirty;
    float v[16], sent[16];
};

struct glein_program
{
    bool ff;
    bool texcoord34;
    GLint loc[U_COUNT];
    float sent[U_COUNT][16];
    bool sentValid[U_COUNT];
    std::vector<glein_uniform> values;
    std::vector<GLint> dirty;
};

static std::vector<glein_program *> glein_programs;
static GLuint glein_appProgram, glein_builtinProgram;

////////// batching //////////

struct glein_vert { float x, y, z, s, t, r, g, b, a, fogEnd, fogScale; };

// What a draw through the layer's attributes depends on besides the WebGL state.
struct glein_ffstate
{
    float m[5][16];
    float fogColor[4];
    float alphaRef, texturing, fog;
    float texCoord3[4], texCoord4[4];
};

enum { GLEIN_MAXBATCH = 49152 };

static std::vector<glein_vert> glein_batch, glein_scratch;
static GLenum glein_batchPrim;
static GLuint glein_batchProgram;
static glein_ffstate glein_batchState;

static GLuint glein_layerVao, glein_ring, glein_directRing, glein_indexRing;
static size_t glein_ringSize = 4 << 20, glein_ringPos;
static size_t glein_directSize = 1 << 20, glein_directPos;
static size_t glein_indexSize = 1 << 20, glein_indexPos;
static float glein_constant[A_COUNT][4];
static bool glein_constantValid[A_COUNT];

static std::vector<uint32_t> glein_indices;
static std::vector<uint8_t> glein_swizzle;

////////// helpers //////////

static FORCE_INLINE glein_buffer *glein_getBuffer(GLuint name)
{
    return name < glein_buffers.size() ? glein_buffers[name] : nullptr;
}

static glein_program *glein_getProgram(GLuint name)
{
    return name < glein_programs.size() ? glein_programs[name] : nullptr;
}

static glein_texture *glein_getTexture(GLuint name, bool create)
{
    if (!name)
        return nullptr;
    if (name >= glein_textures.size())
    {
        if (!create)
            return nullptr;
        glein_textures.resize(name + 1, nullptr);
    }
    glein_texture *t = glein_textures[name];
    if (!t && create)
    {
        t = glein_textures[name] = new glein_texture;
        float const defaults[TP_COUNT] = { GL_REPEAT, GL_REPEAT, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR, 0, 1000, 1 };
        Bmemcpy(t->want, defaults, sizeof(defaults));
        Bmemcpy(t->have, defaults, sizeof(defaults));
        t->dirty = false;
    }
    return t;
}

static void glein_identity(float *m)
{
    Bmemset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.f;
}

static glein_stack &glein_currentStack(void)
{
    switch (glein_matrixMode)
    {
        case GL_PROJECTION: return glein_projection;
        case GL_TEXTURE: return glein_texmat[glein_activeUnit];
        default: return glein_modelview;
    }
}

static FORCE_INLINE float *glein_current(void)
{
    glein_stack &s = glein_currentStack();
    return s.m[s.top];
}

// m = m * b, in double precision.
static void glein_multiply(float *m, double const *b)
{
    float r[16];
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++)
        {
            double sum = 0;
            for (int k = 0; k < 4; k++)
                sum += (double)m[k * 4 + row] * b[c * 4 + k];
            r[c * 4 + row] = (float)sum;
        }
    Bmemcpy(m, r, sizeof(r));
}

static void glein_bindVao(GLuint vao)
{
    if (glein_realVao != vao)
    {
        emscripten_glBindVertexArray(vao);
        glein_realVao = vao;
    }
}

static void glein_bindRealBuffer(GLenum target, GLuint name)
{
    if (target == GL_ARRAY_BUFFER)
    {
        if (glein_realArrayBuffer != name)
        {
            emscripten_glBindBuffer(target, name);
            glein_realArrayBuffer = name;
        }
    }
    else if (target == GL_ELEMENT_ARRAY_BUFFER)
    {
        GLuint &bound = glein_realElementBuffer[glein_realVao == glein_layerVao];
        if (bound != name)
        {
            emscripten_glBindBuffer(target, name);
            bound = name;
        }
    }
    else
        emscripten_glBindBuffer(target, name);
}

static void glein_useRealProgram(GLuint program)
{
    if (glein_realProgram != program)
    {
        emscripten_glUseProgram(program);
        glein_realProgram = program;
    }
}

static void glein_activeRealUnit(int unit)
{
    if (glein_realActiveUnit != unit)
    {
        emscripten_glActiveTexture(GL_TEXTURE0 + unit);
        glein_realActiveUnit = unit;
    }
}

static void glein_setConstant(int attrib, float const *v)
{
    if (glein_constantValid[attrib] && !Bmemcmp(glein_constant[attrib], v, 4 * sizeof(float)))
        return;
    Bmemcpy(glein_constant[attrib], v, 4 * sizeof(float));
    glein_constantValid[attrib] = true;
    emscripten_glVertexAttrib4fv(attrib, v);
}

////////// drawing the batch //////////

static void glein_syncUniforms(glein_program *p, GLuint program);

static void glein_sendUniform(glein_program *p, int u, float const *v)
{
    GLint const loc = p->loc[u];
    int const n = glein_uniformSize[u];

    if (loc < 0 || (p->sentValid[u] && !Bmemcmp(p->sent[u], v, n * sizeof(float))))
        return;

    Bmemcpy(p->sent[u], v, n * sizeof(float));
    p->sentValid[u] = true;

    switch (n)
    {
        case 16: emscripten_glUniformMatrix4fv(loc, 1, GL_FALSE, v); break;
        case 4: emscripten_glUniform4fv(loc, 1, v); break;
        default: emscripten_glUniform1f(loc, v[0]); break;
    }
}

// The layer's uniforms and constant attributes, for a program bound in WebGL.
static void glein_applyState(glein_program *p, glein_ffstate const &s)
{
    for (int u = U_MODELVIEW; u <= U_TEXMAT4; u++)
        glein_sendUniform(p, u, s.m[u]);
    glein_sendUniform(p, U_FOGCOLOR, s.fogColor);
    glein_sendUniform(p, U_ALPHAREF, &s.alphaRef);
    glein_sendUniform(p, U_TEXTURING, &s.texturing);
    glein_sendUniform(p, U_FOG, &s.fog);
    if (p->texcoord34)
    {
        glein_setConstant(A_TEXCOORD3, s.texCoord3);
        glein_setConstant(A_TEXCOORD4, s.texCoord4);
    }
}

static void glein_flushBatch(void)
{
    if (glein_batch.empty())
        return;

    glein_useRealProgram(glein_batchProgram);
    glein_bindVao(glein_layerVao);
    glein_applyState(glein_getProgram(glein_batchProgram), glein_batchState);

    size_t const bytes = glein_batch.size() * sizeof(glein_vert);

    glein_bindRealBuffer(GL_ARRAY_BUFFER, glein_ring);
    if (glein_ringPos + bytes > glein_ringSize)
    {
        // a fresh store, while WebGL may still draw from the old one
        emscripten_glBufferData(GL_ARRAY_BUFFER, glein_ringSize, nullptr, GL_STREAM_DRAW);
        glein_ringPos = 0;
    }
    emscripten_glBufferSubData(GL_ARRAY_BUFFER, glein_ringPos, bytes, glein_batch.data());
    emscripten_glDrawArrays(glein_batchPrim, glein_ringPos / sizeof(glein_vert), glein_batch.size());
    glein_ringPos += bytes;

    glein_batch.clear();
}

void glbuild_ein_flush(void)
{
    glein_flushBatch();
}

// The raster state and texture parameters the renderers asked for, in WebGL; what is
// batched is drawn first if any of it changes.
static void glein_syncRaster(void)
{
    if (glein_rasterDirty)
    {
        glein_rasterDirty = false;

        glein_raster &w = glein_want, &h = glein_have;

        if (Bmemcmp(&w, &h, sizeof(glein_raster)))
        {
            glein_flushBatch();

            if (w.caps != h.caps)
            {
                static GLenum const caps[] = { GL_BLEND, GL_CULL_FACE, GL_DEPTH_TEST, GL_SCISSOR_TEST, GL_STENCIL_TEST, GL_POLYGON_OFFSET_FILL };
                for (int i = 0; i < ARRAY_SSIZE(caps); i++)
                {
                    uint32_t const bit = 1u << i;
                    if ((w.caps ^ h.caps) & bit)
                        (w.caps & bit) ? emscripten_glEnable(caps[i]) : emscripten_glDisable(caps[i]);
                }
            }
            if (Bmemcmp(w.blend, h.blend, sizeof(w.blend)))
                emscripten_glBlendFuncSeparate(w.blend[0], w.blend[1], w.blend[2], w.blend[3]);
            if (Bmemcmp(w.blendEquation, h.blendEquation, sizeof(w.blendEquation)))
                emscripten_glBlendEquationSeparate(w.blendEquation[0], w.blendEquation[1]);
            if (w.depthFunc != h.depthFunc)
                emscripten_glDepthFunc(w.depthFunc);
            if (w.cullFace != h.cullFace)
                emscripten_glCullFace(w.cullFace);
            if (w.frontFace != h.frontFace)
                emscripten_glFrontFace(w.frontFace);
            if (w.depthMask != h.depthMask)
                emscripten_glDepthMask(w.depthMask);
            if (Bmemcmp(w.colorMask, h.colorMask, sizeof(w.colorMask)))
                emscripten_glColorMask(w.colorMask[0], w.colorMask[1], w.colorMask[2], w.colorMask[3]);
            if (w.stencilFunc != h.stencilFunc || w.stencilRef != h.stencilRef || w.stencilMask != h.stencilMask)
                emscripten_glStencilFunc(w.stencilFunc, w.stencilRef, w.stencilMask);
            if (Bmemcmp(w.stencilOp, h.stencilOp, sizeof(w.stencilOp)))
                emscripten_glStencilOp(w.stencilOp[0], w.stencilOp[1], w.stencilOp[2]);
            if (Bmemcmp(w.scissor, h.scissor, sizeof(w.scissor)))
                emscripten_glScissor(w.scissor[0], w.scissor[1], w.scissor[2], w.scissor[3]);
            if (Bmemcmp(w.viewport, h.viewport, sizeof(w.viewport)))
                emscripten_glViewport(w.viewport[0], w.viewport[1], w.viewport[2], w.viewport[3]);
            if (Bmemcmp(w.clearColor, h.clearColor, sizeof(w.clearColor)))
                emscripten_glClearColor(w.clearColor[0], w.clearColor[1], w.clearColor[2], w.clearColor[3]);
            if (Bmemcmp(w.polygonOffset, h.polygonOffset, sizeof(w.polygonOffset)))
                emscripten_glPolygonOffset(w.polygonOffset[0], w.polygonOffset[1]);
            if (Bmemcmp(w.depthRange, h.depthRange, sizeof(w.depthRange)))
                emscripten_glDepthRangef(w.depthRange[0], w.depthRange[1]);
            if (w.lineWidth != h.lineWidth)
                emscripten_glLineWidth(w.lineWidth);
            for (int u = 0; u < GLEIN_UNITS; u++)
                if (w.texture[u] != h.texture[u])
                {
                    glein_activeRealUnit(u);
                    emscripten_glBindTexture(GL_TEXTURE_2D, w.texture[u]);
                }

            h = w;
        }
    }

    if (glein_texturesDirty)
    {
        for (int u = 0; u < GLEIN_UNITS; u++)
        {
            glein_texture *t = glein_getTexture(glein_have.texture[u], false);
            if (!t || !t->dirty)
                continue;

            t->dirty = false;
            glein_texturesDirty--;

            if (!Bmemcmp(t->want, t->have, sizeof(t->want)))
                continue;

            glein_flushBatch();
            glein_activeRealUnit(u);
            for (int i = 0; i < TP_COUNT; i++)
                if (t->want[i] != t->have[i])
                {
                    if (i == TP_ANISOTROPY)
                        emscripten_glTexParameterf(GL_TEXTURE_2D, glein_texParamNames[i], t->want[i]);
                    else
                        emscripten_glTexParameteri(GL_TEXTURE_2D, glein_texParamNames[i], (GLint)t->want[i]);
                    t->have[i] = t->want[i];
                }
        }
    }
}

// The texture the active unit has, bound in WebGL, for an upload or a parameter.
static void glein_bindActiveTexture(void)
{
    int const u = glein_activeUnit;
    if (glein_have.texture[u] != glein_want.texture[u])
    {
        glein_flushBatch();
        glein_activeRealUnit(u);
        emscripten_glBindTexture(GL_TEXTURE_2D, glein_want.texture[u]);
        glein_have.texture[u] = glein_want.texture[u];
    }
    glein_activeRealUnit(u);
}

static void glein_snapshot(glein_program const *p, glein_ffstate &s)
{
    Bmemset(&s, 0, sizeof(s));

    glein_stack const *const stacks[5] = { &glein_modelview, &glein_projection, &glein_texmat[0], &glein_texmat[3], &glein_texmat[4] };
    for (int u = U_MODELVIEW; u <= U_TEXMAT4; u++)
        if (p->loc[u] >= 0)
            Bmemcpy(s.m[u], stacks[u]->m[stacks[u]->top], 16 * sizeof(float));

    if (p->loc[U_FOGCOLOR] >= 0)
        Bmemcpy(s.fogColor, glein_fogColor, sizeof(s.fogColor));

    if (p->loc[U_ALPHAREF] >= 0)
    {
        float ref = -1.f;
        if (glein_alphaTest)
            switch (glein_alphaFunc)
            {
                case GL_ALWAYS: break;
                case GL_NEVER: ref = 2.f; break;
                case GL_GEQUAL: ref = glein_alphaRefValue - (0.5f / 255.f); break;
                default: ref = glein_alphaRefValue; break;
            }
        s.alphaRef = ref;
    }

    if (p->loc[U_TEXTURING] >= 0)
        s.texturing = glein_texture2D[0];
    if (p->loc[U_FOG] >= 0)
        s.fog = glein_fogOn;

    if (p->texcoord34)
    {
        Bmemcpy(s.texCoord3, glein_texCoord[3], sizeof(s.texCoord3));
        Bmemcpy(s.texCoord4, glein_texCoord[4], sizeof(s.texCoord4));
    }
}

static GLenum glein_primitive(GLenum mode)
{
    switch (mode)
    {
        case GL_POINTS: return GL_POINTS;
        case GL_LINES: case GL_LINE_STRIP: case GL_LINE_LOOP: return GL_LINES;
        default: return GL_TRIANGLES;
    }
}

// The vertices of `n` in `mode`, as the triangles, lines or points glein_primitive() gives.
static void glein_expand(GLenum mode, glein_vert const *v, int n)
{
    auto put = [](glein_vert const &x) { glein_batch.push_back(x); };

    switch (mode)
    {
        case GL_POINTS:
            for (int i = 0; i < n; i++) put(v[i]);
            break;
        case GL_LINES:
            for (int i = 0; i + 1 < n; i += 2) put(v[i]), put(v[i + 1]);
            break;
        case GL_LINE_STRIP:
        case GL_LINE_LOOP:
            for (int i = 0; i + 1 < n; i++) put(v[i]), put(v[i + 1]);
            if (mode == GL_LINE_LOOP && n > 2) put(v[n - 1]), put(v[0]);
            break;
        case GL_TRIANGLES:
            for (int i = 0; i + 2 < n; i += 3) put(v[i]), put(v[i + 1]), put(v[i + 2]);
            break;
        case GL_TRIANGLE_STRIP:
            for (int i = 0; i + 2 < n; i++)
                if (i & 1) put(v[i + 1]), put(v[i]), put(v[i + 2]);
                else put(v[i]), put(v[i + 1]), put(v[i + 2]);
            break;
        case GL_TRIANGLE_FAN:
        case GL_POLYGON:
            for (int i = 1; i + 1 < n; i++) put(v[0]), put(v[i]), put(v[i + 1]);
            break;
        case GL_QUADS:
            for (int i = 0; i + 3 < n; i += 4)
                put(v[i]), put(v[i + 1]), put(v[i + 2]), put(v[i]), put(v[i + 2]), put(v[i + 3]);
            break;
        case GL_QUAD_STRIP:
            for (int i = 0; i + 3 < n; i += 2)
                put(v[i]), put(v[i + 1]), put(v[i + 3]), put(v[i]), put(v[i + 3]), put(v[i + 2]);
            break;
    }
}

static int glein_expandedCount(GLenum mode, int n)
{
    switch (mode)
    {
        case GL_POINTS: case GL_LINES: case GL_TRIANGLES: return n;
        case GL_LINE_STRIP: case GL_LINE_LOOP: return 2 * n;
        case GL_QUADS: return n / 4 * 6;
        case GL_QUAD_STRIP: return n * 3;
        default: return n * 3;
    }
}

// Adds `n` vertices drawn as `mode` with the current program to the batch.
static void glein_submit(GLenum mode, glein_vert const *v, int n)
{
    GLuint const program = glein_appProgram ? glein_appProgram : glein_builtinProgram;
    glein_program *const p = glein_getProgram(program);
    GLenum const prim = glein_primitive(mode);

    glein_ffstate state;
    glein_snapshot(p, state);
    glein_syncRaster();
    glein_syncUniforms(p, program);

    int const count = glein_expandedCount(mode, n);

    if (!glein_batch.empty() && (glein_batchProgram != program || glein_batchPrim != prim || glein_batch.size() + count > GLEIN_MAXBATCH ||
                                 Bmemcmp(&state, &glein_batchState, sizeof(state))))
        glein_flushBatch();

    if (glein_batch.empty())
    {
        glein_batchProgram = program;
        glein_batchPrim = prim;
        glein_batchState = state;
        if ((size_t)count > glein_batch.capacity())
            glein_batch.reserve(count);
    }

    size_t const from = glein_batch.size();
    glein_expand(mode, v, n);

    float const fogEnd = glein_fogEnd, fogScale = 1.f / (glein_fogEnd - glein_fogStart);
    for (size_t i = from, end = glein_batch.size(); i < end; i++)
        glein_batch[i].fogEnd = fogEnd, glein_batch[i].fogScale = fogScale;

    if (!glbuild_ein_batch)
        glein_flushBatch();
}

////////// client arrays //////////

static int glein_typeSize(GLenum type)
{
    switch (type)
    {
        case GL_BYTE: case GL_UNSIGNED_BYTE: return 1;
        case GL_SHORT: case GL_UNSIGNED_SHORT: return 2;
        case GL_DOUBLE: return 8;
        default: return 4;
    }
}

static uint8_t const *glein_arrayData(glein_array const &a)
{
    if (!a.buffer)
        return a.pointer;
    glein_buffer const *b = glein_getBuffer(a.buffer);
    return b && b->client ? b->data.data() + (uintptr_t)a.pointer : nullptr;
}

static bool glein_onCpu(glein_array const &a)
{
    if (!a.enabled || !a.buffer)
        return true;
    glein_buffer const *b = glein_getBuffer(a.buffer);
    return b && b->client;
}

// Whether a draw from the client arrays can go into the batch.
static bool glein_batchable(glein_program const *p)
{
    glein_array const &v = glein_vertexArray, &t = glein_texArray[0], &c = glein_colorArray;

    if (!v.enabled || v.type != GL_FLOAT || v.size < 2 || v.size > 3 || !glein_onCpu(v))
        return false;
    if (t.enabled && (t.type != GL_FLOAT || t.size != 2 || !glein_onCpu(t)))
        return false;
    if (c.enabled && !((c.type == GL_FLOAT && c.size >= 3) || (c.type == GL_UNSIGNED_BYTE && c.size >= 3)) )
        return false;
    if (c.enabled && !glein_onCpu(c))
        return false;
    if (p->texcoord34 && (glein_texArray[3].enabled || glein_texArray[4].enabled))
        return false;
    return true;
}

// The client arrays' vertex `i`.
static FORCE_INLINE void glein_fetch(glein_vert &out, int i, uint8_t const *vp, int vstride, uint8_t const *tp, int tstride,
                                     uint8_t const *cp, int cstride)
{
    glein_array const &v = glein_vertexArray, &c = glein_colorArray;

    Bmemcpy(&out.x, vp + (size_t)i * vstride, v.size * sizeof(float));
    if (v.size < 3)
        out.z = 0.f;

    if (tp)
        Bmemcpy(&out.s, tp + (size_t)i * tstride, 2 * sizeof(float));
    else
        out.s = glein_texCoord[0][0], out.t = glein_texCoord[0][1];

    if (cp)
    {
        uint8_t const *p = cp + (size_t)i * cstride;
        if (c.type == GL_FLOAT)
        {
            Bmemcpy(&out.r, p, c.size * sizeof(float));
            if (c.size < 4)
                out.a = 1.f;
        }
        else
        {
            out.r = p[0] / 255.f, out.g = p[1] / 255.f, out.b = p[2] / 255.f;
            out.a = c.size < 4 ? 1.f : p[3] / 255.f;
        }
    }
    else
        Bmemcpy(&out.r, glein_color, 4 * sizeof(float));
}

// Gathers the vertices of a draw from the client arrays, by index or from `first`, into
// glein_scratch; false if they are out of the arrays' data.
static bool glein_gather(GLint first, GLsizei count, GLenum indexType, uint8_t const *indices)
{
    glein_array const &v = glein_vertexArray, &t = glein_texArray[0], &c = glein_colorArray;

    uint8_t const *vp = glein_arrayData(v);
    uint8_t const *tp = t.enabled ? glein_arrayData(t) : nullptr;
    uint8_t const *cp = c.enabled ? glein_arrayData(c) : nullptr;
    int const vstride = v.stride ? v.stride : v.size * sizeof(float);
    int const tstride = t.stride ? t.stride : 2 * sizeof(float);
    int const cstride = c.stride ? c.stride : c.size * glein_typeSize(c.type);

    if (!vp || (t.enabled && !tp) || (c.enabled && !cp))
        return false;

    glein_scratch.resize(count);
    glein_vert *out = glein_scratch.data();

    if (!indices)
    {
        // a buffer's data must hold the whole range
        glein_buffer const *b = glein_getBuffer(v.buffer);
        if (b && (uintptr_t)v.pointer + (size_t)(first + count - 1) * vstride + v.size * sizeof(float) > (size_t)b->size)
            return false;

        for (int i = 0; i < count; i++)
            glein_fetch(out[i], first + i, vp, vstride, tp, tstride, cp, cstride);
        return true;
    }

    for (int i = 0; i < count; i++)
    {
        uint32_t const index = indexType == GL_UNSIGNED_INT ? ((uint32_t const *)indices)[i]
                             : indexType == GL_UNSIGNED_SHORT ? ((uint16_t const *)indices)[i] : indices[i];
        glein_fetch(out[i], index, vp, vstride, tp, tstride, cp, cstride);
    }
    return true;
}

////////// draws that are not batched //////////

static uint32_t glein_upload(GLenum target, GLuint buffer, size_t &pos, size_t &size, void const *data, size_t bytes)
{
    glein_bindRealBuffer(target, buffer);
    pos = (pos + 15) & ~(size_t)15;
    if (bytes > size)
    {
        size = bytes * 2;
        emscripten_glBufferData(target, size, nullptr, GL_STREAM_DRAW);
        pos = 0;
    }
    else if (pos + bytes > size)
    {
        emscripten_glBufferData(target, size, nullptr, GL_STREAM_DRAW);
        pos = 0;
    }
    emscripten_glBufferSubData(target, pos, bytes, data);
    uint32_t const at = pos;
    pos += bytes;
    return at;
}

// The layer's vertex array object as batches use it: the ring's vertices on attributes 0-2 and 5.
static void glein_resetLayerVao(void)
{
    glein_bindVao(glein_layerVao);
    glein_bindRealBuffer(GL_ARRAY_BUFFER, glein_ring);
    emscripten_glVertexAttribPointer(A_VERTEX, 3, GL_FLOAT, GL_FALSE, sizeof(glein_vert), (void *)offsetof(glein_vert, x));
    emscripten_glVertexAttribPointer(A_TEXCOORD0, 2, GL_FLOAT, GL_FALSE, sizeof(glein_vert), (void *)offsetof(glein_vert, s));
    emscripten_glVertexAttribPointer(A_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof(glein_vert), (void *)offsetof(glein_vert, r));
    emscripten_glVertexAttribPointer(A_FOG, 2, GL_FLOAT, GL_FALSE, sizeof(glein_vert), (void *)offsetof(glein_vert, fogEnd));
    for (int a : { A_VERTEX, A_TEXCOORD0, A_COLOR, A_FOG })
        emscripten_glEnableVertexAttribArray(a);
    emscripten_glDisableVertexAttribArray(A_TEXCOORD3);
    emscripten_glDisableVertexAttribArray(A_TEXCOORD4);
}

// A draw from WebGL buffers, or with texture coordinates for units 3 and 4 as arrays:
// each array as an attribute of its own, copied to WebGL when it is CPU data.
static void glein_drawDirect(GLenum mode, GLint first, GLsizei count, GLenum indexType, void const *indices)
{
    glein_flushBatch();
    glein_syncRaster();

    GLuint const program = glein_appProgram ? glein_appProgram : glein_builtinProgram;
    glein_program *const p = glein_getProgram(program);
    glein_syncUniforms(p, program);
    glein_ffstate state;
    glein_snapshot(p, state);

    bool const indexed = indexType != 0;
    uint8_t const *cpuIndices = nullptr;
    GLuint realIndices = 0;

    if (indexed)
    {
        glein_buffer const *b = glein_getBuffer(glein_elementBuffer);
        if (!glein_elementBuffer)
            cpuIndices = (uint8_t const *)indices;
        else if (b && b->client)
            cpuIndices = b->data.data() + (uintptr_t)indices;
        else
            realIndices = glein_elementBuffer;
    }

    // the vertex range CPU arrays need
    GLint lo = indexed ? 0 : first, hi = indexed ? -1 : first + count - 1;
    if (cpuIndices)
        for (int i = 0; i < count; i++)
        {
            GLint const index = indexType == GL_UNSIGNED_INT ? (GLint)((uint32_t const *)cpuIndices)[i]
                              : indexType == GL_UNSIGNED_SHORT ? ((uint16_t const *)cpuIndices)[i] : cpuIndices[i];
            hi = max(hi, index);
        }

    glein_array *const arrays[A_FOG] = { &glein_vertexArray, &glein_texArray[0], &glein_colorArray, &glein_texArray[3], &glein_texArray[4] };
    float const *const constants[A_FOG] = { nullptr, glein_texCoord[0], glein_color, glein_texCoord[3], glein_texCoord[4] };

    for (int a = 0; a < A_FOG; a++)
        if (arrays[a]->enabled && glein_onCpu(*arrays[a]) && hi < lo)
        {
            GLEIN_WARN_ONCE("a draw of CPU vertices with indices in a WebGL buffer is skipped");
            return;
        }

    glein_useRealProgram(program);
    glein_bindVao(glein_layerVao);
    glein_applyState(p, state);

    float const fog[4] = { glein_fogEnd, 1.f / (glein_fogEnd - glein_fogStart), 0.f, 1.f };
    emscripten_glDisableVertexAttribArray(A_FOG);
    glein_constantValid[A_FOG] = false;
    glein_setConstant(A_FOG, fog);

    for (int a = 0; a < A_FOG; a++)
    {
        glein_array const &arr = *arrays[a];

        if (!arr.enabled || (a >= A_TEXCOORD3 && !p->texcoord34))
        {
            emscripten_glDisableVertexAttribArray(a);
            if (constants[a])
            {
                glein_constantValid[a] = false;
                glein_setConstant(a, constants[a]);
            }
            continue;
        }

        int const stride = arr.stride ? arr.stride : arr.size * glein_typeSize(arr.type);
        GLboolean const normalized = a == A_COLOR && arr.type != GL_FLOAT;

        if (!glein_onCpu(arr))
        {
            glein_bindRealBuffer(GL_ARRAY_BUFFER, arr.buffer);
            emscripten_glVertexAttribPointer(a, arr.size, arr.type, normalized, stride, arr.pointer + (size_t)lo * stride);
        }
        else
        {
            uint8_t const *data = glein_arrayData(arr);
            size_t const bytes = (size_t)(hi - lo) * stride + arr.size * glein_typeSize(arr.type);
            uint32_t const at = glein_upload(GL_ARRAY_BUFFER, glein_directRing, glein_directPos, glein_directSize, data + (size_t)lo * stride, bytes);
            emscripten_glVertexAttribPointer(a, arr.size, arr.type, normalized, stride, (void *)(uintptr_t)at);
        }
        emscripten_glEnableVertexAttribArray(a);
    }

    if (mode == GL_POLYGON)
        mode = GL_TRIANGLE_FAN;

    if (mode == GL_QUADS || mode == GL_QUAD_STRIP)
    {
        if (realIndices)
            GLEIN_WARN_ONCE("quads with indices in a WebGL buffer are skipped");
        else
        {
            glein_indices.clear();
            auto index = [&](int i) -> uint32_t {
                if (!indexed) return i;
                return indexType == GL_UNSIGNED_INT ? ((uint32_t const *)cpuIndices)[i]
                     : indexType == GL_UNSIGNED_SHORT ? ((uint16_t const *)cpuIndices)[i] : cpuIndices[i];
            };
            if (mode == GL_QUADS)
                for (int i = 0; i + 3 < count; i += 4)
                    for (int k : { 0, 1, 2, 0, 2, 3 }) glein_indices.push_back(index(i + k));
            else
                for (int i = 0; i + 3 < count; i += 2)
                    for (int k : { 0, 1, 3, 0, 3, 2 }) glein_indices.push_back(index(i + k));
            uint32_t const at = glein_upload(GL_ELEMENT_ARRAY_BUFFER, glein_indexRing, glein_indexPos, glein_indexSize, glein_indices.data(),
                                             glein_indices.size() * sizeof(uint32_t));
            emscripten_glDrawElements(GL_TRIANGLES, glein_indices.size(), GL_UNSIGNED_INT, (void *)(uintptr_t)at);
        }
    }
    else if (!indexed)
        emscripten_glDrawArrays(mode, 0, count);
    else if (realIndices)
    {
        glein_bindRealBuffer(GL_ELEMENT_ARRAY_BUFFER, realIndices);
        emscripten_glDrawElements(mode, count, indexType, indices);
    }
    else
    {
        uint32_t const at = glein_upload(GL_ELEMENT_ARRAY_BUFFER, glein_indexRing, glein_indexPos, glein_indexSize, cpuIndices,
                                         count * glein_typeSize(indexType));
        emscripten_glDrawElements(mode, count, indexType, (void *)(uintptr_t)at);
    }

    glein_resetLayerVao();
    for (int a = A_TEXCOORD0; a < A_COUNT; a++)
        glein_constantValid[a] = false;
}

// A draw with a program of the renderer's own attributes (glsurface's).
static void glein_drawGeneric(GLenum mode, GLint first, GLsizei count, GLenum indexType, void const *indices)
{
    glein_syncRaster();
    glein_flushBatch();
    glein_syncUniforms(glein_getProgram(glein_appProgram), glein_appProgram);
    glein_useRealProgram(glein_appProgram);
    glein_bindVao(0);
    if (indexType)
    {
        glein_bindRealBuffer(GL_ELEMENT_ARRAY_BUFFER, glein_elementBuffer);
        emscripten_glDrawElements(mode, count, indexType, indices);
    }
    else
        emscripten_glDrawArrays(mode, first, count);
}

static void glein_draw(GLenum mode, GLint first, GLsizei count, GLenum indexType, void const *indices)
{
    if (count <= 0)
        return;

    glein_program *const p = glein_getProgram(glein_appProgram ? glein_appProgram : glein_builtinProgram);
    if (!p)
        return;

    if (!p->ff)
    {
        glein_drawGeneric(mode, first, count, indexType, indices);
        return;
    }

    uint8_t const *cpuIndices = nullptr;
    if (indexType)
    {
        glein_buffer const *b = glein_getBuffer(glein_elementBuffer);
        if (!glein_elementBuffer)
            cpuIndices = (uint8_t const *)indices;
        else if (b && b->client)
            cpuIndices = b->data.data() + (uintptr_t)indices;
    }

    if ((!indexType || cpuIndices) && glein_batchable(p))
    {
        if (glein_gather(first, count, indexType, cpuIndices))
            glein_submit(mode, glein_scratch.data(), count);
        else
            GLEIN_WARN_ONCE("a draw beyond its buffer's data is skipped");
        return;
    }

    glein_drawDirect(mode, first, count, indexType, indices);
}

static void APIENTRY glein_DrawArrays(GLenum mode, GLint first, GLsizei count)
{
    glein_draw(mode, first, count, 0, nullptr);
}

static void APIENTRY glein_DrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
    glein_draw(mode, 0, count, type, indices);
}

static void APIENTRY glein_DrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices)
{
    UNREFERENCED_PARAMETER(start);
    UNREFERENCED_PARAMETER(end);
    glein_draw(mode, 0, count, type, indices);
}

////////// immediate mode //////////

static GLenum glein_immediateMode;
static std::vector<glein_vert> glein_immediate;

static void APIENTRY glein_Begin(GLenum mode)
{
    glein_immediateMode = mode;
    glein_immediate.clear();
}

static void APIENTRY glein_End(void)
{
    glein_program *const p = glein_getProgram(glein_appProgram ? glein_appProgram : glein_builtinProgram);
    if (!p || !p->ff || glein_immediate.empty())
        return;

    glein_submit(glein_immediateMode, glein_immediate.data(), glein_immediate.size());
}

static FORCE_INLINE void glein_vertex(float x, float y, float z)
{
    glein_immediate.push_back({ x, y, z, glein_texCoord[0][0], glein_texCoord[0][1], glein_color[0], glein_color[1], glein_color[2], glein_color[3], 0.f, 0.f });
}

static void APIENTRY glein_Vertex2f(GLfloat x, GLfloat y) { glein_vertex(x, y, 0.f); }
static void APIENTRY glein_Vertex2i(GLint x, GLint y) { glein_vertex((float)x, (float)y, 0.f); }
static void APIENTRY glein_Vertex2d(GLdouble x, GLdouble y) { glein_vertex((float)x, (float)y, 0.f); }
static void APIENTRY glein_Vertex2fv(const GLfloat *v) { glein_vertex(v[0], v[1], 0.f); }
static void APIENTRY glein_Vertex3f(GLfloat x, GLfloat y, GLfloat z) { glein_vertex(x, y, z); }
static void APIENTRY glein_Vertex3d(GLdouble x, GLdouble y, GLdouble z) { glein_vertex((float)x, (float)y, (float)z); }
static void APIENTRY glein_Vertex3fv(const GLfloat *v) { glein_vertex(v[0], v[1], v[2]); }

static void glein_texCoordOf(int unit, float s, float t)
{
    if ((unsigned)unit >= GLEIN_UNITS)
        return;
    float *tc = glein_texCoord[unit];
    tc[0] = s, tc[1] = t, tc[2] = 0.f, tc[3] = 1.f;
}

static void APIENTRY glein_TexCoord2f(GLfloat s, GLfloat t) { glein_texCoordOf(0, s, t); }
static void APIENTRY glein_TexCoord2d(GLdouble s, GLdouble t) { glein_texCoordOf(0, (float)s, (float)t); }
static void APIENTRY glein_TexCoord2fv(const GLfloat *v) { glein_texCoordOf(0, v[0], v[1]); }
static void APIENTRY glein_MultiTexCoord2f(GLenum target, GLfloat s, GLfloat t) { glein_texCoordOf(target - GL_TEXTURE0, s, t); }
static void APIENTRY glein_MultiTexCoord2fv(GLenum target, const GLfloat *v) { glein_texCoordOf(target - GL_TEXTURE0, v[0], v[1]); }

static void glein_colorOf(float r, float g, float b, float a)
{
    glein_color[0] = r, glein_color[1] = g, glein_color[2] = b, glein_color[3] = a;
}

static void APIENTRY glein_Color3f(GLfloat r, GLfloat g, GLfloat b) { glein_colorOf(r, g, b, 1.f); }
static void APIENTRY glein_Color3fv(const GLfloat *v) { glein_colorOf(v[0], v[1], v[2], 1.f); }
static void APIENTRY glein_Color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { glein_colorOf(r, g, b, a); }
static void APIENTRY glein_Color4fv(const GLfloat *v) { glein_colorOf(v[0], v[1], v[2], v[3]); }
static void APIENTRY glein_Color3ub(GLubyte r, GLubyte g, GLubyte b) { glein_colorOf(r / 255.f, g / 255.f, b / 255.f, 1.f); }
static void APIENTRY glein_Color4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a) { glein_colorOf(r / 255.f, g / 255.f, b / 255.f, a / 255.f); }
static void APIENTRY glein_Color4ubv(const GLubyte *v) { glein_colorOf(v[0] / 255.f, v[1] / 255.f, v[2] / 255.f, v[3] / 255.f); }

static void glein_rect(float x1, float y1, float x2, float y2)
{
    glein_Begin(GL_TRIANGLE_FAN);
    glein_vertex(x1, y1, 0.f);
    glein_vertex(x2, y1, 0.f);
    glein_vertex(x2, y2, 0.f);
    glein_vertex(x1, y2, 0.f);
    glein_End();
}

static void APIENTRY glein_Recti(GLint x1, GLint y1, GLint x2, GLint y2) { glein_rect(x1, y1, x2, y2); }
static void APIENTRY glein_Rectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2) { glein_rect(x1, y1, x2, y2); }

////////// matrices //////////

static void APIENTRY glein_MatrixMode(GLenum mode) { glein_matrixMode = mode; }
static void APIENTRY glein_LoadIdentity(void) { glein_identity(glein_current()); }
static void APIENTRY glein_LoadMatrixf(const GLfloat *m) { Bmemcpy(glein_current(), m, 16 * sizeof(float)); }

static void APIENTRY glein_MultMatrixf(const GLfloat *m)
{
    double b[16];
    for (int i = 0; i < 16; i++)
        b[i] = m[i];
    glein_multiply(glein_current(), b);
}

static void APIENTRY glein_PushMatrix(void)
{
    glein_stack &s = glein_currentStack();
    if (s.top + 1 >= GLEIN_STACK)
    {
        GLEIN_WARN_ONCE("matrix stack overflow");
        return;
    }
    Bmemcpy(s.m[s.top + 1], s.m[s.top], 16 * sizeof(float));
    s.top++;
}

static void APIENTRY glein_PopMatrix(void)
{
    glein_stack &s = glein_currentStack();
    if (s.top > 0)
        s.top--;
}

static void APIENTRY glein_Ortho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
    double const m[16] = { 2 / (r - l), 0, 0, 0, 0, 2 / (t - b), 0, 0, 0, 0, -2 / (f - n), 0,
                           -(r + l) / (r - l), -(t + b) / (t - b), -(f + n) / (f - n), 1 };
    glein_multiply(glein_current(), m);
}

static void APIENTRY glein_Frustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
    double const m[16] = { 2 * n / (r - l), 0, 0, 0, 0, 2 * n / (t - b), 0, 0, (r + l) / (r - l), (t + b) / (t - b), -(f + n) / (f - n), -1,
                           0, 0, -2 * f * n / (f - n), 0 };
    glein_multiply(glein_current(), m);
}

static void APIENTRY glein_Translatef(GLfloat x, GLfloat y, GLfloat z)
{
    double const m[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1 };
    glein_multiply(glein_current(), m);
}

static void APIENTRY glein_Scalef(GLfloat x, GLfloat y, GLfloat z)
{
    double const m[16] = { x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1 };
    glein_multiply(glein_current(), m);
}

static void APIENTRY glein_Rotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    double const len = sqrt((double)x * x + (double)y * y + (double)z * z);
    if (len == 0)
        return;
    double const a = angle * (3.14159265358979323846 / 180.0), c = cos(a), s = sin(a), nx = x / len, ny = y / len, nz = z / len, ic = 1 - c;
    double const m[16] = { nx * nx * ic + c, ny * nx * ic + nz * s, nx * nz * ic - ny * s, 0,
                           nx * ny * ic - nz * s, ny * ny * ic + c, ny * nz * ic + nx * s, 0,
                           nx * nz * ic + ny * s, ny * nz * ic - nx * s, nz * nz * ic + c, 0,
                           0, 0, 0, 1 };
    glein_multiply(glein_current(), m);
}

////////// fixed-function state //////////

static void glein_capability(GLenum cap, bool on)
{
    uint32_t bit;
    switch (cap)
    {
        case GL_TEXTURE_2D: glein_texture2D[glein_activeUnit] = on; return;
        case GL_ALPHA_TEST: glein_alphaTest = on; return;
        case GL_FOG: glein_fogOn = on; return;
        case GL_BLEND: bit = CAP_BLEND; break;
        case GL_CULL_FACE: bit = CAP_CULL_FACE; break;
        case GL_DEPTH_TEST: bit = CAP_DEPTH_TEST; break;
        case GL_SCISSOR_TEST: bit = CAP_SCISSOR_TEST; break;
        case GL_STENCIL_TEST: bit = CAP_STENCIL_TEST; break;
        case GL_POLYGON_OFFSET_FILL: bit = CAP_POLYGON_OFFSET_FILL; break;
        default: return;  // depth clamp, line smoothing, debug output: none on WebGL
    }
    uint32_t const caps = on ? (glein_want.caps | bit) : (glein_want.caps & ~bit);
    if (caps != glein_want.caps)
        glein_want.caps = caps, glein_rasterDirty = true;
}

static void APIENTRY glein_Enable(GLenum cap) { glein_capability(cap, true); }
static void APIENTRY glein_Disable(GLenum cap) { glein_capability(cap, false); }

static GLboolean APIENTRY glein_IsEnabled(GLenum cap)
{
    switch (cap)
    {
        case GL_TEXTURE_2D: return glein_texture2D[glein_activeUnit];
        case GL_ALPHA_TEST: return glein_alphaTest;
        case GL_FOG: return glein_fogOn;
        case GL_BLEND: return !!(glein_want.caps & CAP_BLEND);
        case GL_CULL_FACE: return !!(glein_want.caps & CAP_CULL_FACE);
        case GL_DEPTH_TEST: return !!(glein_want.caps & CAP_DEPTH_TEST);
        case GL_SCISSOR_TEST: return !!(glein_want.caps & CAP_SCISSOR_TEST);
        case GL_STENCIL_TEST: return !!(glein_want.caps & CAP_STENCIL_TEST);
        case GL_POLYGON_OFFSET_FILL: return !!(glein_want.caps & CAP_POLYGON_OFFSET_FILL);
        case GL_VERTEX_ARRAY: return glein_vertexArray.enabled;
        case GL_COLOR_ARRAY: return glein_colorArray.enabled;
        case GL_TEXTURE_COORD_ARRAY: return glein_texArray[glein_clientUnit].enabled;
        default: return GL_FALSE;
    }
}

static void glein_fog(GLenum pname, float value)
{
    switch (pname)
    {
        case GL_FOG_START: glein_fogStart = value; break;
        case GL_FOG_END: glein_fogEnd = value; break;
        default: break;  // the shaders' fog is linear
    }
}

static void APIENTRY glein_Fogf(GLenum pname, GLfloat param) { glein_fog(pname, param); }
static void APIENTRY glein_Fogi(GLenum pname, GLint param) { glein_fog(pname, (float)param); }

static void APIENTRY glein_Fogfv(GLenum pname, const GLfloat *params)
{
    if (pname == GL_FOG_COLOR)
        Bmemcpy(glein_fogColor, params, sizeof(glein_fogColor));
    else
        glein_fog(pname, params[0]);
}

static void APIENTRY glein_Fogiv(GLenum pname, const GLint *params)
{
    if (pname == GL_FOG_COLOR)
        for (int i = 0; i < 4; i++)
            glein_fogColor[i] = params[i] / 2147483647.f;
    else
        glein_fog(pname, (float)params[0]);
}

static void APIENTRY glein_AlphaFunc(GLenum func, GLfloat ref)
{
    glein_alphaFunc = func;
    glein_alphaRefValue = min(max(ref, 0.f), 1.f);
}

// What WebGL has no use for: the texture environment stays GL_MODULATE, shading smooth,
// polygons filled.
static void APIENTRY glein_TexEnvf(GLenum target, GLenum pname, GLfloat param) { UNREFERENCED_PARAMETER(target); UNREFERENCED_PARAMETER(pname); UNREFERENCED_PARAMETER(param); }
static void APIENTRY glein_TexEnvi(GLenum target, GLenum pname, GLint param) { UNREFERENCED_PARAMETER(target); UNREFERENCED_PARAMETER(pname); UNREFERENCED_PARAMETER(param); }
static void APIENTRY glein_ShadeModel(GLenum mode) { UNREFERENCED_PARAMETER(mode); }
static void APIENTRY glein_PolygonMode(GLenum face, GLenum mode) { UNREFERENCED_PARAMETER(face); UNREFERENCED_PARAMETER(mode); }
static void APIENTRY glein_PushAttrib(GLbitfield mask) { UNREFERENCED_PARAMETER(mask); }
static void APIENTRY glein_PopAttrib(void) {}
static void APIENTRY glein_DrawBuffer(GLenum buf) { UNREFERENCED_PARAMETER(buf); }

static void APIENTRY glein_Hint(GLenum target, GLenum mode)
{
    if (target == GL_GENERATE_MIPMAP_HINT || target == GL_FRAGMENT_SHADER_DERIVATIVE_HINT)
        emscripten_glHint(target, mode);
}

// No texture cache: nothing is read back.
static void APIENTRY glein_GetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params)
{
    UNREFERENCED_PARAMETER(target); UNREFERENCED_PARAMETER(level); UNREFERENCED_PARAMETER(pname);
    *params = 0;
}

static void APIENTRY glein_GetCompressedTexImage(GLenum target, GLint level, void *img)
{
    UNREFERENCED_PARAMETER(target); UNREFERENCED_PARAMETER(level); UNREFERENCED_PARAMETER(img);
}

////////// client arrays //////////

static glein_array *glein_clientArray(GLenum array)
{
    switch (array)
    {
        case GL_VERTEX_ARRAY: return &glein_vertexArray;
        case GL_COLOR_ARRAY: return &glein_colorArray;
        case GL_TEXTURE_COORD_ARRAY: return &glein_texArray[glein_clientUnit];
        default: return nullptr;
    }
}

static void APIENTRY glein_EnableClientState(GLenum array)
{
    if (glein_array *a = glein_clientArray(array))
        a->enabled = true;
}

static void APIENTRY glein_DisableClientState(GLenum array)
{
    if (glein_array *a = glein_clientArray(array))
        a->enabled = false;
}

static void APIENTRY glein_ClientActiveTexture(GLenum texture)
{
    int const unit = texture - GL_TEXTURE0;
    if ((unsigned)unit < GLEIN_UNITS)
        glein_clientUnit = unit;
}

static void glein_pointer(glein_array &a, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    a.size = size, a.type = type, a.stride = stride, a.pointer = (uint8_t const *)pointer, a.buffer = glein_arrayBuffer;
}

static void APIENTRY glein_VertexPointer(GLint size, GLenum type, GLsizei stride, const void *pointer) { glein_pointer(glein_vertexArray, size, type, stride, pointer); }
static void APIENTRY glein_ColorPointer(GLint size, GLenum type, GLsizei stride, const void *pointer) { glein_pointer(glein_colorArray, size, type, stride, pointer); }
static void APIENTRY glein_TexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *pointer) { glein_pointer(glein_texArray[glein_clientUnit], size, type, stride, pointer); }

// Generic attributes, for programs of their own: on the default vertex array object.
static void APIENTRY glein_VertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer)
{
    glein_buffer const *b = glein_getBuffer(glein_arrayBuffer);
    if (!b || b->client)
    {
        GLEIN_WARN_ONCE("generic attributes need a buffer of static data");
        return;
    }
    glein_bindVao(0);
    glein_bindRealBuffer(GL_ARRAY_BUFFER, glein_arrayBuffer);
    emscripten_glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}

static void APIENTRY glein_EnableVertexAttribArray(GLuint index)
{
    glein_bindVao(0);
    emscripten_glEnableVertexAttribArray(index);
}

static void APIENTRY glein_DisableVertexAttribArray(GLuint index)
{
    glein_bindVao(0);
    emscripten_glDisableVertexAttribArray(index);
}

////////// buffers //////////

static glein_buffer *glein_bound(GLenum target)
{
    GLuint const name = target == GL_ARRAY_BUFFER ? glein_arrayBuffer : target == GL_ELEMENT_ARRAY_BUFFER ? glein_elementBuffer : 0;
    return glein_getBuffer(name);
}

static GLuint glein_boundName(GLenum target)
{
    return target == GL_ARRAY_BUFFER ? glein_arrayBuffer : glein_elementBuffer;
}

static void APIENTRY glein_BindBuffer(GLenum target, GLuint buffer)
{
    if (buffer)
    {
        if (buffer >= glein_buffers.size())
            glein_buffers.resize(buffer + 1, nullptr);
        if (!glein_buffers[buffer])
            glein_buffers[buffer] = new glein_buffer();
    }

    if (target == GL_ARRAY_BUFFER)
        glein_arrayBuffer = buffer;
    else if (target == GL_ELEMENT_ARRAY_BUFFER)
        glein_elementBuffer = buffer;
    else
        emscripten_glBindBuffer(target, buffer);
}

static void APIENTRY glein_BufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
    glein_buffer *b = glein_bound(target);
    if (!b)
        return;

    b->size = size;
    b->client = usage == GL_STREAM_DRAW;

    if (b->client)
    {
        b->data.resize(size);
        if (data)
            Bmemcpy(b->data.data(), data, size);
        return;
    }

    b->data.clear();
    b->data.shrink_to_fit();
    glein_bindRealBuffer(target, glein_boundName(target));
    emscripten_glBufferData(target, size, data, usage);
}

static void APIENTRY glein_BufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
    glein_buffer *b = glein_bound(target);
    if (!b)
        return;

    if (b->client)
    {
        if (offset >= 0 && offset + size <= b->size)
            Bmemcpy(b->data.data() + offset, data, size);
        return;
    }

    glein_bindRealBuffer(target, glein_boundName(target));
    emscripten_glBufferSubData(target, offset, size, data);
}

static void *glein_map(GLenum target, GLintptr offset, GLsizeiptr length)
{
    glein_buffer *b = glein_bound(target);
    if (!b || offset < 0 || offset + length > b->size)
        return nullptr;

    if (b->client)
        return b->data.data() + offset;

    b->staging = (uint8_t *)Xrealloc(b->staging, length);
    b->mapOffset = offset;
    b->mapLength = length;
    return b->staging;
}

static void *APIENTRY glein_MapBuffer(GLenum target, GLenum access)
{
    UNREFERENCED_PARAMETER(access);
    glein_buffer *b = glein_bound(target);
    return b ? glein_map(target, 0, b->size) : nullptr;
}

static void *APIENTRY glein_MapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
    UNREFERENCED_PARAMETER(access);
    return glein_map(target, offset, length);
}

static GLboolean APIENTRY glein_UnmapBuffer(GLenum target)
{
    glein_buffer *b = glein_bound(target);
    if (!b)
        return GL_FALSE;

    if (!b->client && b->staging)
    {
        glein_bindRealBuffer(target, glein_boundName(target));
        emscripten_glBufferSubData(target, b->mapOffset, b->mapLength, b->staging);
        DO_FREE_AND_NULL(b->staging);
    }
    return GL_TRUE;
}

static void APIENTRY glein_DeleteBuffers(GLsizei n, const GLuint *buffers)
{
    for (int i = 0; i < n; i++)
    {
        GLuint const name = buffers[i];
        glein_buffer *b = glein_getBuffer(name);
        if (!b)
            continue;

        Xfree(b->staging);
        delete b;
        glein_buffers[name] = nullptr;

        if (glein_arrayBuffer == name) glein_arrayBuffer = 0;
        if (glein_elementBuffer == name) glein_elementBuffer = 0;
        if (glein_realArrayBuffer == name) glein_realArrayBuffer = 0;
        for (GLuint &e : glein_realElementBuffer)
            if (e == name) e = 0;
    }
    emscripten_glDeleteBuffers(n, buffers);
}

static GLboolean APIENTRY glein_IsBuffer(GLuint buffer) { return glein_getBuffer(buffer) != nullptr; }

////////// programs and uniforms //////////

static void glein_describe(GLuint program, bool forceFF)
{
    if (program >= glein_programs.size())
        glein_programs.resize(program + 1, nullptr);

    glein_program *p = glein_programs[program];
    if (!p)
        p = glein_programs[program] = new glein_program();

    p->ff = forceFF || emscripten_glGetAttribLocation(program, "a_vertex") >= 0;
    p->texcoord34 = emscripten_glGetAttribLocation(program, "a_texcoord3") >= 0 || emscripten_glGetAttribLocation(program, "a_texcoord4") >= 0;
    for (int u = 0; u < U_COUNT; u++)
    {
        p->loc[u] = p->ff ? emscripten_glGetUniformLocation(program, glein_uniformNames[u]) : -1;
        p->sentValid[u] = false;
    }
    p->values.clear();
    p->dirty.clear();
}

static void APIENTRY glein_LinkProgram(GLuint program)
{
    for (int a = 0; a < A_COUNT; a++)
        emscripten_glBindAttribLocation(program, a, glein_attribNames[a]);

    glein_flushBatch();
    emscripten_glLinkProgram(program);

    GLint linked = 0;
    emscripten_glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        char log[1024];
        emscripten_glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        log[sizeof(log) - 1] = 0;
        glein_warn(log);
    }

    glein_describe(program, false);
}

static void APIENTRY glein_DeleteProgram(GLuint program)
{
    if (!program)
        return;
    if (glein_batchProgram == program)
        glein_flushBatch();
    if (glein_realProgram == program)
        glein_realProgram = 0, emscripten_glUseProgram(0);
    emscripten_glDeleteProgram(program);
    if (program < glein_programs.size())
    {
        delete glein_programs[program];
        glein_programs[program] = nullptr;
    }
}

static void APIENTRY glein_UseProgram(GLuint program) { glein_appProgram = program; }

// The shaders are embedded with a newline before their #version line, which GLSL ES 3.00
// wants first.
static void APIENTRY glein_ShaderSource(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length)
{
    if (count == 1)
    {
        char const *s = string[0];
        GLint n = length && length[0] >= 0 ? length[0] : (GLint)Bstrlen(s);
        while (n > 0 && (*s == '\n' || *s == '\r' || *s == ' ' || *s == '\t'))
            s++, n--;
        emscripten_glShaderSource(shader, 1, &s, &n);
        return;
    }
    emscripten_glShaderSource(shader, count, string, length);
}

// A uniform's kind: float or int, and its number of components.
enum { UK_F = 1 << 5, UK_I = 2 << 5 };

// The current program's uniform at `loc` as the renderer sets it: WebGL gets it at the
// program's next draw, if it differs from what WebGL has then.
static void glein_setUniform(GLint loc, int kind, void const *v, int n)
{
    glein_program *p = glein_getProgram(glein_appProgram);
    if (!p || loc < 0 || loc >= 4096)
        return;

    if ((size_t)loc >= p->values.size())
        p->values.resize(loc + 1, glein_uniform {});

    glein_uniform &u = p->values[loc];
    uint8_t const k = kind | n;
    if (u.kind == k && !Bmemcmp(u.v, v, n * sizeof(float)))
        return;

    u.kind = k;
    Bmemcpy(u.v, v, n * sizeof(float));
    if (!u.dirty)
    {
        u.dirty = true;
        p->dirty.push_back(loc);
    }
}

// The program's uniforms that changed since its last draw, in WebGL; what is batched
// with the program is drawn first.
static void glein_syncUniforms(glein_program *p, GLuint program)
{
    if (p->dirty.empty())
        return;

    bool bound = false;
    for (GLint const loc : p->dirty)
    {
        glein_uniform &u = p->values[loc];
        u.dirty = false;

        int const n = u.kind & 31;
        if (u.sentKind == u.kind && !Bmemcmp(u.sent, u.v, n * sizeof(float)))
            continue;

        if (!bound)
        {
            if (glein_batchProgram == program)
                glein_flushBatch();
            glein_useRealProgram(program);
            bound = true;
        }

        if (u.kind & UK_I)
            emscripten_glUniform1i(loc, *(GLint const *)u.v);
        else switch (n)
        {
            case 1: emscripten_glUniform1f(loc, u.v[0]); break;
            case 2: emscripten_glUniform2f(loc, u.v[0], u.v[1]); break;
            case 3: emscripten_glUniform3f(loc, u.v[0], u.v[1], u.v[2]); break;
            case 4: emscripten_glUniform4f(loc, u.v[0], u.v[1], u.v[2], u.v[3]); break;
            default: emscripten_glUniformMatrix4fv(loc, 1, GL_FALSE, u.v); break;
        }

        u.sentKind = u.kind;
        Bmemcpy(u.sent, u.v, n * sizeof(float));
    }
    p->dirty.clear();
}

static void APIENTRY glein_Uniform1f(GLint loc, GLfloat a)
{
    float const v[1] = { a };
    glein_setUniform(loc, UK_F, v, 1);
}

static void APIENTRY glein_Uniform2f(GLint loc, GLfloat a, GLfloat b)
{
    float const v[2] = { a, b };
    glein_setUniform(loc, UK_F, v, 2);
}

static void APIENTRY glein_Uniform3f(GLint loc, GLfloat a, GLfloat b, GLfloat c)
{
    float const v[3] = { a, b, c };
    glein_setUniform(loc, UK_F, v, 3);
}

static void APIENTRY glein_Uniform4f(GLint loc, GLfloat a, GLfloat b, GLfloat c, GLfloat d)
{
    float const v[4] = { a, b, c, d };
    glein_setUniform(loc, UK_F, v, 4);
}

static void APIENTRY glein_Uniform1i(GLint loc, GLint a) { glein_setUniform(loc, UK_I, &a, 1); }

// Uniform arrays of more than one element go to WebGL as they come.
static void glein_uniformArray(GLint loc, GLsizei count, const GLfloat *v, int n, void (*send)(GLint, GLsizei, const GLfloat *))
{
    if (count == 1)
    {
        glein_setUniform(loc, UK_F, v, n);
        return;
    }

    glein_program *p = glein_getProgram(glein_appProgram);
    if (!p || loc < 0)
        return;
    glein_syncUniforms(p, glein_appProgram);
    if (glein_batchProgram == glein_appProgram)
        glein_flushBatch();
    glein_useRealProgram(glein_appProgram);
    send(loc, count, v);
    if ((size_t)loc < p->values.size())
        p->values[loc].kind = p->values[loc].sentKind = 0;
}

static void APIENTRY glein_Uniform1fv(GLint loc, GLsizei count, const GLfloat *v) { glein_uniformArray(loc, count, v, 1, emscripten_glUniform1fv); }
static void APIENTRY glein_Uniform2fv(GLint loc, GLsizei count, const GLfloat *v) { glein_uniformArray(loc, count, v, 2, emscripten_glUniform2fv); }
static void APIENTRY glein_Uniform4fv(GLint loc, GLsizei count, const GLfloat *v) { glein_uniformArray(loc, count, v, 4, emscripten_glUniform4fv); }

static void glein_sendMatrix(GLint loc, GLsizei count, const GLfloat *v) { emscripten_glUniformMatrix4fv(loc, count, GL_FALSE, v); }

static void APIENTRY glein_UniformMatrix4fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v)
{
    if (transpose)
    {
        GLEIN_WARN_ONCE("transposed uniform matrices are not supported");
        return;
    }
    glein_uniformArray(loc, count, v, 16, glein_sendMatrix);
}

////////// textures //////////

static void APIENTRY glein_ActiveTexture(GLenum texture)
{
    int const unit = texture - GL_TEXTURE0;
    if ((unsigned)unit < GLEIN_UNITS)
        glein_activeUnit = unit;
}

static void APIENTRY glein_BindTexture(GLenum target, GLuint texture)
{
    if (target != GL_TEXTURE_2D)
    {
        glein_flushBatch();
        glein_activeRealUnit(glein_activeUnit);
        emscripten_glBindTexture(target, texture);
        return;
    }

    GLuint &bound = glein_want.texture[glein_activeUnit];
    if (bound != texture)
        bound = texture, glein_rasterDirty = true;
}

static void APIENTRY glein_DeleteTextures(GLsizei n, const GLuint *textures)
{
    glein_flushBatch();
    for (int i = 0; i < n; i++)
    {
        GLuint const name = textures[i];
        for (int u = 0; u < GLEIN_UNITS; u++)
        {
            if (glein_want.texture[u] == name) glein_want.texture[u] = 0, glein_rasterDirty = true;
            if (glein_have.texture[u] == name) glein_have.texture[u] = 0, glein_rasterDirty = true;
        }
        if (name < glein_textures.size() && glein_textures[name])
        {
            if (glein_textures[name]->dirty)
                glein_texturesDirty--;
            delete glein_textures[name];
            glein_textures[name] = nullptr;
        }
    }
    emscripten_glDeleteTextures(n, textures);
}

static void glein_texParameter(GLenum target, GLenum pname, float value, bool isFloat)
{
    int slot = -1;
    for (int i = 0; i < TP_COUNT; i++)
        if (glein_texParamNames[i] == pname)
            slot = i;

    glein_texture *t = target == GL_TEXTURE_2D ? glein_getTexture(glein_want.texture[glein_activeUnit], true) : nullptr;

    if (slot < 0 || !t)
    {
        glein_bindActiveTexture();
        if (isFloat)
            emscripten_glTexParameterf(target, pname, value);
        else
            emscripten_glTexParameteri(target, pname, (GLint)value);
        return;
    }

    if (t->want[slot] == value)
        return;

    t->want[slot] = value;
    if (!t->dirty)
    {
        t->dirty = true;
        glein_texturesDirty++;
    }
}

static void APIENTRY glein_TexParameteri(GLenum target, GLenum pname, GLint param) { glein_texParameter(target, pname, (float)param, false); }
static void APIENTRY glein_TexParameterf(GLenum target, GLenum pname, GLfloat param) { glein_texParameter(target, pname, param, true); }

// Texture formats WebGL 2 takes: sized internal formats, RGBA for BGRA data.
static GLint glein_internalFormat(GLint internalformat)
{
    switch (internalformat)
    {
        case GL_RED: case GL_R8: return GL_R8;
        case GL_COMPRESSED_RGBA: case GL_RGBA8: case GL_RGBA: case 4: return GL_RGBA8;
        case GL_RGB8: case GL_RGB: case 3: return GL_RGB8;
        default: return internalformat;
    }
}

static void const *glein_pixels(GLsizei width, GLsizei height, GLenum format, void const *pixels)
{
    if (format != GL_BGRA || !pixels)
        return pixels;

    size_t const n = (size_t)width * height * 4;
    glein_swizzle.resize(n);
    uint8_t const *src = (uint8_t const *)pixels;
    uint8_t *dst = glein_swizzle.data();
    for (size_t i = 0; i < n; i += 4)
    {
        dst[i + 0] = src[i + 2];
        dst[i + 1] = src[i + 1];
        dst[i + 2] = src[i + 0];
        dst[i + 3] = src[i + 3];
    }
    return dst;
}

static GLenum glein_format(GLenum format) { return format == GL_BGRA ? GL_RGBA : format; }
static GLenum glein_type(GLenum type) { return type == GL_UNSIGNED_INT_8_8_8_8_REV ? GL_UNSIGNED_BYTE : type; }

static void APIENTRY glein_TexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border,
                                      GLenum format, GLenum type, const void *pixels)
{
    glein_flushBatch();
    glein_bindActiveTexture();
    emscripten_glTexImage2D(target, level, glein_internalFormat(internalformat), width, height, border, glein_format(format), glein_type(type),
                            glein_pixels(width, height, format, pixels));
}

static void APIENTRY glein_TexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                         GLenum format, GLenum type, const void *pixels)
{
    glein_flushBatch();
    glein_bindActiveTexture();
    emscripten_glTexSubImage2D(target, level, xoffset, yoffset, width, height, glein_format(format), glein_type(type),
                               glein_pixels(width, height, format, pixels));
}

static void APIENTRY glein_CompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height,
                                                GLint border, GLsizei imageSize, const void *data)
{
    glein_flushBatch();
    glein_bindActiveTexture();
    emscripten_glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
}

static void APIENTRY glein_BindSampler(GLuint unit, GLuint sampler)
{
    glein_flushBatch();
    emscripten_glBindSampler(unit, sampler);
}

////////// raster state //////////

#define GLEIN_WANT(field, value) do { if (glein_want.field != (value)) glein_want.field = (value), glein_rasterDirty = true; } while (0)

static void APIENTRY glein_BlendFunc(GLenum s, GLenum d) { GLEIN_WANT(blend[0], s); GLEIN_WANT(blend[1], d); GLEIN_WANT(blend[2], s); GLEIN_WANT(blend[3], d); }
static void APIENTRY glein_BlendFuncSeparate(GLenum s, GLenum d, GLenum sa, GLenum da) { GLEIN_WANT(blend[0], s); GLEIN_WANT(blend[1], d); GLEIN_WANT(blend[2], sa); GLEIN_WANT(blend[3], da); }
static void APIENTRY glein_BlendEquation(GLenum e) { GLEIN_WANT(blendEquation[0], e); GLEIN_WANT(blendEquation[1], e); }
static void APIENTRY glein_BlendEquationSeparate(GLenum e, GLenum ea) { GLEIN_WANT(blendEquation[0], e); GLEIN_WANT(blendEquation[1], ea); }
static void APIENTRY glein_DepthFunc(GLenum f) { GLEIN_WANT(depthFunc, f); }
static void APIENTRY glein_DepthMask(GLboolean m) { GLEIN_WANT(depthMask, (GLboolean)!!m); }
static void APIENTRY glein_CullFace(GLenum m) { GLEIN_WANT(cullFace, m); }
static void APIENTRY glein_FrontFace(GLenum m) { GLEIN_WANT(frontFace, m); }
static void APIENTRY glein_LineWidth(GLfloat w) { GLEIN_WANT(lineWidth, w); }

static void APIENTRY glein_ColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
    GLEIN_WANT(colorMask[0], (GLboolean)!!r); GLEIN_WANT(colorMask[1], (GLboolean)!!g);
    GLEIN_WANT(colorMask[2], (GLboolean)!!b); GLEIN_WANT(colorMask[3], (GLboolean)!!a);
}

static void APIENTRY glein_StencilFunc(GLenum f, GLint ref, GLuint mask) { GLEIN_WANT(stencilFunc, f); GLEIN_WANT(stencilRef, ref); GLEIN_WANT(stencilMask, mask); }
static void APIENTRY glein_StencilOp(GLenum a, GLenum b, GLenum c) { GLEIN_WANT(stencilOp[0], a); GLEIN_WANT(stencilOp[1], b); GLEIN_WANT(stencilOp[2], c); }
static void APIENTRY glein_Scissor(GLint x, GLint y, GLsizei w, GLsizei h) { GLEIN_WANT(scissor[0], x); GLEIN_WANT(scissor[1], y); GLEIN_WANT(scissor[2], w); GLEIN_WANT(scissor[3], h); }
static void APIENTRY glein_Viewport(GLint x, GLint y, GLsizei w, GLsizei h) { GLEIN_WANT(viewport[0], x); GLEIN_WANT(viewport[1], y); GLEIN_WANT(viewport[2], w); GLEIN_WANT(viewport[3], h); }
static void APIENTRY glein_ClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { GLEIN_WANT(clearColor[0], r); GLEIN_WANT(clearColor[1], g); GLEIN_WANT(clearColor[2], b); GLEIN_WANT(clearColor[3], a); }
static void APIENTRY glein_PolygonOffset(GLfloat f, GLfloat u) { GLEIN_WANT(polygonOffset[0], f); GLEIN_WANT(polygonOffset[1], u); }
static void APIENTRY glein_DepthRange(GLdouble n, GLdouble f) { GLEIN_WANT(depthRange[0], (GLfloat)n); GLEIN_WANT(depthRange[1], (GLfloat)f); }
static void APIENTRY glein_DepthRangef(GLfloat n, GLfloat f) { GLEIN_WANT(depthRange[0], n); GLEIN_WANT(depthRange[1], f); }

#undef GLEIN_WANT

static void APIENTRY glein_Clear(GLbitfield mask)
{
    glein_syncRaster();
    glein_flushBatch();
    emscripten_glClear(mask);
}

static void APIENTRY glein_ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels)
{
    glein_flushBatch();
    emscripten_glReadPixels(x, y, width, height, format, type, pixels);
}

static void APIENTRY glein_Finish(void) { glein_flushBatch(); emscripten_glFinish(); }
static void APIENTRY glein_Flush(void) { glein_flushBatch(); emscripten_glFlush(); }

////////// queries //////////

static void APIENTRY glein_GetFloatv(GLenum pname, GLfloat *data)
{
    switch (pname)
    {
        case GL_MODELVIEW_MATRIX: Bmemcpy(data, glein_modelview.m[glein_modelview.top], 16 * sizeof(float)); return;
        case GL_PROJECTION_MATRIX: Bmemcpy(data, glein_projection.m[glein_projection.top], 16 * sizeof(float)); return;
        case GL_TEXTURE_MATRIX:
        {
            glein_stack const &s = glein_texmat[glein_activeUnit];
            Bmemcpy(data, s.m[s.top], 16 * sizeof(float));
            return;
        }
        case GL_CURRENT_COLOR: Bmemcpy(data, glein_color, 4 * sizeof(float)); return;
        default: emscripten_glGetFloatv(pname, data); return;
    }
}

static void APIENTRY glein_GetIntegerv(GLenum pname, GLint *data)
{
    switch (pname)
    {
        case GL_ARRAY_BUFFER_BINDING: *data = glein_arrayBuffer; return;
        case GL_ELEMENT_ARRAY_BUFFER_BINDING: *data = glein_elementBuffer; return;
        case GL_CURRENT_PROGRAM: *data = glein_appProgram; return;
        case GL_ACTIVE_TEXTURE: *data = GL_TEXTURE0 + glein_activeUnit; return;
        case GL_TEXTURE_BINDING_2D: *data = glein_want.texture[glein_activeUnit]; return;
        case GL_MATRIX_MODE: *data = glein_matrixMode; return;
        case GL_VIEWPORT: Bmemcpy(data, glein_want.viewport, sizeof(glein_want.viewport)); return;
        default: emscripten_glGetIntegerv(pname, data); return;
    }
}

////////// the built-in program //////////

static char const glein_builtinVert[] =
    "#version 300 es\n"
    "in vec4 a_vertex;\n"
    "in vec4 a_texcoord0;\n"
    "in vec4 a_color;\n"
    "in vec2 a_fog;\n"
    "uniform mat4 u_modelView;\n"
    "uniform mat4 u_projection;\n"
    "uniform mat4 u_textureMatrix0;\n"
    "out vec4 v_color;\n"
    "out vec2 v_texCoord;\n"
    "out float v_fogCoord;\n"
    "flat out vec2 v_fog;\n"
    "void main()\n"
    "{\n"
    "    vec4 eye = u_modelView * a_vertex;\n"
    "    gl_Position = u_projection * eye;\n"
    "    gl_PointSize = 1.0;\n"
    "    v_texCoord = (u_textureMatrix0 * a_texcoord0).xy;\n"
    "    v_color = a_color;\n"
    "    v_fogCoord = abs(eye.z / eye.w);\n"
    "    v_fog = a_fog;\n"
    "}\n";

static char const glein_builtinFrag[] =
    "#version 300 es\n"
    "precision highp float;\n"
    "uniform sampler2D s_texture;\n"
    "uniform float u_ffTexturing;\n"
    "uniform float u_ffFog;\n"
    "uniform vec4 u_fogColor;\n"
    "uniform float u_alphaRef;\n"
    "in vec4 v_color;\n"
    "in vec2 v_texCoord;\n"
    "in float v_fogCoord;\n"
    "flat in vec2 v_fog;\n"
    "out vec4 o_color;\n"
    "void main()\n"
    "{\n"
    "    vec4 color = v_color;\n"
    "    if (u_ffTexturing > 0.5)\n"
    "        color *= texture(s_texture, v_texCoord);\n"
    "    if (u_ffFog > 0.5)\n"
    "        color.rgb = mix(u_fogColor.rgb, color.rgb, clamp((v_fog.x - v_fogCoord) * v_fog.y, 0.0, 1.0));\n"
    "    if (color.a <= u_alphaRef)\n"
    "        discard;\n"
    "    o_color = color;\n"
    "}\n";

static GLuint glein_compile(GLenum type, char const *source)
{
    GLuint const shader = emscripten_glCreateShader(type);
    emscripten_glShaderSource(shader, 1, &source, nullptr);
    emscripten_glCompileShader(shader);

    GLint ok = 0;
    emscripten_glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[1024];
        emscripten_glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        log[sizeof(log) - 1] = 0;
        glein_warn(log);
    }
    return shader;
}

static void glein_makeBuiltin(void)
{
    GLuint const program = emscripten_glCreateProgram();
    GLuint const vert = glein_compile(GL_VERTEX_SHADER, glein_builtinVert);
    GLuint const frag = glein_compile(GL_FRAGMENT_SHADER, glein_builtinFrag);
    emscripten_glAttachShader(program, vert);
    emscripten_glAttachShader(program, frag);
    glein_LinkProgram(program);
    emscripten_glDeleteShader(vert);
    emscripten_glDeleteShader(frag);

    glein_useRealProgram(program);
    emscripten_glUniform1i(emscripten_glGetUniformLocation(program, "s_texture"), 0);
    glein_builtinProgram = program;
}

////////// init //////////

void glbuild_ein_init(void)
{
    glein_identity(glein_modelview.m[0]);
    glein_identity(glein_projection.m[0]);
    for (auto &s : glein_texmat)
        glein_identity(s.m[0]);
    for (auto &tc : glein_texCoord)
        tc[0] = tc[1] = tc[2] = 0.f, tc[3] = 1.f;

    // WebGL's defaults
    Bmemset(&glein_want, 0, sizeof(glein_want));
    glein_want.blend[0] = glein_want.blend[2] = GL_ONE;
    glein_want.blend[1] = glein_want.blend[3] = GL_ZERO;
    glein_want.blendEquation[0] = glein_want.blendEquation[1] = GL_FUNC_ADD;
    glein_want.depthFunc = GL_LESS;
    glein_want.cullFace = GL_BACK;
    glein_want.frontFace = GL_CCW;
    glein_want.depthMask = GL_TRUE;
    for (auto &m : glein_want.colorMask)
        m = GL_TRUE;
    glein_want.stencilFunc = GL_ALWAYS;
    glein_want.stencilMask = ~0u;
    for (auto &op : glein_want.stencilOp)
        op = GL_KEEP;
    emscripten_glGetIntegerv(GL_SCISSOR_BOX, glein_want.scissor);
    emscripten_glGetIntegerv(GL_VIEWPORT, glein_want.viewport);
    glein_want.depthRange[1] = 1.f;
    glein_want.lineWidth = 1.f;
    glein_have = glein_want;

    emscripten_glGenVertexArrays(1, &glein_layerVao);
    emscripten_glGenBuffers(1, &glein_ring);
    emscripten_glGenBuffers(1, &glein_directRing);
    emscripten_glGenBuffers(1, &glein_indexRing);

    glein_bindVao(glein_layerVao);
    glein_bindRealBuffer(GL_ARRAY_BUFFER, glein_ring);
    emscripten_glBufferData(GL_ARRAY_BUFFER, glein_ringSize, nullptr, GL_STREAM_DRAW);
    glein_bindRealBuffer(GL_ARRAY_BUFFER, glein_directRing);
    emscripten_glBufferData(GL_ARRAY_BUFFER, glein_directSize, nullptr, GL_STREAM_DRAW);
    glein_bindRealBuffer(GL_ELEMENT_ARRAY_BUFFER, glein_indexRing);
    emscripten_glBufferData(GL_ELEMENT_ARRAY_BUFFER, glein_indexSize, nullptr, GL_STREAM_DRAW);
    glein_resetLayerVao();

    glein_makeBuiltin();

    glein_batch.reserve(GLEIN_MAXBATCH);

#define GLEIN_POINT(name) glad_gl##name = glein_##name
    GLEIN_POINT(ActiveTexture);
    GLEIN_POINT(AlphaFunc);
    GLEIN_POINT(Begin);
    GLEIN_POINT(BindBuffer);
    GLEIN_POINT(BindSampler);
    GLEIN_POINT(BindTexture);
    GLEIN_POINT(BlendEquation);
    GLEIN_POINT(BlendEquationSeparate);
    GLEIN_POINT(BlendFunc);
    GLEIN_POINT(BlendFuncSeparate);
    GLEIN_POINT(BufferData);
    GLEIN_POINT(BufferSubData);
    GLEIN_POINT(Clear);
    GLEIN_POINT(ClearColor);
    GLEIN_POINT(ClientActiveTexture);
    GLEIN_POINT(Color3f);
    GLEIN_POINT(Color3fv);
    GLEIN_POINT(Color3ub);
    GLEIN_POINT(Color4f);
    GLEIN_POINT(Color4fv);
    GLEIN_POINT(Color4ub);
    GLEIN_POINT(Color4ubv);
    GLEIN_POINT(ColorMask);
    GLEIN_POINT(ColorPointer);
    GLEIN_POINT(CompressedTexImage2D);
    GLEIN_POINT(CullFace);
    GLEIN_POINT(DeleteBuffers);
    GLEIN_POINT(DeleteProgram);
    GLEIN_POINT(DeleteTextures);
    GLEIN_POINT(DepthFunc);
    GLEIN_POINT(DepthMask);
    GLEIN_POINT(DepthRange);
    GLEIN_POINT(DepthRangef);
    GLEIN_POINT(Disable);
    GLEIN_POINT(DisableClientState);
    GLEIN_POINT(DisableVertexAttribArray);
    GLEIN_POINT(DrawArrays);
    GLEIN_POINT(DrawBuffer);
    GLEIN_POINT(DrawElements);
    GLEIN_POINT(DrawRangeElements);
    GLEIN_POINT(Enable);
    GLEIN_POINT(EnableClientState);
    GLEIN_POINT(EnableVertexAttribArray);
    GLEIN_POINT(End);
    GLEIN_POINT(Finish);
    GLEIN_POINT(Flush);
    GLEIN_POINT(Fogf);
    GLEIN_POINT(Fogfv);
    GLEIN_POINT(Fogi);
    GLEIN_POINT(Fogiv);
    GLEIN_POINT(FrontFace);
    GLEIN_POINT(Frustum);
    GLEIN_POINT(GetCompressedTexImage);
    GLEIN_POINT(GetFloatv);
    GLEIN_POINT(GetIntegerv);
    GLEIN_POINT(GetTexLevelParameteriv);
    GLEIN_POINT(Hint);
    GLEIN_POINT(IsBuffer);
    GLEIN_POINT(IsEnabled);
    GLEIN_POINT(LineWidth);
    GLEIN_POINT(LinkProgram);
    GLEIN_POINT(LoadIdentity);
    GLEIN_POINT(LoadMatrixf);
    GLEIN_POINT(MapBuffer);
    GLEIN_POINT(MapBufferRange);
    GLEIN_POINT(MatrixMode);
    GLEIN_POINT(MultMatrixf);
    GLEIN_POINT(MultiTexCoord2f);
    GLEIN_POINT(MultiTexCoord2fv);
    GLEIN_POINT(Ortho);
    GLEIN_POINT(PolygonMode);
    GLEIN_POINT(PolygonOffset);
    GLEIN_POINT(PopAttrib);
    GLEIN_POINT(PopMatrix);
    GLEIN_POINT(PushAttrib);
    GLEIN_POINT(PushMatrix);
    GLEIN_POINT(ReadPixels);
    GLEIN_POINT(Rectf);
    GLEIN_POINT(Recti);
    GLEIN_POINT(Rotatef);
    GLEIN_POINT(Scalef);
    GLEIN_POINT(Scissor);
    GLEIN_POINT(ShadeModel);
    GLEIN_POINT(ShaderSource);
    GLEIN_POINT(StencilFunc);
    GLEIN_POINT(StencilOp);
    GLEIN_POINT(TexCoord2d);
    GLEIN_POINT(TexCoord2f);
    GLEIN_POINT(TexCoord2fv);
    GLEIN_POINT(TexCoordPointer);
    GLEIN_POINT(TexEnvf);
    GLEIN_POINT(TexEnvi);
    GLEIN_POINT(TexImage2D);
    GLEIN_POINT(TexParameterf);
    GLEIN_POINT(TexParameteri);
    GLEIN_POINT(TexSubImage2D);
    GLEIN_POINT(Translatef);
    GLEIN_POINT(Uniform1f);
    GLEIN_POINT(Uniform1fv);
    GLEIN_POINT(Uniform1i);
    GLEIN_POINT(Uniform2f);
    GLEIN_POINT(Uniform2fv);
    GLEIN_POINT(Uniform3f);
    GLEIN_POINT(Uniform4f);
    GLEIN_POINT(Uniform4fv);
    GLEIN_POINT(UniformMatrix4fv);
    GLEIN_POINT(UnmapBuffer);
    GLEIN_POINT(UseProgram);
    GLEIN_POINT(Vertex2d);
    GLEIN_POINT(Vertex2f);
    GLEIN_POINT(Vertex2fv);
    GLEIN_POINT(Vertex2i);
    GLEIN_POINT(Vertex3d);
    GLEIN_POINT(Vertex3f);
    GLEIN_POINT(Vertex3fv);
    GLEIN_POINT(VertexAttribPointer);
    GLEIN_POINT(VertexPointer);
    GLEIN_POINT(Viewport);
#undef GLEIN_POINT

    // what the engine calls besides: all in WebGL 2
    static char const *const needed[] = { "glAttachShader", "glBindAttribLocation", "glCompileShader", "glCreateProgram", "glCreateShader",
                                          "glDeleteShader", "glDetachShader", "glGenBuffers", "glGenTextures", "glGetError", "glGetProgramiv",
                                          "glGetShaderInfoLog", "glGetShaderiv", "glGetString", "glGetUniformLocation", "glIsProgram",
                                          "glIsTexture", "glPixelStorei" };
    void *const *const pointers[] = { (void *const *)&glad_glAttachShader, (void *const *)&glad_glBindAttribLocation,
                                      (void *const *)&glad_glCompileShader, (void *const *)&glad_glCreateProgram,
                                      (void *const *)&glad_glCreateShader, (void *const *)&glad_glDeleteShader,
                                      (void *const *)&glad_glDetachShader, (void *const *)&glad_glGenBuffers,
                                      (void *const *)&glad_glGenTextures, (void *const *)&glad_glGetError,
                                      (void *const *)&glad_glGetProgramiv, (void *const *)&glad_glGetShaderInfoLog,
                                      (void *const *)&glad_glGetShaderiv, (void *const *)&glad_glGetString,
                                      (void *const *)&glad_glGetUniformLocation, (void *const *)&glad_glIsProgram,
                                      (void *const *)&glad_glIsTexture, (void *const *)&glad_glPixelStorei };
    for (int i = 0; i < ARRAY_SSIZE(needed); i++)
        if (!*pointers[i])
            glein_warn(needed[i]);
}

#endif
