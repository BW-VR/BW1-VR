#version 450
// The fragment shader of the layers in the view for videos (vk_video.cpp).
// The texture holds the color multiplied by the alpha, as the runtime of
// the headset reads it. The blend is ONE and ONE_MINUS_SRC_ALPHA.
//
// Compile with:
//   glslangValidator -V --target-env vulkan1.3 --vn kVkVideoFrag
//       -o vk_video_frag.h vk_video.frag

layout(set = 0, binding = 0) uniform sampler2D picture;

layout(push_constant) uniform Quad {
  vec4 corner[4];
  vec4 uv;
  vec4 color;
} quad;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;

void main() { color = texture(picture, uv) * quad.color; }
