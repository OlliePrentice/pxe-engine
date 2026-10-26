#pragma once

#include "pxe/window.hpp"

#include <GLFW/glfw3.h>

namespace pxe {

// The window's GLFW handle, kept out of the public header; Input and
// Overlay read it through here.
struct Window::Impl {
  GLFWwindow* window = nullptr;
};

}  // namespace pxe
