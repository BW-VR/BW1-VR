// The pointer on the panel quads in a frame of the scene, a test
// (src/mod/controller_math.h, RayPanels and PanelSurfaceStep,
// src/mod/quad_layer_math.h, the text flag of kPanel,
// android/src/keyboard_layout.h, QuadOnPanel). main.cpp includes this file
// after Check and Near, just before main.
#pragma once

#include <math.h>
#include <string.h>

#include "../../android/src/keyboard_layout.h"
#include "../../src/mod/controller_math.h"
#include "../../src/mod/quad_layer_math.h"
#include "../../src/mod/scene_stream.h"

namespace panel_pointer_checks {

namespace cm = controller_math;
namespace kl = keyboard_layout;
namespace ql = quad_layer_math;
namespace ss = scene_stream;

// The menu panel quad of ui_panel.cpp for a render of 4080 by 3264: the
// default rect (1280 by 960 in the middle), 140 cm ahead and 10 degrees down
// from a head at the origin, 16 pixels a degree.
inline cm::RayTarget MenuQuad() {
  cm::RayTarget t;
  memset(&t, 0, sizeof(t));
  t.on = true;
  t.panel = true;
  const float anchor[3] = {0.0f, 0.0f, 0.0f};
  const float yaw[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  cm::PanelPose(anchor, yaw, 1.4f, -10.0f * 0.01745329f, t.position, t.orientation);
  t.size[0] = cm::PanelWidth(1280.0f, 16.0f, 1.4f);
  t.size[1] = t.size[0] * 960.0f / 1280.0f;
  t.rect[0] = 1400;
  t.rect[1] = 1152;
  t.rect[2] = 1280;
  t.rect[3] = 960;
  return t;
}

// The point of the quad of a target at a pixel of the panel image.
inline void PixelPoint(const cm::RayTarget& t, float x, float y, float out[3]) {
  cm::QuadPoint(t.position, t.orientation, t.size, (x - t.rect[0]) / t.rect[2],
                (y - t.rect[1]) / t.rect[3], out);
}

inline void Toward(const float a[3], const float b[3], float out[3]) {
  for (int i = 0; i < 3; ++i) out[i] = b[i] - a[i];
}

inline void PanelPointerChecks() {
  printf("\nThe pointer on the panel quads in a frame of the scene\n");
  const float hand[3] = {0.2f, -0.3f, -0.3f};
  cm::RayTarget t[4];
  memset(t, 0, sizeof(t));
  t[0] = MenuQuad();
  {
    // The row "Skip straight to the Creature select" of the start dialog.
    float p[3], d[3];
    PixelPoint(t[0], 2040.0f - 143.0f, 1632.0f + 23.0f, p);
    Toward(hand, p, d);
    const cm::PanelHit h = cm::RayPanels(hand, d, t, 4);
    Check(h.hit && h.target == 0 && fabsf(h.x - 1897.0f) < 0.5f &&
              fabsf(h.y - 1655.0f) < 0.5f,
          "a ray at a pixel of the menu panel quad gives that pixel of the panel image",
          h.x, 1897.0);
    // The last pixel of the rect: the hit stays in the rect.
    PixelPoint(t[0], 1400.0f + 1280.0f - 0.01f, 1152.0f + 960.0f - 0.01f, p);
    Toward(hand, p, d);
    const cm::PanelHit e = cm::RayPanels(hand, d, t, 4);
    Check(e.hit && e.x <= 2679.0f && e.y <= 2111.0f && e.x > 2678.0f,
          "a ray at the corner of the quad gives the last pixel of the rect", e.x, 2679.0);
  }
  {
    // Past the edge of the quad: no hit, and no target.
    float p[3], d[3];
    PixelPoint(t[0], 1300.0f, 1600.0f, p);
    Toward(hand, p, d);
    const cm::PanelHit h = cm::RayPanels(hand, d, t, 4);
    Check(!h.hit && h.target == -1, "a ray past the edge of the panel quads hits nothing",
          h.target, -1);
    // A quad that does not show, and a rect with no width, stop nothing.
    cm::RayTarget off[2] = {t[0], t[0]};
    off[0].on = false;
    off[1].rect[2] = 0;
    PixelPoint(t[0], 2040.0f, 1632.0f, p);
    Toward(hand, p, d);
    const cm::PanelHit n = cm::RayPanels(hand, d, off, 2);
    Check(!n.hit && n.target == -1,
          "a panel quad that does not show and a rect with no width give no hit", n.target, -1);
  }
  {
    // A nearer quad on the ray wins: a help quad between the hand and the
    // menu quad.
    cm::RayTarget two[2] = {t[0], t[0]};
    float p[3], d[3];
    PixelPoint(t[0], 2040.0f, 1632.0f, p);
    Toward(hand, p, d);
    for (int i = 0; i < 3; ++i) two[1].position[i] = hand[i] + 0.5f * d[i];
    two[1].rect[1] = 1958;
    two[1].rect[3] = 1306;
    const cm::PanelHit h = cm::RayPanels(hand, d, two, 2);
    Check(h.hit && h.target == 1 && h.distance < 0.51f,
          "the nearest panel quad along the ray wins", h.target, 1);
    // The order of the targets does not matter.
    cm::RayTarget swapped[2] = {two[1], two[0]};
    const cm::PanelHit s = cm::RayPanels(hand, d, swapped, 2);
    Check(s.hit && s.target == 0 && fabsf(s.x - h.x) < 1e-3f,
          "the nearest quad wins in each order of the targets", s.target, 0);
  }
  {
    // The keyboard of the app in front of the lower part of the menu quad.
    kl::Quad k;
    const bool made = kl::QuadOnPanel(t[0].position, t[0].orientation, t[0].size, &k);
    Check(made && fabsf(k.size[0] - 0.5f * t[0].size[0]) < 1e-4f &&
              fabsf(k.size[1] - k.size[0] * kl::kHeight / kl::kWidth) < 1e-4f,
          "the keyboard on the panel is half as wide as the menu panel quad", k.size[0],
          0.5 * t[0].size[0]);
    // In the frame of the panel quad: 3 cm in front of it, the top 0.35 of
    // its height under its center, and the same turn.
    const float back[4] = {-t[0].orientation[0], -t[0].orientation[1],
                           -t[0].orientation[2], t[0].orientation[3]};
    const float rel[3] = {k.position[0] - t[0].position[0],
                          k.position[1] - t[0].position[1],
                          k.position[2] - t[0].position[2]};
    float local[3];
    cm::QuatRotate(back, rel, local);
    const float top = local[1] + k.size[1] * 0.5f;
    Check(fabsf(local[0]) < 1e-4f && fabsf(local[2] - 0.03f) < 1e-4f &&
              fabsf(top + 0.35f * t[0].size[1]) < 1e-4f &&
              memcmp(k.orientation, t[0].orientation, sizeof(k.orientation)) == 0,
          "the keyboard is 3 cm in front of the panel, its top 0.35 of the height under "
          "the center",
          top, -0.35 * t[0].size[1]);
    const float zero[2] = {0.0f, 1.0f};
    Check(!kl::QuadOnPanel(t[0].position, t[0].orientation, zero, &k),
          "a panel quad with no width gives no keyboard", 0, 0);
    // A ray at the middle of the keyboard: the keyboard stops it before the
    // panel quad behind it, so the hit is no pixel.
    kl::QuadOnPanel(t[0].position, t[0].orientation, t[0].size, &k);
    t[3].on = true;
    t[3].panel = false;
    memcpy(t[3].position, k.position, sizeof(k.position));
    memcpy(t[3].orientation, k.orientation, sizeof(k.orientation));
    memcpy(t[3].size, k.size, sizeof(k.size));
    float d[3];
    Toward(hand, k.position, d);
    const cm::PanelHit behind = cm::RayPanels(hand, d, t, 1);
    const cm::PanelHit h = cm::RayPanels(hand, d, t, 4);
    Check(behind.hit && !h.hit && h.target == 3,
          "the keyboard of the app stops the ray before the panel quad behind it",
          h.target, 3);
    // The middle of the box (the middle of the render) is not behind the
    // keyboard.
    float p[3];
    PixelPoint(t[0], 2040.0f, 1632.0f + 158.0f, p);
    Toward(hand, p, d);
    const cm::PanelHit box = cm::RayPanels(hand, d, t, 4);
    Check(box.hit && box.target == 0,
          "the bottom of a box of 316 pixels in the middle is above the keyboard",
          box.target, 0);
    t[3].on = false;
  }
  {
    // The surface of the pointer.
    cm::PanelSurface s = {false, false, 0.0f, 0.0f};
    cm::PanelHit on = {true, 0, 1900.0f, 1650.0f, 1.0f};
    cm::PanelHit off = {false, -1, 0.0f, 0.0f, 0.0f};
    bool r = cm::PanelSurfaceStep(&s, off, false);
    Check(!r && !s.on, "with no hit and no press the pointer is on the eye view", r ? 1 : 0, 0);
    r = cm::PanelSurfaceStep(&s, on, false);
    Check(r && s.x == 1900.0f && s.y == 1650.0f, "a hit puts the pointer on the panel",
          s.x, 1900.0);
    on.x = 1910.0f;
    r = cm::PanelSurfaceStep(&s, on, true);
    Check(r && s.x == 1910.0f, "a held press on the panel follows the ray on the panel",
          s.x, 1910.0);
    r = cm::PanelSurfaceStep(&s, off, true);
    Check(r && s.x == 1910.0f,
          "a held press that started on the panel keeps the last pixel off the panel", s.x,
          1910.0);
    r = cm::PanelSurfaceStep(&s, off, false);
    Check(!r && !s.on, "after the release the ray off the panel gives the eye view",
          r ? 1 : 0, 0);
    r = cm::PanelSurfaceStep(&s, on, true);
    Check(!r && !s.on, "a press held from the eye view stays on the eye view on the panel",
          r ? 1 : 0, 0);
  }
  {
    // The text flag of the kPanel.
    bwvr::PanelQuad q[2];
    memset(q, 0, sizeof(q));
    q[0].visible = true;
    q[0].rect[0] = 1400;
    q[0].rect[1] = 1152;
    q[0].rect[2] = 1280;
    q[0].rect[3] = 960;
    q[0].width = 2.0f;
    q[0].height = 1.5f;
    q[0].orientation[3] = 1.0f;
    bwvr::PanelQuad creature;
    memset(&creature, 0, sizeof(creature));
    ss::PanelBody p = ql::MakePanel(4080, 3264, q, false, creature);
    Check(p.quads[0].visible == ss::kPanelQuadShows && p.quads[1].visible == 0,
          "a quad that shows has kPanelQuadShows alone", p.quads[0].visible, 1);
    int32_t rect[4];
    Check(!ql::PanelText(p, rect), "a kPanel with no text flag asks for no keyboard", 0, 0);
    ql::MarkPanelText(&p, true);
    Check(p.quads[0].visible == (ss::kPanelQuadShows | ss::kPanelQuadText) &&
              ql::PanelText(p, rect) && rect[2] == 1280 &&
              ql::PanelQuadRect(p.quads[0], p.width, p.height, rect),
          "the text flag goes on the menu quad, and the quad still shows", p.quads[0].visible,
          3);
    ql::MarkPanelText(&p, false);
    Check(p.quads[0].visible == ss::kPanelQuadShows, "the text flag goes again",
          p.quads[0].visible, 1);
    p.quads[0].visible = 0;
    ql::MarkPanelText(&p, true);
    Check(p.quads[0].visible == 0 && !ql::PanelText(p, rect),
          "a menu quad that does not show gets no text flag", p.quads[0].visible, 0);
    // The targets of the kPanel: the rect cut to the panel image.
    p.quads[0].visible = ss::kPanelQuadShows;
    p.quads[0].rect[0] = 3500;
    cm::RayTarget targets[3];
    ql::PanelTargets(p, targets);
    Check(targets[0].on && targets[0].panel && targets[0].rect[2] == 580 && !targets[1].on &&
              !targets[2].on,
          "the targets of a kPanel have the rects that the app shows", targets[0].rect[2], 580);
  }
}

}  // namespace panel_pointer_checks
