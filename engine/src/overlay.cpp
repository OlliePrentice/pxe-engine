#include "pxe/overlay.hpp"

#include "window_impl.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <stdexcept>

namespace pxe {

Overlay::Overlay(Window& window) {
  IMGUI_CHECKVERSION();
  context_ = ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;
  if (!ImGui_ImplGlfw_InitForOpenGL(window.impl_->window, true)) {
    ImGui::DestroyContext(context_);
    throw std::runtime_error("ImGui GLFW backend failed to start");
  }
  if (!ImGui_ImplOpenGL3_Init("#version 410")) {
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(context_);
    throw std::runtime_error("ImGui OpenGL backend failed to start");
  }
}

Overlay::~Overlay() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext(context_);
}

void Overlay::begin() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void Overlay::end() {
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool Overlay::wantsKeyboard() const { return ImGui::GetIO().WantTextInput; }

bool Overlay::wantsMouse() const { return ImGui::GetIO().WantCaptureMouse; }

}  // namespace pxe
