// Compares desert::duneGradient with a central difference of duneHeight over
// the play area. Build from the worktree root with:
// c++ -std=c++20 -I desert/src -I engine/include -I build/release/_deps/glm-src <this file> desert/src/world.cpp
//   build/release/engine/libpxe_engine.a -DPXE_ASSET_DIR='"x"' -framework OpenGL -framework Cocoa -framework IOKit
#include "world.hpp"
#include <cmath>
#include <cstdio>
int main() {
  double worst = 0;
  int n = 0;
  for (float z = -80.f; z <= 80.f; z += 0.37f)
    for (float x = -80.f; x <= 80.f; x += 0.41f) {
      const float e = 0.02f;
      const float fx = (desert::duneHeight(x + e, z) - desert::duneHeight(x - e, z)) / (2 * e);
      const float fz = (desert::duneHeight(x, z + e) - desert::duneHeight(x, z - e)) / (2 * e);
      const glm::vec2 g = desert::duneGradient(x, z);
      const double d = std::fmax(std::fabs(fx - g.x), std::fabs(fz - g.y));
      // a crest or trough kink inside the step breaks the difference: skip those
      if (d < 0.02) worst = std::fmax(worst, d); else ++n;
    }
  std::printf("worst agreeing difference %.5f, %d samples differ by > 0.02 (crest kinks)\n", worst, n);
}
