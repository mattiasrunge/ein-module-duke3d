// Emscripten + legacy GL emulation (EM_GL builds): texture uploads that WebGL takes.
//
// After glad has loaded the function pointers, glbuild_em_init() points
// glTexImage2D/glTexSubImage2D at wrappers that turn desktop formats into WebGL
// ones: unsized GL_RED becomes GL_R8 (WebGL 2) or GL_LUMINANCE (WebGL 1),
// GL_BGRA data is swizzled to GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV becomes
// GL_UNSIGNED_BYTE and generic compressed formats become GL_RGBA8.

#if defined __EMSCRIPTEN__ && defined USE_OPENGL

#include "compat.h"
#include "glad/glad.h"
#include "glbuild.h"
#include "glbuild_em_funcs.h"
#include "baselayer.h"

extern "C" void *emscripten_GetProcAddress(const char *name);

static PFNGLTEXIMAGE2DPROC real_glTexImage2D;
static PFNGLTEXSUBIMAGE2DPROC real_glTexSubImage2D;

static int glbuild_em_webgl2;
static uint8_t *swizzleBuf;
static size_t swizzleBufSize;

static GLint fixInternal(GLint internalformat, GLenum format)
{
    switch (internalformat)
    {
        case GL_RED:
        case GL_R8:
            return glbuild_em_webgl2 ? GL_R8 : GL_LUMINANCE;
        case GL_COMPRESSED_RGBA:
        case GL_RGBA8:
        case GL_RGBA:
        case 4:
            return glbuild_em_webgl2 ? GL_RGBA8 : GL_RGBA;
        case GL_RGB8:
        case GL_RGB:
        case 3:
            return glbuild_em_webgl2 ? GL_RGB8 : GL_RGB;
    }
    UNREFERENCED_PARAMETER(format);
    return internalformat;
}

static GLenum fixFormat(GLenum format)
{
    if (format == GL_RED && !glbuild_em_webgl2)
        return GL_LUMINANCE;
    if (format == GL_BGRA)
        return GL_RGBA;
    return format;
}

static const void *fixPixels(GLsizei width, GLsizei height, GLenum format, const void *pixels)
{
    if (format != GL_BGRA || !pixels)
        return pixels;

    size_t const n = (size_t)width * height * 4;
    if (n > swizzleBufSize)
    {
        swizzleBuf = (uint8_t *)Xrealloc(swizzleBuf, n);
        swizzleBufSize = n;
    }

    uint8_t const *src = (uint8_t const *)pixels;
    for (size_t i = 0; i < n; i += 4)
    {
        swizzleBuf[i + 0] = src[i + 2];
        swizzleBuf[i + 1] = src[i + 1];
        swizzleBuf[i + 2] = src[i + 0];
        swizzleBuf[i + 3] = src[i + 3];
    }
    return swizzleBuf;
}

static GLenum fixType(GLenum type)
{
    return (type == GL_UNSIGNED_INT_8_8_8_8_REV) ? GL_UNSIGNED_BYTE : type;
}

static void APIENTRY em_glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                                     GLint border, GLenum format, GLenum type, const void *pixels)
{
    real_glTexImage2D(target, level, fixInternal(internalformat, format), width, height, border, fixFormat(format),
                      fixType(type), fixPixels(width, height, format, pixels));
}

static void APIENTRY em_glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                                        GLsizei height, GLenum format, GLenum type, const void *pixels)
{
    real_glTexSubImage2D(target, level, xoffset, yoffset, width, height, fixFormat(format), fixType(type),
                         fixPixels(width, height, format, pixels));
}

// Legacy functions the emulation implements in JavaScript but its GetProcAddress
// does not hand out, and a few it lacks.
#undef glFogf
#undef glFogfv
#undef glFogi
#undef glTexEnvf
#undef glDepthRangef
extern "C" {
void glFogf(GLenum pname, GLfloat param);
void glFogfv(GLenum pname, const GLfloat *params);
void glFogi(GLenum pname, GLint param);
void glTexEnvf(GLenum target, GLenum pname, GLfloat param);
void glDepthRangef(GLfloat n, GLfloat f);
}

static void APIENTRY em_glDepthRange(GLdouble n, GLdouble f) { glDepthRangef((GLfloat)n, (GLfloat)f); }

static void APIENTRY em_glRecti(GLint x1, GLint y1, GLint x2, GLint y2)
{
    glBegin(GL_TRIANGLE_FAN);
    glVertex2i(x1, y1);
    glVertex2i(x2, y1);
    glVertex2i(x2, y2);
    glVertex2i(x1, y2);
    glEnd();
}

static void APIENTRY em_glMultiTexCoord2f(GLenum target, GLfloat s, GLfloat t)
{
    if (target == GL_TEXTURE0)
        glTexCoord2f(s, t);
}

// The emulation finds a program's shaders through its attachments when it draws
// with that program, so shaders stay attached and alive once linked.
static void APIENTRY em_glDetachShader(GLuint program, GLuint shader) { UNREFERENCED_PARAMETER(program); UNREFERENCED_PARAMETER(shader); }
static void APIENTRY em_glDeleteShader(GLuint shader) { UNREFERENCED_PARAMETER(shader); }

// The emulation sources glBegin/glEnd vertices from whatever GL_ARRAY_BUFFER is
// bound, and leaves its own scratch buffer bound after an immediate-mode draw.
// Desktop GL ignores the binding there, and the engine caches it, so immediate
// mode runs with the array buffer unbound and the engine's binding comes back after.
static PFNGLBINDBUFFERPROC real_glBindBuffer;
static PFNGLBEGINPROC real_glBegin;
static PFNGLENDPROC real_glEnd;
static GLuint em_arrayBuffer;

static void APIENTRY em_glBindBuffer(GLenum target, GLuint buffer)
{
    if (target == GL_ARRAY_BUFFER)
        em_arrayBuffer = buffer;
    real_glBindBuffer(target, buffer);
}

static void APIENTRY em_glBegin(GLenum mode)
{
    if (em_arrayBuffer)
        real_glBindBuffer(GL_ARRAY_BUFFER, 0);
    real_glBegin(mode);
}

static void APIENTRY em_glEnd(void)
{
    real_glEnd();
    real_glBindBuffer(GL_ARRAY_BUFFER, em_arrayBuffer);
}

// Points still-NULL glad pointers at what Emscripten provides, and logs the ones it lacks.
static void fillMissing(void)
{
    if (!glad_glFogf) glad_glFogf = glFogf;
    if (!glad_glFogfv) glad_glFogfv = glFogfv;
    if (!glad_glFogi) glad_glFogi = glFogi;
    if (!glad_glTexEnvf) glad_glTexEnvf = glTexEnvf;
    if (!glad_glDepthRange) glad_glDepthRange = em_glDepthRange;
    if (!glad_glRecti) glad_glRecti = em_glRecti;
    if (!glad_glMultiTexCoord2f) glad_glMultiTexCoord2f = em_glMultiTexCoord2f;

#define FILL(name)                                                         \
    if (!glad_##name)                                                      \
    {                                                                      \
        *(void **)&glad_##name = emscripten_GetProcAddress(#name);          \
        LOG_F(INFO, "glad: %s was NULL, %s", #name, glad_##name ? "filled" : "still missing"); \
    }
    GLBUILD_EM_FUNCS(FILL)
#undef FILL
}

void glbuild_em_init(int webgl2)
{
    glbuild_em_webgl2 = webgl2;
    fillMissing();
    if (glad_glBindBuffer != em_glBindBuffer)
    {
        real_glBindBuffer = glad_glBindBuffer;
        real_glBegin = glad_glBegin;
        real_glEnd = glad_glEnd;
        glad_glBindBuffer = em_glBindBuffer;
        glad_glBegin = em_glBegin;
        glad_glEnd = em_glEnd;
    }
    glad_glDetachShader = em_glDetachShader;
    glad_glDeleteShader = em_glDeleteShader;

    if (glad_glTexImage2D != em_glTexImage2D)
    {
        real_glTexImage2D = glad_glTexImage2D;
        real_glTexSubImage2D = glad_glTexSubImage2D;
        glad_glTexImage2D = em_glTexImage2D;
        glad_glTexSubImage2D = em_glTexSubImage2D;
    }

    // The emulation uploads a texture matrix to a user shader only once its version
    // has moved, so a matrix the program never loads would read as all zeros there.
    for (int i = 0; i < 8; i++)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
    }
    glActiveTexture(GL_TEXTURE0);
    glMatrixMode(GL_MODELVIEW);

    // WebGL 1 rows of R8/luminance data are not 4-byte aligned.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

#endif
