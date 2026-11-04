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

#include "tree_body_math.h"

#if defined(_M_IX86) || defined(_M_X64) || defined(__SSE2__)
#include <emmintrin.h>
#define BWVR_MESH_SSE2 1
#endif

namespace gpu_mesh_math {

// The values of one draw: the instance data of the vertex shader, 96 bytes.
// row[0] = (m0, m3, m6, m9) and so on: cx = row[0] . (x, y, z, 1).
struct Consts {
  float row[3][4];
  float near_plane, half_x, half_y;
  float uv_shift_u;   // DrawTriangle adds the shift to u (water, fire), else 0
  uint32_t color;     // the diffuse color of the mesh (D3DCOLOR)
  uint32_t specular;  // the specular color of the mesh (the fog)
  int32_t ambient;    // 0 to 255
  uint32_t flags;     // kLit, kTruncate
  float light[3];     // the direction of the light in the space of the mesh
  float uv_shift_v;
};
static_assert(sizeof(Consts) == 96, "Consts");

// The bits of Consts::flags.
const uint32_t kLit = 1u;       // the color is lit by the normal
const uint32_t kTruncate = 2u;  // the light level rounds toward 0 (fn_00859530)
// The mesh has a matrix for each group of vertices (and, with kLit, a light for
// each group). The matrices and the lights are in the ring of the renderer, at
// the float indices that the renderer puts in row[0][0] and row[0][1] of the
// record (the matrix of the record is not used then).
const uint32_t kGroup = 4u;
// The vertex shader makes the depth and the w as the game does (ClipPosition):
// rhw = near * (1 / cz) and z = 1 - rhw, each rounded to a float, then z * w.
// Two layers on the same surface (the snow on a roof, a lit copy over a mesh)
// keep the depth order that the CPU path gives them. Without it the shader
// takes the linear form (w - 1), which has a larger rounding error in z.
const uint32_t kDepthGame = 8u;
// With kDepthGame: the order of the product of y in the loop of the function
// (the shader and ClipPosition): none is (1 / cz * cy) * half_y (fn_0085AB10,
// fn_0085BA30, fn_00859530, fn_0081C090), kYOrderB is (half_y * (1 / cz)) * cy
// (fn_0084BA90) and kYOrderC is (half_y * cy) * (1 / cz) (fn_00856D40).
const uint32_t kYOrderB = 16u;
const uint32_t kYOrderC = 32u;
// The order of the sums of the camera point of fn_0081C090 (the shader).
const uint32_t kVertexOrder = 64u;
// The color of each vertex is in the first word of its normal (fn_0081C090).
const uint32_t kVertexColor = 128u;

// From the 12 floats of the matrix of the game (rows of 3, the translation
// last) to the rows of the shader.
inline void SetMatrix(Consts* c, const float* m) {
  c->row[0][0] = m[0]; c->row[0][1] = m[3]; c->row[0][2] = m[6]; c->row[0][3] = m[9];
  c->row[1][0] = m[1]; c->row[1][1] = m[4]; c->row[1][2] = m[7]; c->row[1][3] = m[10];
  c->row[2][0] = m[2]; c->row[2][1] = m[5]; c->row[2][2] = m[8]; c->row[2][3] = m[11];
}

// THE GROUPS. A person or an animal has a matrix for each group of vertices
// (the mesh group flag at 0x00FA93BC): the mesh has a list of groups, each with
// a count of vertices and the index of a matrix in the list of the part
// (0x00E9FE48, 12 floats each), and the vertices of the groups follow each
// other. The GPU gets the ordinal of the group for each vertex (with the
// data of the mesh), and for each draw the matrices of the groups in the order
// of the list, in the rows of the shader (SetMatrix).
const uint32_t kMostGroups = 128;

// A group of a mesh as the game keeps it: 4 bytes.
struct MeshGroup {
  uint16_t count;
  uint16_t matrix;
};
static_assert(sizeof(MeshGroup) == 4, "MeshGroup");

// The ordinal of the group of each vertex into `out` (vertex_count values).
// False when the counts of the groups are not the vertex count: the loop of
// the game then leaves vertices as they were.
inline bool GroupOrdinals(const MeshGroup* groups, uint32_t group_count, uint32_t vertex_count,
                          uint16_t* out) {
  if (group_count == 0 || group_count > kMostGroups) return false;
  uint32_t at = 0;
  for (uint32_t g = 0; g < group_count; ++g) {
    if (groups[g].count > vertex_count - at) return false;
    for (uint32_t i = 0; i < groups[g].count; ++i) out[at++] = static_cast<uint16_t>(g);
  }
  return at == vertex_count;
}

// The light of a group, as fn_0084BA90 and fn_0085AB10 make it: the position of
// the light (0x00EA9E80, camera space) goes into the space of the group with
// the inverse of the matrix of the group (LHMatrix::SetInverse, 0x007FB290),
// and the vector gets the length 1 with the fast inverse root
// (fn_00841170, a step of Newton from a table of 128 bytes at the pointer
// 0x00EEA394). The game has 24 bit float math here, so the float operations in
// the same order give the same bits.
inline float FastInverseSqrt(float x, const uint8_t* table) {
  uint32_t u;
  memcpy(&u, &x, 4);
  const uint32_t e = (u >> 23) & 0xFFu;
  const uint32_t bits = ((0x5F000000u - (e << 22)) & 0xFF800000u) |
                        (static_cast<uint32_t>(table[(u >> 17) & 0x7Fu]) << 15);
  float y;
  memcpy(&y, &bits, 4);
  return (3.0f - x * y * y) * y * 0.5f;
}
inline void GroupLight(const float matrix[12], const float light_position[3],
                       const uint8_t* table, float out[3]) {
  float inverse[12];
  tree_body_math::Invert(matrix, inverse);
  const float lx = inverse[0] * light_position[0] + inverse[3] * light_position[1] +
                   inverse[6] * light_position[2] + inverse[9];
  const float ly = inverse[1] * light_position[0] + inverse[4] * light_position[1] +
                   inverse[7] * light_position[2] + inverse[10];
  const float lz = inverse[2] * light_position[0] + inverse[5] * light_position[1] +
                   inverse[8] * light_position[2] + inverse[11];
  const float k = FastInverseSqrt(lx * lx + lz * lz + ly * ly, table);
  out[0] = lx * k;
  out[1] = ly * k;
  out[2] = lz * k;
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
  const bool game = (c.flags & kDepthGame) != 0 && cam.z >= c.near_plane && cam.z > 1e-4f;
  float w = cam.z / c.near_plane;
  float game_z = 0.0f;
  float game_x = 0.0f, game_y = 0.0f, game_rhw = 0.0f;
  if (game) {
    const float inv = 1.0f / cam.z;
    const float rhw = c.near_plane * inv;
    w = 1.0f / rhw;
    game_z = 1.0f - rhw;
    game_rhw = rhw;
    game_x = (inv * cam.x + 1.0f) * c.half_x;
    const float product = (c.flags & kYOrderB) != 0   ? (c.half_y * inv) * cam.y
                          : (c.flags & kYOrderC) != 0 ? (c.half_y * cam.y) * inv
                                                      : (inv * cam.y) * c.half_y;
    game_y = c.half_y - product;
  }
  const float kx = c.half_x * v.inv_w;
  const float ky = c.half_y * v.inv_h;
  const float ax = (c.half_x + 0.5f - v.x) * v.inv_w - 1.0f;
  const float ay = (c.half_y + 0.5f - v.y) * v.inv_h - 1.0f;
  float xc = w * ax + cam.x / c.near_plane * kx;
  float yc = w * ay - cam.y / c.near_plane * ky;
  if (right_eye && moves) xc += v.per_rhw * v.inv_w;
  if (game) {
    // The position of the game, then the map of the renderer (vk_scene.vert).
    float sx = game_x;
    if (right_eye && moves) sx += v.per_rhw * game_rhw;
    xc = ((sx + 0.5f - v.x) * v.inv_w - 1.0f) * w;
    yc = ((game_y + 0.5f - v.y) * v.inv_h - 1.0f) * w;
  }
  out[0] = xc;
  out[1] = yc;
  out[2] = game ? ((game_z - v.min_z) * v.inv_z) * w : v.inv_z * (w * (1.0f - v.min_z) - 1.0f);
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

// The texture coordinates of a vertex: DrawTriangle adds the shift of the part
// to u and v (water, fire), and stage 1 reads the same set as stage 0 for the
// vertex format of the game.
inline void TextureCoord(const Consts& c, const float uv[2], float out[2]) {
  out[0] = uv[0] + c.uv_shift_u;
  out[1] = uv[1] + c.uv_shift_v;
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
                         const void* indices, uint32_t triangles,
                         const void* groups = nullptr, uint32_t group_count = 0) {
  MeshSum m = {{0x9E3779B9u, 0x85EBCA6Bu, 0xC2B2AE35u, 0x27D4EB2Fu}, {0, 0, 0, 0}};
  const uint8_t* parts[3] = {static_cast<const uint8_t*>(vertices),
                             static_cast<const uint8_t*>(indices),
                             static_cast<const uint8_t*>(groups)};
  const size_t sizes[3] = {static_cast<size_t>(vertex_count) * 32,
                           static_cast<size_t>(triangles) * 6,
                           static_cast<size_t>(groups != nullptr ? group_count : 0) * 4};
  for (int k = 0; k < 3; ++k) {
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
  const uint32_t hi = m.a[0] + m.a[1] * 3u + m.a[2] * 5u + m.a[3] * 7u + vertex_count * 0x01000193u +
                      triangles + group_count * 0x9E3779B1u;
  return static_cast<uint64_t>(hi) << 32 | lo;
}

// THE CHEAP TEST OF A DRAW. A mesh of the game is an asset: its vertices and
// its indices stay in place for as long as the mesh lives, and a new mesh at
// the same address has other words. Each draw reads the pointers and the
// counts, and this sample over the first, the middle and the last vertex and
// the first and the last triangle (a few steps of the sum). A change of the
// whole mesh (a new mesh, an animation of all vertices) changes the sample.
// A change of a few words in the middle is found by the full sum, which a mesh
// gets at a slow rate, and each draw once the full sum found a change.
inline uint32_t MeshSample(const void* vertices, uint32_t vertex_count, const void* indices,
                           uint32_t triangles) {
  MeshSum m = {{0x9E3779B9u, 0x85EBCA6Bu, 0xC2B2AE35u, 0x27D4EB2Fu}, {0, 0, 0, 0}};
  const uint8_t* v = static_cast<const uint8_t*>(vertices);
  const uint8_t* at[3] = {v, v + static_cast<size_t>(vertex_count / 2) * 32,
                          v + static_cast<size_t>(vertex_count - 1) * 32};
  for (int k = 0; k < 3; ++k) SumBlocks(&m, at[k], 2);
  const uint8_t* ix = static_cast<const uint8_t*>(indices);
  uint8_t tail[16] = {};
  memcpy(tail, ix, 6);
  memcpy(tail + 6, ix + static_cast<size_t>(triangles - 1) * 6, 6);
  SumBlock(&m, tail);
  return m.s[0] ^ (m.s[1] * 31u) ^ (m.s[2] * 0x9E3779B1u) ^ m.s[3] ^ (m.a[0] + m.a[3] * 7u);
}

// THE FUNCTIONS THAT DRAW A LAYER over a mesh (the latch, gpu_mesh_latch). A mesh
// class of the engine has its own draw function on the CPU path. The return
// address of DrawTriangle names the function that called it.
struct LayerFunction {
  uint32_t address, size;
  const char* name;
};
const int kLayerFunctionCount = 6;
const LayerFunction kLayerFunctions[kLayerFunctionCount] = {
    {0x00851870, 0x13D0, "fn_00851870"}, {0x00859D90, 0x960, "fn_00859D90"},
    {0x0084E200, 0x790, "fn_0084E200"},  {0x0085A6F0, 0x420, "fn_0085A6F0"},
    {0x00850FC0, 0x840, "fn_00850FC0"},  {0x0085C7F0, 0x780, "fn_0085C7F0"}};
// The index of the function that holds the return address, or -1 (the sprite
// LH3DSprite::Draw at 0x00840530, the land, the text and the icons are not layers
// over a mesh).
inline int LayerFunctionOf(uint32_t return_address) {
  for (int i = 0; i < kLayerFunctionCount; ++i) {
    if (return_address >= kLayerFunctions[i].address &&
        return_address < kLayerFunctions[i].address + kLayerFunctions[i].size) {
      return i;
    }
  }
  return -1;
}

// WHERE A CHANGE IS. The data of a mesh (the vertices, then the indices) in
// blocks of 32 bytes (one vertex, or 5 and a third triangles), each with a sum.
// A mesh that changed at its place has the first block with another sum.
inline uint32_t BlockCount(uint32_t vertex_count, uint32_t triangles) {
  return vertex_count + (triangles * 6 + 31) / 32;
}
inline uint32_t BlockSum(const uint8_t* p, size_t n) {
  uint8_t block[32] = {};
  memcpy(block, p, n);
  uint32_t h = 2166136261u;
  for (int i = 0; i < 8; ++i) {
    uint32_t w;
    memcpy(&w, block + 4 * i, 4);
    h = (h ^ w) * 16777619u;
    h ^= h >> 15;
  }
  return h;
}
inline void BlockSums(const void* vertices, uint32_t vertex_count, const void* indices,
                      uint32_t triangles, uint32_t* out) {
  const uint8_t* v = static_cast<const uint8_t*>(vertices);
  for (uint32_t i = 0; i < vertex_count; ++i) out[i] = BlockSum(v + 32 * i, 32);
  const uint8_t* x = static_cast<const uint8_t*>(indices);
  const size_t bytes = static_cast<size_t>(triangles) * 6;
  for (size_t at = 0, k = 0; at < bytes; at += 32, ++k) {
    out[vertex_count + k] = BlockSum(x + at, bytes - at < 32 ? bytes - at : 32);
  }
}
// The first block with another sum, or -1.
inline int FirstChangedBlock(const uint32_t* a, const uint32_t* b, uint32_t count) {
  for (uint32_t i = 0; i < count; ++i) {
    if (a[i] != b[i]) return static_cast<int>(i);
  }
  return -1;
}

// WHICH MESH TAKES THE GPU PATH. The state of the device after the material of
// the mesh. The five functions draw a mesh at once, in the order of the calls
// (the z sorter of the engine holds whole objects and calls the function of
// a mesh later, in the order of the depth), and the renderer draws the record
// of a mesh in the same order with the same states. So a mesh with blend, with
// no depth write, with no depth test or with a texture on stage 1 needs no
// other treatment than its states. The parts of the path are bits, so a key
// leaves a part on the CPU path.
const uint32_t kPartBlend = 1u;    // blend, no depth write, no depth test
const uint32_t kPartStage1 = 2u;   // a texture on stage 1
const uint32_t kPartUvShift = 4u;  // the shift of u and v (water, fire)
const uint32_t kPartGroup = 8u;    // a matrix for each group of vertices
const uint32_t kPartDepth = 16u;   // the depth and the w of the game (kDepthGame)
const uint32_t kPartsAll = 31u;

struct DeviceState {
  uint32_t alpha_blend;
  uint32_t z_enable;
  uint32_t z_write;
  bool stage1;        // a color operation and a texture on stage 1
  uint32_t z_func;    // for the log of the compare
  uint32_t cull;
};

// What a mesh has that needs a part: bit k of the result is the reason k.
// The bits are the reasons for the count of the log.
const uint32_t kStateBlend = 1u;
const uint32_t kStateNoZWrite = 2u;
const uint32_t kStateNoZTest = 4u;
const uint32_t kStateStage1 = 8u;
inline uint32_t StateReasons(const DeviceState& s) {
  return (s.alpha_blend != 0 ? kStateBlend : 0u) | (s.z_write == 0 ? kStateNoZWrite : 0u) |
         (s.z_enable == 0 ? kStateNoZTest : 0u) | (s.stage1 ? kStateStage1 : 0u);
}
inline bool StateAllowed(uint32_t reasons, uint32_t parts) {
  if ((reasons & (kStateBlend | kStateNoZWrite | kStateNoZTest)) != 0 &&
      (parts & kPartBlend) == 0) {
    return false;
  }
  if ((reasons & kStateStage1) != 0 && (parts & kPartStage1) == 0) return false;
  return true;
}
inline bool OpaqueState(const DeviceState& s) { return StateReasons(s) == 0; }

// The limits of a mesh for the GPU path: the indices are 16 bit, so a mesh has
// at most 65535 vertices, and the stream sends a mesh of up to this size.
const uint32_t kMostVertices = 20000;
const uint32_t kMostTriangles = 40000;

}  // namespace gpu_mesh_math
