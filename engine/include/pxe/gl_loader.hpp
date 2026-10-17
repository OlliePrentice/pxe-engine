#pragma once

namespace pxe {

// Load every GL entry point the engine calls. `getProc` is glfwGetProcAddress
// (or wgl/glX equivalent). Aborts if a required symbol is missing.
using GlGetProc = void* (*)(const char* name);
void loadGl(GlGetProc getProc);

}  // namespace pxe
