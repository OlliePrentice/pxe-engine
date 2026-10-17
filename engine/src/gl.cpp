#include "pxe/gl_loader.hpp"

#include "gl.hpp"

namespace pxe {

void loadGl(GlGetProc getProc) { pxeLoadGlPointers(getProc); }

}  // namespace pxe
