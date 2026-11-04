#version 450
#extension GL_EXT_multiview : require
// The vertex shader of the meshes on the GPU (gpu_mesh_math.h, docs/software-tnl.md,
// "The meshes on the GPU"). The game keeps the vertices of a mesh in the
// space of the mesh. The CPU loops of the engine (fn_00856D40, fn_0085AB10,
// fn_00859530) make the camera space point, the light and the pixel. This
// shader makes the same from the same values, and gives the clip space
// position of that pixel, so the GPU clips against the near plane and the
// sides of the view, and culls the back faces (the cull mode of the device).
//
// The values of the draw are the instance data (vertex buffer 1, one
// instance): the rows of the matrix, the projection, the colors and the
// light. The same push constants as vk_scene.vert give the viewport, the
// depth and the eyes. The right eye of one walk moves by per_rhw * rhw, which
// is the constant per_rhw * (2 / Width) in clip space (the move of the pixel
// times w is per_rhw * rhw * w = per_rhw, as rhw = 1 / w).
//
// Six variants (LEGACY is the form of gpu_mesh_depth=0). tools/mesh_shaders.ps1 makes
// vk_mesh_vert.h from them:
//   glslc -mfmt=c --target-env=vulkan1.3 -fshader-stage=vertex vk_mesh.vert
//   OFFSETS: the offset of each vertex (a person or an animal, fn_00859530)
//   GROUPS:  a matrix for each group of vertices (and a light for each group
//            when the mesh is lit). The ordinal of the group of a vertex is
//            vertex buffer 2, and the matrices and the lights are the storage
//            buffer of set 2. The record has their float indices in row0.x
//            (the matrices, 12 floats for each group, the rows as SetMatrix
//            makes them) and row0.y (the lights, 4 floats for each group).

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec4 row0;
layout(location = 4) in vec4 row1;
layout(location = 5) in vec4 row2;
layout(location = 6) in vec4 scene;    // near, half_x, half_y, the shift of u
layout(location = 7) in uvec4 colors;  // color, specular, ambient, flags
layout(location = 8) in vec4 light;    // direction xyz in the space of the mesh, the shift of v
#ifdef OFFSETS
layout(location = 9) in vec3 offset;
#endif
#ifdef GROUPS
layout(location = 9) in uint group;
layout(std430, set = 2, binding = 0) readonly buffer Groups { float g[]; } groups;
#endif

layout(push_constant) uniform Draw {
  vec4 viewport;  // X, Y, 2 / Width, 2 / Height
  vec4 depth;     // MinZ, 1 / (MaxZ - MinZ), per_rhw, 0
  uvec4 ops;      // see vk_scene.frag
  vec4 alpha;
  vec4 fog_color;
  uvec4 ops1;
} draw;

#ifndef LEGACY
invariant gl_Position;
#endif

layout(location = 0) out vec4 out_diffuse;
layout(location = 1) out vec4 out_specular;
layout(location = 2) out vec2 out_uv;
layout(location = 3) out vec2 out_uv1;

vec4 Unpack(uint c) {
  return vec4(float((c >> 16) & 255u), float((c >> 8) & 255u),
              float(c & 255u), float(c >> 24)) / 255.0;
}

#ifndef LEGACY
// The camera coordinate with the sum order of the loops of the game and no fused
// operation. order 0: ((x + y) + z) + t, which the five functions use. order 1
// and 2 are the orders of fn_0081C090: ((z + y) + x) + t for x, ((y + z) + x) + t
// for y and z.
float Chain(vec4 r, vec3 p, uint order) {
  precise float a = r.x * p.x;
  precise float b = r.y * p.y;
  precise float c = r.z * p.z;
  precise float s;
  if (order == 1u) {
    s = ((c + b) + a) + r.w;
  } else if (order == 2u) {
    s = ((b + c) + a) + r.w;
  } else {
    s = ((a + b) + c) + r.w;
  }
  return s;
}
#endif

void main() {
  const uint eyes = (draw.ops.w >> 8) & 3u;
  if (((eyes >> gl_ViewIndex) & 1u) == 0u) {
    gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
    out_diffuse = vec4(0.0);
    out_specular = vec4(0.0);
    out_uv = vec2(0.0);
    out_uv1 = vec2(0.0);
    return;
  }
  vec3 p = in_pos;
#ifdef OFFSETS
  p += offset;
#endif
  const vec4 q = vec4(p, 1.0);
#ifdef GROUPS
  const uint at = uint(row0.x) + group * 12u;
  const vec4 r0 = vec4(groups.g[at], groups.g[at + 1u], groups.g[at + 2u], groups.g[at + 3u]);
  const vec4 r1 = vec4(groups.g[at + 4u], groups.g[at + 5u], groups.g[at + 6u], groups.g[at + 7u]);
  const vec4 r2 = vec4(groups.g[at + 8u], groups.g[at + 9u], groups.g[at + 10u], groups.g[at + 11u]);
#else
  const vec4 r0 = row0;
  const vec4 r1 = row1;
  const vec4 r2 = row2;
#endif
  const float near_plane = scene.x;
#ifdef LEGACY
  // LEGACY (the variant for gpu_mesh_depth=0): the linear form, with no
  // invariant and no precise.
  const float cx = dot(r0, q);
  const float cy = dot(r1, q);
  const float cz = dot(r2, q);
  const float w = cz / near_plane;
  const float kx = scene.y * draw.viewport.z;
  const float ky = scene.z * draw.viewport.w;
  const float ax = (scene.y + 0.5 - draw.viewport.x) * draw.viewport.z - 1.0;
  const float ay = (scene.z + 0.5 - draw.viewport.y) * draw.viewport.w - 1.0;
  float xc = w * ax + cx / near_plane * kx;
  const float yc = w * ay - cy / near_plane * ky;
  if (gl_ViewIndex == 1 && (draw.ops.w & 1u) == 0u) {
    xc += draw.depth.z * draw.viewport.z;
  }
  const float zc = draw.depth.y * (w * (1.0 - draw.depth.x) - 1.0);
#else
  // Flag 8: the position of the game. The loops of the game make the camera point
  // with the sums in a fixed order, then rhw = near * (1 / cz), z = 1 - rhw,
  // x = (1 / cz * cx + 1) half_x and y = half_y - 1 / cz * cy * half_y (the order
  // of the product of y depends on the function, flags 16 and 32), each rounded
  // to a float. The vertex shader of the renderer (vk_scene.vert) then makes the
  // clip position of the pixel from them. This shader does the same with
  // precise values, so a mesh on the GPU path and a batch on the CPU path have
  // the same pixels and the same depth where the geometry is the same. Flag 64:
  // the order of the sums of fn_0081C090. Without flag 8 the clip position is
  // linear in the camera point (w - 1 for z), which has a larger rounding error.
  const uint vertex_order = (colors.w & 64u) != 0u ? 1u : 0u;
  precise float cx = Chain(r0, p, vertex_order);
  precise float cy = Chain(r1, p, vertex_order * 2u);
  precise float cz = Chain(r2, p, vertex_order * 2u);
  const bool game_depth = (colors.w & 8u) != 0u && cz >= near_plane && cz > 1e-4;
  precise float inv = 1.0 / cz;
  precise float rhw = near_plane * inv;
  precise float game_z = 1.0 - rhw;
  precise float w = game_depth ? 1.0 / rhw : cz / near_plane;
  float xc;
  float yc;
  if (game_depth) {
    precise float xg = (inv * cx + 1.0) * scene.y;
    const uint y_order = (colors.w >> 4) & 3u;
    precise float y_product = y_order == 1u   ? (scene.z * inv) * cy
                              : y_order == 2u ? (scene.z * cy) * inv
                                              : (inv * cy) * scene.z;
    precise float yg = scene.z - y_product;
    float sx = xg;
    if (gl_ViewIndex == 1 && (draw.ops.w & 1u) == 0u) sx += draw.depth.z * rhw;
    xc = ((sx + 0.5 - draw.viewport.x) * draw.viewport.z - 1.0) * w;
    yc = ((yg + 0.5 - draw.viewport.y) * draw.viewport.w - 1.0) * w;
  } else {
    const float kx = scene.y * draw.viewport.z;
    const float ky = scene.z * draw.viewport.w;
    const float ax = (scene.y + 0.5 - draw.viewport.x) * draw.viewport.z - 1.0;
    const float ay = (scene.z + 0.5 - draw.viewport.y) * draw.viewport.w - 1.0;
    xc = w * ax + cx / near_plane * kx;
    yc = w * ay - cy / near_plane * ky;
    if (gl_ViewIndex == 1 && (draw.ops.w & 1u) == 0u) {
      xc += draw.depth.z * draw.viewport.z;
    }
  }
  precise float zc = game_depth ? ((game_z - draw.depth.x) * draw.depth.y) * w
                                : draw.depth.y * (w * (1.0 - draw.depth.x) - 1.0);
#endif
  gl_Position = vec4(xc, yc, zc, w);

  uint color = colors.x;
  // Flag 128: the color of each vertex is in the first word of the normal
  // (fn_0081C090 has a color for each vertex and no light).
  if ((colors.w & 128u) != 0u) color = floatBitsToUint(in_normal.x);
  if ((colors.w & 1u) != 0u) {
#ifdef GROUPS
    const uint lb = uint(row0.y) + group * 4u;
    const vec3 l = vec3(groups.g[lb], groups.g[lb + 1u], groups.g[lb + 2u]);
#else
    const vec3 l = light.xyz;
#endif
    const float d = (l.z * in_normal.z + l.x * in_normal.x +
                     l.y * in_normal.y) * 255.0;
    const int i = (colors.w & 2u) != 0u ? int(d) : int(roundEven(d));
    const uint ambient = colors.z;
    uint level = ambient;
    if (i >= 0) level = ((255u - ambient) * uint(i) >> 8) + ambient;
    // Each channel keeps its low 8 bits, as the integer math of the game does
    // when the level is above 255.
    const uint r = ((((color >> 16) & 255u) * level) >> 8) & 255u;
    const uint g = ((((color >> 8) & 255u) * level) >> 8) & 255u;
    const uint b = (((color & 255u) * level) >> 8) & 255u;
    color = (color & 0xFF000000u) | (r << 16) | (g << 8) | b;
  }
  out_diffuse = Unpack(color);
  out_specular = Unpack(colors.y);
  // DrawTriangle adds the shift to u and v. Stage 1 reads the same set of
  // texture coordinates as stage 0 for the vertex format of the game.
  out_uv = in_uv + vec2(scene.w, light.w);
  out_uv1 = out_uv;
}
