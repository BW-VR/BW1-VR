// The late compose of the eye picture (vk_compose_math.h). main.cpp includes
// this file after Check and Near, just before main.
#pragma once

#include <stdio.h>

#include <string>

#include "../../src/mod/app_keys.h"
#include "../../src/mod/vk_compose_math.h"
#include "../../src/mod/vk_draw_math.h"

namespace vk_compose_checks {

namespace m = vk_compose_math;

// The 54 vertices of the old compose (ComposeMres of vk_scene.cpp) for the
// centre: a grid of 4 by 4 points. Gives the alpha and the uv of the corner.
struct OldVertex {
  float x, y, alpha, u, v;
};

inline int OldCentre(const m::Layout& l, OldVertex out[54]) {
  const float ix0 = l.inset_x0, iy0 = l.inset_y0;
  const float iw = l.inset_w, ih = l.inset_h;
  const float edge = 0.08f * (iw < ih ? iw : ih);
  const float gx[4] = {ix0, ix0 + edge, ix0 + iw - edge, ix0 + iw};
  const float gy[4] = {iy0, iy0 + edge, iy0 + ih - edge, iy0 + ih};
  const float cu = static_cast<float>(l.center_w) / static_cast<float>(l.center_image_w);
  const float cv = static_cast<float>(l.center_h) / static_cast<float>(l.center_image_h);
  int n = 0;
  for (int j = 0; j < 3; ++j) {
    for (int i = 0; i < 3; ++i) {
      const int corner[6][2] = {{i, j},         {i + 1, j},
                                {i + 1, j + 1}, {i, j},
                                {i + 1, j + 1}, {i, j + 1}};
      for (int k = 0; k < 6; ++k) {
        const int ci = corner[k][0], cj = corner[k][1];
        const bool inside = ci >= 1 && ci <= 2 && cj >= 1 && cj <= 2;
        OldVertex& v = out[n++];
        v.x = gx[ci];
        v.y = gy[cj];
        v.alpha = inside ? 1.0f : 0.0f;
        v.u = (gx[ci] - ix0) / iw * cu;
        v.v = (gy[cj] - iy0) / ih * cv;
      }
    }
  }
  return n;
}

// The interpolated value at (px, py) of the triangle list, or false when no
// triangle has the point. The value: a, u, v of the vertices (a template for
// the two vertex types).
template <typename V, typename Get>
inline bool Interpolate(const V* v, int count, float px, float py, Get get, float out[3]) {
  for (int t = 0; t + 2 < count; t += 3) {
    const float x0 = v[t].x, y0 = v[t].y, x1 = v[t + 1].x, y1 = v[t + 1].y;
    const float x2 = v[t + 2].x, y2 = v[t + 2].y;
    const float det = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
    if (fabsf(det) < 1e-9f) continue;
    const float l0 = ((y1 - y2) * (px - x2) + (x2 - x1) * (py - y2)) / det;
    const float l1 = ((y2 - y0) * (px - x2) + (x0 - x2) * (py - y2)) / det;
    const float l2 = 1.0f - l0 - l1;
    const float e = -1e-5f;
    if (l0 < e || l1 < e || l2 < e) continue;
    float a[3], b[3], c[3];
    get(v[t], a);
    get(v[t + 1], b);
    get(v[t + 2], c);
    for (int k = 0; k < 3; ++k) out[k] = l0 * a[k] + l1 * b[k] + l2 * c[k];
    return true;
  }
  return false;
}

inline float TriangleArea(const m::Vertex* v) {
  return 0.5f * fabsf((v[1].x - v[0].x) * (v[2].y - v[0].y) -
                      (v[2].x - v[0].x) * (v[1].y - v[0].y));
}

inline void CheckLayout(const char* label, const m::Layout& l) {
  printf("  layout %s\n", label);
  m::Vertex grid[m::kVertices];
  const int n = m::BuildGrid(l, grid);
  Check(n == 150 && m::kVertices == 150, "the grid has 150 vertices", n, 150);
  {
    m::Vertex parts[m::kVertices];
    const int made = m::BuildGridParts(l, parts);
    Check(made == m::kVertices, "the grid in parts has all vertices", made, m::kVertices);
    int outer_alpha = 0;
    for (int i = 0; i < m::kOuterVertices; ++i) {
      if (parts[i].alpha != 0.0f) ++outer_alpha;
    }
    Check(outer_alpha == 0, "the outer part has only the outer image", outer_alpha, 0);
    int inner_one = 0;
    for (int i = m::kOuterVertices; i < m::kVertices; ++i) {
      if (parts[i].alpha == 1.0f) ++inner_one;
    }
    Check(inner_one > 0, "the inner part has the centre", inner_one, 1);
    // The same cells: the sum of the places is the same in both orders.
    double whole = 0.0, split = 0.0, alpha_whole = 0.0, alpha_split = 0.0;
    for (int i = 0; i < m::kVertices; ++i) {
      whole += grid[i].x * 3.0 + grid[i].y;
      split += parts[i].x * 3.0 + parts[i].y;
      alpha_whole += grid[i].alpha;
      alpha_split += parts[i].alpha;
    }
    Check(whole == split && alpha_whole == alpha_split, "the parts have the cells of the grid", split,
          whole);
    Check(m::BytesFormat(43) == 37 && m::BytesFormat(50) == 44,
          "an sRGB format has its UNORM format for the bytes", m::BytesFormat(43), 37);
    Check(m::BytesFormat(37) == 37, "a UNORM format stays", m::BytesFormat(37), 37);
  }
  // The cells fill the eye picture once.
  double area = 0.0;
  for (int t = 0; t < n; t += 3) area += TriangleArea(grid + t);
  Near("the triangles fill the eye picture", area,
       static_cast<double>(l.width) * l.height, 1e-4 * l.width * l.height);

  OldVertex old[54];
  OldCentre(l, old);
  // The alpha and the centre uv at points of the centre rect are the ones of
  // the old grid. The points include the soft edge.
  int compared = 0;
  float worst_alpha = 0.0f, worst_uv = 0.0f;
  for (int j = 0; j <= 24; ++j) {
    for (int i = 0; i <= 24; ++i) {
      const float px = l.inset_x0 + (static_cast<float>(i) + 0.37f) / 25.0f * l.inset_w;
      const float py = l.inset_y0 + (static_cast<float>(j) + 0.21f) / 25.0f * l.inset_h;
      float a_new[3], a_old[3];
      const bool in_new = Interpolate(
          grid, n, px, py,
          [](const m::Vertex& v, float o[3]) {
            o[0] = v.alpha;
            o[1] = v.u_center;
            o[2] = v.v_center;
          },
          a_new);
      const bool in_old = Interpolate(
          old, 54, px, py,
          [](const OldVertex& v, float o[3]) {
            o[0] = v.alpha;
            o[1] = v.u;
            o[2] = v.v;
          },
          a_old);
      if (!in_new || !in_old) continue;
      ++compared;
      worst_alpha = fmaxf(worst_alpha, fabsf(a_new[0] - a_old[0]));
      worst_uv = fmaxf(worst_uv, fmaxf(fabsf(a_new[1] - a_old[1]), fabsf(a_new[2] - a_old[2])));
    }
  }
  Check(compared > 500, "points of the centre rect are in both grids", compared, 625);
  Near("the alpha of the soft edge is the one of the old grid", worst_alpha, 0.0, 1e-4);
  Near("the centre uv is the one of the old grid", worst_uv, 0.0, 1e-4);

  // Outside the centre rect the alpha is 0, so the outer image alone shows.
  int outside = 0;
  float worst_out = 0.0f;
  for (int j = 0; j < 20; ++j) {
    for (int i = 0; i < 20; ++i) {
      const float px = (static_cast<float>(i) + 0.5f) / 20.0f * l.width;
      const float py = (static_cast<float>(j) + 0.5f) / 20.0f * l.height;
      if (px >= l.inset_x0 && px <= l.inset_x0 + l.inset_w && py >= l.inset_y0 &&
          py <= l.inset_y0 + l.inset_h) {
        continue;
      }
      float o[3];
      if (!Interpolate(
              grid, n, px, py,
              [](const m::Vertex& v, float r[3]) {
                r[0] = v.alpha;
                r[1] = v.u_outer;
                r[2] = v.v_outer;
              },
              o)) {
        continue;
      }
      ++outside;
      worst_out = fmaxf(worst_out, o[0]);
    }
  }
  Check(outside > 50, "points outside the centre rect are in the grid", outside, 100);
  Near("outside the centre rect the alpha is 0", worst_out, 0.0, 1e-6);

  // The outer uv covers the used part of the outer image over the picture.
  const float ou = static_cast<float>(l.outer_w) / l.outer_image_w;
  const float ov = static_cast<float>(l.outer_h) / l.outer_image_h;
  float o_end[3];
  Interpolate(
      grid, n, l.width - 0.01f, l.height - 0.01f,
      [](const m::Vertex& v, float r[3]) {
        r[0] = v.alpha;
        r[1] = v.u_outer;
        r[2] = v.v_outer;
      },
      o_end);
  Near("the outer uv u at the right edge is the used share of the image", o_end[1], ou, 1e-3);
  Near("the outer uv v at the bottom edge is the used share of the image", o_end[2], ov, 1e-3);
}

// The hash of a shader source (FNV-1a, 64 bit, over the text with LF line
// ends), as tools/compose_shaders.ps1 writes it into the header.
inline bool ShaderHeaderMatches(const std::string& source_name, const std::string& header_name) {
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
  const std::string source = read(dir + source_name);
  const std::string header = read(dir + header_name);
  uint64_t fnv = 0xcbf29ce484222325ull;
  for (char ch : source) {
    if (ch == '\r') continue;
    fnv ^= static_cast<uint8_t>(ch);
    fnv *= 0x100000001b3ull;
  }
  char want[48];
  snprintf(want, sizeof(want), "source-fnv64 %016llx", static_cast<unsigned long long>(fnv));
  return !source.empty() && header.find(want) != std::string::npos;
}

inline void VkComposeChecks() {
  printf("\nThe compose of the eye picture: the grid and the transfer function\n");
  {
    const bool vert = ShaderHeaderMatches("vk_compose.vert", "vk_compose_vert.h");
    const bool frag = ShaderHeaderMatches("vk_compose.frag", "vk_compose_frag.h");
    Check(vert && frag,
          "vk_compose_vert.h and vk_compose_frag.h are made from the sources (run "
          "bwvr/tools/compose_shaders.ps1)",
          (vert ? 1 : 0) + (frag ? 1 : 0), 2);
  }
  // The Steam Frame: a render of 2808 by 2246, the outer image at 25 percent
  // (702 by 562), the centre at 100 percent for 20 degrees (456 by 456).
  m::Layout frame = {2808, 2246, 702, 562, 702, 562, 456, 456, 456, 456, 1000.0f, 700.0f, 456.0f, 456.0f};
  CheckLayout("of the Steam Frame", frame);
  // The images are larger than the used part (they only grow).
  m::Layout grown = {2808, 2246, 702, 562, 800, 640, 456, 456, 512, 512, 100.5f, 33.25f, 456.0f, 456.0f};
  CheckLayout("with images larger than their used part", grown);
  // The centre rect at the corner of the picture: cells of no width.
  m::Layout corner = {2808, 2246, 702, 562, 702, 562, 456, 456, 456, 456, 0.0f, 0.0f, 456.0f, 456.0f};
  m::Vertex g[m::kVertices];
  m::BuildGrid(corner, g);
  double area = 0.0;
  for (int t = 0; t < m::kVertices; t += 3) area += TriangleArea(g + t);
  Near("a centre rect at the corner: the triangles fill the picture", area, 2808.0 * 2246.0,
       1e-4 * 2808.0 * 2246.0);

  // THE SAME PANEL: the hash of the draws of a panel.
  {
    const uint32_t a[5] = {1, 2, 3, 4, 5};
    const uint32_t b[5] = {1, 2, 3, 4, 6};
    const uint64_t seed = 0x9E3779B97F4A7C15ULL;
    const uint64_t ha = vk_draw_math::HashBytes(seed, a, sizeof(a));
    Check(ha == vk_draw_math::HashBytes(seed, a, sizeof(a)), "one set of bytes has one hash", 0, 0);
    Check(ha != vk_draw_math::HashBytes(seed, b, sizeof(b)), "the last bytes change the hash", 0, 0);
    Check(ha != seed, "bytes change the seed", 0, 0);
    Check(vk_draw_math::HashBytes(seed, a, 0) == seed, "no bytes keep the seed", 0, 0);
  }

  // THE COMPOSE AT THE TAKE. The session reads the set of a finished picture. The play
  // thread can have finished the walk after it and have the next walk on the GPU, so
  // two walks write after the picture that the session reads.
  {
    Check(m::NextSet(0, 1, true) == 0 && m::NextSet(2, 1, true) == 0, "one set stays set 0",
          m::NextSet(2, 1, true), 0);
    Check(m::NextSet(0, 3, true) == 1 && m::NextSet(2, 3, true) == 0, "three sets go in turn",
          m::NextSet(2, 3, true), 0);
    Check(m::NextSet(1, 3, false) == 1, "a walk of one eye stays in its set",
          m::NextSet(1, 3, false), 1);
    int bad = 0;
    int set_of[16] = {};
    int set = 0;
    for (int walk = 1; walk < 16; ++walk) {
      set = m::NextSet(set, m::kMostSets, true);
      set_of[walk] = set;
    }
    for (int read = 1; read + 2 < 16; ++read) {
      if (set_of[read] == set_of[read + 1] || set_of[read] == set_of[read + 2]) ++bad;
    }
    Check(bad == 0, "the set of a picture that the session reads has no write of the next two walks",
          bad, 0);
    // A set more does not fit the sets of the descriptors of the module.
    Check(m::kMostSets == 3, "three sets", m::kMostSets, 3);
  }

  // The keys of the debug page: one bit for each key and the bit "sent".
  {
    const int values[app_keys::kKeys] = {1, 0, 1, 1, 0, 1, 1};
    const uint32_t bits = app_keys::Pack(values);
    Check(bits == (app_keys::kSent | app_keys::kMresDraws | app_keys::kComposeLight |
                   app_keys::kPassStore | app_keys::kPanelRect | app_keys::kRingOrder) &&
              (bits & app_keys::kMresSort) == 0 && (bits & app_keys::kComposeDirect) == 0,
          "the keys of the debug page are one bit each, in the order of the enum, and the bit sent",
          bits, 0);
    const int none[app_keys::kKeys] = {0, 0, 0, 0, 0, 0, 0};
    Check(app_keys::Pack(none) == app_keys::kSent,
          "a frame with all keys off still has the bit sent (0 means an older stream)", 0, 0);
  }

  // The indices of the draws of the passes. A record of the eyes points into its
  // store. A flush of the queue (a panel draw between two draws of the eyes)
  // clears the store of the queue and not the store of the records.
  {
    std::vector<uint16_t> queue_store, record_store;
    const uint32_t first_at = static_cast<uint32_t>(record_store.size());
    record_store.insert(record_store.end(), 6, 1);  // a draw of the eyes
    queue_store.insert(queue_store.end(), 3, 2);    // a draw of the panel
    queue_store.clear();                            // its flush
    const uint32_t second_at = static_cast<uint32_t>(record_store.size());
    record_store.insert(record_store.end(), 6, 3);  // the next draw of the eyes
    Check(vk_draw_math::IndexesThere(record_store.size(), first_at, 6) &&
              vk_draw_math::IndexesThere(record_store.size(), second_at, 6),
          "after a flush the indices of the records are still there",
          static_cast<double>(record_store.size()), 12);
    Check(!vk_draw_math::IndexesThere(queue_store.size(), first_at, 6) &&
              !vk_draw_math::IndexesThere(5, 3, 6) && vk_draw_math::IndexesThere(9, 3, 6) &&
              vk_draw_math::IndexesThere(0, 0, 0),
          "a store that was cleared, or that is too short, does not have the indices", 0, 0);
  }

  // The transfer function: the shader decodes the byte, an sRGB target
  // encodes it again, and the byte is the same.
  int bad = 0;
  for (int b = 0; b < 256; ++b) {
    const float c = static_cast<float>(b) / 255.0f;
    const float back = m::LinearToSrgb(m::SrgbToLinear(c)) * 255.0f;
    if (static_cast<int>(back + 0.5f) != b) ++bad;
  }
  Check(bad == 0, "decode then encode gives each byte of 0 to 255 again", bad, 0);
  Near("decode of 0 is 0", m::SrgbToLinear(0.0f), 0.0, 1e-9);
  Near("decode of 1 is 1", m::SrgbToLinear(1.0f), 1.0, 1e-6);
}

}  // namespace vk_compose_checks
