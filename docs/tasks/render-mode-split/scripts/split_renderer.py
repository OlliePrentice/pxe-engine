#!/usr/bin/env python3
"""One-off: split the pre-split engine/src/renderer.cpp (HEAD of the cycle base) into the shared
renderer.cpp and renderer_orthographic.cpp, moving code by line range so it stays unchanged.
The orthographic render() is main's. Usage: split_renderer.py <worktree> <main_render.txt>"""
import subprocess, sys
wt, main_render = sys.argv[1], sys.argv[2]
orig = subprocess.check_output(['git', '-C', wt, 'show', 'HEAD:engine/src/renderer.cpp'], text=True).split('\n')
def L(a, b):  # 1-indexed inclusive
    return '\n'.join(orig[a-1:b]) + '\n'

# ---- renderer.cpp: shared machinery
shared = '''#include "pxe/renderer.hpp"

#include "pxe/config.hpp"
#include "renderer_internal.hpp"
#include "shader.hpp"
#include "gl.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <iterator>
#include <string>

namespace pxe {
namespace {

// Windows a room's sky light comes through, at most (kSkyWindows in common.glsl).
constexpr int kSkyWindows = 8;
// Dark regions drawn, at most (kDarkRegions in common.glsl and edge.frag).
constexpr int kDarkRegions = 8;

''' + L(75, 80) + '''
}  // namespace

namespace detail {

''' + L(45, 58) + '\n' + L(60, 73) + '\n' + L(144, 189) + '\n' + L(200, 267) + '''
}  // namespace detail

using namespace detail;

''' + L(271, 648).replace('void Renderer::init() {', 'void Renderer::init(RenderMode mode) {\n  mode_ = mode;') \
    + '\n' + L(671, 717).replace('void Renderer::drawPresent(const CameraFrame& camera, const FrameParams& params) {',
                                'void Renderer::drawPresent(glm::vec2 texelError, const FrameParams& params) {').replace('camera.texelError', 'texelError') \
    + '\n' + L(719, 748) + '\n' + L(1607, 1630)
open(wt + '/engine/src/renderer.cpp', 'w').write(shared)

# ---- renderer_orthographic.cpp
mm = open(main_render).read()
check = '''  if (mode_ != RenderMode::kOrthographic)
    throw std::invalid_argument(
        "Renderer::render: a CameraFrame needs RenderMode::kOrthographic, but init() chose kPerspective");
'''
first_line = 'void Renderer::render(const Scene& scene, const CameraFrame& camera, const FrameParams& params) {\n'
assert mm.startswith(first_line)
mm = first_line + check + mm[len(first_line):]
assert mm.count('drawPresent(camera, params)') == 1
mm = mm.replace('drawPresent(camera, params)', 'drawPresent(camera.texelError, params)')

def modecheck(name):
    return ('  if (mode_ != RenderMode::kOrthographic) throw std::invalid_argument("Renderer::%s: orthographic mode only");\n' % name)
rt = L(650, 669)
rt = rt.replace('glm::vec3 Renderer::rayThrough(const CameraFrame& camera, int fbW, int fbH, glm::vec2 px) const {\n',
                'glm::vec3 Renderer::rayThrough(const CameraFrame& camera, int fbW, int fbH, glm::vec2 px) const {\n' + modecheck('rayThrough'))
rt = rt.replace('glm::vec2 Renderer::windowPoint(const CameraFrame& camera, int fbW, int fbH, glm::vec3 world) const {\n',
                'glm::vec2 Renderer::windowPoint(const CameraFrame& camera, int fbW, int fbH, glm::vec3 world) const {\n' + modecheck('windowPoint'))
assert rt.count('orthographic mode only') == 2

ortho = '''#include "pxe/renderer.hpp"

#include "pxe/config.hpp"
#include "pxe/fireflies.hpp"
#include "renderer_internal.hpp"
#include "gl.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <iterator>
#include <string>

namespace pxe {
using namespace detail;
namespace {

''' + L(22, 30) + L(31, 36) + L(38, 39) + '\n' + L(87, 104) + '''
}  // namespace

''' + rt + '\n' + L(750, 769) + '\n' + L(771, 952) + '\n' + L(954, 1020) + '\n' + mm + '''
}  // namespace pxe
'''
open(wt + '/engine/src/renderer_orthographic.cpp', 'w').write(ortho)
