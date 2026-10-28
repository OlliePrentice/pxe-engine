#include "pxe/window.hpp"

#include "pxe/gl_loader.hpp"
#include "window_impl.hpp"

#include <stdexcept>

namespace pxe {

Window::Window(const WindowOptions& options) : impl_(std::make_unique<Impl>()) {
  if (!glfwInit()) throw std::runtime_error("glfwInit failed");
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_SAMPLES, 0);
  glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_FALSE);
  glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
  if (!options.visible) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

  impl_->window = glfwCreateWindow(options.width, options.height, options.title.c_str(), nullptr, nullptr);
  if (!impl_->window) {
    glfwTerminate();
    throw std::runtime_error("failed to create an OpenGL 4.1 window");
  }
  glfwMakeContextCurrent(impl_->window);
  glfwSwapInterval(options.vsync ? 1 : 0);
  loadGl(+[](const char* name) -> void* { return reinterpret_cast<void*>(glfwGetProcAddress(name)); });
}

Window::~Window() {
  glfwDestroyWindow(impl_->window);
  glfwTerminate();
}

bool Window::shouldClose() const { return glfwWindowShouldClose(impl_->window) != 0; }

void Window::requestClose() { glfwSetWindowShouldClose(impl_->window, GLFW_TRUE); }

void Window::pollEvents() { glfwPollEvents(); }

void Window::swap() { glfwSwapBuffers(impl_->window); }

glm::ivec2 Window::framebufferSize() const {
  glm::ivec2 size{0};
  glfwGetFramebufferSize(impl_->window, &size.x, &size.y);
  return size;
}

glm::ivec2 Window::windowSize() const {
  glm::ivec2 size{0};
  glfwGetWindowSize(impl_->window, &size.x, &size.y);
  return size;
}

double Window::time() const { return glfwGetTime(); }

void Window::setTitle(const std::string& title) { glfwSetWindowTitle(impl_->window, title.c_str()); }

}  // namespace pxe
