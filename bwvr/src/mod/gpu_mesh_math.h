// THE MESHES OF THE 3D PARTS ON THE GPU (docs/software-tnl.md, "The meshes on
// the GPU"). Pure math with no game and no Vulkan, so that the mod, the headset
// app and the checks in bwvr/test/controller_math/ use it.
//
// The engine transforms, lights and culls each vertex of a mesh on the CPU
// (fn_00856D40, fn_0085AB10, fn_00859530, "The vertex loops with SSE"). With
// gpu_mesh=1 the mod gives the renderer the mesh as it is in the memory of the
// game (the vertices x, y, z, u, v, nx, ny, nz of 32 bytes, and the indices),
// once, and for each draw only the values that the CPU loop reads: the matrix
// of the camera, the values of the projection, the color, the specular color
// and the light. The vertex shader (vk_mesh.vert) makes what the loop makes.
// This file has the same math in C++ for the checks.
//
// WHAT THE SHADER MAKES. The loop of the game gives the camera space point
// c = M p (M is 3 by 4: the rows of 3 at 0x00EA9EA0, the translation last), and
// for a point with z of c at least the near plane n:
//   rhw = n / cz,  z = 1 - rhw,
//   x = (1 + cx / cz) half_x,  y = half_y (1 - cy / cz)
// as the pixel of the game. A point that is out of the view gets a clip flag
// and the game clips the triangle itself (fn_0081A760). The shader gives the
// clip space point of the same pixel in the viewport of the draw, with
// w = cz / n, so the GPU clips against the near plane (cz = n) and the
// sides of the view, and the interpolation is the one of the vertices of the
// game (the renderer made XYZRHW vertices into the same clip space,
// vk_scene.vert). Each term is linear in c, so a point behind the camera
// needs no division.
#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>

#if defined(_M_IX86) || defined(_M_X64) || defined(__SSE2__)
#include <emmintrin.h>
#define BWVR_MESH_SSE2 1
#endif

namespace gpu_mesh_math {

// The values of one draw: the instance data of the vertex shader, 96 bytes.
// row[0] = (m0, m3, m6, m9) and so on: cx = row[0] . (x, y, z, 1).
struct Consts {
  float row[3][4];
  float near_plane, half_x, half_y, reserved;
  uint32_t color;     // the diffuse color of the mesh (D3DCOLOR)
  uint32_t specular;  // the specular color of the mesh (the fog)
  int32_t ambient;    // 0 to 255
  uint32_t flags;     // kLit, kTruncate
  float light[4];     // the direction of the light in the space of the mesh
};
static_assert(sizeof(Consts) == 96, "Consts");

// The bits of Consts::flags.
const uint32_t kLit = 1u;       // the color is lit by the normal
const uint32_t kTruncate = 2u;  // the light level rounds toward 0 (fn_00859530)

// From the 12 floats of the matrix of the game (rows of 3, the translation
// last) to the rows of the shader.
inline void SetMatrix(Consts* c, const float* m) {
  c->row[0][0] = m[0]; c->row[0][1] = m[3]; c->row[0][2] = m[6]; c->row[0][3] = m[9];
  c->row[1][0] = m[1]; c->row[1][1] = m[4]; c->row[1][2] = m[7]; c->row[1][3] = m[10];
  c->row[2][0] = m[2]; c->row[2][1] = m[5]; c->row[2][2] = m[8]; c->row[2][3] = m[11];
}

// The viewport of a draw, as the push constants of the renderer have it.
struct View {
  float x, y;            // X, Y of the viewport in pixels
  float inv_w, inv_h;    // 2 / Width, 2 / Height
  float min_z, inv_z;    // MinZ, 1 / (MaxZ - MinZ)
  float per_rhw;         // the move of the right eye for 1 / w
};

struct Camera {
  float x, y, z;
};

// An offset of a vertex (a person or an animal, fn_00859530) is added in the
// space of the mesh, before the matrix.
inline Camera CameraSpace(const Consts& c, const float p[3], const float* offset) {
  float q[4] = {p[0], p[1], p[2], 1.0f};
  if (offset != nullptr) {
    q[0] += offset[0];
    q[1] += offset[1];
    q[2] += offset[2];
  }
  Camera out;
  out.x = c.row[0][0] * q[0] + c.row[0][1] * q[1] + c.row[0][2] * q[2] + c.row[0][3];
  out.y = c.row[1][0] * q[0] + c.row[1][1] * q[1] + c.row[1][2] * q[2] + c.row[1][3];
  out.z = c.row[2][0] * q[0] + c.row[2][1] * q[1] + c.row[2][2] * q[2] + c.row[2][3];
  return out;
}

// The clip space position of the shader. `right_eye` and `moves`: the right
// eye of a draw that one walk moves by per_rhw * rhw.
inline void ClipPosition(const Consts& c, const View& v, const Camera& cam,
                         bool right_eye, bool moves, float out[4]) {
  const float w = cam.z / c.near_plane;
  const float kx = c.half_x * v.inv_w;
  const float ky = c.half_y * v.inv_h;
  const float ax = (c.half_x + 0.5f - v.x) * v.inv_w - 1.0f;
  const float ay = (c.half_y + 0.5f - v.y) * v.inv_h - 1.0f;
  float xc = w * ax + cam.x / c.near_plane * kx;
  const float yc = w * ay - cam.y / c.near_plane * ky;
  if (right_eye && moves) xc += v.per_rhw * v.inv_w;
  out[0] = xc;
  out[1] = yc;
  out[2] = v.inv_z * (w * (1.0f - v.min_z) - 1.0f);
  out[3] = w;
}

// The pixel of the game for a clip position (the divide that the GPU makes,
// then the inverse of the map of the viewport). x, y in pixels, z the depth
// of the game, rhw = 1 / w.
struct Screen {
  float x, y, z, rhw;
};
inline Screen ScreenOf(const float clip[4], const View& v) {
  Screen s;
  const float nx = clip[0] / clip[3], ny = clip[1] / clip[3];
  s.x = (nx + 1.0f) / v.inv_w - 0.5f + v.x;
  s.y = (ny + 1.0f) / v.inv_h - 0.5f + v.y;
  const float nz = clip[2] / clip[3];
  s.z = nz / v.inv_z + v.min_z;
  s.rhw = 1.0f / clip[3];
  return s;
}

// True when the GPU keeps the point: inside the clip volume of Vulkan
// (-w <= x <= w, -w <= y <= w, 0 <= z <= w).
inline bool InsideClip(const float clip[4]) {
  const float w = clip[3];
  return w > 0.0f && clip[0] >= -w && clip[0] <= w && clip[1] >= -w &&
         clip[1] <= w && clip[2] >= 0.0f && clip[2] <= w;
}

// The light level of a normal: the dot product of the normal and the light
// times 255, rounded to the nearest whole number (kTruncate: toward 0).
inline int32_t LightLevel(const Consts& c, const float n[3]) {
  const float dot =
      (c.light[2] * n[2] + c.light[0] * n[0] + c.light[1] * n[1]) * 255.0f;
  if ((c.flags & kTruncate) != 0) return static_cast<int32_t>(dot);
  return static_cast<int32_t>(nearbyintf(dot));
}

// The color of a vertex: the color of the mesh, or lit by the level.
// Each channel is (channel * level) >> 8, the alpha stays (the same integer
// math as LitColor of fast_transform_math.h and as the shader).
inline uint32_t VertexColor(const Consts& c, const float n[3]) {
  if ((c.flags & kLit) == 0) return c.color;
  const int32_t i = LightLevel(c, n);
  uint32_t level = static_cast<uint32_t>(c.ambient);
  if (i >= 0) {
    level = ((static_cast<uint32_t>(255 - c.ambient) * static_cast<uint32_t>(i)) >> 8) +
            static_cast<uint32_t>(c.ambient);
  }
  const uint32_t r = (((c.color >> 16) & 255u) * level) >> 8;
  const uint32_t g = (((c.color >> 8) & 255u) * level) >> 8;
  const uint32_t b = ((c.color & 255u) * level) >> 8;
  return (c.color & 0xFF000000u) | ((r & 255u) << 16) | ((g & 255u) << 8) | (b & 255u);
}

// The identity of the data of a mesh: a sum over the words of the vertices and
// the indices, so the writer sends them again when the game changed them. Four
// lanes of 32 bits, 16 bytes a step: the lane takes the word into a rotation
// of itself, and a second value adds the lane. A change of any word changes
// the sum with a very high chance, and the sum costs about 80 steps for a
// mesh of 30 vertices. Both the SSE code and the code of other CPUs give the
// same value.
struct MeshSum {
  uint32_t a[4];
  uint32_t s[4];
};
inline void SumBlock(MeshSum* m, const uint8_t* p) {
  for (int i = 0; i < 4; ++i) {
    uint32_t w;
    memcpy(&w, p + 4 * i, 4);
    m->a[i] = ((m->a[i] << 5) | (m->a[i] >> 27)) ^ w;
    m->s[i] += m->a[i];
  }
}
#ifdef BWVR_MESH_SSE2
// The same step with SSE2: 6 operations for 16 bytes.
inline void SumBlocks(MeshSum* m, const uint8_t* p, size_t blocks) {
  __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(m->a));
  __m128i s = _mm_loadu_si128(reinterpret_cast<const __m128i*>(m->s));
  for (; blocks > 0; --blocks, p += 16) {
    const __m128i w = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));
    a = _mm_xor_si128(_mm_or_si128(_mm_slli_epi32(a, 5), _mm_srli_epi32(a, 27)), w);
    s = _mm_add_epi32(s, a);
  }
  _mm_storeu_si128(reinterpret_cast<__m128i*>(m->a), a);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(m->s), s);
}
#else
inline void SumBlocks(MeshSum* m, const uint8_t* p, size_t blocks) {
  for (; blocks > 0; --blocks, p += 16) SumBlock(m, p);
}
#endif
inline uint64_t MeshHash(const void* vertices, uint32_t vertex_count,
                         const void* indices, uint32_t triangles) {
  MeshSum m = {{0x9E3779B9u, 0x85EBCA6Bu, 0xC2B2AE35u, 0x27D4EB2Fu}, {0, 0, 0, 0}};
  const uint8_t* parts[2] = {static_cast<const uint8_t*>(vertices),
                             static_cast<const uint8_t*>(indices)};
  const size_t sizes[2] = {static_cast<size_t>(vertex_count) * 32,
                           static_cast<size_t>(triangles) * 6};
  for (int k = 0; k < 2; ++k) {
    const uint8_t* p = parts[k];
    size_t n = sizes[k];
    SumBlocks(&m, p, n / 16);
    p += n / 16 * 16;
    n %= 16;
    if (n > 0) {
      uint8_t tail[16] = {};
      memcpy(tail, p, n);
      SumBlock(&m, tail);
    }
  }
  const uint32_t lo = m.s[0] ^ (m.s[1] * 31u) ^ (m.s[2] * 0x9E3779B1u) ^ m.s[3];
  const uint32_t hi = m.a[0] + m.a[1] * 3u + m.a[2] * 5u + m.a[3] * 7u + vertex_count * 0x01000193u + triangles;
  return static_cast<uint64_t>(hi) << 32 | lo;
}

// WHICH MESH TAKES THE GPU PATH. The state of the device after the material of
// the mesh: no blend (a mesh with blend sorts by depth), the depth test and
// the depth write on, and no texture on stage 1 (the renderer draws a mesh
// with stage 0 alone).
struct DeviceState {
  uint32_t alpha_blend;
  uint32_t z_enable;
  uint32_t z_write;
  bool stage1;        // a color operation and a texture on stage 1
  uint32_t z_func;    // for the log of the compare
  uint32_t cull;
};
inline bool OpaqueState(const DeviceState& s) {
  return s.alpha_blend == 0 && s.z_enable != 0 && s.z_write != 0 && !s.stage1;
}

// The limits of a mesh for the GPU path: the indices are 16 bit, so a mesh has
// at most 65535 vertices, and the stream sends a mesh of up to this size.
const uint32_t kMostVertices = 20000;
const uint32_t kMostTriangles = 40000;

}  // namespace gpu_mesh_math
