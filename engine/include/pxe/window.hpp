#pragma once

#include <glm/vec2.hpp>

#include <memory>
#include <string>

namespace pxe {

struct WindowOptions {
  int width = 1280;
  int height = 720;
  std::string title;
  // A hidden window still has its OpenGL context and framebuffer: for
  // drawing offscreen and reading the result back.
  bool visible = true;
  // Swap at the display's refresh rate, or as fast as frames are drawn.
  bool vsync = true;
};

// The window the game draws into, with its OpenGL 4.1 core context (forward
// compatible, no multisampling, no sRGB framebuffer, full resolution on high
// DPI displays) current on the calling thread and every GL entry point the
// engine calls loaded. One per process: it starts the windowing system and
// shuts it down again when destroyed. Throws std::runtime_error if the
// system or the window cannot be started.
class Window {
 public:
  explicit Window(const WindowOptions& options);
  ~Window();
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  // True once the user has asked to close the window, or requestClose was called.
  bool shouldClose() const;
  void requestClose();
  // Handle the window system's pending events (input, resizes, close
  // requests). Call once a frame, before Input::update.
  void pollEvents();
  // Present the frame drawn.
  void swap();
  // Size in framebuffer pixels, which is what GL draws into.
  glm::ivec2 framebufferSize() const;
  // Size in window points, which are larger than pixels on a high DPI display.
  glm::ivec2 windowSize() const;
  // Seconds since the window system started.
  double time() const;
  void setTitle(const std::string& title);

 private:
  friend class Input;
  friend class Overlay;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace pxe
