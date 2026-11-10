// The controller in the hand (hint_style=2). See docs/controller-hints.md,
// "The controller in the hand".
//
// WHAT. Half-Life: Alyx shows a hint as the real controller in the hand of
// the player, with the button of the hint lit and a label at the button. The
// mod does the same in the look of its scroll cards. While a hand has a hint,
// the model of its controller fades in at the grip pose of that hand. The
// body of the controller is faint, in a parchment tone. Each button of a
// hint is solid and lit in a warm gold. Each lit button has its own small
// scroll card beside the controller (hint_card.cpp draws the cards), and a
// thin line of ink goes from the button to the inner edge of its card.
//
// THE MODELS are the WebXR Input Profiles (bwvr-controllers beside the mod,
// bwvr/assets/controllers in the repo). hint_model_math.h gives the model of
// the interaction profile and of the name of the headset, and the node of
// each control. A load reads the meshes, the normals and the nodes with
// cgltf, on a thread of its own, at the first hint of that hand. The mod does
// not read the textures.
//
// THE PLACE. A model is in the grip space of its hand: meters, right handed,
// y up and -z ahead, as the grip pose of OpenXR. The overlay gives the grip
// pose in the world (OverlayHand: position and rotation L * M_grip, where
// M_grip has z the other way). So a point p of the model is in the world at
//
//   position + rotation * (p.x, p.y, -p.z) * units_per_meter
//
// and the controller is at the real controller, one to one, at each scale of
// the world. A real hand in the land is drawn on the land (hand_land.cpp),
// and its controller goes with the drawn hand. The line of ink still ends at
// the edge of its card.
//
// THE PRESS. A node "<base>_pressed_value" goes between its min node and its
// max node with the value of the control, as the profiles of WebXR say: the
// trigger and the grip with their travel, a button with its press, and the
// stick with its two axes. The player moves the controller, so this is no
// motion that the player did not make.
//
// THE DRAW. In each eye pass, after the opaque meshes of the overlay and
// before the mesh of the hands, with the depth of the scene, so the land can
// hide the controller. The CPU puts each corner on the screen as the
// overlay does (overlay_math.h, Project). The draws: the lit buttons and
// the lines with the depth write on, the back faces of the body with an
// alpha, the depth of its front faces, the ink outline (the shell of the back
// faces, pushed out along the normals, which the front faces hide inside the
// silhouette), and the front faces with an alpha. So the faint body reads as
// a shape on bright land. A triangle with a corner in front of the near
// plane is not drawn: the controller is at the length of an arm.
//
// THE HAND goes faint (hint_model_math.h, HandOpacity) while its controller
// shows. It stays, so the player still sees where the Hand touches, and the
// controller shows through it.
//
// THE KEYS of bwvr.ini:
//
//   hint_controller=1     the row "Show 3D controller for hints" of the
//                         launcher. 1 gives hint_style=2 and
//                         hint_card_pictures=1. 0 gives hint_style=1 and
//                         hint_card_pictures=0 (hint_model_math.h,
//                         StyleOfSwitch)
//   hint_style            2 the controller in the hand with a card for each
//                         lit button. 1 the cards at the wrists. A hand with
//                         no model has the cards of 1. The default comes from
//                         hint_controller. Ctrl Shift H switches it, with
//                         test_keys=1
//   hint_body_fine=1      1 the simpler body of a heavy model has cells of
//                         3.4 mm at most, and a cell for each side of the
//                         normal, so the ink outline of a thin part does not
//                         break. 0 the cells grow until the body has 2500
//                         triangles, with one cell for all the normals
//   hint_hand_opacity_pct=68
//                         the opacity of the hand while its controller
//                         shows, in percent. 35 is the fainter hand
//   hint_pulse=1          1 the lit buttons pulse, one slow beat a second
//                         from dim to full light. 0 a steady light
//   hint_model_test       a test key: the folder of a model (for example
//                         valve-index) for both hands, in place of the one of
//                         the profile
//   hint_model_test_dir   a test key: the folder of the models, in place of
//                         bwvr-controllers beside runblack.exe

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "../vr/xr_screen.h"
#include "controller_art_math.h"
#include "controllers.h"
#include "hint_card_math.h"
#include "hint_model.h"
#include "hint_model_math.h"
#include "overlay_math.h"

#ifdef BWVR_HAVE_CGLTF
#pragma warning(push, 0)
#include "cgltf.h"
#pragma warning(pop)
#endif

extern void ModLog(const char* format, ...);

// controllers.cpp
bool ControllersCardPose(int which, float grip_position[3],
                         float grip_orientation[4], float head[3], bool* left);
bool ControllersHandInput(int side, bwvr::HandInput* out);
const char* ControllersProfileOf(int side);
const char* ControllersSystemName();
bool ControllersHeadTurn(float out[4]);
int ControllersHandCm();
int ControllersHandScalePct();
uint64_t ControllersFrameCount();

namespace {

namespace hm = hint_model;
namespace art = controller_art;

// --- the settings ------------------------------------------------------------------

bool g_read = false;
int g_style = 2;          // hint_style, and its test key
bool g_test_keys = false;
bool g_body_fine = true;  // hint_body_fine
int g_hand_opacity_pct = hint_model::kHandOpacityPct;  // hint_hand_opacity_pct
bool g_pulse = true;      // hint_pulse
char g_test_model[64] = "";
char g_dir[MAX_PATH] = "";

bool GameFolder(char* out, size_t size) {
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return false;
  char* slash = strrchr(path, '\\');
  if (slash == nullptr) return false;
  slash[1] = 0;
  strcpy_s(out, size, path);
  return true;
}

void ReadSettings() {
  if (g_read) return;
  g_read = true;
  char folder[MAX_PATH] = "", ini[MAX_PATH] = "";
  if (GameFolder(folder, sizeof(folder))) {
    strcpy_s(ini, folder);
    strcat_s(ini, "bwvr.ini");
  }
  // The switch of the launcher gives the default (hint_model_math.h,
  // StyleOfSwitch). hint_style itself still acts, for a test.
  const int controller =
      static_cast<int>(GetPrivateProfileIntA("bwvr", "hint_controller", 1, ini));
  g_style = static_cast<int>(GetPrivateProfileIntA(
      "bwvr", "hint_style", hm::StyleOfSwitch(controller), ini));
  if (g_style != 1) g_style = 2;
  g_test_keys = GetPrivateProfileIntA("bwvr", "test_keys", 0, ini) != 0;
  g_body_fine = GetPrivateProfileIntA("bwvr", "hint_body_fine", 1, ini) != 0;
  g_hand_opacity_pct = static_cast<int>(GetPrivateProfileIntA(
      "bwvr", "hint_hand_opacity_pct", hint_model::kHandOpacityPct, ini));
  if (g_hand_opacity_pct < 0) g_hand_opacity_pct = 0;
  if (g_hand_opacity_pct > 100) g_hand_opacity_pct = 100;
  g_pulse = GetPrivateProfileIntA("bwvr", "hint_pulse", 1, ini) != 0;
  GetPrivateProfileStringA("bwvr", "hint_model_test", "", g_test_model,
                           sizeof(g_test_model), ini);
  char dir[MAX_PATH] = "";
  GetPrivateProfileStringA("bwvr", "hint_model_test_dir", "", dir,
                           sizeof(dir), ini);
  if (dir[0] != 0) {
    strcpy_s(g_dir, dir);
    const size_t n = strlen(g_dir);
    if (n > 0 && g_dir[n - 1] != '\\' && g_dir[n - 1] != '/') {
      strcat_s(g_dir, "\\");
    }
  } else {
    strcpy_s(g_dir, folder);
    strcat_s(g_dir, "bwvr-controllers\\");
  }
  ModLog("hint model   : hint_controller=%d, hint_style=%d, %s. The models "
         "are in %s%s%s",
         controller != 0 ? 1 : 0, g_style,
         g_style == 2 ? "while a hand has a hint, the model of its controller "
                        "shows in the hand with the buttons of the hint lit, "
                        "and each lit button has its own card"
                      : "the hints are on the cards at the wrists",
         g_dir, g_test_model[0] != 0 ? ". hint_model_test gives the model " : "",
         g_test_model);
  ModLog("hint model   : hint_hand_opacity_pct=%d, the hand is at %d percent of "
         "its opacity while its controller shows. hint_pulse=%d, %s",
         g_hand_opacity_pct, g_hand_opacity_pct, g_pulse ? 1 : 0,
         g_pulse ? "the lit buttons pulse from dim to full light, one beat a "
                   "second"
                 : "the lit buttons have a steady light");
}

// --- the model -----------------------------------------------------------------------

const int kNameSize = 64;
struct Node {
  char name[kNameSize];
  int parent;
  bool has_trs;  // a node with a matrix keeps it, with no press
  hm::Trs rest;
  hm::Matrix rest_matrix;
  // A node of a press: its min and max nodes, the axis and the input.
  int min_node, max_node;
  int axis;
  int input;
  bool hidden;  // under a node that only shows at a touch
  int owner;    // the nearest value node of a control at or above, or -1
  hm::Matrix model;  // in the grip space, this frame
};

struct Part {
  int node;
  int first;  // the first vertex
  int count;
};

// The inputs that move a node.
enum Input { kInNone, kInTrigger, kInSqueeze, kInStickClick, kInPrimary,
             kInSecondary, kInMenu, kInStickX, kInStickY };

const int kMostVertices = 60000;
// THE OUTLINE of the body: each corner of the shell is this far out along
// its normal, in meters of the grip space.
const float kShellM = 0.0012f;

struct Model {
  char id[kNameSize];
  bool left;
  int nodes;
  Node* node;
  int* order;  // each parent before its children
  int parts;
  Part* part;
  int vertices;
  float* position;  // 3 floats a vertex, in the space of its node
  float* normal;
  int triangles;
  uint32_t* index;  // 3 a triangle, into the vertices
  int* triangle_part;
  int value_node[art::kControls];  // or -1
  bool lit_mesh[art::kControls];   // the control has a mesh of its own
  float anchor[art::kControls][3];  // in the grip space, at rest
  float anchor_half[art::kControls][3];  // the half size of its box
  float low[3], high[3];  // the box of the meshes at rest, grip space
  double load_ms;
  // Each vertex in the grip space at rest, its normal, and its corner of the
  // outline. A part under a node of a press moves, and the others do not.
  float* grip_pos;
  float* grip_nrm;
  float* grip_shell;
  bool* part_moving;
  int read_vertices;  // before the weld
  int body_read;      // the triangles of the body in the file
  int body_kept;      // and after SimplifyBody
  float body_cell;    // the cell of SimplifyBody in meters, or 0
  signed char* triangle_control;  // for a picture of a panel, or null
};

void FreeModel(Model* m) {
  if (m == nullptr) return;
  free(m->node);
  free(m->order);
  free(m->part);
  free(m->position);
  free(m->normal);
  free(m->index);
  free(m->triangle_part);
  free(m->grip_pos);
  free(m->grip_nrm);
  free(m->grip_shell);
  free(m->part_moving);
  free(m->triangle_control);
  free(m);
}

int FindNode(const Model& m, const char* name) {
  for (int i = 0; i < m.nodes; ++i) {
    if (strcmp(m.node[i].name, name) == 0) return i;
  }
  return -1;
}

bool EndsWith(const char* text, const char* end) {
  const size_t a = strlen(text), b = strlen(end);
  return a >= b && strcmp(text + a - b, end) == 0;
}

// The input of a value node, from its name (hint_model_math.h, the nodes).
void InputOf(const char* name, int* input, int* axis) {
  *axis = hm::kAxisButton;
  *input = kInNone;
  if (strstr(name, "_touched") != nullptr) return;
  const bool x = strstr(name, "_xaxis_") != nullptr;
  const bool y = strstr(name, "_yaxis_") != nullptr;
  if (x || y) {
    *axis = x ? hm::kAxisX : hm::kAxisY;
    if (strstr(name, "thumbstick") != nullptr ||
        strstr(name, "touchpad") != nullptr) {
      *input = x ? kInStickX : kInStickY;
    }
    return;
  }
  if (strncmp(name, "xr_standard_trigger", 19) == 0) *input = kInTrigger;
  else if (strncmp(name, "xr_standard_squeeze", 19) == 0) *input = kInSqueeze;
  else if (strncmp(name, "xr_standard_thumbstick", 22) == 0 ||
           strncmp(name, "xr_standard_touchpad", 20) == 0) {
    *input = kInStickClick;
  } else if (strncmp(name, "a_button", 8) == 0 ||
             strncmp(name, "x_button", 8) == 0) {
    *input = kInPrimary;
  } else if (strncmp(name, "b_button", 8) == 0 ||
             strncmp(name, "y_button", 8) == 0) {
    *input = kInSecondary;
  } else if (strncmp(name, "menu", 4) == 0) {
    *input = kInMenu;
  }
}

// THE BODY OF A HEAVY MODEL. The body is faint, so it needs less detail
// than the lit parts. A part that is not a control and that no press moves
// is body. When the body has more than kBodyTriangles triangles, the load
// puts its vertices in cells of a grid in the grip space: one vertex for
// each cell of each part, at the mean place, with the mean normal. A
// triangle with two corners in one cell goes. The cell starts at 2 mm and
// grows by 30 percent until the body has kBodyTriangles or less. The lit
// parts keep all their detail.
//
// With hint_body_fine=1 the cell stops at kMostCellM (2, 2.6 and 3.4 mm),
// also when the body then has more triangles, and a cell is also for one
// side of the normal (hint_model::NormalSide). A larger cell, or one cell
// for the two faces of a thin part (a ring, a strap), makes the part one
// sheet with a mean normal of almost zero, and its ink outline breaks.
const int kBodyTriangles = 2500;
const float kMostCellM = 0.0034f;  // with hint_body_fine=1

bool BodyPart(const Model& m, int p) {
  return !m.part_moving[p] && m.node[m.part[p].node].owner < 0;
}

void SimplifyBody(Model* m) {
  int body = 0;
  for (int t = 0; t < m->triangles; ++t) {
    if (BodyPart(*m, m->triangle_part[t])) ++body;
  }
  m->body_read = m->body_kept = body;
  m->body_cell = 0.0f;
  if (body <= kBodyTriangles) return;
  std::vector<int> cluster(m->vertices, -1);
  std::unordered_map<uint64_t, int> cells;
  float cell = 0.002f;
  int clusters = 0;
  int kept = body;
  for (int round = 0; round < 16; ++round, cell *= 1.3f) {
    cells.clear();
    clusters = 0;
    for (int p = 0; p < m->parts; ++p) {
      const Part& part = m->part[p];
      const bool is_body = BodyPart(*m, p);
      for (int k = 0; k < part.count; ++k) {
        const int v = part.first + k;
        if (!is_body) {
          cluster[v] = -1;
          continue;
        }
        const float* g = &m->grip_pos[v * 3];
        // 8 bits of the part, 3 of the side and 17 for each axis (cells of
        // 2 mm or more, so more than 100 m each way).
        uint64_t key = static_cast<uint64_t>(p) & 0xFF;
        key = (key << 3) |
              static_cast<uint64_t>(
                  g_body_fine ? hm::NormalSide(&m->grip_nrm[v * 3]) : 0);
        for (int a = 0; a < 3; ++a) {
          const int64_t c = static_cast<int64_t>(floorf(g[a] / cell)) + (1 << 16);
          key = (key << 17) | (static_cast<uint64_t>(c) & 0x1FFFF);
        }
        auto found = cells.find(key);
        if (found == cells.end()) {
          cells.emplace(key, clusters);
          cluster[v] = clusters++;
        } else {
          cluster[v] = found->second;
        }
      }
    }
    kept = 0;
    for (int t = 0; t < m->triangles; ++t) {
      if (!BodyPart(*m, m->triangle_part[t])) continue;
      const uint32_t* i = &m->index[t * 3];
      const int a = cluster[i[0]], b = cluster[i[1]], c = cluster[i[2]];
      if (a != b && b != c && a != c) ++kept;
    }
    if (kept <= kBodyTriangles) break;
    if (g_body_fine && cell * 1.3f > kMostCellM) break;
  }
  // The new vertices: each part in its order, a body part as its cells.
  std::vector<float> sum_pos(clusters * 3, 0.0f), sum_nrm(clusters * 3, 0.0f);
  std::vector<int> count(clusters, 0);
  for (int v = 0; v < m->vertices; ++v) {
    const int c = cluster[v];
    if (c < 0) continue;
    for (int a = 0; a < 3; ++a) {
      sum_pos[c * 3 + a] += m->grip_pos[v * 3 + a];
      sum_nrm[c * 3 + a] += m->grip_nrm[v * 3 + a];
    }
    ++count[c];
  }
  const int most = m->vertices;  // the new count is never more
  float* position = static_cast<float*>(malloc(sizeof(float) * 3 * most));
  float* normal = static_cast<float*>(malloc(sizeof(float) * 3 * most));
  float* grip_pos = static_cast<float*>(malloc(sizeof(float) * 3 * most));
  float* grip_nrm = static_cast<float*>(malloc(sizeof(float) * 3 * most));
  float* grip_shell = static_cast<float*>(malloc(sizeof(float) * 3 * most));
  if (position == nullptr || normal == nullptr || grip_pos == nullptr ||
      grip_nrm == nullptr || grip_shell == nullptr) {
    free(position); free(normal); free(grip_pos); free(grip_nrm);
    free(grip_shell);
    return;
  }
  std::vector<int> remap(m->vertices, -1);
  std::vector<int> cluster_new(clusters, -1);
  int out = 0;
  for (int p = 0; p < m->parts; ++p) {
    Part& part = m->part[p];
    const int first = out;
    for (int k = 0; k < part.count; ++k) {
      const int v = part.first + k;
      const int c = cluster[v];
      if (c >= 0) {
        if (cluster_new[c] < 0) {
          cluster_new[c] = out;
          float n[3];
          for (int a = 0; a < 3; ++a) {
            grip_pos[out * 3 + a] = sum_pos[c * 3 + a] / count[c];
            n[a] = sum_nrm[c * 3 + a];
          }
          overlay_math::Normalize(n);
          for (int a = 0; a < 3; ++a) {
            grip_nrm[out * 3 + a] = n[a];
            grip_shell[out * 3 + a] = grip_pos[out * 3 + a] + n[a] * kShellM;
            // A body part never moves, so the space of its node is not in
            // use. The place in the grip space stands in for it.
            position[out * 3 + a] = grip_pos[out * 3 + a];
            normal[out * 3 + a] = n[a];
          }
          ++out;
        }
        remap[v] = cluster_new[c];
      } else {
        for (int a = 0; a < 3; ++a) {
          position[out * 3 + a] = m->position[v * 3 + a];
          normal[out * 3 + a] = m->normal[v * 3 + a];
          grip_pos[out * 3 + a] = m->grip_pos[v * 3 + a];
          grip_nrm[out * 3 + a] = m->grip_nrm[v * 3 + a];
          grip_shell[out * 3 + a] = m->grip_shell[v * 3 + a];
        }
        remap[v] = out++;
      }
    }
    part.first = first;
    part.count = out - first;
  }
  int t_out = 0;
  for (int t = 0; t < m->triangles; ++t) {
    const uint32_t* i = &m->index[t * 3];
    const uint32_t a = static_cast<uint32_t>(remap[i[0]]);
    const uint32_t b = static_cast<uint32_t>(remap[i[1]]);
    const uint32_t c = static_cast<uint32_t>(remap[i[2]]);
    if (a == b || b == c || a == c) continue;
    m->index[t_out * 3 + 0] = a;
    m->index[t_out * 3 + 1] = b;
    m->index[t_out * 3 + 2] = c;
    m->triangle_part[t_out] = m->triangle_part[t];
    ++t_out;
  }
  free(m->position); free(m->normal); free(m->grip_pos); free(m->grip_nrm);
  free(m->grip_shell);
  m->position = position;
  m->normal = normal;
  m->grip_pos = grip_pos;
  m->grip_nrm = grip_nrm;
  m->grip_shell = grip_shell;
  m->vertices = out;
  m->triangles = t_out;
  m->body_kept = kept;
  m->body_cell = cell;
}

// THE WELD. Many files have each corner of a face as a vertex of its own,
// also where faces share the place and the normal of the corner. One vertex
// for each place and normal in a part gives fewer corners to put on the
// screen in each eye. A place is the same when its floats are the same, and
// a normal is the same within about 2.5 degrees.
void Weld(Model* m) {
  const int n = m->vertices;
  int* order = static_cast<int*>(malloc(sizeof(int) * (n > 0 ? n : 1)));
  int* remap = static_cast<int*>(malloc(sizeof(int) * (n > 0 ? n : 1)));
  float* pos = static_cast<float*>(malloc(sizeof(float) * 3 * (n > 0 ? n : 1)));
  float* nrm = static_cast<float*>(malloc(sizeof(float) * 3 * (n > 0 ? n : 1)));
  if (order == nullptr || remap == nullptr || pos == nullptr ||
      nrm == nullptr) {
    free(order);
    free(remap);
    free(pos);
    free(nrm);
    return;
  }
  const float* src = m->position;
  auto same_place = [src](int x, int y) {
    return memcmp(&src[x * 3], &src[y * 3], sizeof(float) * 3) == 0;
  };
  int out = 0;
  for (int p = 0; p < m->parts; ++p) {
    Part& part = m->part[p];
    for (int k = 0; k < part.count; ++k) order[k] = part.first + k;
    std::sort(order, order + part.count, [src](int x, int y) {
      const int c = memcmp(&src[x * 3], &src[y * 3], sizeof(float) * 3);
      return c != 0 ? c < 0 : x < y;
    });
    const int first = out;
    for (int i = 0; i < part.count; ++i) {
      const int v = order[i];
      int match = -1;
      for (int j = i - 1; j >= 0 && same_place(order[j], v); --j) {
        const int w = order[j];
        const float* a = &m->normal[v * 3];
        const float* b = &m->normal[w * 3];
        if (a[0] * b[0] + a[1] * b[1] + a[2] * b[2] > 0.999f) {
          match = remap[w];
          break;
        }
      }
      if (match >= 0) {
        remap[v] = match;
        continue;
      }
      for (int k = 0; k < 3; ++k) {
        pos[out * 3 + k] = m->position[v * 3 + k];
        nrm[out * 3 + k] = m->normal[v * 3 + k];
      }
      remap[v] = out++;
    }
    part.first = first;
    part.count = out - first;
  }
  for (int i = 0; i < m->triangles * 3; ++i) {
    m->index[i] = static_cast<uint32_t>(remap[m->index[i]]);
  }
  free(m->position);
  free(m->normal);
  m->position = pos;
  m->normal = nrm;
  m->read_vertices = m->vertices;
  m->vertices = out;
  free(order);
  free(remap);
}

#ifdef BWVR_HAVE_CGLTF

void TrsOf(const cgltf_node* n, hm::Trs* out) {
  for (int k = 0; k < 3; ++k) {
    out->t[k] = n->has_translation ? n->translation[k] : 0.0f;
    out->s[k] = n->has_scale ? n->scale[k] : 1.0f;
  }
  for (int k = 0; k < 4; ++k) {
    out->q[k] = n->has_rotation ? n->rotation[k] : (k == 3 ? 1.0f : 0.0f);
  }
}

// Reads one model file into a new Model. Null with the reason in `why`.
Model* ReadModel(const char* path, const char* id, bool left, char* why,
                 size_t why_size) {
  cgltf_options options;
  memset(&options, 0, sizeof(options));
  cgltf_data* data = nullptr;
  if (cgltf_parse_file(&options, path, &data) != cgltf_result_success ||
      data == nullptr) {
    _snprintf_s(why, why_size, _TRUNCATE, "the file %s is not there or is not "
                "a glTF file", path);
    return nullptr;
  }
  if (cgltf_load_buffers(&options, data, path) != cgltf_result_success) {
    _snprintf_s(why, why_size, _TRUNCATE, "the data of %s is not complete",
                path);
    cgltf_free(data);
    return nullptr;
  }
  Model* m = static_cast<Model*>(calloc(1, sizeof(Model)));
  strcpy_s(m->id, id);
  m->left = left;
  m->nodes = static_cast<int>(data->nodes_count);
  m->node = static_cast<Node*>(calloc(m->nodes > 0 ? m->nodes : 1,
                                      sizeof(Node)));
  m->order = static_cast<int*>(calloc(m->nodes > 0 ? m->nodes : 1,
                                      sizeof(int)));
  for (int i = 0; i < m->nodes; ++i) {
    const cgltf_node* n = &data->nodes[i];
    Node& d = m->node[i];
    if (n->name != nullptr) strncpy_s(d.name, n->name, _TRUNCATE);
    d.parent = n->parent != nullptr
                   ? static_cast<int>(n->parent - data->nodes)
                   : -1;
    d.has_trs = !n->has_matrix;
    TrsOf(n, &d.rest);
    if (n->has_matrix) {
      for (int c = 0; c < 4; ++c) {
        for (int k = 0; k < 3; ++k) d.rest_matrix.m[c * 3 + k] = n->matrix[c * 4 + k];
      }
    } else {
      hm::MatrixOfTrs(d.rest, &d.rest_matrix);
    }
    d.min_node = d.max_node = -1;
    d.owner = -1;
  }
  // Each parent before its children.
  {
    int n = 0;
    bool* placed = static_cast<bool*>(calloc(m->nodes > 0 ? m->nodes : 1, 1));
    bool more = true;
    while (more && n < m->nodes) {
      more = false;
      for (int i = 0; i < m->nodes; ++i) {
        if (placed[i]) continue;
        const int p = m->node[i].parent;
        if (p < 0 || placed[p]) {
          placed[i] = true;
          m->order[n++] = i;
          more = true;
        }
      }
    }
    free(placed);
  }
  // The nodes of a press, and the nodes that only show at a touch.
  for (int i = 0; i < m->nodes; ++i) {
    Node& d = m->node[i];
    if (!EndsWith(d.name, "_value")) continue;
    char base[kNameSize];
    strncpy_s(base, d.name, _TRUNCATE);
    base[strlen(base) - 6] = 0;
    char name[kNameSize + 8];
    _snprintf_s(name, _TRUNCATE, "%s_min", base);
    const int lo = FindNode(*m, name);
    _snprintf_s(name, _TRUNCATE, "%s_max", base);
    const int hi = FindNode(*m, name);
    if (lo >= 0 && hi >= 0 && d.has_trs && m->node[lo].has_trs &&
        m->node[hi].has_trs) {
      d.min_node = lo;
      d.max_node = hi;
      InputOf(d.name, &d.input, &d.axis);
    }
  }
  for (int c = 0; c < art::kControls; ++c) {
    m->value_node[c] = -1;
    const char* bases[hm::kMostBases];
    const int count = hm::NodeBases(c, left, bases);
    for (int b = 0; b < count && m->value_node[c] < 0; ++b) {
      char name[kNameSize + 16];
      _snprintf_s(name, _TRUNCATE, "%s_pressed_value", bases[b]);
      m->value_node[c] = FindNode(*m, name);
    }
  }
  for (int i = 0; i < m->nodes; ++i) {
    for (int at = i; at >= 0; at = m->node[at].parent) {
      if (strstr(m->node[at].name, "_axes_touched_value") != nullptr) {
        m->node[i].hidden = true;
      }
      if (m->node[i].owner < 0) {
        for (int c = 0; c < art::kControls; ++c) {
          if (m->value_node[c] == at) m->node[i].owner = at;
        }
      }
    }
  }
  // The meshes: the triangles of each primitive of each node.
  int vertices = 0, triangles = 0, parts = 0;
  for (int i = 0; i < m->nodes; ++i) {
    const cgltf_mesh* mesh = data->nodes[i].mesh;
    if (mesh == nullptr || m->node[i].hidden) continue;
    for (cgltf_size p = 0; p < mesh->primitives_count; ++p) {
      const cgltf_primitive& prim = mesh->primitives[p];
      if (prim.type != cgltf_primitive_type_triangles) continue;
      const cgltf_accessor* position = nullptr;
      for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
        if (prim.attributes[a].type == cgltf_attribute_type_position) {
          position = prim.attributes[a].data;
        }
      }
      if (position == nullptr) continue;
      vertices += static_cast<int>(position->count);
      triangles += static_cast<int>(
          (prim.indices != nullptr ? prim.indices->count : position->count) / 3);
      ++parts;
    }
  }
  if (vertices <= 0 || vertices > kMostVertices) {
    _snprintf_s(why, why_size, _TRUNCATE, "%s has %d vertices, and the mod "
                "draws 1 to %d", path, vertices, kMostVertices);
    cgltf_free(data);
    FreeModel(m);
    return nullptr;
  }
  m->position = static_cast<float*>(malloc(sizeof(float) * 3 * vertices));
  m->normal = static_cast<float*>(malloc(sizeof(float) * 3 * vertices));
  m->index = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * 3 * triangles));
  m->triangle_part = static_cast<int*>(malloc(sizeof(int) * triangles));
  m->part = static_cast<Part*>(calloc(parts, sizeof(Part)));
  int v = 0, t = 0, pn = 0;
  for (int i = 0; i < m->nodes; ++i) {
    const cgltf_mesh* mesh = data->nodes[i].mesh;
    if (mesh == nullptr || m->node[i].hidden) continue;
    for (cgltf_size p = 0; p < mesh->primitives_count; ++p) {
      const cgltf_primitive& prim = mesh->primitives[p];
      if (prim.type != cgltf_primitive_type_triangles) continue;
      const cgltf_accessor* position = nullptr;
      const cgltf_accessor* normal = nullptr;
      for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
        if (prim.attributes[a].type == cgltf_attribute_type_position) {
          position = prim.attributes[a].data;
        } else if (prim.attributes[a].type == cgltf_attribute_type_normal) {
          normal = prim.attributes[a].data;
        }
      }
      if (position == nullptr) continue;
      Part& part = m->part[pn];
      part.node = i;
      part.first = v;
      part.count = static_cast<int>(position->count);
      for (int k = 0; k < part.count; ++k) {
        cgltf_accessor_read_float(position, k, &m->position[(v + k) * 3], 3);
        float* n = &m->normal[(v + k) * 3];
        if (normal == nullptr ||
            !cgltf_accessor_read_float(normal, k, n, 3)) {
          n[0] = 0.0f;
          n[1] = 1.0f;
          n[2] = 0.0f;
        }
      }
      const int count = static_cast<int>(
          (prim.indices != nullptr ? prim.indices->count : position->count) / 3);
      for (int k = 0; k < count; ++k) {
        for (int c = 0; c < 3; ++c) {
          const int at = k * 3 + c;
          const cgltf_size index = prim.indices != nullptr
                                       ? cgltf_accessor_read_index(prim.indices, at)
                                       : static_cast<cgltf_size>(at);
          m->index[(t + k) * 3 + c] =
              static_cast<uint32_t>(v) +
              static_cast<uint32_t>(index < static_cast<cgltf_size>(part.count)
                                        ? index
                                        : 0);
        }
        m->triangle_part[t + k] = pn;
      }
      v += part.count;
      t += count;
      ++pn;
    }
  }
  m->vertices = v;
  m->triangles = t;
  m->parts = pn;
  cgltf_free(data);
  Weld(m);

  // The rest pose: the box, and the anchor of each control.
  for (int s = 0; s < m->nodes; ++s) {
    Node& d = m->node[m->order[s]];
    hm::Matrix local = d.rest_matrix;
    if (d.parent >= 0) {
      hm::MatrixMultiply(m->node[d.parent].model, local, &d.model);
    } else {
      d.model = local;
    }
  }
  for (int k = 0; k < 3; ++k) {
    m->low[k] = 1e9f;
    m->high[k] = -1e9f;
  }
  float c_low[art::kControls][3], c_high[art::kControls][3];
  for (int c = 0; c < art::kControls; ++c) {
    for (int k = 0; k < 3; ++k) {
      c_low[c][k] = 1e9f;
      c_high[c][k] = -1e9f;
    }
  }
  for (int p = 0; p < m->parts; ++p) {
    const Part& part = m->part[p];
    const Node& d = m->node[part.node];
    for (int k = 0; k < part.count; ++k) {
      float g[3];
      hm::MatrixPoint(d.model, &m->position[(part.first + k) * 3], g);
      for (int a = 0; a < 3; ++a) {
        if (g[a] < m->low[a]) m->low[a] = g[a];
        if (g[a] > m->high[a]) m->high[a] = g[a];
      }
      for (int c = 0; c < art::kControls; ++c) {
        if (d.owner < 0 || d.owner != m->value_node[c]) continue;
        for (int a = 0; a < 3; ++a) {
          if (g[a] < c_low[c][a]) c_low[c][a] = g[a];
          if (g[a] > c_high[c][a]) c_high[c][a] = g[a];
        }
      }
    }
  }
  for (int c = 0; c < art::kControls; ++c) {
    m->lit_mesh[c] = m->value_node[c] >= 0 && c_low[c][0] <= c_high[c][0];
    if (m->lit_mesh[c]) {
      for (int a = 0; a < 3; ++a) {
        m->anchor[c][a] = 0.5f * (c_low[c][a] + c_high[c][a]);
        m->anchor_half[c][a] = 0.5f * (c_high[c][a] - c_low[c][a]);
      }
    } else if (m->value_node[c] >= 0) {
      const hm::Matrix& at = m->node[m->value_node[c]].model;
      for (int a = 0; a < 3; ++a) m->anchor[c][a] = at.m[9 + a];
    }
  }
  // The vertices in the grip space at rest, and the parts that move.
  m->grip_pos = static_cast<float*>(malloc(sizeof(float) * 3 * m->vertices));
  m->grip_nrm = static_cast<float*>(malloc(sizeof(float) * 3 * m->vertices));
  m->grip_shell =
      static_cast<float*>(malloc(sizeof(float) * 3 * m->vertices));
  m->part_moving = static_cast<bool*>(calloc(m->parts > 0 ? m->parts : 1, 1));
  if (m->grip_pos == nullptr || m->grip_nrm == nullptr ||
      m->grip_shell == nullptr || m->part_moving == nullptr) {
    _snprintf_s(why, why_size, _TRUNCATE, "no memory for %s", path);
    FreeModel(m);
    return nullptr;
  }
  for (int p = 0; p < m->parts; ++p) {
    const Part& part = m->part[p];
    const Node& d = m->node[part.node];
    for (int at = part.node; at >= 0; at = m->node[at].parent) {
      if (m->node[at].min_node >= 0 && m->node[at].input != kInNone) {
        m->part_moving[p] = true;
      }
    }
    for (int k = 0; k < part.count; ++k) {
      const int at = part.first + k;
      float* g = &m->grip_pos[at * 3];
      float* gn = &m->grip_nrm[at * 3];
      hm::MatrixPoint(d.model, &m->position[at * 3], g);
      const float* n = &m->normal[at * 3];
      for (int a = 0; a < 3; ++a) {
        gn[a] = d.model.m[a] * n[0] + d.model.m[3 + a] * n[1] +
                d.model.m[6 + a] * n[2];
      }
      overlay_math::Normalize(gn);
      for (int a = 0; a < 3; ++a) {
        m->grip_shell[at * 3 + a] = g[a] + gn[a] * kShellM;
      }
    }
  }
  SimplifyBody(m);
  return m;
}

#else  // BWVR_HAVE_CGLTF

Model* ReadModel(const char*, const char*, bool, char* why, size_t why_size) {
  _snprintf_s(why, why_size, _TRUNCATE, "this build has no glTF reader "
              "(third_party/cgltf was not there at the build)");
  return nullptr;
}

#endif  // BWVR_HAVE_CGLTF

// --- the load on a thread --------------------------------------------------------------

struct Load {
  char id[kNameSize];
  char path[MAX_PATH];
  bool left;
  Model* result;
  char why[320];
  double ms;
};

// For each side: the model in use, the folder that it must have, and a load
// that runs.
Model* g_model[2] = {nullptr, nullptr};
char g_wanted[2][kNameSize] = {"", ""};
char g_failed[2][kNameSize] = {"", ""};
Load g_load[2] = {};
HANDLE g_thread[2] = {nullptr, nullptr};
bool g_said_profile[2] = {false, false};
char g_said_id[2][kNameSize] = {"", ""};

DWORD WINAPI LoadThread(void* arg) {
  Load* load = static_cast<Load*>(arg);
  LARGE_INTEGER t0, t1, frequency;
  QueryPerformanceFrequency(&frequency);
  QueryPerformanceCounter(&t0);
  load->result = ReadModel(load->path, load->id, load->left, load->why,
                           sizeof(load->why));
  QueryPerformanceCounter(&t1);
  load->ms = (t1.QuadPart - t0.QuadPart) * 1000.0 /
             static_cast<double>(frequency.QuadPart);
  return 0;
}

const char* const kControlName[art::kControls] = {
    "first face button", "second face button", "stick click", "menu button",
    "trigger", "grip", "stick", "pad"};

// The arrays of the frame of a side for its new model (Grow, below).
void PrepareSide(int side);

void TakeLoad(int side) {
  if (g_thread[side] == nullptr) return;
  if (WaitForSingleObject(g_thread[side], 0) != WAIT_OBJECT_0) return;
  CloseHandle(g_thread[side]);
  g_thread[side] = nullptr;
  Load& load = g_load[side];
  const char* name = side == 0 ? "left" : "right";
  if (load.result == nullptr) {
    strcpy_s(g_failed[side], load.id);
    ModLog("hint model   : the model %s of the %s hand did not load: %s. The "
           "hints of that hand are on the card at the wrist", load.id, name,
           load.why);
    return;
  }
  FreeModel(g_model[side]);
  g_model[side] = load.result;
  load.result = nullptr;
  const Model& m = *g_model[side];
  // The arrays of the frame, now and not at the first hint.
  PrepareSide(side);
  char controls[256] = "";
  for (int c = 0; c < art::kControls; ++c) {
    if (m.value_node[c] < 0) continue;
    char one[64];
    _snprintf_s(one, _TRUNCATE, "%s%s%s", controls[0] != 0 ? ", " : "",
                kControlName[c], m.lit_mesh[c] ? "" : " (no mesh)");
    strncat_s(controls, one, _TRUNCATE);
  }
  ModLog("hint model   : the model %s of the %s hand loaded in %.1f ms on a "
         "thread: %d nodes, %d meshes, %d vertices (%d in the file), %d "
         "triangles, the body %d triangles (%d in the file, cells of %.1f "
         "mm%s), a box of "
         "%.1f by %.1f by %.1f cm. The controls with a node: %s", m.id, name,
         load.ms, m.nodes, m.parts, m.vertices, m.read_vertices,
         m.triangles, m.body_kept, m.body_read, m.body_cell * 1000.0f,
         m.body_cell > 0.0f && g_body_fine ? ", hint_body_fine"
                                            : "",
         (m.high[0] - m.low[0]) * 100.0f, (m.high[1] - m.low[1]) * 100.0f,
         (m.high[2] - m.low[2]) * 100.0f, controls);
}

// The folder of the model of a side now, or null for none.
const char* WantedId(int side) {
  if (g_test_model[0] != 0) return g_test_model;
  return hm::ModelOfProfile(ControllersProfileOf(side),
                            ControllersSystemName());
}

bool Ready(int side, bool any_style = false) {
  ReadSettings();
  if ((g_style != 2 && !any_style) || side < 0 || side > 1) return false;
  TakeLoad(side);
  const char* id = WantedId(side);
  if (!g_said_profile[side] ||
      strcmp(g_said_id[side], id != nullptr ? id : "") != 0) {
    g_said_profile[side] = true;
    strcpy_s(g_said_id[side], id != nullptr ? id : "");
    ModLog("hint model   : the %s hand reports the profile \"%s\" and the "
           "headset \"%s\", so the model is %s%s", side == 0 ? "left" : "right",
           ControllersProfileOf(side), ControllersSystemName(),
           id != nullptr ? id : "none (the card at the wrist)",
           g_test_model[0] != 0 ? " (hint_model_test)" : "");
  }
  if (id == nullptr) return false;
  if (g_model[side] != nullptr && strcmp(g_model[side]->id, id) == 0) {
    return true;
  }
  if (strcmp(g_failed[side], id) == 0 || g_thread[side] != nullptr) {
    return false;
  }
  // A new load. A folder with no file of the hand has none.glb for both.
  Load& load = g_load[side];
  memset(&load, 0, sizeof(load));
  strcpy_s(load.id, id);
  load.left = side == 0;
  _snprintf_s(load.path, _TRUNCATE, "%s%s\\%s.glb", g_dir, id,
              side == 0 ? "left" : "right");
  if (GetFileAttributesA(load.path) == INVALID_FILE_ATTRIBUTES) {
    _snprintf_s(load.path, _TRUNCATE, "%s%s\\none.glb", g_dir, id);
  }
  strcpy_s(g_wanted[side], id);
  g_thread[side] = CreateThread(nullptr, 0, LoadThread, &load, 0, nullptr);
  if (g_thread[side] == nullptr) {
    strcpy_s(g_failed[side], id);
    ModLog("hint model   : no thread for the load of the model %s", id);
  }
  return false;
}

// --- the cards and the fade of each side ----------------------------------------------

struct Side {
  // The cards of the last call of HintModelPlace.
  int count;
  HintModelCard card[kHintModelCards];
  // The inner edge of each card in the grip space, for the lines.
  float edge[kHintModelCards][3];
  bool show;
  float fade;
  // The side of the cards in the view, and right in the view.
  int stack_side;
  float last_right[3];
  // The order of the cards from the top in the last frame, and the controls
  // of the cards then.
  int column_count;
  int column[kHintModelCards];
  int column_control[kHintModelCards];
  uint64_t frames_shown;
  bool said_show;
  // The pulse of the lit buttons: the controls that are lit (a bit for each
  // control), and the time when that set began.
  unsigned pulse_controls;
  int64_t pulse_start;
};
Side g_side[2] = {};

// The grip space of a hand, from its grip pose in the LOCAL space.
void GripToLocal(const float position[3], const float orientation[4],
                 const float p[3], float out[3]) {
  float turned[3];
  controller_math::QuatRotate(orientation, p, turned);
  for (int k = 0; k < 3; ++k) out[k] = position[k] + turned[k];
}

void LocalToGrip(const float position[3], const float orientation[4],
                 const float p[3], float out[3]) {
  const float inverse[4] = {-orientation[0], -orientation[1], -orientation[2],
                            orientation[3]};
  const float d[3] = {p[0] - position[0], p[1] - position[1],
                      p[2] - position[2]};
  controller_math::QuatRotate(inverse, d, out);
}

// --- the frame of the overlay ---------------------------------------------------------

struct ScreenVertex {
  float x, y, z, rhw;
  uint32_t diffuse, specular;
  float u, v;
};
static_assert(sizeof(ScreenVertex) == 32, "the game's vertex is 32 bytes");
const DWORD kFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR |
                   D3DFVF_TEX1;

// THE LINES and the marks: a tube of 6 sides for each line, 2 mm across as
// a stroke of ink, a dot of ink where the line leaves its button, and a
// small gold diamond at a button with no mesh of its own (the grip of an
// Index).
const int kLineSides = 6;
const int kExtraVertices = kHintModelCards * (kLineSides * 2 + 12) + 8;
const int kExtraTriangles = kHintModelCards * (kLineSides * 2 + 16) + 8;
const float kLineRadiusM = 0.0010f;
const float kDotRadiusM = 0.0024f;
const float kMarkRadiusM = 0.006f;

// THE OUTLINE of the body: a dark ink edge at the silhouette, so the faint
// body reads on bright land. It is the shell of the back faces, pushed out
// along the normals by kShellM, behind the front faces of the body.
const float kOutlineAlpha = 0.85f;

// The colors. The lit buttons are a warm gold, the body is a warm
// parchment, and the lines and the outline are a dark brown ink.
const float kGold[3] = {1.00f, 0.76f, 0.28f};
const float kParchment[3] = {0.92f, 0.74f, 0.48f};
const float kInk[3] = {0.24f, 0.13f, 0.05f};

// THE MESH OF A SIDE in this frame. The model keeps each vertex in the grip
// space at rest (grip_pos, grip_nrm, grip_shell). A side keeps its own copy,
// and each frame changes only the vertices of the parts that a press moves.
// The grip space goes to the world with the pose of the hand:
//
//   world = position + rotation * (x, y, -z) * units
//
// so the frame needs no world position for each vertex. Each eye puts the
// vertices on its screen with one matrix (DrawSide).
struct Built {
  bool have;
  const Model* copied;  // the model of the copy in pos, nrm and shell
  int capacity;
  float* pos;    // the grip space, 3 floats a vertex
  float* nrm;
  float* shell;  // the corner of the outline, pushed out along the normal
  uint32_t* color;
  bool* lit;     // the vertex is in a lit part
  float rotation[9];
  float position[3];
  float units;
  // The lines and the marks, in the world.
  int extra_vertices;
  int extra_triangles;
  float extra_world[kExtraVertices * 3];
  uint32_t extra_color[kExtraVertices];
  uint32_t extra_index[kExtraTriangles * 3];
  // The cache of one eye: the screen vertex of each vertex, the eye that
  // made it, and the place of each vertex in each draw of that eye.
  ScreenVertex* screen;
  int* stamp;
  int* map_lit;
  int* map_back;
  int* map_front;
};
Built g_built[2] = {};
uint64_t g_build_frame = 0;
bool g_any_built = false;
// The controllers frame of the last build. A frame that did not build (the
// mouse owns the pointer, the intro, a flat frame) draws nothing.
uint64_t g_built_controllers_frame = 0;
int g_eye_stamp = 0;

// The draws of one eye: compact vertex lists, so a draw sends only the
// vertices that it uses.
ScreenVertex* g_draw_lit = nullptr;
ScreenVertex* g_draw_back = nullptr;
ScreenVertex* g_draw_shell = nullptr;
ScreenVertex* g_draw_front = nullptr;
WORD* g_list_lit = nullptr;
WORD* g_list_back = nullptr;
WORD* g_list_front = nullptr;
int g_draw_capacity = 0;
int g_list_capacity = 0;

// THE COST, on the CPU: the build of a frame and the draws of its eyes.
LARGE_INTEGER g_frequency = {};
int64_t g_build_ticks = 0;
int64_t g_draw_ticks = 0;
int64_t g_cost_sum = 0;
int64_t g_cost_most = 0;
uint64_t g_cost_frames = 0;
uint64_t g_cost_triangles = 0;
uint64_t g_cost_sent = 0;   // the vertices that the draws sent
int g_frame_triangles = 0;
int g_frame_sent = 0;
int g_frame_draws = 0;      // the draws of the eyes of the last build
uint64_t g_cost_eyes = 0;   // the draws of the eyes in the window
float g_units = 0.0f;       // world units in a meter of the room, this frame
bool g_said_cost = false;
bool g_said_draw = false;
bool g_said_failed = false;
uint64_t g_draws = 0;

int64_t Now() {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return t.QuadPart;
}

double Ms(int64_t ticks) {
  if (g_frequency.QuadPart == 0) QueryPerformanceFrequency(&g_frequency);
  return static_cast<double>(ticks) * 1000.0 /
         static_cast<double>(g_frequency.QuadPart);
}

uint32_t Argb(const float rgb[3], float level, float alpha) {
  uint32_t out = 0;
  float a = alpha * 255.0f;
  if (!(a > 0.0f)) a = 0.0f;
  if (a > 255.0f) a = 255.0f;
  out |= static_cast<uint32_t>(a + 0.5f) << 24;
  for (int k = 0; k < 3; ++k) {
    float v = rgb[k] * level * 255.0f;
    if (!(v > 0.0f)) v = 0.0f;
    if (v > 255.0f) v = 255.0f;
    out |= static_cast<uint32_t>(v + 0.5f) << (16 - 8 * k);
  }
  return out;
}

template <typename T>
bool GrowArray(T** p, int count) {
  free(*p);
  *p = static_cast<T*>(malloc(sizeof(T) * (count > 0 ? count : 1)));
  return *p != nullptr;
}

// The arrays of a side and of the draws for a model. The load calls it on
// the frame thread when it takes the model, so no hint frame allocates.
bool Grow(Built* b, const Model& m) {
  if (b->capacity < m.vertices) {
    bool ok = GrowArray(&b->pos, m.vertices * 3) &&
              GrowArray(&b->nrm, m.vertices * 3) &&
              GrowArray(&b->shell, m.vertices * 3) &&
              GrowArray(&b->color, m.vertices) &&
              GrowArray(&b->lit, m.vertices) &&
              GrowArray(&b->screen, m.vertices) &&
              GrowArray(&b->stamp, m.vertices) &&
              GrowArray(&b->map_lit, m.vertices) &&
              GrowArray(&b->map_back, m.vertices) &&
              GrowArray(&b->map_front, m.vertices);
    b->capacity = ok ? m.vertices : 0;
    b->copied = nullptr;
    if (!ok) return false;
    for (int v = 0; v < m.vertices; ++v) b->stamp[v] = -1;
  }
  const int vertices = m.vertices + kExtraVertices;
  const int indices = m.triangles * 3 + kExtraTriangles * 3;
  if (vertices > g_draw_capacity) {
    const bool ok = GrowArray(&g_draw_lit, vertices) &&
                    GrowArray(&g_draw_back, vertices) &&
                    GrowArray(&g_draw_shell, vertices) &&
                    GrowArray(&g_draw_front, vertices);
    g_draw_capacity = ok ? vertices : 0;
    if (!ok) return false;
  }
  if (indices > g_list_capacity) {
    const bool ok = GrowArray(&g_list_lit, indices) &&
                    GrowArray(&g_list_back, indices) &&
                    GrowArray(&g_list_front, indices);
    g_list_capacity = ok ? indices : 0;
    if (!ok) return false;
  }
  if (b->copied != &m) {
    memcpy(b->pos, m.grip_pos, sizeof(float) * 3 * m.vertices);
    memcpy(b->nrm, m.grip_nrm, sizeof(float) * 3 * m.vertices);
    memcpy(b->shell, m.grip_shell, sizeof(float) * 3 * m.vertices);
    b->copied = &m;
  }
  return true;
}

void PrepareSide(int side) {
  if (g_model[side] == nullptr) return;
  g_built[side].copied = nullptr;
  Grow(&g_built[side], *g_model[side]);
}

float InputValue(const bwvr::HandInput& in, int input) {
  switch (input) {
    case kInTrigger: return in.trigger;
    case kInSqueeze: return in.squeeze;
    case kInStickClick: return in.stick_click ? 1.0f : 0.0f;
    case kInPrimary: return in.primary ? 1.0f : 0.0f;
    case kInSecondary: return in.secondary ? 1.0f : 0.0f;
    case kInMenu: return in.menu ? 1.0f : 0.0f;
    case kInStickX: return in.stick[0];
    case kInStickY: return in.stick[1];
    default: return 0.0f;
  }
}

// A point of the grip space in the world (THE PLACE above).
void GripToWorld(const OverlayHand& hand, float units, const float p[3],
                 float out[3]) {
  const float q[3] = {p[0] * units, p[1] * units, -p[2] * units};
  for (int r = 0; r < 3; ++r) {
    out[r] = hand.position[r] + hand.rotation[r * 3 + 0] * q[0] +
             hand.rotation[r * 3 + 1] * q[1] + hand.rotation[r * 3 + 2] * q[2];
  }
}

// The extras: a tube from a to b, and a diamond at a point, in the world.
void AddTube(Built* b, const float a[3], const float c[3], float radius,
             uint32_t color) {
  float axis[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
  const float length = sqrtf(axis[0] * axis[0] + axis[1] * axis[1] +
                             axis[2] * axis[2]);
  if (!(length > 1e-6f)) return;
  if (b->extra_vertices + kLineSides * 2 > kExtraVertices ||
      b->extra_triangles + kLineSides * 2 > kExtraTriangles) {
    return;
  }
  for (int k = 0; k < 3; ++k) axis[k] /= length;
  float side[3] = {axis[2], 0.0f, -axis[0]};
  if (fabsf(axis[1]) > 0.9f) {
    side[0] = 1.0f;
    side[1] = 0.0f;
    side[2] = 0.0f;
  }
  overlay_math::Normalize(side);
  const float other[3] = {axis[1] * side[2] - axis[2] * side[1],
                          axis[2] * side[0] - axis[0] * side[2],
                          axis[0] * side[1] - axis[1] * side[0]};
  const int first = b->extra_vertices;
  for (int e = 0; e < 2; ++e) {
    const float* end = e == 0 ? a : c;
    for (int s = 0; s < kLineSides; ++s) {
      const float angle = 6.2831853f * static_cast<float>(s) / kLineSides;
      const float cs = cosf(angle), sn = sinf(angle);
      const int v = first + e * kLineSides + s;
      for (int k = 0; k < 3; ++k) {
        b->extra_world[v * 3 + k] = end[k] + (side[k] * cs + other[k] * sn) *
                                                 radius;
      }
      b->extra_color[v] = color;
    }
  }
  for (int s = 0; s < kLineSides; ++s) {
    const uint32_t i0 = first + s, i1 = first + (s + 1) % kLineSides;
    const uint32_t j0 = i0 + kLineSides, j1 = i1 + kLineSides;
    uint32_t* t = &b->extra_index[b->extra_triangles * 3];
    t[0] = i0; t[1] = i1; t[2] = j1;
    t[3] = i0; t[4] = j1; t[5] = j0;
    b->extra_triangles += 2;
  }
  b->extra_vertices += kLineSides * 2;
}

void AddDiamond(Built* b, const float at[3], float radius, uint32_t color) {
  if (b->extra_vertices + 6 > kExtraVertices ||
      b->extra_triangles + 8 > kExtraTriangles) {
    return;
  }
  const int first = b->extra_vertices;
  const float dir[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
                           {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
  for (int i = 0; i < 6; ++i) {
    for (int k = 0; k < 3; ++k) {
      b->extra_world[(first + i) * 3 + k] = at[k] + dir[i][k] * radius;
    }
    b->extra_color[first + i] = color;
  }
  const int face[8][3] = {{0, 2, 4}, {2, 1, 4}, {1, 3, 4}, {3, 0, 4},
                          {2, 0, 5}, {1, 2, 5}, {3, 1, 5}, {0, 3, 5}};
  for (int f = 0; f < 8; ++f) {
    uint32_t* t = &b->extra_index[b->extra_triangles * 3];
    for (int k = 0; k < 3; ++k) t[k] = static_cast<uint32_t>(first + face[f][k]);
    ++b->extra_triangles;
  }
  b->extra_vertices += 6;
}

// The colors of a frame: 64 steps of the light for the gold and for the
// parchment, so each vertex is one table read.
const int kLevels = 64;

// The meshes of one side for this frame.
void BuildSide(int side, const OverlayHand& real, const float stop[3],
               float units) {
  OverlayHand hand = real;
  for (int k = 0; k < 3; ++k) hand.position[k] += stop[k];
  Built& b = g_built[side];
  b.have = false;
  Side& s = g_side[side];
  Model* m = g_model[side];
  if (m == nullptr || !real.valid || !(s.fade > 0.0f) || !(units > 0.0f)) {
    return;
  }
  if (!Grow(&b, *m)) return;
  for (int k = 0; k < 9; ++k) b.rotation[k] = hand.rotation[k];
  for (int k = 0; k < 3; ++k) b.position[k] = hand.position[k];
  b.units = units;
  bwvr::HandInput input;
  memset(&input, 0, sizeof(input));
  ControllersHandInput(side, &input);
  // The nodes, with the press. Only the parts under a node of a press move,
  // so only their vertices change.
  for (int o = 0; o < m->nodes; ++o) {
    Node& d = m->node[m->order[o]];
    hm::Matrix local = d.rest_matrix;
    if (d.min_node >= 0 && d.input != kInNone) {
      hm::Trs trs;
      hm::LerpTrs(m->node[d.min_node].rest, m->node[d.max_node].rest,
                  hm::ResponseWeight(d.axis, InputValue(input, d.input)),
                  &trs);
      hm::MatrixOfTrs(trs, &local);
    }
    if (d.parent >= 0) {
      hm::MatrixMultiply(m->node[d.parent].model, local, &d.model);
    } else {
      d.model = local;
    }
  }
  // The lit nodes: the value node of each control of a card.
  int lit_owner[kHintModelCards];
  int lit_count = 0;
  for (int c = 0; c < s.count; ++c) {
    const int control = s.card[c].control;
    if (control < 0 || control >= art::kControls) continue;
    if (m->value_node[control] >= 0) lit_owner[lit_count++] = m->value_node[control];
  }
  const float fade = s.fade;
  // THE PULSE. A new set of lit buttons starts its beat at full light. The
  // time is read once in a frame, so both eyes have one light.
  unsigned controls = 0;
  for (int c = 0; c < s.count; ++c) {
    const int control = s.card[c].control;
    if (control >= 0 && control < art::kControls) controls |= 1u << control;
  }
  const int64_t now = Now();
  if (controls != s.pulse_controls || s.pulse_start == 0) {
    s.pulse_controls = controls;
    s.pulse_start = now;
  }
  const float pulse =
      g_pulse ? hm::PulseLevel(static_cast<double>(now - s.pulse_start) /
                               static_cast<double>(g_frequency.QuadPart))
              : 1.0f;
  uint32_t gold[kLevels + 1], parchment[kLevels + 1];
  for (int l = 0; l <= kLevels; ++l) {
    const float up = static_cast<float>(l) / kLevels;
    gold[l] = Argb(kGold, (0.55f + 0.45f * up) * pulse, fade);
    parchment[l] = Argb(kParchment, 0.50f + 0.50f * up, hm::kBodyAlpha * fade);
  }
  // The y of a normal in the world: the second row of the rotation, with
  // the flip of z.
  const float ry[3] = {hand.rotation[3], hand.rotation[4], -hand.rotation[5]};
  for (int p = 0; p < m->parts; ++p) {
    const Part& part = m->part[p];
    const Node& d = m->node[part.node];
    bool lit = false;
    for (int l = 0; l < lit_count; ++l) {
      if (d.owner >= 0 && d.owner == lit_owner[l]) lit = true;
    }
    if (m->part_moving[p]) {
      for (int k = 0; k < part.count; ++k) {
        const int v = part.first + k;
        float* g = &b.pos[v * 3];
        float* gn = &b.nrm[v * 3];
        hm::MatrixPoint(d.model, &m->position[v * 3], g);
        const float* n = &m->normal[v * 3];
        for (int a = 0; a < 3; ++a) {
          gn[a] = d.model.m[a] * n[0] + d.model.m[3 + a] * n[1] +
                  d.model.m[6 + a] * n[2];
        }
        overlay_math::Normalize(gn);
        for (int a = 0; a < 3; ++a) {
          b.shell[v * 3 + a] = g[a] + gn[a] * kShellM;
        }
      }
    }
    const uint32_t* table = lit ? gold : parchment;
    for (int k = 0; k < part.count; ++k) {
      const int v = part.first + k;
      const float* gn = &b.nrm[v * 3];
      float up = ry[0] * gn[0] + ry[1] * gn[1] + ry[2] * gn[2];
      if (!(up > 0.0f)) up = 0.0f;
      if (up > 1.0f) up = 1.0f;
      b.color[v] = table[static_cast<int>(up * kLevels + 0.5f)];
      b.lit[v] = lit;
    }
  }
  // The lines and the marks.
  b.extra_vertices = 0;
  b.extra_triangles = 0;
  const uint32_t ink = Argb(kInk, 1.0f, fade);
  const float line_radius = kLineRadiusM * units;
  for (int c = 0; c < s.count; ++c) {
    const int control = s.card[c].control;
    if (control < 0 || control >= art::kControls ||
        m->value_node[control] < 0) {
      continue;
    }
    // The anchor moves with the press of its node.
    float anchor[3];
    const Node& value = m->node[m->value_node[control]];
    if (m->lit_mesh[control]) {
      // The anchor at rest, moved as the value node moved:
      //
      //   anchor = model * inverse(rest_model) * anchor at rest
      //
      // rest_model is the parent now with the rest transform of the node.
      hm::Matrix rest_model;
      if (value.parent >= 0) {
        hm::MatrixMultiply(m->node[value.parent].model, value.rest_matrix,
                           &rest_model);
      } else {
        rest_model = value.rest_matrix;
      }
      const float* a = rest_model.m;
      const float d[3] = {m->anchor[control][0] - a[9],
                          m->anchor[control][1] - a[10],
                          m->anchor[control][2] - a[11]};
      const float c00 = a[4] * a[8] - a[7] * a[5];
      const float c01 = a[7] * a[2] - a[1] * a[8];
      const float c02 = a[1] * a[5] - a[4] * a[2];
      const float det = a[0] * c00 + a[3] * c01 + a[6] * c02;
      if (fabsf(det) > 1e-12f) {
        const float inv[9] = {
            c00 / det, c01 / det, c02 / det,
            (a[6] * a[5] - a[3] * a[8]) / det,
            (a[0] * a[8] - a[6] * a[2]) / det,
            (a[3] * a[2] - a[0] * a[5]) / det,
            (a[3] * a[7] - a[6] * a[4]) / det,
            (a[6] * a[1] - a[0] * a[7]) / det,
            (a[0] * a[4] - a[3] * a[1]) / det};
        float x[3];
        for (int k = 0; k < 3; ++k) {
          x[k] = inv[k] * d[0] + inv[3 + k] * d[1] + inv[6 + k] * d[2];
        }
        hm::MatrixPoint(value.model, x, anchor);
      } else {
        for (int k = 0; k < 3; ++k) anchor[k] = m->anchor[control][k];
      }
      // A lit part hides the middle of its box, so the line starts where it
      // leaves the box, toward its card, and a dot of ink marks that place.
      float dir[3] = {s.edge[c][0] - anchor[0], s.edge[c][1] - anchor[1],
                      s.edge[c][2] - anchor[2]};
      overlay_math::Normalize(dir);
      float t = 1e9f;
      for (int k = 0; k < 3; ++k) {
        if (fabsf(dir[k]) > 1e-6f) {
          const float tk = m->anchor_half[control][k] / fabsf(dir[k]);
          if (tk < t) t = tk;
        }
      }
      if (t > 0.05f) t = 0.0f;
      for (int k = 0; k < 3; ++k) anchor[k] += dir[k] * t;
    } else {
      for (int k = 0; k < 3; ++k) anchor[k] = value.model.m[9 + k];
    }
    float from[3], to[3];
    GripToWorld(hand, units, anchor, from);
    GripToWorld(real, units, s.edge[c], to);
    AddTube(&b, from, to, line_radius, ink);
    if (m->lit_mesh[control]) {
      AddDiamond(&b, from, kDotRadiusM * units, ink);
    } else {
      AddDiamond(&b, from, kMarkRadiusM * units, Argb(kGold, pulse, fade));
    }
  }
  b.have = true;
}

// --- the draw ---------------------------------------------------------------------------

struct RenderSet {
  D3DRENDERSTATETYPE state;
  DWORD value;
};
const RenderSet kRender[] = {
    {D3DRENDERSTATE_ZENABLE, D3DZB_TRUE},
    {D3DRENDERSTATE_ZWRITEENABLE, TRUE},
    {D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL},
    {D3DRENDERSTATE_ALPHABLENDENABLE, TRUE},
    {D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA},
    {D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA},
    {D3DRENDERSTATE_ALPHATESTENABLE, FALSE},
    {D3DRENDERSTATE_FOGENABLE, FALSE},
    {D3DRENDERSTATE_CULLMODE, D3DCULL_NONE},
    {D3DRENDERSTATE_SHADEMODE, D3DSHADE_GOURAUD},
    {D3DRENDERSTATE_FILLMODE, D3DFILL_SOLID},
    {D3DRENDERSTATE_STENCILENABLE, FALSE},
    {D3DRENDERSTATE_COLORKEYENABLE, FALSE},
};
const int kRenderCount = sizeof(kRender) / sizeof(kRender[0]);
struct StageSet {
  DWORD stage;
  D3DTEXTURESTAGESTATETYPE state;
  DWORD value;
};
const StageSet kStage[] = {
    {0, D3DTSS_COLOROP, D3DTOP_SELECTARG1},
    {0, D3DTSS_COLORARG1, D3DTA_DIFFUSE},
    {0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1},
    {0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE},
    {1, D3DTSS_COLOROP, D3DTOP_DISABLE},
};
const int kStageCount = sizeof(kStage) / sizeof(kStage[0]);
DWORD g_render_was[kRenderCount];
bool g_render_changed[kRenderCount];
DWORD g_stage_was[kStageCount];
bool g_stage_changed[kStageCount];

// Each state is read before it changes, and the value that was read goes
// back after the draw, in the reverse order, as overlay.cpp does it.
void SetStates(IDirect3DDevice7* device) {
  for (int i = 0; i < kRenderCount; ++i) {
    g_render_changed[i] = false;
    DWORD was = 0;
    if (FAILED(device->GetRenderState(kRender[i].state, &was))) continue;
    DWORD want = kRender[i].value;
    if (kRender[i].state == D3DRENDERSTATE_ZENABLE && was != D3DZB_FALSE) {
      want = was;
    }
    if (was == want) continue;
    if (SUCCEEDED(device->SetRenderState(kRender[i].state, want))) {
      g_render_was[i] = was;
      g_render_changed[i] = true;
    }
  }
  for (int i = 0; i < kStageCount; ++i) {
    g_stage_changed[i] = false;
    DWORD was = 0;
    if (FAILED(device->GetTextureStageState(kStage[i].stage, kStage[i].state,
                                            &was))) {
      continue;
    }
    if (was == kStage[i].value) continue;
    if (SUCCEEDED(device->SetTextureStageState(kStage[i].stage,
                                               kStage[i].state,
                                               kStage[i].value))) {
      g_stage_was[i] = was;
      g_stage_changed[i] = true;
    }
  }
}

void PutStatesBack(IDirect3DDevice7* device) {
  for (int i = kStageCount - 1; i >= 0; --i) {
    if (!g_stage_changed[i]) continue;
    device->SetTextureStageState(kStage[i].stage, kStage[i].state,
                                 g_stage_was[i]);
    g_stage_changed[i] = false;
  }
  for (int i = kRenderCount - 1; i >= 0; --i) {
    if (!g_render_changed[i]) continue;
    device->SetRenderState(kRender[i].state, g_render_was[i]);
    g_render_changed[i] = false;
  }
}

// The eye in the world, from the camera matrix of overlay_math::ToCamera.
bool EyeOf(const float m[12], float eye[3]) {
  // camera = A * p + t, with the columns of A at m[0..2], m[3..5], m[6..8].
  const float a00 = m[0], a01 = m[3], a02 = m[6];
  const float a10 = m[1], a11 = m[4], a12 = m[7];
  const float a20 = m[2], a21 = m[5], a22 = m[8];
  const float det = a00 * (a11 * a22 - a12 * a21) -
                    a01 * (a10 * a22 - a12 * a20) +
                    a02 * (a10 * a21 - a11 * a20);
  if (!(fabsf(det) > 1e-12f)) return false;
  const float inv[9] = {(a11 * a22 - a12 * a21) / det,
                        (a02 * a21 - a01 * a22) / det,
                        (a01 * a12 - a02 * a11) / det,
                        (a12 * a20 - a10 * a22) / det,
                        (a00 * a22 - a02 * a20) / det,
                        (a02 * a10 - a00 * a12) / det,
                        (a10 * a21 - a11 * a20) / det,
                        (a01 * a20 - a00 * a21) / det,
                        (a00 * a11 - a01 * a10) / det};
  const float t[3] = {m[9], m[10], m[11]};
  for (int r = 0; r < 3; ++r) {
    eye[r] = -(inv[r * 3 + 0] * t[0] + inv[r * 3 + 1] * t[1] +
               inv[r * 3 + 2] * t[2]);
  }
  return true;
}

// One corner on the screen: camera = c * p + c0, then the projection of the
// overlay (overlay_math.h, Project). False in front of the near plane.
inline bool ProjectCorner(const float c[9], const float c0[3], const float p[3],
                          float near_plane, float hx, float hy,
                          uint32_t color, ScreenVertex* out) {
  const float z = c[6] * p[0] + c[7] * p[1] + c[8] * p[2] + c0[2];
  if (z < near_plane) return false;
  const float x = c[0] * p[0] + c[1] * p[1] + c[2] * p[2] + c0[0];
  const float y = c[3] * p[0] + c[4] * p[1] + c[5] * p[2] + c0[1];
  const float inv = 1.0f / z;
  out->x = (1.0f + x * inv) * hx;
  out->y = (1.0f - y * inv) * hy;
  out->rhw = near_plane * inv;
  out->z = 1.0f - out->rhw;
  out->diffuse = color;
  out->specular = 0xFF000000u;
  out->u = out->v = 0.0f;
  return true;
}

// THE DRAW OF ONE SIDE in one eye. The grip space goes to the camera with
// one matrix: camera = A * (position + R * F * p * units) + t, where A and t
// are the camera of the eye and F flips z. Each corner is put on the screen
// once, and the triangles go to four compact lists, so a draw sends only the
// vertices it uses. The side that the eye sees comes from the first normal
// of a triangle and the eye in the grip space.
int DrawSide(IDirect3DDevice7* device, int side, const float m[12],
             float near_plane, float hx, float hy, const float eye[3],
             HRESULT* result) {
  Built& b = g_built[side];
  const Model* model = g_model[side];
  if (!b.have || model == nullptr || b.copied != model) return 0;
  // A, as rows, from the columns of m.
  const float a[9] = {m[0], m[3], m[6], m[1], m[4], m[7], m[2], m[5], m[8]};
  float r[9];  // R * F * units
  for (int i = 0; i < 3; ++i) {
    r[i * 3 + 0] = b.rotation[i * 3 + 0] * b.units;
    r[i * 3 + 1] = b.rotation[i * 3 + 1] * b.units;
    r[i * 3 + 2] = -b.rotation[i * 3 + 2] * b.units;
  }
  float c[9], c0[3];
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      c[i * 3 + j] = a[i * 3 + 0] * r[0 * 3 + j] + a[i * 3 + 1] * r[1 * 3 + j] +
                     a[i * 3 + 2] * r[2 * 3 + j];
    }
    c0[i] = a[i * 3 + 0] * b.position[0] + a[i * 3 + 1] * b.position[1] +
            a[i * 3 + 2] * b.position[2] + m[9 + i];
  }
  // The eye in the grip space: F * R^T * (eye - position) / units.
  const float d[3] = {eye[0] - b.position[0], eye[1] - b.position[1],
                      eye[2] - b.position[2]};
  float e[3];
  for (int j = 0; j < 3; ++j) {
    e[j] = (b.rotation[0 * 3 + j] * d[0] + b.rotation[1 * 3 + j] * d[1] +
            b.rotation[2 * 3 + j] * d[2]) /
           b.units;
  }
  e[2] = -e[2];
  const uint32_t outline =
      Argb(kInk, 1.0f, kOutlineAlpha * g_side[side].fade);
  const int stamp = ++g_eye_stamp;
  int n_lit = 0, n_back = 0, n_front = 0;
  int i_lit = 0, i_back = 0, i_front = 0;
  for (int t = 0; t < model->triangles; ++t) {
    const uint32_t* tri = &model->index[t * 3];
    bool seen = true;
    for (int k = 0; k < 3 && seen; ++k) {
      const uint32_t v = tri[k];
      if (b.stamp[v] != stamp) {
        b.stamp[v] = stamp;
        b.map_lit[v] = b.map_back[v] = b.map_front[v] = -1;
        if (!ProjectCorner(c, c0, &b.pos[v * 3], near_plane, hx, hy,
                           b.color[v], &b.screen[v])) {
          b.screen[v].rhw = -1.0f;
        }
      }
      if (b.screen[v].rhw < 0.0f) seen = false;
    }
    if (!seen) continue;
    if (b.lit[tri[0]]) {
      for (int k = 0; k < 3; ++k) {
        const uint32_t v = tri[k];
        if (b.map_lit[v] < 0) {
          b.map_lit[v] = n_lit;
          g_draw_lit[n_lit++] = b.screen[v];
        }
        g_list_lit[i_lit++] = static_cast<WORD>(b.map_lit[v]);
      }
      continue;
    }
    const float* p0 = &b.pos[tri[0] * 3];
    const float* n0 = &b.nrm[tri[0] * 3];
    const bool front = n0[0] * (e[0] - p0[0]) + n0[1] * (e[1] - p0[1]) +
                           n0[2] * (e[2] - p0[2]) >
                       0.0f;
    if (front) {
      for (int k = 0; k < 3; ++k) {
        const uint32_t v = tri[k];
        if (b.map_front[v] < 0) {
          b.map_front[v] = n_front;
          g_draw_front[n_front++] = b.screen[v];
        }
        g_list_front[i_front++] = static_cast<WORD>(b.map_front[v]);
      }
      continue;
    }
    // A back face, and its corner of the outline. A corner of the outline in
    // front of the near plane takes the corner of the body.
    for (int k = 0; k < 3; ++k) {
      const uint32_t v = tri[k];
      if (b.map_back[v] < 0) {
        b.map_back[v] = n_back;
        if (!ProjectCorner(c, c0, &b.shell[v * 3], near_plane, hx, hy,
                           outline, &g_draw_shell[n_back])) {
          g_draw_shell[n_back] = b.screen[v];
          g_draw_shell[n_back].diffuse = outline;
        }
        ++n_back;
      }
      g_list_back[i_back++] = static_cast<WORD>(b.map_back[v]);
    }
  }
  // The lines and the marks, from the world, after the lit parts.
  const int extra_base = n_lit;
  bool extra_ok = true;
  for (int v = 0; v < b.extra_vertices; ++v) {
    float cam[3];
    overlay_math::ToCamera(m, &b.extra_world[v * 3], cam);
    ScreenVertex& s = g_draw_lit[n_lit + v];
    if (cam[2] < near_plane) {
      extra_ok = false;
      break;
    }
    s.x = (1.0f + cam[0] / cam[2]) * hx;
    s.y = (1.0f - cam[1] / cam[2]) * hy;
    s.rhw = near_plane / cam[2];
    s.z = 1.0f - s.rhw;
    s.diffuse = b.extra_color[v];
    s.specular = 0xFF000000u;
    s.u = s.v = 0.0f;
  }
  if (extra_ok && b.extra_vertices > 0) {
    n_lit += b.extra_vertices;
    for (int k = 0; k < b.extra_triangles * 3; ++k) {
      g_list_lit[i_lit++] = static_cast<WORD>(extra_base + b.extra_index[k]);
    }
  }
  if (n_lit > 65535 || n_back > 65535 || n_front > 65535) return 0;

  auto draw = [&](const ScreenVertex* vertices, int count, const WORD* list,
                  int indices) {
    if (count <= 0 || indices <= 0) return;
    const HRESULT hr = device->DrawIndexedPrimitive(
        D3DPT_TRIANGLELIST, kFvf, const_cast<ScreenVertex*>(vertices),
        static_cast<DWORD>(count), const_cast<WORD*>(list),
        static_cast<DWORD>(indices), 0);
    if (FAILED(hr)) *result = hr;
    g_frame_sent += count;
  };
  // THREE DRAWS. The lit parts and the lines, with the depth write. The
  // front faces of the body, faint, with the depth write, so the outline
  // does not show inside the silhouette. The outline: the shell of the back
  // faces in ink, behind the front faces, so only a thin edge round the body
  // shows. The back faces of the body do not draw: under the outline and the
  // front faces they add little.
  device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, TRUE);
  draw(g_draw_lit, n_lit, g_list_lit, i_lit);
  draw(g_draw_front, n_front, g_list_front, i_front);
  device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, FALSE);
  draw(g_draw_shell, n_back, g_list_back, i_back);
  return (i_lit + i_back + i_front) / 3;
}

// THE TEST KEY: Ctrl Shift H, with test_keys=1, while the game is in front.
void PollKey() {
  static bool was = false;
  if (!g_test_keys) return;
  const bool down = (GetAsyncKeyState('H') & 0x8000) != 0 &&
                    (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 &&
                    (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
  const bool pressed = down && !was;
  was = down;
  if (!pressed) return;
  DWORD process = 0;
  const HWND window = GetForegroundWindow();
  if (window != nullptr) GetWindowThreadProcessId(window, &process);
  if (process != GetCurrentProcessId()) return;
  g_style = g_style == 2 ? 1 : 2;
  ModLog("hint model   : Ctrl Shift H: hint_style=%d, %s", g_style,
         g_style == 2 ? "the controller in the hand with a card for each lit "
                        "button"
                      : "the cards at the wrists");
}

}  // namespace

int HintModelStyle() {
  ReadSettings();
  return g_style;
}

bool HintModelReady(int side) { return Ready(side); }

bool HintModelHasControl(int side, int control) {
  if (side < 0 || side > 1 || control < 0 || control >= art::kControls) {
    return false;
  }
  const Model* m = g_model[side];
  return m != nullptr && m->value_node[control] >= 0;
}

// The palm corners of hint_card_math::HandOutline.
bool HintModelPanelMesh(int side, HintModelMesh* out, bool* none) {
  *none = false;
  if (side < 0 || side > 1) {
    *none = true;
    return false;
  }
  if (!Ready(side, true)) {
    ReadSettings();
    const char* id = WantedId(side);
    *none = id == nullptr || strcmp(g_failed[side], id) == 0;
    return false;
  }
  Model* m = g_model[side];
  if (m->triangle_control == nullptr) {
    m->triangle_control = static_cast<signed char*>(
        malloc(m->triangles > 0 ? m->triangles : 1));
    if (m->triangle_control == nullptr) {
      *none = true;
      return false;
    }
    // The stick and its click have one node: the triangles are the stick's.
    static const int kOrder[] = {art::kTrigger, art::kGrip,   art::kStick,
                                 art::kFirst,   art::kSecond, art::kMenu,
                                 art::kPad,     art::kStickClick};
    for (int t = 0; t < m->triangles; ++t) {
      const int owner = m->node[m->part[m->triangle_part[t]].node].owner;
      signed char c = -1;
      for (int k = 0; k < art::kControls && c < 0 && owner >= 0; ++k) {
        if (m->value_node[kOrder[k]] == owner) {
          c = static_cast<signed char>(kOrder[k]);
        }
      }
      m->triangle_control[t] = c;
    }
  }
  memset(out, 0, sizeof(*out));
  out->id = m->id;
  out->vertices = m->vertices;
  out->position = m->grip_pos;
  out->normal = m->grip_nrm;
  out->triangles = m->triangles;
  out->index = m->index;
  out->control = m->triangle_control;
  for (int c = 0; c < art::kControls && c < 8; ++c) {
    out->has[c] = m->value_node[c] >= 0;
    for (int k = 0; k < 3; ++k) {
      out->anchor[c][k] = m->anchor[c][k];
      out->half[c][k] = m->anchor_half[c][k];
    }
  }
  for (int k = 0; k < 3; ++k) {
    out->low[k] = m->low[k];
    out->high[k] = m->high[k];
  }
  return true;
}

const int kPalmCorners = 8;

bool HintModelPlace(int which, const HintModelCard* cards, int count,
                    bool show, float dt, HintModelPlaced* out) {
  float grip[3], turn[4], head[3];
  bool left = false;
  if (!ControllersCardPose(which, grip, turn, head, &left)) return false;
  const int side = left ? 0 : 1;
  Side& s = g_side[side];
  const Model* m = g_model[side];
  if (m == nullptr) {
    s.fade = 0.0f;
    s.show = false;
    return false;
  }
  if (count > kHintModelCards) count = kHintModelCards;
  // The cards of a hint that ends stay while they fade out.
  if (count > 0) {
    s.count = count;
    for (int i = 0; i < count; ++i) s.card[i] = cards[i];
  }
  s.show = show && count > 0;
  s.fade = hint_card_math::FadeStep(
      s.fade, s.show, dt, s.show ? hm::kModelFadeIn : hm::kModelFadeOut);
  // A controller that faded out starts its next pulse at full light.
  if (!(s.fade > 0.0f)) s.pulse_start = 0;
  if (s.show) {
    ++s.frames_shown;
    if (!s.said_show) {
      s.said_show = true;
      ModLog("hint model   : the controller %s shows in the %s hand, with %d "
             "cards", m->id, left ? "left" : "right", count);
    }
  }
  if (s.count <= 0) return false;
  // The outline: the corners of the palm (HandOutline gives them first),
  // and the box of the controller. A hand that holds a controller is
  // closed, so the tips of the open fingers do not count.
  const float hand_length = ControllersHandCm() * 0.01f *
                            (ControllersHandScalePct() * 0.01f);
  const float hand_m = hand_length > 0.05f ? hand_length : 0.19f;
  float outline[hint_card_math::kOutlinePoints + 8][3];
  hint_card_math::HandOutline(grip, turn, left, hand_m, outline);
  for (int c = 0; c < 8; ++c) {
    const float corner[3] = {(c & 1) ? m->high[0] : m->low[0],
                             (c & 2) ? m->high[1] : m->low[1],
                             (c & 4) ? m->high[2] : m->low[2]};
    GripToLocal(grip, turn, corner, outline[kPalmCorners + c]);
  }
  const float middle[3] = {0.5f * (m->low[0] + m->high[0]),
                           0.5f * (m->low[1] + m->high[1]),
                           0.5f * (m->low[2] + m->high[2])};
  hm::StackIn in;
  memset(&in, 0, sizeof(in));
  for (int k = 0; k < 3; ++k) in.head[k] = head[k];
  GripToLocal(grip, turn, middle, in.center);
  in.outline = outline;
  in.outline_count = kPalmCorners + 8;
  in.side = s.fade > 0.0f && s.stack_side != 0 ? s.stack_side : 0;
  in.first = left ? -1 : 1;
  for (int k = 0; k < 3; ++k) {
    in.last_right[k] = s.last_right[k];
  }
  if (in.last_right[0] == 0.0f && in.last_right[1] == 0.0f &&
      in.last_right[2] == 0.0f) {
    in.last_right[0] = 1.0f;
  }
  in.gap = 0.02f;
  in.spacing = 0.006f;
  // The right and the up of the head, so the column and the cards are level
  // in the view.
  float head_turn[4];
  if (ControllersHeadTurn(head_turn)) {
    const float x_axis[3] = {1.0f, 0.0f, 0.0f};
    const float y_axis[3] = {0.0f, 1.0f, 0.0f};
    controller_math::QuatRotate(head_turn, x_axis, in.head_right);
    controller_math::QuatRotate(head_turn, y_axis, in.head_up);
  }
  in.count = s.count;
  for (int i = 0; i < s.count; ++i) {
    in.width[i] = s.card[i].width;
    in.height[i] = s.card[i].height;
    const int control = s.card[i].control;
    in.has_anchor[i] = control >= 0 && control < art::kControls &&
                       m->value_node[control] >= 0;
    if (in.has_anchor[i]) {
      GripToLocal(grip, turn, m->anchor[control], in.anchor[i]);
    }
  }
  // The order of the last frame, for the same cards.
  in.keep_count = 0;
  if (s.column_count == s.count) {
    bool same = true;
    for (int i = 0; i < s.count; ++i) {
      if (s.column_control[i] != s.card[i].control) same = false;
    }
    if (same) {
      in.keep_count = s.count;
      for (int i = 0; i < s.count; ++i) in.keep_column[i] = s.column[i];
    }
  }
  hm::StackOut stack;
  if (!hm::Stack(in, &stack)) return false;
  s.column_count = s.count;
  for (int i = 0; i < s.count; ++i) {
    s.column[stack.slot[i]] = i;
    s.column_control[i] = s.card[i].control;
  }
  s.stack_side = stack.side;
  for (int k = 0; k < 3; ++k) s.last_right[k] = stack.right[k];
  for (int i = 0; i < s.count; ++i) {
    LocalToGrip(grip, turn, stack.edge[i], s.edge[i]);
    for (int k = 0; k < 3; ++k) out->center[i][k] = stack.center[i][k];
    for (int k = 0; k < 4; ++k) {
      out->orientation[i][k] = stack.orientation[i][k];
    }
  }
  out->side = stack.side;
  out->changed = stack.changed;
  return true;
}

// THE COST of a frame is the build and the draws of all its eye passes. A
// build closes the frame before it, so the count does not depend on the
// number of eye passes of a frame.
void CloseCost() {
  if (g_frame_draws <= 0) return;
  const int64_t cost = g_build_ticks + g_draw_ticks;
  g_cost_sum += cost;
  if (cost > g_cost_most) g_cost_most = cost;
  ++g_cost_frames;
  g_cost_eyes += static_cast<uint64_t>(g_frame_draws);
  g_cost_triangles += static_cast<uint64_t>(g_frame_triangles);
  g_cost_sent += static_cast<uint64_t>(g_frame_sent);
  g_frame_draws = 0;
  g_frame_sent = 0;
  if (!g_said_cost && g_cost_frames >= 120) {
    g_said_cost = true;
    ModLog("hint model   : the controllers cost %.3f ms a frame on the CPU "
           "over %llu frames (the build and %.1f eye passes a frame), %.3f ms "
           "at most, %llu triangles and %llu vertices sent a frame",
           Ms(g_cost_sum) / static_cast<double>(g_cost_frames), g_cost_frames,
           static_cast<double>(g_cost_eyes) /
               static_cast<double>(g_cost_frames),
           Ms(g_cost_most), g_cost_triangles / g_cost_frames,
           g_cost_sent / g_cost_frames);
  }
}

void HintModelBuild(const OverlayHands& hands, const float stop[2][3]) {
  if (g_frequency.QuadPart == 0) QueryPerformanceFrequency(&g_frequency);
  CloseCost();
  const int64_t start = Now();
  g_units = hands.units_per_meter;
  g_any_built = false;
  ++g_build_frame;
  g_built_controllers_frame = ControllersFrameCount();
  for (int side = 0; side < 2; ++side) {
    g_built[side].have = false;
    if (g_style != 2) continue;
    BuildSide(side, hands.hand[side], stop[side], hands.units_per_meter);
    if (g_built[side].have) g_any_built = true;
  }
  g_build_ticks = g_any_built ? Now() - start : 0;
  g_draw_ticks = 0;
  g_frame_triangles = 0;
}

bool HintModelHasDraw() {
  return g_any_built && g_built_controllers_frame == ControllersFrameCount();
}

float HintModelHandOpacity(int side) {
  if (side < 0 || side > 1 || g_style != 2 || g_model[side] == nullptr) {
    return 1.0f;
  }
  return hm::HandOpacity(g_side[side].fade, g_hand_opacity_pct);
}

int HintModelDraw(IDirect3DDevice7* device, const float camera[12],
                  float near_plane, float half_x, float half_y) {
  if (!HintModelHasDraw() || device == nullptr) return 0;
  const int64_t start = Now();
  float eye[3];
  if (!EyeOf(camera, eye)) return 0;
  SetStates(device);
  HRESULT result = S_OK;
  int drawn = 0;
  for (int side = 0; side < 2; ++side) {
    drawn += DrawSide(device, side, camera, near_plane, half_x, half_y, eye,
                      &result);
  }
  // The depth write goes back to the value of SetStates, so PutStatesBack
  // puts back the value of the game.
  device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, TRUE);
  PutStatesBack(device);
  g_draw_ticks += Now() - start;
  g_frame_triangles += drawn;
  ++g_frame_draws;
  if (FAILED(result)) {
    if (!g_said_failed) {
      g_said_failed = true;
      ModLog("hint model   : a draw of a controller failed (0x%08X)",
             static_cast<unsigned>(result));
    }
    return -1;
  }
  ++g_draws;
  if (!g_said_draw && drawn > 0) {
    g_said_draw = true;
    ModLog("hint model   : the first controller is drawn in an eye picture, "
           "%d triangles", drawn);
  }
  return drawn;
}

void HintModelPollKey() {
  ReadSettings();
  PollKey();
}

void HintModelReport() {
  if (g_cost_frames == 0 && g_side[0].frames_shown == 0 &&
      g_side[1].frames_shown == 0) {
    return;
  }
  CloseCost();
  ModLog("hint model   : the controller showed in %llu frames of the left hand "
         "and %llu of the right hand. The cost: %.3f ms a frame, %.3f ms at "
         "most, %llu triangles a frame, over %llu frames with a draw",
         g_side[0].frames_shown, g_side[1].frames_shown,
         g_cost_frames > 0
             ? Ms(g_cost_sum) / static_cast<double>(g_cost_frames)
             : 0.0,
         Ms(g_cost_most),
         g_cost_frames > 0 ? g_cost_triangles / g_cost_frames : 0ull,
         g_cost_frames);
  g_side[0].frames_shown = g_side[1].frames_shown = 0;
  g_cost_sum = g_cost_most = 0;
  g_cost_frames = g_cost_triangles = g_cost_eyes = g_cost_sent = 0;
}
