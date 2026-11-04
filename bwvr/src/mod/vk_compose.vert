#version 450
// The vertex shader of the late compose of the eye picture (vk_compose.cpp,
// vk_compose_math.h). A vertex has its place in pixels of the eye picture, so
// the clip position is the place over the size of the picture. The fragment
// shader mixes the outer image and the centre image by the alpha.
//
// Compile with bwvr/tools/compose_shaders.ps1.

layout(location = 0) in vec2 in_pos;       // pixels of the eye picture
layout(location = 1) in float in_alpha;    // the share of the centre image
layout(location = 2) in vec2 in_uv_outer;
layout(location = 3) in vec2 in_uv_center;

layout(push_constant) uniform Compose {
  vec4 scale;  // 2 / width, 2 / height
  vec4 tint;   // the color of the marks of the test (1 when off)
  uvec4 flags;  // x: 1 when the target is an sRGB format
} pc;

layout(location = 0) out float out_alpha;
layout(location = 1) out vec2 out_uv_outer;
layout(location = 2) out vec2 out_uv_center;

void main() {
  gl_Position = vec4(in_pos * pc.scale.xy - 1.0, 0.0, 1.0);
  out_alpha = in_alpha;
  out_uv_outer = in_uv_outer;
  out_uv_center = in_uv_center;
}
