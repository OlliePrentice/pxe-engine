#pragma once

// Khronos core-profile headers declare pointer typedefs only, unless
// GL_GLEXT_PROTOTYPES is set. We load function pointers ourselves so the
// same code path works on macOS (where the OpenGL framework is deprecated),
// Windows (opengl32.dll exports only GL 1.1), and Linux.

#include <GL/glcorearb.h>

#include <cstdio>
#include <cstdlib>

#define PXE_GL_EACH(M)                              \
  M(PFNGLCULLFACEPROC, glCullFace)                  \
  M(PFNGLFRONTFACEPROC, glFrontFace)                \
  M(PFNGLSCISSORPROC, glScissor)                    \
  M(PFNGLGENQUERIESPROC, glGenQueries)              \
  M(PFNGLDELETEQUERIESPROC, glDeleteQueries)        \
  M(PFNGLBEGINQUERYPROC, glBeginQuery)              \
  M(PFNGLENDQUERYPROC, glEndQuery)                  \
  M(PFNGLGETQUERYOBJECTUI64VPROC, glGetQueryObjectui64v) \
  M(PFNGLENABLEPROC, glEnable)                      \
  M(PFNGLDISABLEPROC, glDisable)                    \
  M(PFNGLVIEWPORTPROC, glViewport)                  \
  M(PFNGLCLEARPROC, glClear)                        \
  M(PFNGLCLEARCOLORPROC, glClearColor)              \
  M(PFNGLCLEARDEPTHPROC, glClearDepth)              \
  M(PFNGLCLEARBUFFERFVPROC, glClearBufferfv)        \
  M(PFNGLDEPTHFUNCPROC, glDepthFunc)                \
  M(PFNGLDEPTHMASKPROC, glDepthMask)                \
  M(PFNGLBLENDFUNCPROC, glBlendFunc)                \
  M(PFNGLBLENDEQUATIONPROC, glBlendEquation)        \
  M(PFNGLMAPBUFFERRANGEPROC, glMapBufferRange)      \
  M(PFNGLUNMAPBUFFERPROC, glUnmapBuffer)            \
  M(PFNGLUNIFORM2FVPROC, glUniform2fv)              \
  M(PFNGLCOLORMASKPROC, glColorMask)                \
  M(PFNGLPOLYGONOFFSETPROC, glPolygonOffset)        \
  M(PFNGLGETERRORPROC, glGetError)                  \
  M(PFNGLGETSTRINGPROC, glGetString)                \
  M(PFNGLGETINTEGERVPROC, glGetIntegerv)            \
  M(PFNGLPIXELSTOREIPROC, glPixelStorei)            \
  M(PFNGLREADPIXELSPROC, glReadPixels)              \
  M(PFNGLREADBUFFERPROC, glReadBuffer)              \
  M(PFNGLDRAWBUFFERPROC, glDrawBuffer)              \
  M(PFNGLTEXPARAMETERIPROC, glTexParameteri)        \
  M(PFNGLTEXPARAMETERFVPROC, glTexParameterfv)      \
  M(PFNGLTEXIMAGE2DPROC, glTexImage2D)              \
  M(PFNGLTEXIMAGE3DPROC, glTexImage3D)              \
  M(PFNGLTEXBUFFERPROC, glTexBuffer)                \
  M(PFNGLFRAMEBUFFERTEXTURELAYERPROC, glFramebufferTextureLayer) \
  M(PFNGLCOPYTEXSUBIMAGE2DPROC, glCopyTexSubImage2D) \
  M(PFNGLBINDTEXTUREPROC, glBindTexture)            \
  M(PFNGLGENTEXTURESPROC, glGenTextures)            \
  M(PFNGLDELETETEXTURESPROC, glDeleteTextures)      \
  M(PFNGLACTIVETEXTUREPROC, glActiveTexture)        \
  M(PFNGLDRAWARRAYSPROC, glDrawArrays)              \
  M(PFNGLDRAWELEMENTSPROC, glDrawElements)          \
  M(PFNGLGENBUFFERSPROC, glGenBuffers)              \
  M(PFNGLDELETEBUFFERSPROC, glDeleteBuffers)        \
  M(PFNGLBINDBUFFERPROC, glBindBuffer)              \
  M(PFNGLBUFFERDATAPROC, glBufferData)              \
  M(PFNGLBUFFERSUBDATAPROC, glBufferSubData)        \
  M(PFNGLCREATESHADERPROC, glCreateShader)          \
  M(PFNGLSHADERSOURCEPROC, glShaderSource)          \
  M(PFNGLCOMPILESHADERPROC, glCompileShader)        \
  M(PFNGLGETSHADERIVPROC, glGetShaderiv)            \
  M(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog)  \
  M(PFNGLDELETESHADERPROC, glDeleteShader)          \
  M(PFNGLCREATEPROGRAMPROC, glCreateProgram)        \
  M(PFNGLATTACHSHADERPROC, glAttachShader)          \
  M(PFNGLLINKPROGRAMPROC, glLinkProgram)            \
  M(PFNGLGETPROGRAMIVPROC, glGetProgramiv)          \
  M(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) \
  M(PFNGLUSEPROGRAMPROC, glUseProgram)              \
  M(PFNGLDELETEPROGRAMPROC, glDeleteProgram)        \
  M(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) \
  M(PFNGLUNIFORM1FPROC, glUniform1f)                \
  M(PFNGLUNIFORM2FPROC, glUniform2f)                \
  M(PFNGLUNIFORM3FPROC, glUniform3f)                \
  M(PFNGLUNIFORM4FPROC, glUniform4f)                \
  M(PFNGLUNIFORM1IPROC, glUniform1i)                \
  M(PFNGLUNIFORM1IVPROC, glUniform1iv)              \
  M(PFNGLUNIFORM1FVPROC, glUniform1fv)              \
  M(PFNGLUNIFORM3FVPROC, glUniform3fv)              \
  M(PFNGLUNIFORM4FVPROC, glUniform4fv)              \
  M(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv)  \
  M(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
  M(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
  M(PFNGLVERTEXATTRIBDIVISORPROC, glVertexAttribDivisor) \
  M(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)    \
  M(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)    \
  M(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays) \
  M(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers)    \
  M(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer)    \
  M(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers) \
  M(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D) \
  M(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) \
  M(PFNGLDRAWBUFFERSPROC, glDrawBuffers)            \
  M(PFNGLDRAWELEMENTSINSTANCEDPROC, glDrawElementsInstanced) \
  M(PFNGLDRAWARRAYSINSTANCEDPROC, glDrawArraysInstanced) \
  M(PFNGLFINISHPROC, glFinish)

#define PXE_GL_DECLARE(TYPE, NAME) inline TYPE NAME = nullptr;
PXE_GL_EACH(PXE_GL_DECLARE)
#undef PXE_GL_DECLARE

inline void pxeLoadGlPointers(void* (*getProc)(const char*)) {
#define PXE_GL_LOAD(TYPE, NAME)                                      \
  NAME = reinterpret_cast<TYPE>(getProc(#NAME));                    \
  if (!NAME) {                                                       \
    std::fprintf(stderr, "pxe: missing GL entry point %s\n", #NAME); \
    std::exit(1);                                                    \
  }
  PXE_GL_EACH(PXE_GL_LOAD)
#undef PXE_GL_LOAD
}

#undef PXE_GL_EACH

inline void pxeGlCheck(const char* where) {
  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    std::fprintf(stderr, "pxe: GL error 0x%x at %s\n", err, where);
  }
}
