// THE DRAW OF A MESH WITH SSE (docs/backend.md, "The parts with SSE").
// main.cpp includes this file after fast_transform_checks.h, just before main.
//
// The references are the x87 code of the game, instruction by instruction:
// the test of the back face of fn_00856D40 and its three siblings, and the
// vertex loops of fn_00859530.
// Each check compares the bits of the SSE functions with them, with the x87
// unit at the precision of 24 bits that Direct3D sets.
#pragma once

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "../../src/mod/part_draw_math.h"

namespace part_draw_checks {

using fast_transform_checks::Random;
using fast_transform_math::Consts;
using fast_transform_math::Light;

// fn_00856D40 from 0x008572AB to 0x008572F5: gives true when the triangle
// stays (the area is above 0).
inline bool RefFront(const uint8_t* table1, uint32_t i0, uint32_t i1, uint32_t i2) {
  const float zero = 0.0f;
  const uint8_t* p0 = table1 + static_cast<size_t>(i0) * 32;
  const uint8_t* p1 = table1 + static_cast<size_t>(i1) * 32;
  const uint8_t* p2 = table1 + static_cast<size_t>(i2) * 32;
  int front = 0;
  __asm {
    mov edi, p0
    mov esi, p1
    mov eax, p2
    fld dword ptr [eax + 4]
    fsub dword ptr [edi + 4]
    fld dword ptr [esi]
    fsub dword ptr [edi]
    fmulp st(1), st
    fld dword ptr [esi + 4]
    fsub dword ptr [edi + 4]
    fld dword ptr [eax]
    fsub dword ptr [edi]
    fmulp st(1), st
    fsubp st(1), st
    fcomp zero
    fnstsw ax
    test ah, 0x41
    jne f_back
    mov front, 1
  f_back:
  }
  return front != 0;
}

// The loop after the vertices of fn_00856D40 (and fn_0085AB10 with no flags).
// The clip of the game is `clip`, as in BuildTriangles.
template <typename Clip>
inline uint32_t RefTriangles(const uint16_t* indices, int32_t triangles,
                             bool no_cull, const uint32_t* flags,
                             const uint8_t* table1, uint16_t* list,
                             uint32_t size, Clip&& clip) {
  for (int32_t k = 0; k < triangles; ++k) {
    const uint32_t i0 = indices[3 * k], i1 = indices[3 * k + 1], i2 = indices[3 * k + 2];
    const uint32_t f0 = flags != nullptr ? flags[i0] : 0;
    const uint32_t f1 = flags != nullptr ? flags[i1] : 0;
    const uint32_t f2 = flags != nullptr ? flags[i2] : 0;
    if ((f0 | f1 | f2) == 0) {
      if (no_cull || RefFront(table1, i0, i1, i2)) {
        list[size++] = static_cast<uint16_t>(i0);
        list[size++] = static_cast<uint16_t>(i1);
        list[size++] = static_cast<uint16_t>(i2);
      }
    } else if ((f0 & f1 & f2) == 0) {
      size = clip(i0, i1, i2, size);
    }
  }
  return size;
}

// fn_00859530, one vertex. kind 1: the loop from 0x008595C2 (three offsets),
// kind 2: the loop from 0x00859871 (one offset, for y). The game has the
// output at ecx + 4. Here ecx is the output. `moves` points at the offsets
// of this vertex.
inline void RefOffset(const Consts* c, const Light* l, int kind, const float* moves,
                      const uint8_t* vertex, uint8_t* result, uint32_t* flags) {
  const float one = 1.0f, zero = 0.0f, full = 255.0f;
  const float* dir = l->direction;
  double wide = 0.0;
  int32_t level = 0;
  if (kind == 1) {
    __asm {
      mov esi, c
      mov ebx, vertex
      mov ecx, result
      mov edx, moves
      fld dword ptr [edx]
      fadd dword ptr [ebx]
      fld dword ptr [ebx + 4]
      fadd dword ptr [edx + 4]
      fld dword ptr [ebx + 8]
      fadd dword ptr [edx + 8]
      fld st(2)
      fmul dword ptr [esi]
      fld st(1)
      fmul dword ptr [esi + 0x18]
      faddp st(1), st
      fld st(2)
      fmul dword ptr [esi + 0xc]
      faddp st(1), st
      fadd dword ptr [esi + 0x24]
      fstp dword ptr [ecx]
      fld st(0)
      fmul dword ptr [esi + 0x1c]
      fld st(2)
      fmul dword ptr [esi + 0x10]
      faddp st(1), st
      fld st(3)
      fmul dword ptr [esi + 4]
      faddp st(1), st
      fadd dword ptr [esi + 0x28]
      fstp dword ptr [ecx + 4]
      fmul dword ptr [esi + 0x20]
      fxch st(1)
      fmul dword ptr [esi + 0x14]
      faddp st(1), st
      fxch st(1)
      fmul dword ptr [esi + 8]
      faddp st(1), st
      fadd dword ptr [esi + 0x2c]
      fstp dword ptr [ecx + 0xc]
    }
  } else {
    __asm {
      mov esi, c
      mov ebx, vertex
      mov ecx, result
      mov edx, moves
      fld dword ptr [ebx]
      fld dword ptr [ebx + 4]
      fadd dword ptr [edx]
      fld dword ptr [ebx + 8]
      fld st(0)
      fmul dword ptr [esi + 0x18]
      fld st(2)
      fmul dword ptr [esi + 0xc]
      faddp st(1), st
      fld st(3)
      fmul dword ptr [esi]
      faddp st(1), st
      fadd dword ptr [esi + 0x24]
      fstp dword ptr [ecx]
      fld st(2)
      fmul dword ptr [esi + 4]
      fld st(1)
      fmul dword ptr [esi + 0x1c]
      faddp st(1), st
      fld st(2)
      fmul dword ptr [esi + 0x10]
      faddp st(1), st
      fadd dword ptr [esi + 0x28]
      fstp dword ptr [ecx + 4]
      fmul dword ptr [esi + 0x20]
      fxch st(1)
      fmul dword ptr [esi + 0x14]
      faddp st(1), st
      fxch st(1)
      fmul dword ptr [esi + 8]
      faddp st(1), st
      fadd dword ptr [esi + 0x2c]
      fstp dword ptr [ecx + 0xc]
    }
  }
  __asm {
    mov ebx, c
    mov esi, vertex
    mov ecx, result
    mov edi, flags
    mov edx, [ebx + 0x48]
    mov [ecx + 0x14], edx
    mov edx, dir
    fld dword ptr [edx]
    fmul dword ptr [esi + 0x14]
    fld dword ptr [edx + 8]
    fmul dword ptr [esi + 0x1c]
    faddp st(1), st
    fld dword ptr [edx + 4]
    fmul dword ptr [esi + 0x18]
    faddp st(1), st
    fmul full
    fstp wide
    cvttsd2si eax, wide
    mov level, eax
    mov edx, [esi + 0xc]
    mov [ecx + 0x18], edx
    mov edx, [esi + 0x10]
    mov [ecx + 0x1c], edx
    fld dword ptr [ecx + 0xc]
    fcomp dword ptr [ebx + 0x30]
    fnstsw ax
    test ah, 1
    je k_far
    mov dword ptr [edi], 0x20
    jmp k_x
  k_far:
    mov dword ptr [edi], 0
  k_x:
    fld dword ptr [ecx]
    fcomp dword ptr [ecx + 0xc]
    fnstsw ax
    test ah, 0x41
    jne k_x_left
    mov eax, [edi]
    or eax, 0x10
    jmp k_x_store
  k_x_left:
    fld dword ptr [ecx + 0xc]
    fchs
    fcomp dword ptr [ecx]
    fnstsw ax
    test ah, 0x41
    jne k_y
    mov eax, [edi]
    or eax, 8
  k_x_store:
    mov [edi], eax
  k_y:
    fld dword ptr [ecx + 4]
    fcomp dword ptr [ecx + 0xc]
    fnstsw ax
    test ah, 0x41
    jne k_y_up
    mov eax, [edi]
    or eax, 4
    jmp k_y_store
  k_y_up:
    fld dword ptr [ecx + 0xc]
    fchs
    fcomp dword ptr [ecx + 4]
    fnstsw ax
    test ah, 0x41
    jne k_flags
    mov eax, [edi]
    or eax, 2
  k_y_store:
    mov [edi], eax
  k_flags:
    cmp dword ptr [edi], 0
    jne k_done
    fld one
    fdiv dword ptr [ecx + 0xc]
    fst dword ptr [ecx + 0xc]
    fmul dword ptr [ecx]
    fadd one
    fmul dword ptr [ebx + 0x34]
    fst dword ptr [ecx]
    fcomp zero
    fnstsw ax
    test ah, 1
    je k_sx_top
    mov dword ptr [ecx], 0
    jmp k_sy
  k_sx_top:
    fld dword ptr [ecx]
    fcomp dword ptr [ebx + 0x3c]
    fnstsw ax
    test ah, 0x41
    jne k_sy
    mov eax, [ebx + 0x3c]
    mov [ecx], eax
  k_sy:
    fld dword ptr [ecx + 0xc]
    fmul dword ptr [ecx + 4]
    fmul dword ptr [ebx + 0x38]
    fsubr dword ptr [ebx + 0x38]
    fst dword ptr [ecx + 4]
    fcomp zero
    fnstsw ax
    test ah, 1
    je k_sy_top
    mov dword ptr [ecx + 4], 0
    jmp k_depth
  k_sy_top:
    fld dword ptr [ecx + 4]
    fcomp dword ptr [ebx + 0x40]
    fnstsw ax
    test ah, 0x41
    jne k_depth
    mov eax, [ebx + 0x40]
    mov [ecx + 4], eax
  k_depth:
    fld dword ptr [ebx + 0x30]
    fmul dword ptr [ecx + 0xc]
    fst dword ptr [ecx + 0xc]
    fld one
    fsub st, st(1)
    fstp dword ptr [ecx + 8]
    fstp st(0)
  k_done:
  }
  const uint32_t diffuse = fast_transform_checks::RefDiffuse(level, l);
  memcpy(result + 16, &diffuse, 4);
}

inline void PartDrawChecks() {
  printf("\nThe draw of a mesh with SSE\n");
  unsigned int saved = 0;
  _controlfp_s(&saved, 0, 0);
  unsigned int now = 0;
  _controlfp_s(&now, _PC_24 | _RC_NEAR, _MCW_PC | _MCW_RC);

  Random r = {777u};

  const uint32_t kCount = 4099;
  static uint8_t want[kCount * 32];
  static uint8_t got[kCount * 32];

  // THE TRIANGLES. Meshes of random size. The vertices have x and y on a
  // coarse grid, so that some triangles have an area of exactly 0. A flag
  // table with 0 for most vertices.
  int tri_diff = 0, tri_kept = 0, tri_dropped = 0, tri_clipped = 0;
  static uint8_t table1[300 * 32];
  static uint32_t table0[300];
  static uint16_t indices[3 * 400];
  static uint16_t list_want[3 * 400 + 64];
  static uint16_t list_got[3 * 400 + 64];
  for (int round = 0; round < 400; ++round) {
    const uint32_t vertices = 3 + r.Next() % 290;
    const int32_t triangles = static_cast<int32_t>(r.Next() % 400);
    const bool with_flags = round % 3 != 0;
    const bool no_cull = round % 5 == 0;
    for (uint32_t i = 0; i < vertices; ++i) {
      float f[8] = {};
      f[0] = static_cast<float>(static_cast<int>(r.Next() % 40)) * 8.0f;
      f[1] = static_cast<float>(static_cast<int>(r.Next() % 40)) * 8.0f;
      if (round % 7 == 0) f[0] = r.Range(-1000.0f, 1000.0f);
      if (round % 11 == 0 && i % 9 == 0) f[1] = NAN;
      memcpy(table1 + i * 32, f, 32);
      table0[i] = (r.Next() % 6 == 0) ? (r.Next() % 64) : 0;
    }
    for (int32_t k = 0; k < 3 * triangles; ++k) {
      indices[k] = static_cast<uint16_t>(r.Next() % vertices);
    }
    const uint32_t* flags = with_flags ? table0 : nullptr;
    auto clip = [&](uint32_t i0, uint32_t i1, uint32_t i2, uint32_t size) {
      ++tri_clipped;
      return size + 0u * (i0 + i1 + i2);
    };
    memset(list_want, 0xA5, sizeof(list_want));
    memset(list_got, 0xA5, sizeof(list_got));
    const uint32_t before = static_cast<uint32_t>(tri_clipped);
    const uint32_t a = RefTriangles(indices, triangles, no_cull, flags, table1, list_want, 0, clip);
    const uint32_t mid = static_cast<uint32_t>(tri_clipped);
    const uint32_t b = part_draw_math::BuildTriangles(indices, triangles, no_cull, flags,
                                                      table1, list_got, 0, clip);
    const uint32_t after = static_cast<uint32_t>(tri_clipped);
    if (a != b || memcmp(list_want, list_got, sizeof(list_want)) != 0 ||
        mid - before != after - mid) {
      ++tri_diff;
    }
    tri_kept += static_cast<int>(a / 3);
    tri_dropped += triangles - static_cast<int>(a / 3);
  }
  Check(tri_kept > 5000 && tri_dropped > 5000 && tri_clipped > 500,
        "the triangle checks keep, drop and clip many triangles", tri_kept, tri_dropped);

  // THE OFFSETS. The vertex loops of fn_00859530.
  static uint8_t in[kCount * 32];
  static float offsets[kCount * 3];
  static uint32_t want_flags[kCount];
  static uint32_t got_flags[kCount];
  int off_diff[3] = {0, 0, 0};
  int projected = 0, clipped_flags = 0;
  for (int round = 0; round < 8; ++round) {
    Consts c;
    fast_transform_checks::MakeConsts(&r, &c);
    Light l;
    l.direction[0] = r.Range(-1.0f, 1.0f);
    l.direction[1] = r.Range(-1.0f, 1.0f);
    l.direction[2] = r.Range(-1.0f, 1.0f);
    l.ambient = static_cast<int32_t>(r.Next() % 256);
    l.color = r.Next();
    for (uint32_t i = 0; i < kCount; ++i) {
      fast_transform_checks::FillVertex(&r, in + i * 32, i % 97 == 5 ? 1 : (i % 89 == 7 ? 2 : 0));
      for (int j = 0; j < 3; ++j) offsets[i * 3 + j] = r.Range(-40.0f, 40.0f);
    }
    // A normal that is too long for the level: the result of __ftol.
    float big[3] = {3.0e9f, -3.0e9f, 5.0e10f};
    memcpy(in + 20 * 32 + 20, big, 12);
    float tiny[3] = {-0.003f, 0.001f, 0.002f};
    memcpy(in + 21 * 32 + 20, tiny, 12);
    for (int kind = 1; kind <= 2; ++kind) {
      memset(want, 0x3C, sizeof(want));
      memset(got, 0x3C, sizeof(got));
      const int per = kind == 1 ? 3 : 1;
      for (uint32_t i = 0; i < kCount; ++i) {
        RefOffset(&c, &l, kind, offsets + i * per, in + i * 32, want + i * 32, &want_flags[i]);
      }
      part_draw_math::TransformOffsets(c, l, kind == 1 ? part_draw_math::kXyz : part_draw_math::kY,
                                       in, offsets, kCount, got, got_flags);
      for (uint32_t i = 0; i < kCount; ++i) {
        if (memcmp(want + i * 32, got + i * 32, 32) != 0 || want_flags[i] != got_flags[i]) {
          ++off_diff[kind];
        }
        if (want_flags[i] == 0) ++projected; else ++clipped_flags;
      }
    }
  }
  // Meshes of 1 to 7 vertices: each count of vertices after the blocks of 4.
  int few_diff = 0;
  for (uint32_t few_count = 1; few_count <= 7; ++few_count) {
    Consts c;
    fast_transform_checks::MakeConsts(&r, &c);
    Light l = {{0.3f, -0.8f, 0.5f}, 90, 0xFF80C040u};
    for (uint32_t i = 0; i < few_count; ++i) {
      fast_transform_checks::FillVertex(&r, in + i * 32, 0);
      for (int j = 0; j < 3; ++j) offsets[i * 3 + j] = r.Range(-40.0f, 40.0f);
    }
    for (int kind = 1; kind <= 2; ++kind) {
      memset(want, 0x33, 8 * 32);
      memset(got, 0x33, 8 * 32);
      const int per = kind == 1 ? 3 : 1;
      for (uint32_t i = 0; i < few_count; ++i) {
        RefOffset(&c, &l, kind, offsets + i * per, in + i * 32, want + i * 32, &want_flags[i]);
      }
      part_draw_math::TransformOffsets(c, l, kind == 1 ? part_draw_math::kXyz : part_draw_math::kY,
                                       in, offsets, few_count, got, got_flags);
      if (memcmp(want, got, 8 * 32) != 0 ||
          memcmp(want_flags, got_flags, few_count * 4) != 0) {
        ++few_diff;
      }
    }
  }
  _controlfp_s(&now, saved, _MCW_PC | _MCW_RC);
  Check(projected > 1000 && clipped_flags > 1000,
        "the offset checks have vertices with and with no clip flag", projected, clipped_flags);
  Near("BuildTriangles: meshes with another list or another count of clips than the x87 code "
       "of the game, of 400",
       tri_diff, 0, 0);
  Near("TransformOffsets with three offsets: vertices with other bits than the x87 code of "
       "the game, of 8 times 4099",
       off_diff[1], 0, 0);
  Near("TransformOffsets with one offset: vertices with other bits than the x87 code of the "
       "game, of 8 times 4099",
       off_diff[2], 0, 0);
  Near("TransformOffsets: meshes of 1 to 7 vertices with other bits, and other bytes past "
       "the mesh",
       few_diff, 0, 0);
}

}  // namespace part_draw_checks
