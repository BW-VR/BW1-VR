// The pointer on the screen quad, the keyboard of the headset app and the
// ring of the keys, a test (src/mod/controller_math.h, RayQuad and the
// screen pointer, android/src/keyboard_layout.h, src/common/input_channel.h).
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include <math.h>
#include <stddef.h>
#include <string.h>

#include <string>
#include <vector>

#include "../../android/src/keyboard_layout.h"
#include "../../src/common/input_channel.h"
#include "../../src/mod/controller_math.h"
#include "../../src/mod/scene_stream.h"
#include "../../src/mod/screen_quad_math.h"

namespace screen_pointer_checks {

namespace cm = controller_math;
namespace kl = keyboard_layout;
namespace ic = input_channel;

inline bool Close(float a, float b, float e = 1e-4f) { return fabsf(a - b) <= e; }

// The direction from a to b.
inline void Toward(const float a[3], const float b[3], float out[3]) {
  for (int i = 0; i < 3; ++i) out[i] = b[i] - a[i];
}

inline void RayChecks() {
  printf("\nA ray against a quad (the pointer on the screen quad)\n");
  // The screen quad of the PC: 2.6 m wide at 2 m, a render of 4:3.
  const scene_stream::ScreenBody s =
      screen_quad_math::MakeScreen(0, false, 0, 1600, 1200, 2.0f, 2.6f);
  const float origin[3] = {0.0f, 0.0f, 0.0f};
  {
    const float d[3] = {0.0f, 0.0f, -1.0f};
    const cm::QuadHit h = cm::RayQuad(origin, d, s.position, s.orientation, s.size);
    Check(h.hit && h.inside && Close(h.u, 0.5f) && Close(h.v, 0.5f) &&
              Close(h.distance, 2.0f),
          "a ray at the center hits the middle of the quad", h.u, 0.5);
  }
  {
    // The top left corner, a little inside.
    const float corner[3] = {-1.29f, 0.97f, -2.0f};
    float d[3];
    Toward(origin, corner, d);
    const cm::QuadHit h = cm::RayQuad(origin, d, s.position, s.orientation, s.size);
    Check(h.inside && h.u < 0.01f && h.v < 0.01f,
          "a ray at the top left corner gives u and v near 0", h.u, 0.0);
    // Past the right edge.
    const float out[3] = {1.4f, 0.0f, -2.0f};
    Toward(origin, out, d);
    const cm::QuadHit o = cm::RayQuad(origin, d, s.position, s.orientation, s.size);
    Check(o.hit && !o.inside && o.u > 1.0f,
          "a ray past the right edge meets the plane, but not the quad", o.u, 1.0);
  }
  {
    // From behind the quad, toward the player: the back side.
    const float behind[3] = {0.0f, 0.0f, -3.0f};
    const float d[3] = {0.0f, 0.0f, 1.0f};
    const cm::QuadHit h = cm::RayQuad(behind, d, s.position, s.orientation, s.size);
    Check(!h.hit, "a ray from behind the quad gives no hit", h.hit ? 1 : 0, 0);
    // A hand past the quad that points away from it.
    const float away[3] = {0.0f, 0.0f, -1.0f};
    const cm::QuadHit a = cm::RayQuad(behind, away, s.position, s.orientation, s.size);
    Check(!a.hit, "a quad behind the hand gives no hit", a.hit ? 1 : 0, 0);
    // Parallel to the quad.
    const float side[3] = {1.0f, 0.0f, 0.0f};
    const cm::QuadHit p = cm::RayQuad(origin, side, s.position, s.orientation, s.size);
    Check(!p.hit, "a ray parallel to the quad gives no hit", p.hit ? 1 : 0, 0);
  }
  {
    // A quad turned 90 degrees to the left (about y): it is at the left of
    // the player and faces +x. A ray to the left hits it. Its right edge is
    // the edge ahead of the player (-z).
    const float turn[4] = {0.0f, sinf(0.25f * 3.14159265f), 0.0f, cosf(0.25f * 3.14159265f)};
    const float pos[3] = {-2.0f, 0.0f, 0.0f};
    const float size[2] = {2.0f, 1.0f};
    const float target[3] = {-2.0f, 0.25f, -0.5f};
    float d[3];
    Toward(origin, target, d);
    const cm::QuadHit h = cm::RayQuad(origin, d, pos, turn, size);
    Check(h.inside && Close(h.u, 0.75f, 1e-3f) && Close(h.v, 0.25f, 1e-3f),
          "a ray at a turned quad gives u and v in the frame of the quad", h.u, 0.75);
  }
  {
    // AimAlong turns -z onto a direction, and AimForward gives it back.
    const float dirs[4][3] = {{0.3f, -0.2f, -1.0f}, {-0.7f, 0.4f, -0.3f},
                              {0.0f, 0.0f, -1.0f}, {0.5f, 0.1f, 0.8f}};
    bool all = true;
    for (const auto& dir : dirs) {
      float q[4], f[3];
      cm::AimAlong(dir, q);
      cm::AimForward(q, f);
      const float n = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
      for (int i = 0; i < 3; ++i) all = all && Close(f[i], dir[i] / n, 1e-4f);
    }
    Check(all, "the aim of AimAlong points along the direction", all ? 1 : 0, 1);
  }
  {
    // The pixel of a hit, and no pixel off the quad.
    cm::QuadHit h = {true, true, 0.25f, 0.75f, 2.0f};
    float x = -1.0f, y = -1.0f;
    const bool got = cm::ScreenPixel(h, 1600, 1200, &x, &y);
    Check(got && Close(x, 400.0f) && Close(y, 900.0f),
          "a hit at u 0.25, v 0.75 is the render pixel 400, 900", x, 400);
    h.u = 1.0f;
    h.v = 1.0f;
    cm::ScreenPixel(h, 1600, 1200, &x, &y);
    Check(Close(x, 1599.0f) && Close(y, 1199.0f),
          "a hit on the bottom right edge stays in the render", x, 1599);
    h.inside = false;
    x = 7.0f;
    const bool none = cm::ScreenPixel(h, 1600, 1200, &x, &y);
    Check(!none && Close(x, 7.0f), "off the quad the pixel stays", x, 7);
  }
}

inline void HandChecks() {
  printf("\nThe hand that points on the screen quad, and the press\n");
  cm::ScreenHands s;
  cm::ScreenHandsReset(&s, 1);
  const bool both[2] = {true, true};
  {
    const float t[2] = {0.0f, 0.0f};
    Check(cm::ScreenHandsStep(&s, t, both) == 1, "with no trigger the right hand points", 1, 1);
  }
  {
    const float t[2] = {0.9f, 0.0f};
    Check(cm::ScreenHandsStep(&s, t, both) == 0, "the left trigger gives the pointer to the left hand", 0, 0);
  }
  {
    const float t[2] = {0.9f, 0.9f};
    Check(cm::ScreenHandsStep(&s, t, both) == 1,
          "the right trigger that closes while the left is held takes the pointer", 1, 1);
  }
  {
    const float t[2] = {0.0f, 0.4f};
    Check(cm::ScreenHandsStep(&s, t, both) == 1 && s.down[1],
          "a trigger at 40 percent stays closed (open below 35)", s.down[1] ? 1 : 0, 1);
  }
  {
    const float t[2] = {0.9f, 0.0f};
    const bool left_off[2] = {false, true};
    cm::ScreenHandsStep(&s, t, left_off);
    Check(s.hand == 1 && !s.down[0], "a hand that is not active does not take the pointer", s.hand, 1);
  }

  cm::ScreenPress p = {false, false};
  Check(!cm::ScreenPressStep(&p, true, false, true) && p.need_up,
        "a press that starts off the quad does nothing", 0, 0);
  Check(!cm::ScreenPressStep(&p, true, true, true),
        "the trigger must open before it presses again", 0, 0);
  Check(!cm::ScreenPressStep(&p, false, true, true) && !p.need_up, "the trigger opens", 0, 0);
  Check(cm::ScreenPressStep(&p, true, true, true), "a press on the quad holds the left button", 1, 1);
  Check(cm::ScreenPressStep(&p, true, false, true),
        "a press that started may end off the quad", 1, 1);
  Check(!cm::ScreenPressStep(&p, true, true, false) && p.need_up,
        "a hand that cannot act lets go, and the trigger must open", 0, 0);
}

inline void KeyboardChecks() {
  printf("\nThe keyboard of the headset app\n");
  const int n = kl::KeyCount();
  Check(n == 39, "the keyboard has the digits, the letters, space, back space and Enter", n, 39);
  bool inside = true, apart = true, own = true, codes = true;
  int letters = 0, digits = 0, back = 0, enter = 0, space = 0;
  for (int i = 0; i < n; ++i) {
    kl::Key key;
    kl::Rect r;
    kl::KeyAt(i, &key, &r);
    inside = inside && r.l >= 0 && r.t >= 0 && r.r <= kl::kWidth && r.b <= kl::kHeight;
    // The middle of each key gives that key.
    own = own && kl::KeyIndexAt((r.l + r.r) * 0.5f, (r.t + r.b) * 0.5f) == i;
    for (int j = i + 1; j < n; ++j) {
      kl::Key other;
      kl::Rect o;
      kl::KeyAt(j, &other, &o);
      if (r.l < o.r && o.l < r.r && r.t < o.b && o.t < r.b) apart = false;
      if (key.code == other.code) codes = false;
    }
    if (key.code >= 'a' && key.code <= 'z') ++letters;
    if (key.code >= '0' && key.code <= '9') ++digits;
    if (key.code == ic::kKeyBack) ++back;
    if (key.code == ic::kKeyEnter) ++enter;
    if (key.code == ' ') ++space;
  }
  Check(letters == 26 && digits == 10 && back == 1 && enter == 1 && space == 1 && codes,
        "each letter and digit is one key, with one space, back space and Enter", letters, 26);
  Check(inside, "each key is in the picture", inside ? 1 : 0, 1);
  Check(apart, "no two keys overlap", apart ? 1 : 0, 1);
  Check(own, "the middle of each key gives that key", own ? 1 : 0, 1);
  {
    kl::Key a, b;
    kl::Rect ra, rb;
    kl::KeyAt(0, &a, &ra);
    kl::KeyAt(1, &b, &rb);
    const int between = kl::KeyIndexAt((ra.r + rb.l) * 0.5f, (ra.t + ra.b) * 0.5f);
    Check(between == -1, "the gap between two keys is no key", between, -1);
    Check(kl::KeyIndexAt(5.0f, 5.0f) == -1, "the edge of the picture is no key", 0, 0);
  }

  // The quad under the screen quad of the PC, facing the head.
  const scene_stream::ScreenBody s =
      screen_quad_math::MakeScreen(0, false, 0, 1380, 1104, 2.0f, 2.6f);
  kl::Quad q;
  const bool made = kl::QuadUnder(s.position, s.size, &q);
  const float screen_bottom = s.position[1] - s.size[1] * 0.5f;
  const float top = q.position[1] + q.size[1] * 0.5f;  // before the turn
  Check(made && top < screen_bottom && Close(q.size[0], 2.6f * kl::kWidthOfScreen),
        "the keyboard is under the screen quad", top, screen_bottom);
  {
    // Its front faces the origin.
    const float front[3] = {0.0f, 0.0f, 1.0f};
    float normal[3];
    cm::QuatRotate(q.orientation, front, normal);
    float to[3];
    const float origin[3] = {0.0f, 0.0f, 0.0f};
    Toward(q.position, origin, to);
    const float len = sqrtf(to[0] * to[0] + to[1] * to[1] + to[2] * to[2]);
    const float dot = (normal[0] * to[0] + normal[1] * to[1] + normal[2] * to[2]) / len;
    Check(dot > 0.9999f, "the front of the keyboard faces the head at the origin", dot, 1);
  }
  {
    // A ray from a hand to the middle of the key "g" types "g".
    int g = -1;
    for (int i = 0; i < n; ++i) {
      kl::Key key;
      kl::Rect r;
      kl::KeyAt(i, &key, &r);
      if (key.code == 'g') g = i;
    }
    kl::Key key;
    kl::Rect r;
    kl::KeyAt(g, &key, &r);
    float point[3];
    kl::PixelToPoint(q, (r.l + r.r) * 0.5f, (r.t + r.b) * 0.5f, point);
    const float hand[3] = {0.2f, -0.4f, -0.3f};
    float d[3];
    Toward(hand, point, d);
    float x = 0.0f, y = 0.0f;
    const bool hit = kl::RayToPixel(hand, d, q, &x, &y);
    Check(hit && kl::KeyIndexAt(x, y) == g, "a ray at the middle of g gives the key g",
          hit ? kl::KeyIndexAt(x, y) : -2, g);
    const float up[3] = {0.0f, 1.0f, -1.0f};
    Check(!kl::RayToPixel(hand, up, q, &x, &y), "a ray up at the screen misses the keyboard", 0, 0);
  }
}

inline void KeyRingChecks() {
  printf("\nThe ring of the keys from the app to the mod\n");
  Check(offsetof(ic::File, keys) == ic::kKeysOffset && offsetof(ic::Head, key_write) == 56 &&
            offsetof(ic::Head, key_read) == 60 && sizeof(ic::KeyEvent) == 8,
        "the keys have one layout for the mod and the app",
        static_cast<double>(offsetof(ic::File, keys)), ic::kKeysOffset);
  Check(ic::kVersion == 1 && offsetof(ic::File, events) == 1024 &&
            offsetof(ic::Head, events_dropped) == 52,
        "the version, the events and the head before the keys stay as they were", ic::kVersion,
        1);
  std::vector<uint8_t> bytes(ic::kFileBytes, 0);
  ic::File* f = reinterpret_cast<ic::File*>(bytes.data());
  ic::MakeHead(f);
  const char* word = "bwvrtest";
  for (const char* c = word; *c != 0; ++c) {
    const ic::KeyEvent k = {static_cast<uint32_t>(*c), 7};
    ic::PushKey(f, k);
  }
  const ic::KeyEvent enter = {ic::kKeyEnter, 8};
  ic::PushKey(f, enter);
  ic::KeyEvent out[ic::kKeys];
  bool broken = false;
  uint32_t got = ic::TakeKeys(f, out, 4, &broken);
  std::string text;
  for (uint32_t i = 0; i < got; ++i) text += static_cast<char>(out[i].code);
  got = ic::TakeKeys(f, out, ic::kKeys, &broken);
  for (uint32_t i = 0; i < got; ++i) {
    if (out[i].code != ic::kKeyEnter) text += static_cast<char>(out[i].code);
  }
  Check(text == "bwvrtest" && got == 5 && out[got - 1].code == ic::kKeyEnter && !broken,
        "the keys come in their order, also over two takes", static_cast<double>(text.size()), 8);
  Check(ic::TakeKeys(f, out, ic::kKeys, &broken) == 0, "a key is taken one time", 0, 0);
  // A full ring drops the new key and counts it.
  int pushed = 0;
  for (uint32_t i = 0; i < ic::kKeys + 3; ++i) {
    const ic::KeyEvent k = {'a', i};
    if (ic::PushKey(f, k)) ++pushed;
  }
  Check(pushed == static_cast<int>(ic::kKeys) && f->head.keys_dropped == 3,
        "a full ring drops the new keys and counts them", f->head.keys_dropped, 3);
  Check(ic::DropOldKeys(f) == ic::kKeys && ic::TakeKeys(f, out, ic::kKeys, &broken) == 0,
        "the mod drops the keys of an earlier run at its start", 0, 0);
  f->head.key_write = f->head.key_read + 1000;
  Check(ic::TakeKeys(f, out, ic::kKeys, &broken) == 0 && broken &&
            f->head.key_read == f->head.key_write,
        "indices that are not valid give no key, and the mod takes the write index", 0, 0);
  // The pulses of the mod still use their own ring.
  const ic::Event e = {1, 1, 0.5f, 0.02f};
  Check(ic::PushEvent(f, e) && f->head.event_write == 1 && f->head.key_write == f->head.key_read,
        "a pulse does not touch the ring of the keys", 0, 0);
  // The flag of a dialog box that takes text is a bit of its own.
  Check(scene_stream::kScreenText != scene_stream::kScreenCpu &&
            sizeof(scene_stream::ScreenBody) == 72,
        "kScreenText is a new bit, and the kScreen keeps its layout",
        static_cast<double>(sizeof(scene_stream::ScreenBody)), 72);
}

inline void ScreenPointerChecks() {
  RayChecks();
  HandChecks();
  KeyboardChecks();
  KeyRingChecks();
}

}  // namespace screen_pointer_checks
