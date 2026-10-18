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

void Mesh::upload(const std::vector<TerrainVertex>& vertices,
                  const std::vector<std::uint32_t>& indices) {
  destroy();
  indexCount_ = static_cast<int>(indices.size());
  if (vertices.empty() || indices.empty()) return;

  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glGenBuffers(1, &ebo_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(TerrainVertex)),
               vertices.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)), indices.data(),
               GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), reinterpret_cast<void*>(0));
  glBindVertexArray(0);
}

void Mesh::draw() const {
  if (!vao_ || indexCount_ == 0) return;
  glBindVertexArray(vao_);
  glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
}

}  // namespace pxe
