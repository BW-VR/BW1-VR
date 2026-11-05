#version 450
// The fragment shader of the late compose of the eye picture (vk_compose.cpp,
// vk_compose_math.h). The eye picture is the outer image, and over the rect of
// the centre the mix of the outer image and the centre image by the alpha of
// the vertices (the soft edge). The mix is in the bytes of the game, as the
// blend of the old compose. The alpha of the picture is (1 - a) + a * a: the
// blend of the old compose, where the outer image has the alpha 1 and the
// centre draws with the alpha a.
//
// A target of an sRGB format encodes what the shader writes. The game gives
// its bytes as they are, so for such a target (flags.x = 1) the shader decodes
// the bytes first, and the target stores them again.
//
// Compile with bwvr/tools/compose_shaders.ps1.

// THE COMPOSE IN PARTS (vk_compose_fast=1): a cell outside the rect of the centre has only
// the outer image, so its pipeline has kOuterOnly=1 and reads one image. THE PANEL BY A
// DRAW: kOuterOnly=2 writes the pixels of the outer image with their alpha.
layout(constant_id = 0) const int kOuterOnly = 0;

layout(set = 0, binding = 0) uniform sampler2D outer_image;
layout(set = 0, binding = 1) uniform sampler2D center_image;

layout(push_constant) uniform Compose {
  vec4 scale;
  vec4 tint;
  uvec4 flags;
} pc;

layout(location = 0) in float in_alpha;
layout(location = 1) in vec2 in_uv_outer;
layout(location = 2) in vec2 in_uv_center;

layout(location = 0) out vec4 out_color;

vec3 SrgbToLinear(vec3 c) {
  const vec3 low = c / 12.92;
  const vec3 high = pow((c + 0.055) / 1.055, vec3(2.4));
  return mix(high, low, lessThanEqual(c, vec3(0.04045)));
}

void main() {
  if (kOuterOnly == 2) {
    out_color = textureLod(outer_image, in_uv_outer, 0.0);
    return;
  }
  float a = kOuterOnly != 0 ? 0.0 : in_alpha;
  vec3 rgb;
  if (a >= 1.0) {
    // The middle of the centre: the outer image has no share here.
    rgb = textureLod(center_image, in_uv_center, 0.0).rgb;
  } else {
    rgb = textureLod(outer_image, in_uv_outer, 0.0).rgb * pc.tint.rgb;
    if (a > 0.0) {
      rgb = mix(rgb, textureLod(center_image, in_uv_center, 0.0).rgb, a);
    }
  }
  if (pc.flags.x != 0u) rgb = SrgbToLinear(rgb);
  out_color = vec4(rgb, (1.0 - a) + a * a);
}
