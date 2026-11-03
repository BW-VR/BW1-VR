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

#include "../../src/mod/gpu_mesh_math.h"
#include "../../src/mod/scene_stream.h"
#include "../../src/mod/stream_split.h"

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
  // clip flags, except for a point on the edge.
  const int kMeshes = 300;
  const int kVertices = 64;
  static uint8_t in[kVertices * 32];
  static uint8_t out[kVertices * 32];
  static uint32_t flags[kVertices];
  int projected = 0, kept_by_gpu = 0, flag_mismatch = 0, edge_mismatch = 0;
  double worst_xy = 0.0, worst_z = 0.0, worst_rhw = 0.0;
  for (int mesh = 0; mesh < kMeshes; ++mesh) {
    for (int i = 0; i < 12; ++i) c.m[i] = r.Range(-1.5f, 1.5f);
    for (int i = 9; i < 12; ++i) c.m[i] = r.Range(-3.0f, 3.0f);
    // A matrix of the game has the field of view in its basis: make the
    // first two columns large like it.
    for (int i : {0, 1, 2, 3, 4, 5}) c.m[i] *= 1.3f;
    for (int v = 0; v < kVertices; ++v) {
      float f[8];
      for (int k = 0; k < 8; ++k) f[k] = r.Range(-4.0f, 4.0f);
      memcpy(in + v * 32, f, 32);
    }
    fast_transform_math::TransformClip(c, in, kVertices, out, flags);
    gm::Consts g = {};
    gm::SetMatrix(&g, c.m);
    g.near_plane = c.near_plane;
    g.half_x = c.half_x;
    g.half_y = c.half_y;
    for (int v = 0; v < kVertices; ++v) {
      const float* p = reinterpret_cast<const float*>(in + v * 32);
      const gm::Camera cam = gm::CameraSpace(g, p, nullptr);
      float clip[4];
      gm::ClipPosition(g, view, cam, false, false, clip);
      const bool inside = gm::InsideClip(clip);
      const bool cpu_inside = flags[v] == 0;
      if (inside != cpu_inside) {
        // A point within 2 pixels of an edge of the view can be on the other
        // side: the GPU map has the half pixel of Direct3D 9 (the clip planes
        // of the GPU are half a pixel from the planes of the game).
        const float margin = fminf(fminf(cam.z - c.near_plane, cam.z - fabsf(cam.x)),
                                   cam.z - fabsf(cam.y));
        if (fabsf(margin) < 4e-3f * fmaxf(c.near_plane, fabsf(cam.z))) {
          ++edge_mismatch;
        } else {
          ++flag_mismatch;
        }
        continue;
      }
      if (!cpu_inside) continue;
      ++projected;
      ++kept_by_gpu;
      const float* o = reinterpret_cast<const float*>(out + v * 32);
      const gm::Screen s = gm::ScreenOf(clip, view);
      const double dx = fabs(static_cast<double>(s.x) - o[0]);
      const double dy = fabs(static_cast<double>(s.y) - o[1]);
      // The game clamps x and y to the screen.
      if (o[0] > 0.0f && o[0] < c.max_x && o[1] > 0.0f && o[1] < c.max_y) {
        if (dx > worst_xy) worst_xy = dx;
        if (dy > worst_xy) worst_xy = dy;
      }
      const double dz = fabs(static_cast<double>(s.z) - o[2]);
      if (dz > worst_z) worst_z = dz;
      const double drhw = fabs(static_cast<double>(s.rhw) - o[3]) / o[3];
      if (drhw > worst_rhw) worst_rhw = drhw;
    }
  }
  Check(flag_mismatch == 0 && projected > 2000,
        "the clip test of the GPU is the clip flags of the game (no point away from an edge differs)",
        flag_mismatch, 0);
  Check(worst_xy < 0.05,
        "the pixel of the GPU position is the pixel of the game to 0.05 of a pixel",
        worst_xy, 0.0);
  Check(worst_z < 2e-4,
        "the depth of the GPU position is the depth of the game (1 - near / z) to 0.0002",
        worst_z, 0.0);
  Check(worst_rhw < 2e-5, "the rhw of the GPU position is the rhw of the game to 2e-5 relative",
        worst_rhw, 0.0);
  printf("  (%d points projected, %d edge points, the worst pixel %.5f, depth %.7f, rhw %.2e)\n",
         projected, edge_mismatch, worst_xy, worst_z, worst_rhw);
  (void)kept_by_gpu;

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

  // WHICH MESH TAKES THE GPU PATH.
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
          "a mesh takes the GPU path with no blend, a depth write and no stage 1",
          plain && !blend && !no_write && !stage1, 1);
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
