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
// Compile with (OFFSETS: the second shader, with the offset of each vertex):
//   glslc -mfmt=c --target-env=vulkan1.3 -fshader-stage=vertex vk_mesh.vert
//   glslc -mfmt=c --target-env=vulkan1.3 -fshader-stage=vertex -DOFFSETS vk_mesh.vert

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec4 row0;
layout(location = 4) in vec4 row1;
layout(location = 5) in vec4 row2;
layout(location = 6) in vec4 scene;    // near, half_x, half_y, unused
layout(location = 7) in uvec4 colors;  // color, specular, ambient, flags
layout(location = 8) in vec4 light;    // direction xyz in the space of the mesh
#ifdef OFFSETS
layout(location = 9) in vec3 offset;
#endif

layout(push_constant) uniform Draw {
  vec4 viewport;  // X, Y, 2 / Width, 2 / Height
  vec4 depth;     // MinZ, 1 / (MaxZ - MinZ), per_rhw, 0
  uvec4 ops;      // see vk_scene.frag
  vec4 alpha;
  vec4 fog_color;
  uvec4 ops1;
} draw;

layout(location = 0) out vec4 out_diffuse;
layout(location = 1) out vec4 out_specular;
layout(location = 2) out vec2 out_uv;
layout(location = 3) out vec2 out_uv1;

vec4 Unpack(uint c) {
  return vec4(float((c >> 16) & 255u), float((c >> 8) & 255u),
              float(c & 255u), float(c >> 24)) / 255.0;
}

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
  const float cx = dot(row0, q);
  const float cy = dot(row1, q);
  const float cz = dot(row2, q);
  const float near_plane = scene.x;
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
  gl_Position = vec4(xc, yc, zc, w);

  uint color = colors.x;
  if ((colors.w & 1u) != 0u) {
    const float d = (light.z * in_normal.z + light.x * in_normal.x +
                     light.y * in_normal.y) * 255.0;
    const int i = (colors.w & 2u) != 0u ? int(d) : int(roundEven(d));
    const uint ambient = colors.z;
    uint level = ambient;
    if (i >= 0) level = ((255u - ambient) * uint(i) >> 8) + ambient;
    const uint r = (((color >> 16) & 255u) * level) >> 8;
    const uint g = (((color >> 8) & 255u) * level) >> 8;
    const uint b = ((color & 255u) * level) >> 8;
    color = (color & 0xFF000000u) | (r << 16) | (g << 8) | b;
  }
  out_diffuse = Unpack(color);
  out_specular = Unpack(colors.y);
  out_uv = in_uv;
  out_uv1 = vec2(0.0);
}
