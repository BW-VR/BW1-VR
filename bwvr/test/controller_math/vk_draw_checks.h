// The bound state, the sort and the merge of the draws of the walk, a test
// (vk_draw_math.h). main.cpp includes this file after Check, just before
// main.
#pragma once

#include "../../src/mod/vk_draw_math.h"

namespace vk_draw_checks {

inline vk_draw_math::Draw Opaque(uint64_t pipeline, uint64_t set,
                                 const uint32_t* push) {
  vk_draw_math::Draw d = {};
  d.pipeline = pipeline;
  d.set = set;
  d.cull = 1;
  d.zenable = 1;
  d.zwrite = 1;
  d.zfunc = vk_draw_math::kLessEqual;
  d.type = vk_draw_math::kTriangleList;
  d.push = push;
  d.push_bytes = 16;
  d.vertex_count = 3;
  return d;
}

inline void VkDrawChecks() {
  using namespace vk_draw_math;
  printf("\nThe bound state, the sort and the merge of the draws of the "
         "Vulkan renderer\n");

  // THE BOUND STATE.
  BoundState b;
  const uint64_t p1 = 0x1111, p2 = 0x2222;
  Check(b.Need(kPipeline, &p1, 8), "the first bind of a pipeline is made", 1,
        1);
  Check(!b.Need(kPipeline, &p1, 8), "the same pipeline again is skipped", 0,
        0);
  Check(b.Need(kPipeline, &p2, 8), "another pipeline is bound", 1, 1);
  Check(b.Need(kPipeline, &p1, 8), "the first pipeline again is bound", 1, 1);
  Check(b.Skipped(kPipeline) == 1 && b.Made(kPipeline) == 3,
        "one skip and three binds of pipelines",
        static_cast<double>(b.Skipped(kPipeline)), 1);
  const float vp1[6] = {0, 0, 100, 80, 0, 1};
  const float vp2[6] = {0, 0, 100, 81, 0, 1};
  Check(b.Need(kViewport, vp1, sizeof(vp1)) && !b.Need(kViewport, vp1, sizeof(vp1)),
        "a viewport is set one time", 1, 1);
  Check(b.Need(kViewport, vp2, sizeof(vp2)), "a viewport that differs in one "
        "value is set", 1, 1);
  Check(b.Need(kCullMode, &p1, 4), "a slot is apart from the other slots", 1,
        1);
  b.Reset();
  Check(b.Need(kPipeline, &p1, 8) && b.Need(kViewport, vp2, sizeof(vp2)),
        "after a reset (a new command buffer) each state is bound again", 1,
        1);
  b.Forget(kPipeline);
  Check(b.Need(kPipeline, &p1, 8) && !b.Need(kViewport, vp2, sizeof(vp2)),
        "a forgotten slot binds again, the other slots stay", 1, 1);
  Check(b.Need(kSets, &p1, 4) && b.Need(kSets, &p1, 8),
        "a value of another size is bound", 1, 1);
  BoundState off;
  off.SetOn(false);
  Check(off.Need(kPipeline, &p1, 8) && off.Need(kPipeline, &p1, 8) &&
            off.SkippedAll() == 0,
        "vk_bind_skip=0: each bind is made", 1, 1);
  uint8_t big[kMostStateBytes + 1] = {};
  Check(b.Need(kPush, big, sizeof(big)) && b.Need(kPush, big, sizeof(big)),
        "a value over the size of a slot is always bound", 1, 1);

  // THE SORT: which draws may move.
  const uint32_t push_a[4] = {1, 2, 3, 4};
  const uint32_t push_b[4] = {1, 2, 3, 5};
  Draw d = Opaque(1, 1, push_a);
  Check(MayMove(d), "an opaque draw with a depth test and a depth write may "
        "move", 1, 1);
  d.zfunc = kLess;
  Check(MayMove(d), "with the test LESS too", 1, 1);
  d.zfunc = 8;  // D3DCMP_ALWAYS
  Check(!MayMove(d), "a draw with the test ALWAYS keeps its place", 0, 0);
  d = Opaque(1, 1, push_a);
  d.blend = (1u << 16) | 0x0506;
  Check(!MayMove(d), "a draw with blend keeps its place", 0, 0);
  d = Opaque(1, 1, push_a);
  d.zwrite = 0;
  Check(!MayMove(d), "a draw with no depth write keeps its place", 0, 0);
  d = Opaque(1, 1, push_a);
  d.zenable = 0;
  Check(!MayMove(d), "a draw with no depth test keeps its place", 0, 0);

  // THE SORT: the order.
  Draw list[7] = {Opaque(2, 5, push_a), Opaque(1, 7, push_a),
                  Opaque(2, 5, push_b), Opaque(1, 6, push_a),
                  Opaque(1, 6, push_a), Opaque(3, 1, push_a),
                  Opaque(1, 1, push_a)};
  list[4].blend = 1u << 16;  // a draw with blend in the middle
  uint32_t order[7];
  uint32_t moved = SortDraws(list, 7, false, order);
  bool same = moved == 0;
  for (uint32_t i = 0; i < 7; ++i) same = same && order[i] == i;
  Check(same, "vk_draw_sort=0: the order stays", moved, 0);
  moved = SortDraws(list, 7, true, order);
  // Draws 0 to 3 are a row: by pipeline, then set, stable. Draw 4 stays.
  // Draws 5 and 6 are a row.
  const uint32_t want[7] = {3, 1, 0, 2, 4, 6, 5};
  same = true;
  for (uint32_t i = 0; i < 7; ++i) same = same && order[i] == want[i];
  Check(same, "the rows before and after a draw with blend get a sort, and "
        "the draw with blend keeps its place", order[0], 3);
  Check(moved == 5, "5 draws got another place", moved, 5);
  Check(order[2] == 0 && order[3] == 2,
        "two draws with the same key keep their order (a stable sort)",
        order[2], 0);
  Draw sorted[3] = {Opaque(1, 1, push_a), Opaque(1, 2, push_a),
                    Opaque(2, 1, push_a)};
  Check(SortDraws(sorted, 3, true, order) == 0, "a sorted row does not move",
        0, 0);

  // THE MERGE.
  Draw a = Opaque(1, 1, push_a);
  a.vertex_at = 1024;
  a.vertex_count = 6;
  Draw c = Opaque(1, 1, push_a);
  c.vertex_at = 1024 + 6 * 32 + 64;  // after the indices of a, at a vertex
  c.index_count = 6;
  Check(CanJoin(a, c), "two lists with the same state join", 1, 1);
  Draw e = c;
  e.push = push_b;
  Check(!CanJoin(a, e), "other push constants do not join", 0, 0);
  e = c;
  e.set = 2;
  Check(!CanJoin(a, e), "another descriptor set does not join", 0, 0);
  e = c;
  e.set1 = 9;
  Check(!CanJoin(a, e), "a draw with texture stage 1 does not join", 0, 0);
  e = c;
  e.type = 5;  // D3DPT_TRIANGLESTRIP
  Check(!CanJoin(a, e), "a strip does not join", 0, 0);
  e = c;
  e.cull = 2;
  Check(!CanJoin(a, e), "another cull mode does not join", 0, 0);
  e = c;
  e.zwrite = 0;
  Check(!CanJoin(a, e), "another depth write does not join", 0, 0);
  e = c;
  e.vertex_at = a.vertex_at + 16;
  Check(!CanJoin(a, e), "vertices that are not at a full vertex of the base "
        "do not join", 0, 0);
  e = c;
  e.vertex_at = a.vertex_at - 32;
  Check(!CanJoin(a, e), "vertices before the base do not join", 0, 0);
  e = c;
  e.index_count = 2;
  Check(!CanJoin(a, e), "a draw with no full triangle does not join", 0, 0);
  e = c;
  e.index_count = 7;
  Check(ListIndices(e) == 6, "a list of 7 indices draws 6", ListIndices(e), 6);
  Check(ListIndices(a) == 6, "a list of 6 vertices with no indices draws 6",
        ListIndices(a), 6);

  uint32_t out[6];
  JoinIndices(a, a, nullptr, out);
  Check(out[0] == 0 && out[5] == 5, "the base draw with no indices gives 0 to "
        "5", out[5], 5);
  const uint16_t idx[6] = {0, 1, 2, 2, 1, 3};
  JoinIndices(a, c, idx, out);
  const uint32_t base = (6 * 32 + 64) / 32;  // 8
  Check(out[0] == base && out[3] == base + 2 && out[5] == base + 3,
        "the indices of the next draw point to its own vertices", out[5],
        base + 3);
}

}  // namespace vk_draw_checks
