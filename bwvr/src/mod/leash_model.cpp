// The pictures of the three leashes. See leash_model.h.
//
// THE FILES OF THE GAME [V, runblack.exe 1.42]. The leash posts of the
// Temple (TempleLeash 0x00464650, from CitadelHeart::CreateLeashes) load
// "Data\MISC\leash.l3d" three times, and one texture "data\textures\
// leash.raw" with its alpha in "leasha.raw". The three kinds differ only in
// the strip of the texture on the mesh: object +0x68 and +0x6C (fn_007F9B70)
// get a V offset of 0.375 for Aggression, 0.125 for Learning and 0.25 for
// Compassion. The rope of the leash in the Hand (GInterface::
// SetLocalLeashFromInterfaceStatus 0x005D9510) uses the same V ranges.
//
// THE MESH is an L3D file (docs/hand-model.md): a header with the count and
// the table of the sub meshes at +0x0C and +0x10, a sub mesh with the count
// and the table of its primitives at +4 and +8, and a primitive with the
// count and the place of its vertices at +16 and +20 and of its triangles
// at +24 and +28. A vertex is 8 floats (the point, u, v, the normal), and a
// triangle is 3 words. leash.l3d is a band round the +Z axis, 1.06 units
// across, with an inner and an outer face.
//
// THE TEXTURE is 256 by 256 pixels: leash.raw has R, G, B for each pixel and
// leasha.raw the alpha, the top row first.
//
// THE PICTURE. The band stands on its axis and turns about the up axis, as
// on a turntable, seen from 36 degrees above. It is drawn with
// panel_raster.h at one sample for each pixel (a picture of the turn costs
// about 0.6 ms), in a box of at most kMaxSize pixels. The triangles go from the far one to the near one, over
// what is there, so the soft edges of the alpha show the far side of the
// band. The read of the files is one time, at the first picture.
//
// THE KEY of bwvr.ini: leash_pictures=1. 0 shows the icon of each kind on
// the sheet of the leashes, with no read of the files.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "leash_model.h"
#include "panel_raster.h"

extern void ModLog(const char* format, ...);

namespace {

const int kMaxVerts = 256;
const int kMaxTris = 256;
const int kTex = 256;
const int kMaxSize = 192;
const int kSamples = 1;
const float kElevationDeg = 36.0f;

struct Mesh {
  int verts;
  float p[kMaxVerts][3];
  float n[kMaxVerts][3];
  float uv[kMaxVerts][2];
  int tris;
  uint16_t t[kMaxTris][3];
  float radius;  // the largest distance of a point from the axis and middle
};

bool g_read = false;
bool g_ok = false;
Mesh g_mesh;
uint8_t g_tex[kTex * kTex * 4];
float g_color[kMaxSize * kSamples * kMaxSize * kSamples * 4];
float g_depth[kMaxSize * kSamples * kMaxSize * kSamples];
uint8_t g_id[kMaxSize * kSamples * kMaxSize * kSamples];
uint8_t g_picture[kMaxSize * kMaxSize * 4];

bool GamePath(const char* file, char out[MAX_PATH]) {
  const DWORD length = GetModuleFileNameA(nullptr, out, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return false;
  char* slash = strrchr(out, '\\');
  if (slash == nullptr) return false;
  slash[1] = 0;
  return strcat_s(out, MAX_PATH, file) == 0;
}

// The whole file in a buffer of the caller's free. Null when it cannot read.
uint8_t* ReadAll(const char* file, size_t* size) {
  char path[MAX_PATH];
  if (!GamePath(file, path)) return nullptr;
  FILE* f = nullptr;
  if (fopen_s(&f, path, "rb") != 0 || f == nullptr) return nullptr;
  fseek(f, 0, SEEK_END);
  const long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t* data = n > 0 && n < (8 << 20) ? static_cast<uint8_t*>(malloc(n))
                                         : nullptr;
  if (data != nullptr && fread(data, 1, n, f) != static_cast<size_t>(n)) {
    free(data);
    data = nullptr;
  }
  fclose(f);
  *size = data != nullptr ? static_cast<size_t>(n) : 0;
  return data;
}

uint32_t U32(const uint8_t* d, size_t at) {
  uint32_t v;
  memcpy(&v, d + at, 4);
  return v;
}

// The sub meshes and their primitives into one mesh. why gets the reason of
// a false.
bool ParseL3D(const uint8_t* d, size_t n, Mesh* m, const char** why) {
  memset(m, 0, sizeof(*m));
  *why = "the file is too short";
  if (n < 0x50) return false;
  *why = "the file is no L3D";
  if (memcmp(d, "L3D0", 4) != 0) return false;
  *why = "a table is out of the file";
  const uint32_t subs = U32(d, 0x0C), table = U32(d, 0x10);
  if (subs == 0 || subs > 16 || table + 4ull * subs > n) return false;
  for (uint32_t s = 0; s < subs; ++s) {
    const uint32_t sub = U32(d, table + 4 * s);
    if (sub + 12ull > n) return false;
    const uint32_t prims = U32(d, sub + 4), prim_table = U32(d, sub + 8);
    if (prims > 16 || prim_table + 4ull * prims > n) return false;
    for (uint32_t k = 0; k < prims; ++k) {
      const uint32_t prim = U32(d, prim_table + 4 * k);
      if (prim + 48ull > n) return false;
      const uint32_t vc = U32(d, prim + 16), vo = U32(d, prim + 20);
      const uint32_t tc = U32(d, prim + 24), to = U32(d, prim + 28);
      *why = "the mesh has more points or triangles than the mod keeps";
      if (m->verts + vc > static_cast<uint32_t>(kMaxVerts) ||
          m->tris + tc > static_cast<uint32_t>(kMaxTris)) {
        return false;
      }
      *why = "a table is out of the file";
      if (vo + 32ull * vc > n || to + 6ull * tc > n) return false;
      const int base = m->verts;
      for (uint32_t v = 0; v < vc; ++v) {
        float f[8];
        memcpy(f, d + vo + 32 * v, 32);
        float* p = m->p[m->verts];
        for (int i = 0; i < 3; ++i) {
          p[i] = f[i];
          m->n[m->verts][i] = f[5 + i];
        }
        m->uv[m->verts][0] = f[3];
        m->uv[m->verts][1] = f[4];
        const float r = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
        if (r > m->radius) m->radius = r;
        ++m->verts;
      }
      for (uint32_t t = 0; t < tc; ++t) {
        uint16_t w[3];
        memcpy(w, d + to + 6 * t, 6);
        *why = "a triangle names a point that is not there";
        if (w[0] >= vc || w[1] >= vc || w[2] >= vc) return false;
        for (int i = 0; i < 3; ++i) {
          m->t[m->tris][i] = static_cast<uint16_t>(base + w[i]);
        }
        ++m->tris;
      }
    }
  }
  *why = "the mesh has no triangle";
  return m->tris > 0 && m->radius > 0.0f;
}

void Read() {
  if (g_read) return;
  g_read = true;
  char path[MAX_PATH];
  bool on = true;
  if (GamePath("bwvr.ini", path)) {
    on = GetPrivateProfileIntA("bwvr", "leash_pictures", 1, path) != 0;
  }
  if (!on) {
    ModLog("leash        : leash_pictures=0: the sheet of the leashes shows "
           "the icon of each kind");
    return;
  }
  size_t n = 0, nc = 0, na = 0;
  uint8_t* mesh = ReadAll("Data\\Misc\\leash.l3d", &n);
  uint8_t* color = ReadAll("Data\\Textures\\leash.raw", &nc);
  uint8_t* alpha = ReadAll("Data\\Textures\\leasha.raw", &na);
  const char* why = "the game has no file Data\\Misc\\leash.l3d";
  bool ok = mesh != nullptr && ParseL3D(mesh, n, &g_mesh, &why);
  if (ok && (color == nullptr || nc != kTex * kTex * 3 || alpha == nullptr ||
             na != kTex * kTex)) {
    ok = false;
    why = "the texture leash.raw or leasha.raw is not 256 by 256";
  }
  if (ok) {
    for (int i = 0; i < kTex * kTex; ++i) {
      const float a = alpha[i] / 255.0f;
      g_tex[i * 4 + 0] = static_cast<uint8_t>(color[i * 3 + 2] * a + 0.5f);
      g_tex[i * 4 + 1] = static_cast<uint8_t>(color[i * 3 + 1] * a + 0.5f);
      g_tex[i * 4 + 2] = static_cast<uint8_t>(color[i * 3 + 0] * a + 0.5f);
      g_tex[i * 4 + 3] = alpha[i];
    }
  }
  free(mesh);
  free(color);
  free(alpha);
  g_ok = ok;
  if (ok) {
    ModLog("leash        : leash_pictures=1: the pictures of the leashes come "
           "from Data\\Misc\\leash.l3d (%d points, %d triangles) and "
           "Data\\Textures\\leash.raw with leasha.raw", g_mesh.verts,
           g_mesh.tris);
  } else {
    ModLog("leash        : leash_pictures=1, but %s. The sheet of the leashes "
           "shows the icon of each kind", why);
  }
}

// The point of the mesh in the view space of the picture: the axis of the
// band (+Z of the mesh) up, then the turn about the up axis.
void Place(const float in[3], float c, float s, float out[3]) {
  const float x = in[0], y = in[2], z = -in[1];
  out[0] = c * x + s * z;
  out[1] = y;
  out[2] = -s * x + c * z;
}

struct Order {
  float depth;
  int tri;
};

int CompareFar(const void* a, const void* b) {
  const float da = static_cast<const Order*>(a)->depth;
  const float db = static_cast<const Order*>(b)->depth;
  return da < db ? 1 : (da > db ? -1 : 0);
}

}  // namespace

bool LeashPicturesOn() {
  Read();
  return g_ok;
}

bool LeashModelDraw(uint8_t* bgra, int pitch, int kind, float turn_deg, int x0,
                    int y0, int size, float alpha) {
  if (!LeashPicturesOn() || kind < 1 || kind > 3) return false;
  if (size > kMaxSize) size = kMaxSize;
  if (size < 16) return false;
  const float offset = kind == 1 ? 0.375f : kind == 2 ? 0.125f : 0.25f;
  panel_raster::Target target;
  const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  panel_raster::Start(&target, size, size, kSamples, g_color, g_depth, g_id,
                      clear);
  const float e = kElevationDeg * 0.01745329f;
  const float eye[3] = {0.0f, 10.0f * sinf(e), 10.0f * cosf(e)};
  const float at[3] = {0.0f, 0.0f, 0.0f};
  const float up[3] = {0.0f, 1.0f, 0.0f};
  panel_raster::View view;
  const float scale = 0.46f * target.width / g_mesh.radius;
  if (!panel_raster::LookAt(eye, at, up, 0.0f, scale, target.width,
                            target.height, &view)) {
    return false;
  }
  const float t = turn_deg * 0.01745329f;
  const float c = cosf(t), s = sinf(t);
  static float placed[kMaxVerts][3];
  static float lit[kMaxVerts];
  static float projected[kMaxVerts][3];
  float to_light[3] = {-0.4f, 0.8f, 0.6f};
  panel_raster::Normalize(to_light);
  for (int i = 0; i < g_mesh.verts; ++i) {
    Place(g_mesh.p[i], c, s, placed[i]);
    float n[3];
    Place(g_mesh.n[i], c, s, n);
    panel_raster::Normalize(n);
    lit[i] = panel_raster::Light(n, to_light, 0.45f);
    if (!panel_raster::Project(view, placed[i], projected[i])) return false;
  }
  // From the far triangle to the near one, each over what is there.
  static Order order[kMaxTris];
  for (int i = 0; i < g_mesh.tris; ++i) {
    const uint16_t* w = g_mesh.t[i];
    order[i].tri = i;
    order[i].depth =
        projected[w[0]][2] + projected[w[1]][2] + projected[w[2]][2];
  }
  qsort(order, g_mesh.tris, sizeof(Order), CompareFar);
  const panel_raster::Texture texture = {g_tex, kTex, kTex, kTex};
  panel_raster::State state = panel_raster::Opaque(1);
  state.depth_test = false;
  state.blend = panel_raster::kOver;
  state.texture = &texture;
  for (int i = 0; i < g_mesh.tris; ++i) {
    const uint16_t* w = g_mesh.t[order[i].tri];
    panel_raster::Corner corner[3];
    for (int k = 0; k < 3; ++k) {
      const int v = w[k];
      corner[k].x = projected[v][0];
      corner[k].y = projected[v][1];
      corner[k].depth = projected[v][2];
      for (int a = 0; a < 3; ++a) corner[k].color[a] = lit[v];
      corner[k].color[3] = 1.0f;
      corner[k].u = g_mesh.uv[v][0];
      corner[k].v = g_mesh.uv[v][1] + offset;
    }
    panel_raster::Triangle(&target, corner, state);
  }
  panel_raster::Resolve(target, g_picture, size);
  // Over the picture of the caller, at the alpha of the caller.
  const int k = static_cast<int>(alpha * 256.0f + 0.5f);
  for (int y = 0; y < size; ++y) {
    const uint8_t* in = g_picture + static_cast<size_t>(y) * size * 4;
    uint8_t* out = bgra + (static_cast<size_t>(y0 + y) * pitch + x0) * 4;
    for (int x = 0; x < size; ++x, in += 4, out += 4) {
      const int a = (in[3] * k) >> 8;
      if (a == 0) continue;
      const int keep = 255 - a;
      for (int ch = 0; ch < 4; ++ch) {
        out[ch] = static_cast<uint8_t>(((in[ch] * k) >> 8) +
                                       (out[ch] * keep + 127) / 255);
      }
    }
  }
  return true;
}
