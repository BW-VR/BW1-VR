// THE MESHES ON THE GPU (docs/software-tnl.md, "The meshes on the GPU").
// main.cpp includes this file after part_draw_checks.h, just before main.
//
// The references are the loops of the game (fast_transform_math.h: the bits
// of the x87 code of fn_00856D40 and fn_0085AB10). The GPU path makes the same
// numbers in float, in another order of the operations, so the checks have a
// tolerance for the pixel, and compare the whole numbers (the color, the
// clip test) exactly.
#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>

#include <string>
#include <vector>

#include "../../src/mod/gpu_mesh_math.h"
#include "../../src/mod/mesh_store_math.h"
#include "../../src/mod/scene_stream.h"
#include "../../src/mod/stream_split.h"
#include "../../src/mod/vk_draw_math.h"

namespace gpu_mesh_checks {

using fast_transform_checks::Random;
namespace gm = gpu_mesh_math;

inline void GpuMeshChecks() {
  printf("\nThe meshes on the GPU\n");
  Random r = {4242u};

  // The values of the game for a render of 1280 by 720, 70 degrees.
  fast_transform_math::Consts c = {};
  c.near_plane = 0.15f;
  c.half_x = 640.0f;
  c.half_y = 360.0f;
  c.max_x = 1279.0f;
  c.max_y = 719.0f;
  c.color = 0xFF808890u;
  c.specular = 0xFF000000u;

  gm::View view = {};
  view.x = 0.0f;
  view.y = 0.0f;
  view.inv_w = 2.0f / 1280.0f;
  view.inv_h = 2.0f / 720.0f;
  view.min_z = 0.0f;
  view.inv_z = 1.0f;
  view.per_rhw = 0.0f;

  // THE POSITION. Random matrices and meshes: the points that the loop of the
  // game projects (no clip flag) give the same pixel, the same depth and the
  // same rhw on the GPU, and the clip test of the GPU says the same as the
  // clip flags, except for a point on the edge. The check runs for the viewport
  // of the whole render, for a viewport with an offset, a smaller size and a
  // MinZ above 0 (the viewport of a pass of foveated resolution has such
  // values), and for a viewport with the render of a headset (4080 by 3264).
  struct Worst {
    int projected = 0, flag_mismatch = 0, edge_mismatch = 0, z_exact = 0, x_exact = 0, y_exact = 0;
    double xy = 0.0, z = 0.0, rhw = 0.0;
  };
  const auto position = [&](const gm::View& view, bool whole, uint32_t seed, int meshes,
                            uint32_t mode_flags = 0) {
    Random rr = {seed};
    Worst w;
    const int kVertices = 64;
    static uint8_t in[kVertices * 32];
    static uint8_t out[kVertices * 32];
    static uint32_t flags[kVertices];
    for (int mesh = 0; mesh < meshes; ++mesh) {
      for (int i = 0; i < 12; ++i) c.m[i] = rr.Range(-1.5f, 1.5f);
      for (int i = 9; i < 12; ++i) c.m[i] = rr.Range(-3.0f, 3.0f);
      // A matrix of the game has the field of view in its basis: make the
      // first two columns large like it.
      for (int i : {0, 1, 2, 3, 4, 5}) c.m[i] *= 1.3f;
      for (int v = 0; v < kVertices; ++v) {
        float f[8];
        for (int k = 0; k < 8; ++k) f[k] = rr.Range(-4.0f, 4.0f);
        memcpy(in + v * 32, f, 32);
      }
      fast_transform_math::TransformClip(c, in, kVertices, out, flags);
      gm::Consts g = {};
      gm::SetMatrix(&g, c.m);
      g.near_plane = c.near_plane;
      g.half_x = c.half_x;
      g.half_y = c.half_y;
      g.flags = mode_flags;
      for (int v = 0; v < kVertices; ++v) {
        const float* p = reinterpret_cast<const float*>(in + v * 32);
        const gm::Camera cam = gm::CameraSpace(g, p, nullptr);
        float clip[4];
        gm::ClipPosition(g, view, cam, false, false, clip);
        const bool inside = gm::InsideClip(clip);
        const bool cpu_inside = flags[v] == 0;
        if (whole && inside != cpu_inside) {
          // A point within 2 pixels of an edge of the view can be on the other
          // side: the GPU map has the half pixel of Direct3D 9 (the clip planes
          // of the GPU are half a pixel from the planes of the game).
          const float margin = fminf(fminf(cam.z - c.near_plane, cam.z - fabsf(cam.x)),
                                     cam.z - fabsf(cam.y));
          if (fabsf(margin) < 4e-3f * fmaxf(c.near_plane, fabsf(cam.z))) {
            ++w.edge_mismatch;
          } else {
            ++w.flag_mismatch;
          }
          continue;
        }
        if (!cpu_inside || !inside) continue;
        ++w.projected;
        const float* o = reinterpret_cast<const float*>(out + v * 32);
        const gm::Screen s = gm::ScreenOf(clip, view);
        const double dx = fabs(static_cast<double>(s.x) - o[0]);
        const double dy = fabs(static_cast<double>(s.y) - o[1]);
        // The game clamps x and y to the screen.
        if (o[0] > 0.0f && o[0] < c.max_x && o[1] > 0.0f && o[1] < c.max_y) {
          if (dx > w.xy) w.xy = dx;
          if (dy > w.xy) w.xy = dy;
        }
        const double dz = fabs(static_cast<double>(s.z) - o[2]);
        if (dz > w.z) w.z = dz;
        if (dz == 0.0) ++w.z_exact;
        if (dx == 0.0) ++w.x_exact;
        if (dy == 0.0) ++w.y_exact;
        const double drhw = fabs(static_cast<double>(s.rhw) - o[3]) / o[3];
        if (drhw > w.rhw) w.rhw = drhw;
      }
    }
    return w;
  };
  const Worst whole = position(view, true, 4242u, 300);
  Check(whole.flag_mismatch == 0 && whole.projected > 2000,
        "the clip test of the GPU is the clip flags of the game (no point away from an edge differs)",
        whole.flag_mismatch, 0);
  Check(whole.xy < 0.0005,
        "the pixel of the GPU position is the pixel of the game to 0.0005 of a pixel",
        whole.xy, 0.0);
  Check(whole.z < 2e-7,
        "the depth of the GPU position is the depth of the game (1 - near / z) to 2e-7",
        whole.z, 0.0);
  Check(whole.rhw < 8e-7, "the rhw of the GPU position is the rhw of the game to 8e-7 relative",
        whole.rhw, 0.0);
  printf("  (%d points projected, %d edge points, the worst pixel %.5f, depth %.7f, rhw %.2e)\n",
         whole.projected, whole.edge_mismatch, whole.xy, whole.z, whole.rhw);
  {
    // THE DEPTH OF THE GAME (kDepthGame). The shader makes rhw = near * (1 / cz)
    // and z = 1 - rhw as the game does, so the depth after the divide of the GPU
    // is the depth of the loop of the game for nearly each point, and the
    // layers of the CPU path and the GPU path keep one order. The linear form
    // is exact for fewer points.
    const Worst linear = whole;
    const Worst game = position(view, true, 4242u, 300, gm::kDepthGame | gm::kYOrderC);
    const double exact_game = static_cast<double>(game.z_exact) / game.projected;
    const double exact_linear = static_cast<double>(linear.z_exact) / linear.projected;
    printf("  (x and y exact: %.1f and %.1f percent with the position of the game, %.1f and %.1f percent linear)\n",
           100.0 * game.x_exact / game.projected, 100.0 * game.y_exact / game.projected,
           100.0 * linear.x_exact / linear.projected, 100.0 * linear.y_exact / linear.projected);
    Check(game.flag_mismatch == 0 && game.z < 1.2e-7 && exact_game > 0.9 && exact_game > exact_linear &&
              game.x_exact >= linear.x_exact && game.y_exact >= linear.y_exact,
          "the depth of the game is the depth of the loop of the game for nine of ten points, more than the linear form",
          exact_game, 1.0);
    printf("  (depth of the game: %.1f percent of the points exact, worst %.7f. The linear form: %.1f percent, worst %.7f)\n",
           100.0 * exact_game, game.z, 100.0 * exact_linear, linear.z);
  }
  {
    // Far points (the roofs of a village are 20 to 300 units away): the depth of
    // the game against the depth that each form gives after the divide.
    gm::Consts g = {};
    g.near_plane = 0.15f;
    g.half_x = 640.0f;
    g.half_y = 360.0f;
    gm::Consts gg = g;
    gg.flags = gm::kDepthGame;
    int exact_linear = 0, exact_game = 0, total = 0;
    double worst_linear = 0.0, worst_game = 0.0;
    for (int k = 0; k < 20000; ++k) {
      const float cz = r.Range(20.0f, 300.0f);
      const float truth = 1.0f - g.near_plane * (1.0f / cz);
      const gm::Camera cam = {r.Range(-cz, cz), r.Range(-cz, cz), cz};
      float a[4], b[4];
      gm::ClipPosition(g, view, cam, false, false, a);
      gm::ClipPosition(gg, view, cam, false, false, b);
      const double dl = fabs(static_cast<double>(gm::ScreenOf(a, view).z) - truth);
      const double dg = fabs(static_cast<double>(gm::ScreenOf(b, view).z) - truth);
      exact_linear += dl == 0.0;
      exact_game += dg == 0.0;
      worst_linear = fmax(worst_linear, dl);
      worst_game = fmax(worst_game, dg);
      ++total;
    }
    Check(exact_game > exact_linear && worst_game <= worst_linear,
          "far points: the depth of the game is nearer to the depth of the loop than the linear form",
          exact_game, exact_linear);
    printf("  (far points: exact %.1f percent and worst %.2e with the depth of the game, %.1f percent and %.2e linear)\n",
           100.0 * exact_game / total, worst_game, 100.0 * exact_linear / total, worst_linear);
  }
  {
    // A viewport with an offset, a smaller size and a MinZ: the pixel of the
    // game and its rhw stay, and the depth is the depth of the game after the
    // map of MinZ and MaxZ.
    gm::View sub = view;
    sub.x = 128.0f;
    sub.y = 72.0f;
    sub.inv_w = 2.0f / 1000.0f;
    sub.inv_h = 2.0f / 560.0f;
    sub.min_z = 0.125f;
    sub.inv_z = 1.0f / (0.875f - 0.125f);
    const Worst w = position(sub, false, 777u, 12000);
    Check(w.projected > 500 && w.xy < 0.002 && w.z < 4e-7 && w.rhw < 2e-6,
          "a viewport with an offset, a smaller size and a MinZ gives the same pixel, depth and rhw",
          w.xy, 0.002);
    printf("  (offset viewport: %d points, the worst pixel %.5f, depth %.7f, rhw %.2e)\n",
           w.projected, w.xy, w.z, w.rhw);
    // The render of a headset eye.
    fast_transform_math::Consts saved = c;
    c.half_x = 2040.0f;
    c.half_y = 1632.0f;
    c.max_x = 4079.0f;
    c.max_y = 3263.0f;
    gm::View big = view;
    big.inv_w = 2.0f / 4080.0f;
    big.inv_h = 2.0f / 3264.0f;
    const Worst h = position(big, true, 99u, 300);
    Check(h.flag_mismatch == 0 && h.projected > 2000 && h.xy < 0.005 && h.z < 4e-7 && h.rhw < 2e-6,
          "a render of 4080 by 3264 gives the same pixel, depth and rhw, and the same clip test",
          h.xy, 0.005);
    printf("  (4080 by 3264: %d points, the worst pixel %.5f, depth %.7f, rhw %.2e)\n",
           h.projected, h.xy, h.z, h.rhw);
    c = saved;
  }

  // THE RIGHT EYE moves a point by per_rhw * rhw in pixels (one_walk_math.h):
  // the move in clip space is the constant per_rhw * inv_w for each point.
  {
    gm::Consts g = {};
    for (int i = 0; i < 12; ++i) c.m[i] = i % 4 == 0 ? 1.0f : 0.0f;
    gm::SetMatrix(&g, c.m);
    g.near_plane = c.near_plane;
    g.half_x = c.half_x;
    g.half_y = c.half_y;
    gm::View v = view;
    v.per_rhw = 37.5f;
    const float p[3] = {0.4f, -0.2f, 3.0f};
    const gm::Camera cam = gm::CameraSpace(g, p, nullptr);
    float left[4], right[4], still[4];
    gm::ClipPosition(g, v, cam, false, true, left);
    gm::ClipPosition(g, v, cam, true, true, right);
    gm::ClipPosition(g, v, cam, true, false, still);
    const gm::Screen a = gm::ScreenOf(left, v), b = gm::ScreenOf(right, v);
    const float rhw = c.near_plane / cam.z;
    Check(fabsf((b.x - a.x) - v.per_rhw * rhw) < 0.01f && fabsf(b.y - a.y) < 1e-3f &&
              still[0] == left[0],
          "the right eye moves a point by per_rhw * rhw pixels, and a still draw does not move",
          b.x - a.x, v.per_rhw * rhw);
  }

  // THE LIGHT AND THE COLOR. The color of a vertex is the same as LitColor
  // of the game, bit for bit, for the level that the game gets (rounded to the
  // nearest for fn_0085AB10, toward 0 for fn_00859530).
  {
    int diff_near = 0, diff_trunc = 0;
    fast_transform_math::Light l = {};
    for (int round = 0; round < 20000; ++round) {
      float d[3] = {r.Range(-1.0f, 1.0f), r.Range(-1.0f, 1.0f), r.Range(-1.0f, 1.0f)};
      const float len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
      for (float& x : d) x /= len;
      float n[3] = {r.Range(-1.0f, 1.0f), r.Range(-1.0f, 1.0f), r.Range(-1.0f, 1.0f)};
      const float nl = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
      for (float& x : n) x /= nl;
      memcpy(l.direction, d, sizeof(d));
      l.ambient = static_cast<int32_t>(r.Next() % 160);
      l.color = r.Next();
      gm::Consts g = {};
      memcpy(g.light, d, sizeof(d));
      g.ambient = l.ambient;
      g.color = l.color;
      g.flags = gm::kLit;
      // The loop of the game: i = ((l2 nz + l0 nx) + l1 ny) 255, rounded.
      const float dot = (l.direction[2] * n[2] + l.direction[0] * n[0] +
                         l.direction[1] * n[1]) * 255.0f;
      const int32_t level_near = static_cast<int32_t>(nearbyintf(dot));
      const int32_t level_trunc = static_cast<int32_t>(dot);
      if (gm::VertexColor(g, n) != fast_transform_math::LitColor(level_near, l)) ++diff_near;
      g.flags |= gm::kTruncate;
      if (gm::VertexColor(g, n) != fast_transform_math::LitColor(level_trunc, l)) ++diff_trunc;
    }
    Check(diff_near == 0, "the lit color is LitColor of the game (level rounded to the nearest)",
          diff_near, 0);
    Check(diff_trunc == 0, "the lit color is LitColor of the game (level toward 0)",
          diff_trunc, 0);
    gm::Consts flat = {};
    flat.color = 0x80ABCDEFu;
    const float n[3] = {0, 1, 0};
    Check(gm::VertexColor(flat, n) == 0x80ABCDEFu, "a mesh that is not lit has the color of the mesh",
          gm::VertexColor(flat, n), 0x80ABCDEFu);
  }

  // THE HASH. A change of any byte changes it, the same data give the same
  // value, and the SSE sum is the sum of the scalar code.
  {
    static uint8_t vertices[40 * 32];
    static uint16_t indices[60 * 3];
    for (auto& b : vertices) b = static_cast<uint8_t>(r.Next() >> 13);
    for (auto& i : indices) i = static_cast<uint16_t>(r.Next() % 40);
    const uint64_t base = gm::MeshHash(vertices, 40, indices, 60);
    Check(base == gm::MeshHash(vertices, 40, indices, 60), "the hash of the same data is the same",
          0, 0);
    int missed = 0;
    for (int k = 0; k < 400; ++k) {
      uint8_t* target = (k % 2 == 0) ? vertices + r.Next() % sizeof(vertices)
                                     : reinterpret_cast<uint8_t*>(indices) + r.Next() % sizeof(indices);
      const uint8_t old = *target;
      *target = static_cast<uint8_t>(old ^ (1u << (r.Next() % 8)));
      if (gm::MeshHash(vertices, 40, indices, 60) == base) ++missed;
      *target = old;
    }
    Check(missed == 0, "the hash changes when one bit of the vertices or the indices changes",
          missed, 0);
    gm::MeshSum a = {{1, 2, 3, 4}, {0, 0, 0, 0}}, b = a;
    gm::SumBlocks(&a, vertices, 100);
    for (int k = 0; k < 100; ++k) gm::SumBlock(&b, vertices + 16 * k);
    Check(memcmp(&a, &b, sizeof(a)) == 0, "the SSE sum of the hash is the sum of the scalar code",
          memcmp(&a, &b, sizeof(a)), 0);
  }

  // WHICH MESH TAKES THE GPU PATH. The reasons of a state, and the parts that
  // take them.
  {
    gm::DeviceState s = {0, 1, 1, false};
    const bool plain = gm::OpaqueState(s);
    s.alpha_blend = 1;
    const bool blend = gm::OpaqueState(s);
    s.alpha_blend = 0;
    s.z_write = 0;
    const bool no_write = gm::OpaqueState(s);
    s.z_write = 1;
    s.stage1 = true;
    const bool stage1 = gm::OpaqueState(s);
    Check(plain && !blend && !no_write && !stage1,
          "a mesh is opaque with no blend, a depth write and no stage 1",
          plain && !blend && !no_write && !stage1, 1);
    int wrong = 0;
    for (uint32_t bits = 0; bits < 16; ++bits) {
      gm::DeviceState d = {(bits & 1u) != 0 ? 1u : 0u, (bits & 4u) != 0 ? 0u : 1u,
                           (bits & 2u) != 0 ? 0u : 1u, (bits & 8u) != 0};
      const uint32_t reasons = gm::StateReasons(d);
      if (reasons != bits) ++wrong;
      for (uint32_t parts = 0; parts < 16; ++parts) {
        const bool want = ((bits & 7u) == 0 || (parts & gm::kPartBlend) != 0) &&
                          ((bits & 8u) == 0 || (parts & gm::kPartStage1) != 0);
        if (gm::StateAllowed(reasons, parts) != want) ++wrong;
      }
    }
    Check(wrong == 0,
          "each state has its reasons, and a part takes exactly the reasons of its kind",
          wrong, 0);
  }

  // THE ORDER. A mesh with blend or with no depth write keeps its place in the
  // order of the calls: the sort of the renderer moves only draws that may
  // move, and no draw moves past one that may not. (The five functions draw at
  // once, so the order of the calls is the order of the CPU path.)
  {
    namespace dm = vk_draw_math;
    const auto draw = [](uint64_t pipeline, uint32_t blend, uint32_t zwrite, uint32_t zenable) {
      dm::Draw d = {};
      d.pipeline = pipeline;
      d.blend = blend;
      d.zwrite = zwrite;
      d.zenable = zenable;
      d.zfunc = dm::kLessEqual;
      d.type = 99;
      return d;
    };
    // opaque 5, blend, opaque 3, opaque 1, no depth write, no depth test, opaque 2
    dm::Draw draws[7] = {draw(5, 0, 1, 1), draw(2, 0x10206, 1, 1), draw(3, 0, 1, 1),
                         draw(1, 0, 1, 1), draw(4, 0, 0, 1),       draw(6, 0, 1, 0),
                         draw(2, 0, 1, 1)};
    uint32_t order[7];
    dm::SortDraws(draws, 7, true, order);
    const uint32_t want[7] = {0, 1, 3, 2, 4, 5, 6};
    bool same = true;
    for (int i = 0; i < 7; ++i) same = same && order[i] == want[i];
    Check(same, "the sort keeps a mesh with blend, with no depth write or with no depth test in place",
          order[2], 3);
  }

  // THE TEXTURE COORDINATES. The shift of the part is added to u and v as
  // DrawTriangle adds it, and stage 1 reads the same set.
  {
    gm::Consts g = {};
    g.uv_shift_u = 0.125f;
    g.uv_shift_v = -0.375f;
    const float uv[2] = {0.25f, 0.5f};
    float out[2];
    gm::TextureCoord(g, uv, out);
    gm::Consts none = {};
    float same[2];
    gm::TextureCoord(none, uv, same);
    Check(out[0] == 0.375f && out[1] == 0.125f && same[0] == uv[0] && same[1] == uv[1],
          "the texture coordinates have the shift of the part and none without it", out[0], 0.375f);
  }

  // THE CHEAP TEST OF A DRAW. The sample changes when the first, the middle or
  // the last vertex changes, or the first or the last triangle, and the full
  // sum finds any other change.
  {
    static uint8_t vertices[41 * 32];
    static uint16_t indices[60 * 3];
    for (auto& b : vertices) b = static_cast<uint8_t>(r.Next() >> 13);
    for (auto& i : indices) i = static_cast<uint16_t>(r.Next() % 41);
    const uint32_t base = gm::MeshSample(vertices, 41, indices, 60);
    const uint64_t full = gm::MeshHash(vertices, 41, indices, 60);
    int sample_miss = 0, sample_noise = 0, full_miss = 0;
    const size_t places[5] = {5, 20 * 32 + 9, 40 * 32 + 31, 3 * 32, 38 * 32};
    for (size_t k = 0; k < 5; ++k) {
      const uint8_t old = vertices[places[k]];
      vertices[places[k]] = static_cast<uint8_t>(old ^ 0x10u);
      const bool in_sample = k < 3;
      if ((gm::MeshSample(vertices, 41, indices, 60) != base) != in_sample) {
        if (in_sample) {
          ++sample_miss;
        } else {
          ++sample_noise;
        }
      }
      if (gm::MeshHash(vertices, 41, indices, 60) == full) ++full_miss;
      vertices[places[k]] = old;
    }
    const size_t index_places[4] = {1, 5, 60 * 3 * 2 - 3, 60 * 3 * 2 - 1};
    for (size_t k = 0; k < 4; ++k) {
      uint8_t* byte = reinterpret_cast<uint8_t*>(indices) + index_places[k];
      const uint8_t old = *byte;
      *byte = static_cast<uint8_t>(old ^ 0x01u);
      if (gm::MeshSample(vertices, 41, indices, 60) == base) ++sample_miss;
      if (gm::MeshHash(vertices, 41, indices, 60) == full) ++full_miss;
      *byte = old;
    }
    Check(sample_miss == 0 && sample_noise == 0 && full_miss == 0 &&
              gm::MeshSample(vertices, 41, indices, 60) == base,
          "the sample sees the first, middle and last vertex and the first and last triangle, "
          "and the full sum sees each other change",
          sample_miss + sample_noise + full_miss, 0);
  }

  // THE WRITER. A record has one growth of the buffer: the bytes are the head,
  // the body and the two blocks, padded to 4.
  {
    namespace ss = scene_stream;
    ss::Writer w;
    const uint8_t body[7] = {1, 2, 3, 4, 5, 6, 7};
    const uint8_t a[5] = {8, 9, 10, 11, 12};
    const uint8_t b[3] = {13, 14, 15};
    const size_t start = w.Bytes().size();
    w.Record(ss::kMeshDraw, body, sizeof(body), a, sizeof(a), b, sizeof(b));
    w.Record(ss::kDraw, body, 4);
    const std::vector<uint8_t>& bytes = w.Bytes();
    const uint32_t first_size = 7 + 5 + 3;
    bool ok = bytes.size() == start + 8 + 16 + 8 + 4;
    uint32_t head[2];
    memcpy(head, bytes.data() + start, 8);
    ok = ok && head[0] == static_cast<uint32_t>(ss::kMeshDraw) && head[1] == first_size;
    for (uint32_t i = 0; i < 15 && ok; ++i) ok = bytes[start + 8 + i] == i + 1;
    ok = ok && bytes[start + 8 + 15] == 0;
    memcpy(head, bytes.data() + start + 24, 8);
    ok = ok && head[0] == static_cast<uint32_t>(ss::kDraw) && head[1] == 4 &&
         bytes[start + 32] == 1 && bytes[start + 35] == 4;
    ss::Reader reader = ss::Reader::Records(bytes.data() + start, bytes.size() - start);
    ss::Kind kind;
    const uint8_t* got;
    uint32_t got_bytes;
    ok = ok && reader.Next(&kind, &got, &got_bytes) && kind == ss::kMeshDraw &&
         got_bytes == first_size && reader.Next(&kind, &got, &got_bytes) && kind == ss::kDraw &&
         got_bytes == 4 && !reader.Next(&kind, &got, &got_bytes);
    Check(ok, "a record has its head, body and blocks in one piece, padded to 4 bytes", ok, 1);
  }

  // THE SHADER. vk_mesh_vert.h holds the SPIR-V of vk_mesh.vert. Its first line
  // names the hash of the source (tools/mesh_shaders.ps1 makes both), so a
  // source with no new header fails here.
  {
    std::string dir = __FILE__;
    dir = dir.substr(0, dir.find_last_of("/\\")) + "/../../src/mod/";
    const auto read = [](const std::string& path) {
      std::string text;
      FILE* f = nullptr;
      if (fopen_s(&f, path.c_str(), "rb") != 0 || f == nullptr) return text;
      char chunk[4096];
      size_t n;
      while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) text.append(chunk, n);
      fclose(f);
      return text;
    };
    const std::string source = read(dir + "vk_mesh.vert");
    const std::string header = read(dir + "vk_mesh_vert.h");
    uint64_t fnv = 0xcbf29ce484222325ull;
    for (char ch : source) {
      if (ch == '\r') continue;
      fnv ^= static_cast<uint8_t>(ch);
      fnv *= 0x100000001b3ull;
    }
    char want[48];
    snprintf(want, sizeof(want), "source-fnv64 %016llx", static_cast<unsigned long long>(fnv));
    const bool ok = !source.empty() && header.find(want) != std::string::npos;
    Check(ok, "vk_mesh_vert.h is made from this vk_mesh.vert (run bwvr/tools/mesh_shaders.ps1)",
          ok, 1);
  }

  // THE GROUPS. The ordinal of the group of each vertex, the light of a group
  // (the inverse of its matrix, the light position, the fast root), and a
  // record with the matrices and the lights of the groups.
  {
    const gm::MeshGroup groups[3] = {{3, 7}, {0, 2}, {4, 5}};
    uint16_t ordinals[7] = {};
    const bool good = gm::GroupOrdinals(groups, 3, 7, ordinals);
    const uint16_t want[7] = {0, 0, 0, 2, 2, 2, 2};
    bool same = good;
    for (int i = 0; i < 7; ++i) same = same && ordinals[i] == want[i];
    uint16_t scratch[9];
    const bool short_sum = gm::GroupOrdinals(groups, 3, 8, scratch);
    const bool long_sum = gm::GroupOrdinals(groups, 3, 6, scratch);
    const bool none = gm::GroupOrdinals(groups, 0, 7, scratch);
    Check(same && !short_sum && !long_sum && !none,
          "the ordinals of the groups follow the counts, and counts that are not the vertex count fail",
          same, 1);

    // The table of the fast root: for each of the 128 places the 8 bits that
    // make the best first guess (the game builds its table at the start).
    uint8_t table[128];
    for (int i = 0; i < 128; ++i) {
      double best_error = 1e30;
      int best = 0;
      for (int t = 0; t < 256; ++t) {
        double worst = 0.0;
        for (int k = 0; k < 8; ++k) {
          // A float with the 7 bits of the place at bits 17 to 23 and a middle value below.
          const uint32_t u = 0x3E000000u | (static_cast<uint32_t>(i) << 17) |
                             (static_cast<uint32_t>(k) << 14) | 0x2000u;
          float x;
          memcpy(&x, &u, 4);
          if (!(x > 0.0f) || !isfinite(x)) continue;
          const uint32_t e = (u >> 23) & 0xFFu;
          const uint32_t bits = ((0x5F000000u - (e << 22)) & 0xFF800000u) |
                                (static_cast<uint32_t>(t) << 15);
          float y;
          memcpy(&y, &bits, 4);
          const double error = fabs(static_cast<double>(y) * sqrt(static_cast<double>(x)) - 1.0);
          if (error > worst) worst = error;
        }
        if (worst < best_error) {
          best_error = worst;
          best = t;
        }
      }
      table[i] = static_cast<uint8_t>(best);
    }
    double worst_root = 0.0;
    for (int k = 0; k < 2000; ++k) {
      const float x = powf(2.0f, r.Range(-20.0f, 20.0f)) * r.Range(1.0f, 2.0f);
      const double want_root = 1.0 / sqrt(static_cast<double>(x));
      const double error = fabs(gm::FastInverseSqrt(x, table) / want_root - 1.0);
      if (error > worst_root) worst_root = error;
    }
    // One step of Newton from a guess with an error of a few percent.
    Check(worst_root < 1e-3, "the fast root with a step of Newton is within 0.1 percent of the root",
          worst_root, 0.0);

    // The light of a group: a turn about y and a move, and a light position.
    float matrix[12] = {};
    const float angle = 0.6f;
    matrix[0] = cosf(angle); matrix[2] = -sinf(angle);
    matrix[4] = 1.0f;
    matrix[6] = sinf(angle); matrix[8] = cosf(angle);
    matrix[9] = 2.0f; matrix[10] = -1.0f; matrix[11] = 5.0f;
    const float light_position[3] = {3.0f, 4.0f, -2.0f};
    float light[3];
    gm::GroupLight(matrix, light_position, table, light);
    // The reference: p - t in the space of the rotation, then length 1.
    double d[3] = {light_position[0] - matrix[9], light_position[1] - matrix[10],
                   light_position[2] - matrix[11]};
    double v[3];
    for (int i = 0; i < 3; ++i) {
      v[i] = d[0] * matrix[3 * i] + d[1] * matrix[3 * i + 1] + d[2] * matrix[3 * i + 2];
    }
    const double len = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    double worst_light = 0.0;
    for (int i = 0; i < 3; ++i) {
      worst_light = fmax(worst_light, fabs(light[i] - v[i] / len));
    }
    Check(worst_light < 1e-3, "the light of a group is the light position in the space of the group, length 1",
          worst_light, 0.0);

    // A record with groups: the matrices (12 floats) and the lights (4) follow.
    namespace ss = scene_stream;
    ss::MeshDrawBody draw = {};
    draw.mesh = 0x1234;
    draw.consts.flags = gm::kLit | gm::kGroup;
    draw.group_count = 3;
    float group_floats[3 * 16];
    for (int i = 0; i < 48; ++i) group_floats[i] = static_cast<float>(i) * 0.25f;
    ss::Writer w;
    w.Record(ss::kMeshDraw, &draw, sizeof(draw), nullptr, 0, group_floats, sizeof(group_floats));
    ss::Reader reader = ss::Reader::Records(w.Bytes().data() + 8, w.Bytes().size() - 8);
    ss::Kind kind;
    const uint8_t* body = nullptr;
    uint32_t bytes = 0;
    bool ok = reader.Next(&kind, &body, &bytes) && kind == ss::kMeshDraw &&
              bytes == sizeof(draw) + sizeof(group_floats);
    ss::MeshDrawBody back;
    ok = ok && ss::Body(body, bytes, &back) && back.group_count == 3 &&
         memcmp(body + sizeof(back), group_floats, sizeof(group_floats)) == 0;
    ss::MeshDataBody data = {};
    data.group_count = 3;
    static uint8_t vertex_bytes[7 * 32];
    static uint16_t indices[3] = {0, 1, 2};
    w.Clear();
    w.Record(ss::kMeshData, &data, sizeof(data), vertex_bytes, sizeof(vertex_bytes), indices,
             sizeof(indices), ordinals, sizeof(ordinals));
    ss::Reader again = ss::Reader::Records(w.Bytes().data() + 8, w.Bytes().size() - 8);
    ok = ok && again.Next(&kind, &body, &bytes) && kind == ss::kMeshData &&
         bytes == sizeof(data) + sizeof(vertex_bytes) + sizeof(indices) + sizeof(ordinals);
    Check(ok, "a draw with groups and the data of a mesh with groups go through the writer and the reader",
          ok, 1);
  }

  // THE LATCH. The return address names the function of a mesh class that draws a
  // layer; the sprite, the land and the text are not.
  {
    const bool ok = gm::LayerFunctionOf(0x00852BA7) == 0 && gm::LayerFunctionOf(0x0085A6D9) == 1 &&
                    gm::LayerFunctionOf(0x0084E8F6) == 2 && gm::LayerFunctionOf(0x0085AA7A) == 3 &&
                    gm::LayerFunctionOf(0x008517F7) == 4 && gm::LayerFunctionOf(0x0085CED5) == 5 &&
                    gm::LayerFunctionOf(0x00840C5C) == -1 && gm::LayerFunctionOf(0x00875627) == -1 &&
                    gm::LayerFunctionOf(0x0081CCB7) == -1 && gm::LayerFunctionOf(0x78E0C2A0) == -1 &&
                    gm::LayerFunctionOf(0x00851870) == 0 && gm::LayerFunctionOf(0x00851870 + 0x13D0) == -1;
    Check(ok, "the return address of a layer names the function of its mesh class, and a sprite does not", ok, 1);
  }

  // THE STORE OF THE MESH DATA (mesh_store_math.h). The guard of a draw with
  // groups, the list of data that wait, and the gate of a walk.
  {
    namespace ms = mesh_store_math;
    const bool guard = ms::GroupDrawOk(3, 3, 128) && ms::GroupDrawOk(3, 5, 128) &&
                       !ms::GroupDrawOk(3, 2, 128) && !ms::GroupDrawOk(0, 2, 128) &&
                       !ms::GroupDrawOk(3, 200, 128) && !ms::GroupDrawOk(3, 0, 128);
    Check(guard, "a draw with groups needs at least as many matrices as the data have groups, and at most 128",
          guard, 1);

    struct Item {
      uint64_t id;
      size_t size;
      int version;
    };
    ms::WaitList<Item> list(1000);
    bool ok = list.Put({1, 100, 1}) && list.Put({2, 200, 1}) && list.Bytes() == 300;
    // A newer version of a mesh replaces the entry of the mesh.
    ok = ok && list.Put({1, 150, 2}) && list.Count() == 2 && list.Bytes() == 350;
    int version_of_1 = 0;
    for (const Item& item : list.Items()) {
      if (item.id == 1) version_of_1 = item.version;
    }
    ok = ok && version_of_1 == 2;
    // A direct store of mesh 2 removes its entry: a flush has nothing of it.
    ok = ok && list.Remove(2) && !list.Has(2) && !list.Remove(2) && list.Bytes() == 150;
    // A flush takes the entries that found a range.
    list.Put({3, 300, 1});
    list.Put({4, 400, 1});
    list.RemoveIf([](Item& item) { return item.id == 3 || item.id == 1; });
    ok = ok && list.Count() == 1 && list.Has(4) && list.Bytes() == 400;
    // No room: the item is dropped, and the older version of its mesh goes too,
    // so the list never keeps an older version for a newer one.
    ok = ok && !list.Put({4, 2000, 2}) && !list.Has(4) && list.Empty() && list.Bytes() == 0;
    Check(ok, "the list of data that wait has one entry for each mesh, and a direct store removes it", ok, 1);

    ms::WalkGate gate;
    const bool once = gate.Once(0) && !gate.Once(0) && gate.Once(7) && !gate.Once(7) && gate.Once(8);
    Check(once, "the gate lets an action run once in each walk (a full arena flushes once in a walk)", once, 1);
  }

  // WHERE A CHANGE IS. The sums of the blocks of 32 bytes find the first changed
  // vertex or the block of the indices.
  {
    static uint8_t vertices[37 * 32];
    static uint16_t indices[50 * 3];
    for (auto& b : vertices) b = static_cast<uint8_t>(r.Next() >> 13);
    for (auto& i : indices) i = static_cast<uint16_t>(r.Next() % 37);
    const uint32_t count = gm::BlockCount(37, 50);
    static uint32_t before[64], after[64];
    gm::BlockSums(vertices, 37, indices, 50, before);
    bool ok = count == 37 + (50 * 6 + 31) / 32 && count <= 64;
    gm::BlockSums(vertices, 37, indices, 50, after);
    ok = ok && gm::FirstChangedBlock(before, after, count) == -1;
    vertices[21 * 32 + 17] ^= 0x04;
    gm::BlockSums(vertices, 37, indices, 50, after);
    ok = ok && gm::FirstChangedBlock(before, after, count) == 21;
    vertices[21 * 32 + 17] ^= 0x04;
    reinterpret_cast<uint8_t*>(indices)[299] ^= 0x01;  // the last byte of the last triangle
    gm::BlockSums(vertices, 37, indices, 50, after);
    ok = ok && gm::FirstChangedBlock(before, after, count) == static_cast<int>(37 + 299 / 32);
    Check(ok, "the sums of the blocks find the first changed vertex or index block", ok, 1);
  }

  // THE RECORDS. A mesh goes into the stream and out of it with its offsets.
  {
    namespace ss = scene_stream;
    static float vertices[4 * 8];
    static uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 32; ++i) vertices[i] = static_cast<float>(i);
    ss::Writer w;
    ss::MeshDataBody d = {};
    d.mesh = 0x00ABCDEFull;
    d.hash = gm::MeshHash(vertices, 4, indices, 2);
    d.vertex_count = 4;
    d.triangle_count = 2;
    w.Record(ss::kMeshData, &d, sizeof(d), vertices, sizeof(vertices), indices, sizeof(indices));
    ss::MeshDrawBody draw = {};
    draw.mesh = d.mesh;
    draw.offset_floats = 12;
    draw.consts.near_plane = 0.15f;
    float offsets[12];
    for (int i = 0; i < 12; ++i) offsets[i] = 0.5f * i;
    w.Record(ss::kMeshDraw, &draw, sizeof(draw), offsets, sizeof(offsets));
    ss::Reader reader(w.Bytes().data(), w.Bytes().size());
    ss::Kind kind;
    const uint8_t* body;
    uint32_t bytes;
    bool ok = reader.Ok() && reader.Next(&kind, &body, &bytes) && kind == ss::kMeshData &&
              bytes == sizeof(d) + sizeof(vertices) + sizeof(indices);
    ss::MeshDataBody back;
    ok = ok && ss::Body(body, bytes, &back) && back.mesh == d.mesh && back.hash == d.hash &&
         memcmp(body + sizeof(back), vertices, sizeof(vertices)) == 0;
    ok = ok && reader.Next(&kind, &body, &bytes) && kind == ss::kMeshDraw &&
         bytes == sizeof(draw) + sizeof(offsets);
    ss::MeshDrawBody draw_back;
    ok = ok && ss::Body(body, bytes, &draw_back) && draw_back.offset_floats == 12 &&
         draw_back.consts.near_plane == 0.15f &&
         memcmp(body + sizeof(draw_back), offsets, sizeof(offsets)) == 0;
    Check(ok, "kMeshData and kMeshDraw go through the writer and the reader", ok, 1);
    // A frame that loses its walks keeps the data of the meshes and loses the draws.
    std::vector<uint8_t> kept;
    const uint8_t* unit = w.Bytes().data() + 8;
    const size_t n = w.Bytes().size() - 8;
    stream_split::StateOnly(unit, n, &kept);
    ss::Reader after = ss::Reader::Records(kept.data(), kept.size());
    int data_records = 0, draw_records = 0;
    while (after.Next(&kind, &body, &bytes)) {
      if (kind == ss::kMeshData) ++data_records;
      if (kind == ss::kMeshDraw) ++draw_records;
    }
    Check(data_records == 1 && draw_records == 0 &&
              !stream_split::WalkKind(ss::kMeshData) && stream_split::WalkKind(ss::kMeshDraw),
          "a frame with no walks keeps kMeshData and loses kMeshDraw", data_records, 1);
  }
}

}  // namespace gpu_mesh_checks
