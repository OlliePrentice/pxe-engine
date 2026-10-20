#include "pxe/mesh.hpp"

#include "gl.hpp"

namespace pxe {

Mesh::~Mesh() { destroy(); }

Mesh::Mesh(Mesh&& other) noexcept
    : vao_(other.vao_), vbo_(other.vbo_), ebo_(other.ebo_), indexCount_(other.indexCount_) {
  other.vao_ = other.vbo_ = other.ebo_ = 0;
  other.indexCount_ = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
  if (this != &other) {
    destroy();
    vao_ = other.vao_;
    vbo_ = other.vbo_;
    ebo_ = other.ebo_;
    indexCount_ = other.indexCount_;
    other.vao_ = other.vbo_ = other.ebo_ = 0;
    other.indexCount_ = 0;
  }
  return *this;
}

void Mesh::destroy() {
  if (ebo_) glDeleteBuffers(1, &ebo_);
  if (vbo_) glDeleteBuffers(1, &vbo_);
  if (vao_) glDeleteVertexArrays(1, &vao_);
  vao_ = vbo_ = ebo_ = 0;
  indexCount_ = 0;
}

void Mesh::upload(const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices) {
  destroy();
  indexCount_ = static_cast<int>(indices.size());
  if (vertices.empty() || indices.empty()) return;

  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glGenBuffers(1, &ebo_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
               vertices.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)), indices.data(),
               GL_STATIC_DRAW);
  const auto stride = static_cast<GLsizei>(sizeof(Vertex));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(sizeof(float) * 3));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(sizeof(float) * 6));
  glBindVertexArray(0);
}

void Mesh::update(const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices) {
  if (!vao_) {
    upload(vertices, indices);
    return;
  }
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), nullptr, GL_STREAM_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Mesh::draw() const {
  if (!vao_ || indexCount_ == 0) return;
  glBindVertexArray(vao_);
  glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
}

}  // namespace pxe
