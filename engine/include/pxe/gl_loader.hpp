#pragma once

namespace pxe {

// Load every GL entry point the engine calls. `getProc` is the window
// system's GL lookup (pxe::Window passes its own when it makes its context).
// Aborts if a required symbol is missing.
using GlGetProc = void* (*)(const char* name);
void loadGl(GlGetProc getProc);

}  // namespace pxe
