// A small rasterizer on the CPU for the 3D pictures of the panels: a mesh
// drawn into a picture of a panel, with no GPU and no device. The page
// Controls of the settings window draws the models of the controllers of the
// player with it (controls_page.cpp), and the ring can draw a model that
// turns. No Windows and no game here, so the math test compiles this file
// (panel_raster_checks.h).
//
// THE TARGET has `ss` by `ss` samples for each pixel of the picture. Each
// sample has a color (premultiplied RGBA, floats from 0 to 1), a depth (the
// distance from the eye along the line of sight, in the units of the mesh)
// and an id (the part that drew it, 0 for none). Resolve makes the picture
// from the mean of the samples of each pixel, so an edge is smooth.
//
// THE VIEW is a camera at `eye` that looks at a point. Project gives a point
// of the mesh in samples of the target, and its depth. A perspective view
// has a focal length in samples. An orthographic view (focal 0) has a scale
// in samples for each unit.
//
// A TRIANGLE gets its corners in samples (Project gives them), each with a
// color, a texture place and 1 / depth. The colors and the texture places go
// across the triangle with the right perspective. A sample is in the
// triangle by the top left rule, so two triangles with one edge cover each
// sample of that edge one time. The state of a draw sets the depth test, the
// depth write, the blend (replace or over), the id, a texture and the cull
// of back faces.
//
// THE OUTLINE puts ink at the edges of the parts, as a pen line of a drawing:
// a sample is ink when a sample near it (in a disc of `radius` samples) is
// nearer to the eye and of a different part, or is nearer by more than
// `jump`. So the outline goes round the shape on the paper (the paper has no
// part and is far), round a part in front of another part, and along a fold
// of one part where its far side shows behind its near side.
//
// THE COST. A sample of a triangle is a few multiplies. The outline reads the
// disc of each sample in the box that the draws touched, so keep its radius
// small (2 or 3 samples).

#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace panel_raster {

// --- the vectors -----------------------------------------------------------------

inline float Dot(const float a[3], const float b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline void Cross(const float a[3], const float b[3], float out[3]) {
  const float x = a[1] * b[2] - a[2] * b[1];
  const float y = a[2] * b[0] - a[0] * b[2];
  const float z = a[0] * b[1] - a[1] * b[0];
  out[0] = x;
  out[1] = y;
  out[2] = z;
}

inline bool Normalize(float v[3]) {
  const float n = sqrtf(Dot(v, v));
  if (!(n > 1e-12f)) return false;
  v[0] /= n;
  v[1] /= n;
  v[2] /= n;
  return true;
}

// --- the view --------------------------------------------------------------------

struct View {
  float eye[3];
  float right[3];    // across the picture, to the right
  float down[3];     // down the picture
  float forward[3];  // along the line of sight
  float focal;       // samples, or 0 for an orthographic view
  float scale;       // samples for each unit, for an orthographic view
  float cx, cy;      // the middle of the picture in samples
  float near_plane;  // a point nearer than this does not project
};

// A view from `eye` that looks at `target`, with `up` up in the picture. A
// perspective view when focal > 0, else orthographic with `scale`. width and
// height are the size of the target in samples. False when the eye is at the
// target or up is along the line of sight.
inline bool LookAt(const float eye[3], const float target[3],
                   const float up[3], float focal, float scale, int width,
                   int height, View* out) {
  View v;
  memset(&v, 0, sizeof(v));
  for (int k = 0; k < 3; ++k) {
    v.eye[k] = eye[k];
    v.forward[k] = target[k] - eye[k];
  }
  if (!Normalize(v.forward)) return false;
  Cross(v.forward, up, v.right);
  if (!Normalize(v.right)) return false;
  float up_now[3];
  Cross(v.right, v.forward, up_now);
  for (int k = 0; k < 3; ++k) v.down[k] = -up_now[k];
  v.focal = focal;
  v.scale = scale;
  v.cx = width * 0.5f;
  v.cy = height * 0.5f;
  v.near_plane = 1e-4f;
  *out = v;
  return true;
}

// A point to the target: out[0], out[1] in samples, out[2] the depth. False
// when the point is not in front of the near plane.
inline bool Project(const View& v, const float p[3], float out[3]) {
  const float d[3] = {p[0] - v.eye[0], p[1] - v.eye[1], p[2] - v.eye[2]};
  const float z = Dot(d, v.forward);
  if (v.focal > 0.0f && !(z > v.near_plane)) return false;
  const float x = Dot(d, v.right);
  const float y = Dot(d, v.down);
  if (v.focal > 0.0f) {
    out[0] = v.cx + v.focal * x / z;
    out[1] = v.cy + v.focal * y / z;
  } else {
    out[0] = v.cx + v.scale * x;
    out[1] = v.cy + v.scale * y;
  }
  out[2] = z;
  return true;
}

// The diffuse light of a face: 0 to 1, with `ambient` in the dark.
inline float Light(const float normal[3], const float to_light[3],
                   float ambient) {
  float d = Dot(normal, to_light);
  if (d < 0.0f) d = 0.0f;
  return ambient + (1.0f - ambient) * d;
}

// --- the target ------------------------------------------------------------------

struct Target {
  int width, height;  // in samples
  int ss;             // samples across one pixel, 1 to 4
  float* color;       // 4 floats a sample: premultiplied R, G, B, A
  float* depth;       // the depth of each sample, 1e30 for none
  uint8_t* id;        // the part of each sample, 0 for none
  // The box that the draws touched, in samples: x0, y0, x1, y1 (x1 and y1
  // past the end). Empty when x0 >= x1.
  int touched[4];
};

const float kFar = 1e30f;

// The size of the arrays of a target of `pixels_w` by `pixels_h` pixels.
inline size_t ColorFloats(int pixels_w, int pixels_h, int ss) {
  return static_cast<size_t>(pixels_w) * ss * pixels_h * ss * 4;
}
inline size_t Samples(int pixels_w, int pixels_h, int ss) {
  return static_cast<size_t>(pixels_w) * ss * pixels_h * ss;
}

// A target on the arrays of the caller, cleared to `background`
// (premultiplied RGBA).
inline void Start(Target* t, int pixels_w, int pixels_h, int ss, float* color,
                  float* depth, uint8_t* id, const float background[4]) {
  t->ss = ss < 1 ? 1 : (ss > 4 ? 4 : ss);
  t->width = pixels_w * t->ss;
  t->height = pixels_h * t->ss;
  t->color = color;
  t->depth = depth;
  t->id = id;
  const size_t n = static_cast<size_t>(t->width) * t->height;
  for (size_t i = 0; i < n; ++i) {
    for (int k = 0; k < 4; ++k) color[i * 4 + k] = background[k];
    depth[i] = kFar;
  }
  memset(id, 0, n);
  t->touched[0] = t->width;
  t->touched[1] = t->height;
  t->touched[2] = 0;
  t->touched[3] = 0;
}

// --- the triangles ---------------------------------------------------------------

// A texture: BGRA bytes, color multiplied by alpha, `pitch` pixels a line.
// The place u, v goes from 0 to 1 across and down, and repeats.
struct Texture {
  const uint8_t* bgra;
  int width, height, pitch;
};

// The premultiplied color of a texture at u, v, with a bilinear filter.
inline void Sample(const Texture& tex, float u, float v, float out[4]) {
  const float x = u * tex.width - 0.5f;
  const float y = v * tex.height - 0.5f;
  const float fx = floorf(x), fy = floorf(y);
  const float ax = x - fx, ay = y - fy;
  const auto wrap = [](int i, int n) {
    const int m = i % n;
    return m < 0 ? m + n : m;
  };
  const int x0 = wrap(static_cast<int>(fx), tex.width);
  const int y0 = wrap(static_cast<int>(fy), tex.height);
  const int x1 = wrap(x0 + 1, tex.width);
  const int y1 = wrap(y0 + 1, tex.height);
  const uint8_t* p[4] = {
      tex.bgra + (static_cast<size_t>(y0) * tex.pitch + x0) * 4,
      tex.bgra + (static_cast<size_t>(y0) * tex.pitch + x1) * 4,
      tex.bgra + (static_cast<size_t>(y1) * tex.pitch + x0) * 4,
      tex.bgra + (static_cast<size_t>(y1) * tex.pitch + x1) * 4};
  const float w[4] = {(1 - ax) * (1 - ay), ax * (1 - ay), (1 - ax) * ay,
                      ax * ay};
  // BGRA to RGBA.
  static const int kFrom[4] = {2, 1, 0, 3};
  for (int k = 0; k < 4; ++k) {
    float s = 0.0f;
    for (int c = 0; c < 4; ++c) s += w[c] * p[c][kFrom[k]];
    out[k] = s / 255.0f;
  }
}

// A corner of a triangle: x, y in samples and the depth (Project), the color
// (premultiplied RGBA) and the place on the texture.
struct Corner {
  float x, y, depth;
  float color[4];
  float u, v;
};

enum Blend { kReplace = 0, kOver = 1 };

struct State {
  bool depth_test;   // a sample behind the depth there is not drawn
  bool depth_write;  // a drawn sample keeps its depth and its id
  int blend;         // kReplace or kOver
  uint8_t id;        // the part, for the outline
  bool cull_back;    // a triangle with its corners clockwise on the picture
                     // (its back to the eye) is not drawn
  const Texture* texture;  // null for none: the color of the corners only
};

inline State Opaque(uint8_t id) {
  State s = {true, true, kReplace, id, true, nullptr};
  return s;
}

// Twice the signed area of a, b, c on the picture, with y down. Less than 0
// when a, b, c go counterclockwise as the eye sees them: the front of a
// triangle of a glTF mesh, whose front faces go counterclockwise.
inline float Area2(float ax, float ay, float bx, float by, float cx,
                   float cy) {
  return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

// The top left rule: the edge from a to b of a triangle of signed area
// `area` owns the samples on it when it is a left edge (the inside of the
// triangle is to its right) or a top edge (flat, with the inside below it).
inline bool OwnsEdge(float ax, float ay, float bx, float by, float area) {
  // The direction into the triangle.
  const float nx = -(by - ay) / area;
  const float ny = (bx - ax) / area;
  return nx > 0.0f || (nx == 0.0f && ny > 0.0f);
}

// Draws a triangle. The answer is the count of samples that it drew.
inline int Triangle(Target* t, const Corner c[3], const State& s) {
  const float area = Area2(c[0].x, c[0].y, c[1].x, c[1].y, c[2].x, c[2].y);
  if (fabsf(area) < 1e-12f) return 0;
  const bool front = area < 0.0f;
  if (s.cull_back && !front) return 0;
  for (int k = 0; k < 3; ++k) {
    if (!(c[k].depth > 0.0f)) return 0;
  }
  float x0 = c[0].x, x1 = c[0].x, y0 = c[0].y, y1 = c[0].y;
  for (int k = 1; k < 3; ++k) {
    if (c[k].x < x0) x0 = c[k].x;
    if (c[k].x > x1) x1 = c[k].x;
    if (c[k].y < y0) y0 = c[k].y;
    if (c[k].y > y1) y1 = c[k].y;
  }
  int ix0 = static_cast<int>(floorf(x0));
  int iy0 = static_cast<int>(floorf(y0));
  int ix1 = static_cast<int>(ceilf(x1));
  int iy1 = static_cast<int>(ceilf(y1));
  if (ix0 < 0) ix0 = 0;
  if (iy0 < 0) iy0 = 0;
  if (ix1 > t->width) ix1 = t->width;
  if (iy1 > t->height) iy1 = t->height;
  if (ix0 >= ix1 || iy0 >= iy1) return 0;
  // 1 / depth and each attribute / depth go across the picture in a line.
  float inv[3], attr[3][6];
  for (int k = 0; k < 3; ++k) {
    inv[k] = 1.0f / c[k].depth;
    for (int a = 0; a < 4; ++a) attr[k][a] = c[k].color[a] * inv[k];
    attr[k][4] = c[k].u * inv[k];
    attr[k][5] = c[k].v * inv[k];
  }
  // The edges: edge e is from corner e + 1 to corner e + 2, and its weight
  // is that of corner e.
  const int next[3] = {1, 2, 0};
  bool owns[3];
  for (int e = 0; e < 3; ++e) {
    const Corner& a = c[next[e]];
    const Corner& b = c[next[next[e]]];
    owns[e] = OwnsEdge(a.x, a.y, b.x, b.y, area);
  }
  int drawn = 0;
  for (int y = iy0; y < iy1; ++y) {
    const float py = y + 0.5f;
    for (int x = ix0; x < ix1; ++x) {
      const float px = x + 0.5f;
      float w[3];
      bool in = true;
      for (int e = 0; e < 3 && in; ++e) {
        const Corner& a = c[next[e]];
        const Corner& b = c[next[next[e]]];
        const float v = Area2(a.x, a.y, b.x, b.y, px, py) / area;
        w[e] = v;
        if (v < 0.0f || (v == 0.0f && !owns[e])) in = false;
      }
      if (!in) continue;
      const float one = w[0] * inv[0] + w[1] * inv[1] + w[2] * inv[2];
      if (!(one > 0.0f)) continue;
      const float depth = 1.0f / one;
      const size_t at = static_cast<size_t>(y) * t->width + x;
      if (s.depth_test && depth > t->depth[at]) continue;
      float col[4];
      for (int a = 0; a < 4; ++a) {
        col[a] = (w[0] * attr[0][a] + w[1] * attr[1][a] + w[2] * attr[2][a]) *
                 depth;
      }
      if (s.texture != nullptr) {
        const float u =
            (w[0] * attr[0][4] + w[1] * attr[1][4] + w[2] * attr[2][4]) * depth;
        const float v =
            (w[0] * attr[0][5] + w[1] * attr[1][5] + w[2] * attr[2][5]) * depth;
        float tex[4];
        Sample(*s.texture, u, v, tex);
        for (int a = 0; a < 4; ++a) col[a] *= tex[a];
      }
      float* d = &t->color[at * 4];
      if (s.blend == kOver) {
        const float keep = 1.0f - col[3];
        for (int a = 0; a < 4; ++a) d[a] = col[a] + d[a] * keep;
      } else {
        for (int a = 0; a < 4; ++a) d[a] = col[a];
      }
      if (s.depth_write) {
        t->depth[at] = depth;
        t->id[at] = s.id;
      }
      ++drawn;
    }
  }
  if (drawn > 0) {
    if (ix0 < t->touched[0]) t->touched[0] = ix0;
    if (iy0 < t->touched[1]) t->touched[1] = iy0;
    if (ix1 > t->touched[2]) t->touched[2] = ix1;
    if (iy1 > t->touched[3]) t->touched[3] = iy1;
  }
  return drawn;
}

// --- the outline -----------------------------------------------------------------

// Ink of `ink` (premultiplied RGBA) over each sample at an edge (THE OUTLINE
// above). `radius` is in samples. `scratch` has one byte for each sample.
// The answer is the count of samples of ink.
inline int Outline(Target* t, const float ink[4], int radius, float jump,
                   uint8_t* scratch) {
  if (t->touched[0] >= t->touched[2]) return 0;
  const int r = radius < 1 ? 1 : radius;
  const int x0 = t->touched[0] - r < 0 ? 0 : t->touched[0] - r;
  const int y0 = t->touched[1] - r < 0 ? 0 : t->touched[1] - r;
  const int x1 = t->touched[2] + r > t->width ? t->width : t->touched[2] + r;
  const int y1 = t->touched[3] + r > t->height ? t->height : t->touched[3] + r;
  const int rr = r * r + r;  // a round disc
  int count = 0;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      const size_t at = static_cast<size_t>(y) * t->width + x;
      const float d = t->depth[at];
      const uint8_t id = t->id[at];
      bool edge = false;
      for (int dy = -r; dy <= r && !edge; ++dy) {
        const int qy = y + dy;
        if (qy < 0 || qy >= t->height) continue;
        for (int dx = -r; dx <= r; ++dx) {
          if (dx * dx + dy * dy > rr) continue;
          const int qx = x + dx;
          if (qx < 0 || qx >= t->width) continue;
          const size_t q = static_cast<size_t>(qy) * t->width + qx;
          const float dq = t->depth[q];
          if (dq >= d) continue;
          if (t->id[q] != id || d - dq > jump) {
            edge = true;
            break;
          }
        }
      }
      scratch[at] = edge ? 1 : 0;
    }
  }
  const float keep = 1.0f - ink[3];
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      const size_t at = static_cast<size_t>(y) * t->width + x;
      if (scratch[at] == 0) continue;
      float* c = &t->color[at * 4];
      for (int a = 0; a < 4; ++a) c[a] = ink[a] + c[a] * keep;
      ++count;
    }
  }
  return count;
}

// --- the picture -----------------------------------------------------------------

inline uint8_t Byte(float v) {
  if (!(v > 0.0f)) return 0;
  if (v >= 1.0f) return 255;
  return static_cast<uint8_t>(v * 255.0f + 0.5f);
}

// The picture: BGRA, color multiplied by alpha, `pitch` pixels a line. Each
// pixel is the mean of its samples.
inline void Resolve(const Target& t, uint8_t* bgra, int pitch) {
  const int ss = t.ss;
  const int w = t.width / ss, h = t.height / ss;
  const float n = 1.0f / static_cast<float>(ss * ss);
  for (int y = 0; y < h; ++y) {
    uint8_t* row = bgra + static_cast<size_t>(y) * pitch * 4;
    for (int x = 0; x < w; ++x) {
      float s[4] = {0, 0, 0, 0};
      for (int sy = 0; sy < ss; ++sy) {
        const float* c =
            &t.color[(static_cast<size_t>(y * ss + sy) * t.width + x * ss) * 4];
        for (int sx = 0; sx < ss; ++sx, c += 4) {
          for (int a = 0; a < 4; ++a) s[a] += c[a];
        }
      }
      uint8_t* p = row + x * 4;
      p[0] = Byte(s[2] * n);
      p[1] = Byte(s[1] * n);
      p[2] = Byte(s[0] * n);
      p[3] = Byte(s[3] * n);
    }
  }
}

// The part of each pixel: the part of the most samples of the pixel that
// have a part, into `part` (0 for none), and its share of the samples into
// `share` (0 to 255). For a light on one part after the draw, with a smooth
// edge.
inline void ResolveParts(const Target& t, uint8_t* part, uint8_t* share) {
  const int ss = t.ss;
  const int w = t.width / ss, h = t.height / ss;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      uint8_t ids[16];
      int counts[16];
      int kinds = 0;
      for (int sy = 0; sy < ss; ++sy) {
        const uint8_t* s =
            &t.id[static_cast<size_t>(y * ss + sy) * t.width + x * ss];
        for (int sx = 0; sx < ss; ++sx) {
          if (s[sx] == 0) continue;
          int k = 0;
          while (k < kinds && ids[k] != s[sx]) ++k;
          if (k == kinds) {
            ids[kinds] = s[sx];
            counts[kinds++] = 0;
          }
          ++counts[k];
        }
      }
      int best = -1;
      for (int k = 0; k < kinds; ++k) {
        if (best < 0 || counts[k] > counts[best]) best = k;
      }
      const size_t at = static_cast<size_t>(y) * w + x;
      part[at] = best >= 0 ? ids[best] : 0;
      share[at] = best >= 0 ? static_cast<uint8_t>(
                                  counts[best] * 255 / (ss * ss))
                            : 0;
    }
  }
}

}  // namespace panel_raster
