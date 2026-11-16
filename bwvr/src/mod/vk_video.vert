#version 450
// The vertex shader of the layers in the view for videos (vk_video.cpp,
// docs/backend.md, "The view for videos"). One draw of 4 vertices for each
// quad, as a strip: top left, top right, bottom left, bottom right. The
// push constants give the clip position of each corner, with its w, so the
// device interpolates the texture with the perspective of the eye.
//
// Compile with:
//   glslangValidator -V --target-env vulkan1.3 --vn kVkVideoVert
//       -o vk_video_vert.h vk_video.vert

layout(push_constant) uniform Quad {
  vec4 corner[4];
  vec4 uv;     // u0, v0, u1, v1
  vec4 color;  // the factor of each texel
} quad;

layout(location = 0) out vec2 out_uv;

void main() {
  const int i = gl_VertexIndex;
  gl_Position = quad.corner[i];
  out_uv = vec2((i & 1) != 0 ? quad.uv.z : quad.uv.x,
                (i & 2) != 0 ? quad.uv.w : quad.uv.y);
}
