// THE CONTROLLER IN THE HAND (hint_model_math.h, docs/controller-hints.md,
// "The controller in the hand"): the model of each profile, the nodes of
// each control, the press of a control, and the cards of the buttons with
// their lines. main.cpp includes this file after hint_checks.h.
#pragma once

#include "../../src/mod/hint_model_math.h"

namespace hint_model_checks {

inline bool Is(const char* a, const char* b) {
  if (a == nullptr || b == nullptr) return a == b;
  return strcmp(a, b) == 0;
}

// The corners of a box of the controller round `center`, turned by q.
inline void BoxCorners(const float center[3], const float q[4],
                       const float half[3], float out[8][3]) {
  for (int c = 0; c < 8; ++c) {
    const float local[3] = {(c & 1 ? 1.0f : -1.0f) * half[0],
                            (c & 2 ? 1.0f : -1.0f) * half[1],
                            (c & 4 ? 1.0f : -1.0f) * half[2]};
    float turned[3];
    controller_math::QuatRotate(q, local, turned);
    for (int k = 0; k < 3; ++k) out[c][k] = center[k] + turned[k];
  }
}

inline void HintModelChecks() {
  namespace hm = hint_model;
  namespace art = controller_art;
  namespace map = button_map;
  using hint_checks::Random;
  using hint_checks::Turn;
  printf("\nThe controller in the hand\n");

  // THE ONE SWITCH of the launcher (hint_controller): on gives the controller
  // in the hand and the pictures of a hand with no model, off gives the
  // cards at the wrists with the button words only. Each value that is not
  // 0 is on, as a checkbox of the launcher writes 1.
  const bool by_switch =
      hm::StyleOfSwitch(1) == 2 && hm::PicturesOfSwitch(1) == 1 &&
      hm::StyleOfSwitch(0) == 1 && hm::PicturesOfSwitch(0) == 0 &&
      hm::StyleOfSwitch(7) == 2 && hm::PicturesOfSwitch(-1) == 1;
  Check(by_switch, "hint_controller=1 gives hint_style 2 with pictures, and 0 "
        "gives hint_style 1 with the button words only", by_switch, 1);

  // THE MODEL OF EACH PROFILE.
  const char* touch = "oculus/touch_controller";
  const bool quest =
      Is(hm::ModelOfProfile(touch, "Meta Quest 3"), "meta-quest-touch-plus") &&
      Is(hm::ModelOfProfile(touch, "Meta Quest 3S"),
         "meta-quest-touch-plus") &&
      Is(hm::ModelOfProfile(touch, "Meta Quest Pro"),
         "meta-quest-touch-pro") &&
      Is(hm::ModelOfProfile(touch, "Oculus Quest2"), "oculus-touch-v3") &&
      Is(hm::ModelOfProfile(touch, "Meta Quest 2"), "oculus-touch-v3") &&
      Is(hm::ModelOfProfile(touch, "Oculus Quest"), "oculus-touch-v2") &&
      Is(hm::ModelOfProfile(touch, "Oculus Rift S"), "oculus-touch-v2") &&
      Is(hm::ModelOfProfile(touch, ""), "meta-quest-touch-plus") &&
      Is(hm::ModelOfProfile(touch, nullptr), "meta-quest-touch-plus") &&
      Is(hm::ModelOfProfile("fake", ""), "meta-quest-touch-plus") &&
      Is(hm::ModelOfProfile(touch, "PICO 4"), "pico-4");
  Check(quest, "the Touch profile of Virtual Desktop gives the model of each "
        "Quest from the name of the headset, and the fake controller is a "
        "Touch Plus", quest, 1);
  const bool others =
      Is(hm::ModelOfProfile("valve/index_controller", "Index"),
         "valve-index") &&
      Is(hm::ModelOfProfile("htc/vive_controller", "Vive"), "htc-vive") &&
      Is(hm::ModelOfProfile("htc/vive_cosmos_controller", ""),
         "htc-vive-cosmos") &&
      Is(hm::ModelOfProfile("htc/vive_focus3_controller", ""),
         "htc-vive-focus-3") &&
      hm::ModelOfProfile("microsoft/motion_controller", "Samsung") ==
          nullptr &&
      Is(hm::ModelOfProfile("microsoft/motion_controller", "HP Reverb G2"),
         "hp-mixed-reality") &&
      Is(hm::ModelOfProfile("hp/mixed_reality_controller", ""),
         "hp-mixed-reality") &&
      Is(hm::ModelOfProfile("bytedance/pico4_controller", ""), "pico-4") &&
      Is(hm::ModelOfProfile("meta/touch_controller_plus", ""),
         "meta-quest-touch-plus") &&
      Is(hm::ModelOfProfile("facebook/touch_controller_pro", ""),
         "meta-quest-touch-pro") &&
      Is(hm::ModelOfProfile("khr/simple_controller", ""), "generic-trigger");
  Check(others, "each other profile gives its model", others, 1);
  const bool none = hm::ModelOfProfile("", "Meta Quest 3") == nullptr &&
                    hm::ModelOfProfile(nullptr, "") == nullptr &&
                    hm::ModelOfProfile("acme/glove", "Acme") == nullptr;
  Check(none, "no profile yet, and an unknown profile, give no model (the "
        "cards of hint_style=1)", none, 1);

  // THE SIDE OF A NORMAL in the simpler body: two opposite normals never
  // share a side, and a normal near an axis has the side of that axis.
  uint32_t side_seed = 77;
  int opposite_same = 0;
  for (int i = 0; i < 2000; ++i) {
    float n[3] = {Random(&side_seed) * 2.0f - 1.0f,
                  Random(&side_seed) * 2.0f - 1.0f,
                  Random(&side_seed) * 2.0f - 1.0f};
    const float back[3] = {-n[0], -n[1], -n[2]};
    if (hm::NormalSide(n) == hm::NormalSide(back)) ++opposite_same;
  }
  Check(opposite_same == 0, "two opposite normals always have different "
        "sides, so the two faces of a thin part stay apart", opposite_same, 0);
  const float px[3] = {0.9f, 0.3f, -0.2f}, ny[3] = {0.1f, -0.95f, 0.2f};
  const float pz[3] = {0.0f, 0.0f, 1.0f}, nz[3] = {0.4f, 0.4f, -0.8f};
  const bool sides = hm::NormalSide(px) == 0 && hm::NormalSide(ny) == 3 &&
                     hm::NormalSide(pz) == 4 && hm::NormalSide(nz) == 5;
  Check(sides, "a normal near an axis has the side of that axis and its sign",
        sides, 1);

  // THE NODES OF EACH CONTROL.
  const char* bases[hm::kMostBases];
  const bool nodes =
      hm::NodeBases(art::kFirst, false, bases) == 1 &&
      Is(bases[0], "a_button") &&
      hm::NodeBases(art::kFirst, true, bases) == 2 &&
      Is(bases[0], "x_button") && Is(bases[1], "a_button") &&
      hm::NodeBases(art::kSecond, true, bases) == 2 &&
      Is(bases[0], "y_button") &&
      hm::NodeBases(art::kTrigger, true, bases) == 1 &&
      Is(bases[0], "xr_standard_trigger") &&
      hm::NodeBases(art::kGrip, false, bases) == 1 &&
      Is(bases[0], "xr_standard_squeeze") &&
      hm::NodeBases(art::kStick, false, bases) == 2 &&
      Is(bases[0], "xr_standard_thumbstick") &&
      Is(bases[1], "xr_standard_touchpad") &&
      hm::NodeBases(art::kMenu, true, bases) == 1 && Is(bases[0], "menu") &&
      hm::NodeBases(99, true, bases) == 0;
  Check(nodes, "each control has the node names of the WebXR models: X and "
        "Y on a left Touch, A and B on an Index, the pad of a wand for its "
        "stick", nodes, 1);

  // THE CONTROL OF A WORD.
  bool plain = false;
  const bool words =
      hm::ControlOfWord(map::kTouch, false, L"Grip", &plain) == art::kGrip &&
      plain &&
      hm::ControlOfWord(map::kTouch, false, L"Grip hold", &plain) ==
          art::kGrip &&
      !plain &&
      hm::ControlOfWord(map::kTouch, false, L"Trigger x2", &plain) ==
          art::kTrigger &&
      !plain &&
      hm::ControlOfWord(map::kTouch, true, L"X", &plain) == art::kFirst &&
      plain &&
      hm::ControlOfWord(map::kTouch, false, L"X", &plain) == -1 &&
      hm::ControlOfWord(map::kTouch, false, L"Stick click", &plain) ==
          art::kStickClick &&
      plain &&
      hm::ControlOfWord(map::kTouch, false, L"Stick", &plain) == art::kStick &&
      hm::ControlOfWord(map::kWand, false, L"Pad click", &plain) ==
          art::kStickClick &&
      hm::ControlOfWord(map::kTouch, true, L"Menu", &plain) == art::kMenu &&
      hm::ControlOfWord(map::kTouch, false, L"", &plain) == -1 &&
      hm::ControlOfWord(map::kTouch, false, L"Gripper", &plain) == -1;
  Check(words, "each button word names its control, \"Stick click\" is not "
        "\"Stick\", and a word with x2 or hold is not plain", words, 1);

  // THE PRESS.
  const hm::Trs a = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f},
                     {1.0f, 1.0f, 1.0f}};
  const float s45 = sinf(0.3927f), c45 = cosf(0.3927f);
  const hm::Trs b = {{0.0f, -0.004f, 0.0f}, {s45, 0.0f, 0.0f, c45},
                     {1.0f, 1.0f, 1.0f}};
  hm::Trs mid, end;
  hm::LerpTrs(a, b, 0.5f, &mid);
  hm::LerpTrs(a, b, 2.0f, &end);
  const bool lerp = fabsf(mid.t[1] + 0.002f) < 1e-6f &&
                    fabsf(end.q[0] - s45) < 1e-5f &&
                    fabsf(mid.q[0] - sinf(0.19635f)) < 2e-3f;
  Check(lerp, "the press goes on the line between the min and the max node, "
        "and stops at the max node", mid.t[1], -0.002);
  const bool weight = hm::ResponseWeight(hm::kAxisButton, 0.25f) == 0.25f &&
                      hm::ResponseWeight(hm::kAxisX, -1.0f) == 0.0f &&
                      hm::ResponseWeight(hm::kAxisX, 1.0f) == 1.0f &&
                      hm::ResponseWeight(hm::kAxisY, 1.0f) == 0.0f &&
                      hm::ResponseWeight(hm::kAxisY, 0.0f) == 0.5f &&
                      hm::ResponseWeight(hm::kAxisButton, 3.0f) == 1.0f;
  Check(weight, "a button gives its value, an axis gives (value + 1) / 2, "
        "and the y axis of WebXR is down", weight, 1);
  hm::Matrix ma, mb, mab;
  hm::MatrixOfTrs(b, &mb);
  const hm::Trs moved = {{0.1f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f},
                         {2.0f, 2.0f, 2.0f}};
  hm::MatrixOfTrs(moved, &ma);
  hm::MatrixMultiply(ma, mb, &mab);
  const float p[3] = {0.0f, 0.0f, 0.01f};
  float q1[3], q2[3], q3[3];
  hm::MatrixPoint(mb, p, q1);
  hm::MatrixPoint(ma, q1, q2);
  hm::MatrixPoint(mab, p, q3);
  const bool chain = fabsf(q2[0] - q3[0]) < 1e-6f &&
                     fabsf(q2[1] - q3[1]) < 1e-6f &&
                     fabsf(q2[2] - q3[2]) < 1e-6f &&
                     fabsf(q1[1] - (-0.004f - 0.01f * sinf(0.7854f))) < 1e-5f;
  Check(chain, "a node in its parent is the product of the two matrices",
        q3[0], q2[0]);

  // THE CROSSING OF TWO LINES.
  const float o[2] = {0.0f, 0.0f}, x1[2] = {1.0f, 1.0f};
  const float y0[2] = {0.0f, 1.0f}, y1[2] = {1.0f, 0.0f};
  const float z0[2] = {2.0f, 0.0f}, z1[2] = {3.0f, 1.0f};
  const bool cross = hm::SegmentsCross(o, x1, y0, y1) &&
                     !hm::SegmentsCross(o, x1, z0, z1) &&
                     !hm::SegmentsCross(o, x1, x1, z0);
  Check(cross, "two lines that cross, two that do not, and two that only "
        "touch at an end", cross, 1);

  // THE CARDS OF THE BUTTONS, in 3000 poses of play with 1 to 4 cards.
  uint32_t seed = 4242u;
  int placed = 0, crossed = 0, overlapped = 0, covered = 0, off_edge = 0;
  int outer = 0, faced = 0, poses = 0, cards = 0, off_middle = 0;
  float worst_middle = 0.0f;
  float worst_roll = 0.0f;    // the height of one end of a card over the other, in the view
  float worst_column = 0.0f;  // the spread of the inner edges across the view
  float worst_reach = 0.0f;
  for (int n = 0; n < 3000; ++n) {
    float q[4];
    Turn(Random(&seed) * 80.0f - 40.0f, Random(&seed) * 100.0f - 80.0f,
         Random(&seed) * 120.0f - 60.0f, q);
    const bool left = (n & 1) != 0;
    const float head[3] = {0.0f, 0.0f, 0.0f};
    const float grip[3] = {(left ? -1.0f : 1.0f) * (0.05f + Random(&seed) * 0.25f),
                           -0.20f - Random(&seed) * 0.30f,
                           -0.25f - Random(&seed) * 0.35f};
    float outline[hint_card_math::kOutlinePoints + 8][3];
    hint_card_math::HandOutline(grip, q, left, 0.19f, outline);
    const float half[3] = {0.035f, 0.045f, 0.07f};
    float center[3];
    const float shift[3] = {0.0f, 0.01f, -0.02f};
    float turned[3];
    controller_math::QuatRotate(q, shift, turned);
    for (int k = 0; k < 3; ++k) center[k] = grip[k] + turned[k];
    // The palm corners (the first 8 points of HandOutline) and the box of
    // the controller, as hint_model.cpp gives them.
    BoxCorners(center, q, half, &outline[8]);
    hm::StackIn in;
    memset(&in, 0, sizeof(in));
    for (int k = 0; k < 3; ++k) {
      in.head[k] = head[k];
      in.center[k] = center[k];
    }
    in.outline = outline;
    in.outline_count = 16;
    in.side = 0;
    in.first = left ? -1 : 1;
    in.last_right[0] = 1.0f;
    in.gap = 0.02f;
    in.spacing = 0.006f;
    // A head that looks 25 degrees down.
    in.head_right[0] = 1.0f;
    in.head_up[1] = cosf(25.0f * 0.0174533f);
    in.head_up[2] = -sinf(25.0f * 0.0174533f);
    in.count = 1 + static_cast<int>(Random(&seed) * 3.99f);
    for (int i = 0; i < in.count; ++i) {
      in.width[i] = 0.04f + Random(&seed) * 0.08f;
      in.height[i] = 0.03f;
      in.has_anchor[i] = !(i == 0 && Random(&seed) < 0.2f);
      const float local[3] = {(Random(&seed) * 2.0f - 1.0f) * half[0],
                              (Random(&seed) * 2.0f - 1.0f) * half[1],
                              (Random(&seed) * 2.0f - 1.0f) * half[2]};
      float a_turned[3];
      controller_math::QuatRotate(q, local, a_turned);
      for (int k = 0; k < 3; ++k) in.anchor[i][k] = center[k] + a_turned[k];
    }
    ++poses;
    hm::StackOut out;
    if (!hm::Stack(in, &out)) continue;
    ++placed;
    if (out.crossings != 0) ++crossed;
    if (out.side == (left ? -1 : 1)) ++outer;
    {
      // The view of the head: x along its right, y along its up, over the
      // depth along its forward.
      const float fwd[3] = {0.0f, -sinf(25.0f * 0.0174533f),
                            -cosf(25.0f * 0.0174533f)};
      auto view = [&](const float p[3], float o[2]) {
        const float d = p[0] * fwd[0] + p[1] * fwd[1] + p[2] * fwd[2];
        o[0] = (p[0] * in.head_right[0] + p[1] * in.head_right[1] +
                p[2] * in.head_right[2]) / d;
        o[1] = (p[0] * in.head_up[0] + p[1] * in.head_up[1] +
                p[2] * in.head_up[2]) / d;
      };
      float x_low = 1e9f, x_high = -1e9f;
      for (int i = 0; i < in.count; ++i) {
        const float x_axis[3] = {1.0f, 0.0f, 0.0f};
        float cx[3];
        controller_math::QuatRotate(out.orientation[i], x_axis, cx);
        float end0[3], end1[3], va[2], vb[2], ve[2];
        for (int k = 0; k < 3; ++k) {
          end0[k] = out.center[i][k] - cx[k] * 0.5f * in.width[i];
          end1[k] = out.center[i][k] + cx[k] * 0.5f * in.width[i];
        }
        view(end0, va);
        view(end1, vb);
        view(out.edge[i], ve);
        if (fabsf(va[1] - vb[1]) > worst_roll) worst_roll = fabsf(va[1] - vb[1]);
        if (ve[0] < x_low) x_low = ve[0];
        if (ve[0] > x_high) x_high = ve[0];
      }
      if (x_high - x_low > worst_column) worst_column = x_high - x_low;
    }
    {
      float lowest = 1e9f, highest = -1e9f;
      for (int i = 0; i < in.count; ++i) {
        const float h = (out.center[i][0] - center[0]) * out.up[0] +
                        (out.center[i][1] - center[1]) * out.up[1] +
                        (out.center[i][2] - center[2]) * out.up[2];
        if (h < lowest) lowest = h;
        if (h > highest) highest = h;
      }
      const float middle = fabsf(0.5f * (lowest + highest));
      if (middle > worst_middle) worst_middle = middle;
      if (middle > 0.015f) ++off_middle;
    }
    hint_card_math::Card card[hm::kMaxCards];
    for (int i = 0; i < in.count; ++i) {
      for (int k = 0; k < 3; ++k) card[i].center[k] = out.center[i][k];
      for (int k = 0; k < 4; ++k) {
        card[i].orientation[k] = out.orientation[i][k];
      }
      // The line ends on the inner edge of its card, along the width of the
      // card as it is turned.
      float d[3];
      for (int k = 0; k < 3; ++k) d[k] = out.center[i][k] - out.edge[i][k];
      const float x_axis[3] = {1.0f, 0.0f, 0.0f};
      float card_right[3];
      controller_math::QuatRotate(out.orientation[i], x_axis, card_right);
      const float along = d[0] * card_right[0] + d[1] * card_right[1] +
                          d[2] * card_right[2];
      if (fabsf(fabsf(along) - 0.5f * in.width[i]) > 1e-4f ||
          fabsf(controller_math::Distance(out.center[i], out.edge[i]) -
                0.5f * in.width[i]) > 1e-4f) {
        ++off_edge;
      }
      ++cards;
      const float reach = controller_math::Distance(out.center[i], center);
      if (reach > worst_reach) worst_reach = reach;
      // The front of the card faces the head.
      const float front_local[3] = {0.0f, 0.0f, 1.0f};
      float front[3];
      controller_math::QuatRotate(out.orientation[i], front_local, front);
      float to_head[3];
      for (int k = 0; k < 3; ++k) to_head[k] = head[k] - out.center[i][k];
      overlay_math::Normalize(to_head);
      if (front[0] * to_head[0] + front[1] * to_head[1] +
              front[2] * to_head[2] > 0.99985f) {
        ++faced;
      }
      if (hint_card_math::CardCoversAny(card[i], in.width[i], in.height[i],
                                        0.0f, head, outline,
                                        in.outline_count)) {
        ++covered;
      }
    }
    for (int i = 0; i < in.count; ++i) {
      for (int j = i + 1; j < in.count; ++j) {
        if (hint_card_math::CardsOverlap(card[i], in.width[i], in.height[i],
                                         card[j], in.width[j], in.height[j],
                                         head)) {
          ++overlapped;
        }
      }
    }
  }
  Check(placed == poses, "each pose of play places its cards", placed, poses);
  Check(crossed == 0, "no two lines from the buttons to their cards cross in "
        "the view", crossed, 0);
  Check(overlapped == 0, "no two cards of a hand overlap in the view",
        overlapped, 0);
  Check(covered == 0, "no card covers a point of the hand or of the "
        "controller in the view", covered, 0);
  Check(off_edge == 0, "each line ends at the middle of the inner edge of "
        "its card", off_edge, 0);
  Check(faced == cards, "each card faces the head from its own middle "
        "(less than 1 degree off its line of sight)", faced, cards);
  Check(outer == placed, "each pose has the cards on the outer side of the "
        "hand", outer, placed);
  Check(worst_roll < 0.001f, "each card is level in the view of a head that "
        "looks 25 degrees down", worst_roll, 0.001);
  Check(worst_column < 0.01f, "the inner edges of the cards of a hand are on "
        "a vertical line in the view (spread under 0.6 degrees)", worst_column,
        0.01);
  Check(off_middle == 0, "the middle of each column is within 1.5 cm of the "
        "height of the middle of the controller", worst_middle, 0.015);
  Check(worst_reach < 0.35f, "no card is more than 35 cm from the middle of "
        "the controller", worst_reach, 0.35);

  // TWO BUTTONS IN THE WRONG ORDER: the trigger under the face buttons and
  // a card list with the trigger first. Stack puts the card of the face
  // button on top.
  {
    float outline[8][3];
    const float center[3] = {0.2f, -0.3f, -0.45f};
    const float q[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    const float box[3] = {0.035f, 0.045f, 0.07f};
    BoxCorners(center, q, box, outline);
    hm::StackIn in;
    memset(&in, 0, sizeof(in));
    for (int k = 0; k < 3; ++k) in.center[k] = center[k];
    in.outline = outline;
    in.outline_count = 8;
    in.first = 1;
    in.last_right[0] = 1.0f;
    in.gap = 0.02f;
    in.spacing = 0.006f;
    in.count = 2;
    for (int i = 0; i < 2; ++i) {
      in.width[i] = 0.06f;
      in.height[i] = 0.03f;
      in.has_anchor[i] = true;
    }
    const float trigger[3] = {0.2f, -0.33f, -0.48f};
    const float button[3] = {0.21f, -0.27f, -0.44f};
    for (int k = 0; k < 3; ++k) {
      in.anchor[0][k] = trigger[k];
      in.anchor[1][k] = button[k];
    }
    hm::StackOut out;
    const bool ok = hm::Stack(in, &out) && out.slot[1] == 0 &&
                    out.slot[0] == 1 && out.crossings == 0 && out.side == 1;
    Check(ok, "the card of the higher button goes on top, so the two lines "
          "do not cross", out.slot[1], 0);
  }

  // THE HAND WHILE THE CONTROLLER SHOWS.
  const bool faint = hm::HandOpacity(0.0f) == 1.0f &&
                     fabsf(hm::HandOpacity(1.0f) - 0.68f) < 1e-6f &&
                     hm::HandOpacity(2.0f) == hm::HandOpacity(1.0f) &&
                     fabsf(hm::HandOpacity(0.5f) - 0.84f) < 1e-6f;
  Check(faint, "the hand goes to 68 percent while the controller shows, and "
        "back with the fade", hm::HandOpacity(1.0f), 0.68);
  // Half of the clear part of the old 35 percent: 1 - (1 - 0.35) / 2.
  Near("68 percent is the old clear part halved, to a percent",
       hm::HandOpacity(1.0f), 1.0 - (1.0 - 0.35) * 0.5, 0.0051);
  Near("hint_hand_opacity_pct=35 gives the old opacity",
       hm::HandOpacity(1.0f, hm::kHandOpacityPctOld), 0.35, 1e-6);
  Near("hint_hand_opacity_pct=100 keeps the hand", hm::HandOpacity(1.0f, 100),
       1.0, 1e-6);

  // THE PULSE OF A LIT BUTTON.
  Near("the pulse starts at full light", hm::PulseLevel(0.0), 1.0, 1e-6);
  Near("half a beat later it is dim", hm::PulseLevel(0.5 / hm::kPulseHz),
       hm::kPulseDim, 1e-5);
  Near("one beat later it is full again", hm::PulseLevel(1.0 / hm::kPulseHz),
       1.0, 1e-5);
  Near("a beat is one second", hm::kPulseHz, 1.0, 1e-6);
  {
    // The light stays in the range, and no frame of 90 Hz steps by more
    // than 4 percent: a smooth beat, with no flash.
    float low = 1.0f, high = 0.0f, step = 0.0f;
    float last = hm::PulseLevel(0.0);
    for (int f = 1; f <= 900; ++f) {
      const float level = hm::PulseLevel(f / 90.0);
      if (level < low) low = level;
      if (level > high) high = level;
      if (fabsf(level - last) > step) step = fabsf(level - last);
      last = level;
    }
    Check(low >= hm::kPulseDim - 1e-5f && high <= 1.0f + 1e-6f,
          "the pulse stays between the dim level and full light", low,
          hm::kPulseDim);
    Check(step < 0.04f, "a frame of 90 Hz changes the light by less than 4 "
          "percent", step, 0.04);
  }
}

}  // namespace hint_model_checks
