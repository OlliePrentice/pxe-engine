#pragma once

struct ImGuiContext;

namespace pxe {

class Window;

// Dear ImGui hosted over a window: owns ImGui's context and its platform
// (window events) and OpenGL backends. The game builds its UI with ImGui's
// own calls between begin() and end(), and styles it through ImGui::GetStyle
// and ImGui::GetIO once the overlay is made. UI layout is not saved to an
// .ini file. Create after the Window, and destroy before it; one at a time.
// Throws std::runtime_error if a backend cannot start.
class Overlay {
 public:
  explicit Overlay(Window& window);
  ~Overlay();
  Overlay(const Overlay&) = delete;
  Overlay& operator=(const Overlay&) = delete;

  // Start a UI frame. Every frame that calls begin() must call end().
  void begin();
  // Draw the frame's UI over what is already in the window's framebuffer, at
  // window resolution. Call after the scene is presented, before swap.
  void end();

  // True while a text field or slider takes typing, so keys should not
  // also drive the game.
  bool wantsKeyboard() const;
  // True while the pointer is over a UI window, so a click belongs to the UI.
  bool wantsMouse() const;

 private:
  ImGuiContext* context_ = nullptr;
};

}  // namespace pxe
