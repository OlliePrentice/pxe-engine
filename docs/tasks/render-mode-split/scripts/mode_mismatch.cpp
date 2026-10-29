// Throwaway check (not part of the build): a renderer in each mode, given the other mode's frame,
// throws std::invalid_argument with the spec's message, before drawing. Build and run with
// docs/tasks/render-mode-split/scripts/mode_mismatch.sh.
#include "pxe/camera.hpp"
#include "pxe/first_person_camera.hpp"
#include "pxe/renderer.hpp"
#include "pxe/scene.hpp"
#include "pxe/window.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>

namespace {
int failures = 0;
template <class F>
void expectThrow(const char* what, const std::string& message, F&& call) {
  try {
    call();
    std::printf("FAIL %s: did not throw\n", what);
    ++failures;
  } catch (const std::invalid_argument& e) {
    const bool ok = message == e.what();
    std::printf("%s %s: \"%s\"\n", ok ? "PASS" : "FAIL", what, e.what());
    if (!ok) ++failures;
  }
}
}  // namespace

int main() {
  pxe::WindowOptions options;
  options.width = 640;
  options.height = 360;
  options.visible = false;
  options.vsync = false;
  pxe::Window window(options);
  pxe::Scene scene;
  pxe::FrameParams params;
  params.present = false;

  pxe::Renderer ortho;
  ortho.init(pxe::RenderMode::kOrthographic);
  pxe::Renderer persp;
  persp.init(pxe::RenderMode::kPerspective);
  std::printf("modes %d %d\n", static_cast<int>(ortho.mode()), static_cast<int>(persp.mode()));

  pxe::FirstPersonCamera eye;
  pxe::Camera camera;
  const pxe::PerspectiveFrame pf = eye.frame(ortho.rtW(), ortho.rtH());
  const pxe::CameraFrame cf = camera.frame(persp.rtW(), persp.rtH());

  expectThrow("ortho.render(PerspectiveFrame)",
              "Renderer::render: a PerspectiveFrame needs RenderMode::kPerspective, but init() chose kOrthographic",
              [&] { ortho.render(scene, pf, params); });
  expectThrow("persp.render(CameraFrame)",
              "Renderer::render: a CameraFrame needs RenderMode::kOrthographic, but init() chose kPerspective",
              [&] { persp.render(scene, cf, params); });
  expectThrow("persp.rayThrough", "Renderer::rayThrough: orthographic mode only",
              [&] { persp.rayThrough(cf, 1920, 1080, {10.f, 10.f}); });
  expectThrow("persp.windowPoint", "Renderer::windowPoint: orthographic mode only",
              [&] { persp.windowPoint(cf, 1920, 1080, {0.f, 0.f, 0.f}); });
  // The ordinary calls work (orthographic rayThrough returns, both renderers draw an empty scene).
  ortho.rayThrough(cf, 1920, 1080, {10.f, 10.f});
  ortho.upload(scene);
  persp.upload(scene);
  ortho.render(scene, cf, params);
  persp.render(scene, pf, params);
  std::printf("%s\n", failures == 0 ? "all mismatches throw" : "FAILURES");
  return failures == 0 ? 0 : 1;
}
