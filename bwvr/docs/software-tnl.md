# The engine transforms on the CPU

Facts in this file are for `runblack.exe` v1.42, SHA1
`9c9aea96f046911ab1b7e496fae511ed5ebbfefe`. They come from the disassembly.

## Summary

`LH3DTech` is a complete software transform and lighting engine. It projects
every point on the CPU against a matrix at **`0x00EA9E40`**, and `LH3DRender`
only hands the finished screen space vertices to Direct3D.

The meshes of the 3D parts can take this work off the CPU: with `gpu_mesh=1`
the renderer of the app transforms, lights, clips and culls them on the GPU
("The meshes on the GPU").

The Direct3D VIEW transform that the engine also sets is a copy that nothing
in the scene reads. Changes to `LH3DRender::g_d3d_world2view` or to
`D3DTRANSFORMSTATE_VIEW` do not reach the screen.

The matrix at `0x00EA9E40` holds position, orientation and field of view
together. One hook on `LH3DTech::UpdateCamera` owns the camera. The mod uses
this for both eyes (`bwvr/src/mod/eye_camera.cpp`), for the head camera and
for the asymmetric frustum of each eye.

## The Direct3D submission

`LH3DRender::DrawTriangle` at `0x0082F810` ends like this:

```
0x0082F8FD  mov eax, dword ptr [0xeca638]   ; LH3DRender::Direct3DDevice7
0x0082F902  mov esi, dword ptr [eax]        ; its vtable
0x0082F904  push 0                          ; dwFlags
0x0082F906  push edi                        ; dwIndexCount
0x0082F90B  push edi                        ; lpwIndices
0x0082F90C  push edx                        ; dwVertexCount
0x0082F90D  push ecx                        ; lpvVertices
0x0082F90E  push 0x1c4                      ; dwVertexTypeDesc  <-- the FVF
0x0082F913  push 4                          ; D3DPT_TRIANGLELIST
0x0082F915  push eax                        ; this
0x0082F916  call dword ptr [esi + 0x68]     ; slot 26, DrawIndexedPrimitive
```

FVF `0x1C4` is `D3DFVF_XYZRHW | DIFFUSE | SPECULAR | TEX1`. **XYZRHW means the
vertices are already in screen space.** The struct that the decomp calls
`Vertex3D` is 32 bytes and holds x, y, z, rhw, diffuse, specular, u, v. The
name is misleading. All three draw paths use this format. `DrawAndClip` and
`DrawAndClip2D` take their FVF as an argument, and every call passes `0x1C4`.

## Calling conventions

`LH3DRender::DrawTriangle` is **`__fastcall`**, not `__cdecl`. All 26 call
sites look like this:

```
mov ecx, <vertex array>           ; arg 1 in ecx
movzx edx, word ptr [count]       ; arg 2 in edx
push <index count>                ; arg 4 on the stack
push <index array>                ; arg 3 on the stack
call 0x0082F810                   ; the callee ends in "ret 8"
```

`DrawAndClip` and `DrawAndClip2D` are `__cdecl`. They read every argument from
the stack and end in a plain `ret`.

`LH3DTech::UpdateCamera` takes two pointer arguments in `ecx` and `edx`, has no
stack arguments and ends in a plain `ret`. That is MSVC `__fastcall` with two
arguments.

Check the convention of every Intel compiled function before you hook it. The
bw1-decomp signature says `__cdecl` because the mangled name encodes that, and
the mangled name does not record what the Intel compiler did. The epilogue
does: `ret N` means the callee cleans the stack, and a prologue that reads
`ecx` or `edx` before it writes them means register arguments. A wrong
convention leaves the stack 8 bytes out on every call, and the game dies some
seconds later with no clear symptom.

## Where the transform happens

`LH3DTech::ProjectPoint` at `0x00819390`:

```
cam.x = p.x*m[0] + p.y*m[3] + p.z*m[6] + m[9]
cam.y = p.x*m[1] + p.y*m[4] + p.z*m[7] + m[10]
cam.z = p.x*m[2] + p.y*m[5] + p.z*m[8] + m[11]

if (cam.z < near_plane) return 0;          // near plane clip, near at 0x00E839E0

screen_x = (1.0 + cam.x / cam.z) * half_res_x
screen_y = (1.0 - cam.y / cam.z) * half_res_y
```

That is the whole projection. There is no separate projection matrix. The
center of the projection is a hardcoded 1.0. An asymmetric frustum for each
eye must move this center.

`m` is the 12 floats at `0x00EA9E40`. The layout is **4 rows of 3**, with the
translation in the last row, so the camera basis vectors are the **columns**:

- column 0 is the camera right axis,
- column 1 is up,
- column 2 is forward,
- `m[9..11]` is the camera space origin.

This matches the decomp's `LHMatrix::SetIdentityMatrix`, which writes 1.0 into
`m[0]`, `m[4]` and `m[8]`. `g_d3d_world2view` is not an `LHMatrix`. It is the
first 12 floats of a Direct3D 4x4.

### The field of view lives in the matrix

For the same camera, the engine writes two different matrices:

```
Direct3D VIEW row 0     [ -0.7071  -0.4082  -0.5774 ]
world2camera row 0      [ -1.0099  -1.0365  -0.5774 ]
```

Column 0 is scaled by 1.4281 and column 1 by 2.5389. Those are exactly
`InfoTransform.inv_half_tan_fov_x` and `inv_half_tan_fov_y`.
`LH3DTech::UpdateCamera` bakes the field of view into the basis, and
`ProjectPoint` then only divides and scales to half the resolution. So one
matrix carries orientation, position and field of view, and each eye changes
all three. The unscaled copies and the other members of the camera family are
in `stereo-artifacts.md`.

## InfoTransform, at 0x00E839E4

60 bytes. Values at 1280x720:

```
resolution        1280 x 720
aspect x/y        1.7778
half res          640.00, 360.00
inv half tan fov  x 1.4281   y 2.5389
cos half fov      0.8192    (squared 0.5736)
implied fov       70.0 deg horizontal, 43.0 deg vertical
cull cone half    35.0 deg
```

1. The field of view is **70 degrees horizontal and 43 vertical**, and the
   vertical follows the window: `tan(21.5) / tan(35) = 0.5626`, which is 9/16.
   The game adapts the frustum to the aspect of the window.
2. `cos_half_fov` is `cos(35)`, the horizontal half angle. **This is the
   culling cone**, and it is one float in the same struct as the projection.
   A wider frustum for two eyes needs a wider cull cone, and that is one
   change.
3. The **field order in the decomp header is swapped**. `0x1C` holds the
   horizontal term and `0x20` the vertical one, the opposite of the names
   `InvHalfTanFovY` and `InvHalfTanFovX`. The table in `address-map.md` uses
   the order of the binary.

## The camera entry points

Two functions write the whole matrix. Both take a position and a focus point
in `ecx` and `edx`.

| Address | Name | Notes |
| --- | --- | --- |
| `0x00819920` | `LH3DTech::UpdateCamera(LHPoint&, LHPoint&)` | Builds the matrix, then copies it to Direct3D through `LH3DRender::SetD3DMatrix` |
| `0x00819F50` | unnamed, 0x800 bytes | Same shape and same argument registers, its own up vector at `0x00EA1B28` |

Both call `LH3DTech::UpdateWorldToCamera` at `0x00819690`, which normalizes
`focus - position` and builds the basis.

`LH3DTech::UpdateCamera` calls `LHMatrix::SetInverse` right after, so a camera
to world matrix is kept at `0x00EA9E80`. Use it to turn a screen position back
into the world.

Every change to the matrix at `0x00EA9E40` in a long session came through
`LH3DTech::UpdateCamera`. The second writer at `0x00819F50` did not run in
that path. So one hook on one function owns the camera. In gameplay the game
calls it about once per frame, which is the rate that rendering for each eye
needs.

## What this means for the mod

- **One matrix drives everything.** Position, orientation and field of view
  are in the same 12 floats, written by two known functions.
- **Culling is ours.** The cull cone is a single float in `InfoTransform`.
- **The scene never uses the fixed function pipeline.** The backend only
  has to present the finished picture.
- **The scene renders twice**, once for each eye, with two matrices. There
  is no reprojection.

A 180 degree roll of the matrix at `0x00EA9E40` from a hook on
`LH3DTech::UpdateCamera` turns the whole picture upside down. This is the
test for any claim that a change reaches the picture.

## The meshes on the GPU

`gpu_mesh=1` (0 is the default, `gpu_mesh.cpp`, the math in
`gpu_mesh_math.h`, the shader `vk_mesh.vert`) takes the transform, the light,
the clip and the cull of the meshes of the 3D parts off the CPU of the game.
The renderer of the app does them on the GPU (native ARM, no FEX). It needs
`backend=vulkan`. With another backend the meshes take the CPU path, and the
log says so.

**Which functions.** A 3D part draws each mesh through one of five functions
(`backend.md`, "The parts with SSE"). Each one runs the loop over the
vertices, the loop over the triangles, the material, and
`LH3DRender::DrawTriangle`:

| Function | Light | Clip flags | Offsets |
| --- | --- | --- | --- |
| `fn_00856D40` | no | yes | no |
| `fn_0085AB10` | yes (the normal of each vertex, and the mesh color) | no | no |
| `fn_0084BA90` | yes (as `fn_0085AB10`) | yes | no |
| `fn_0085BA30` | no | no | no |
| `fn_00859530` | yes (the level rounds toward 0) | yes | one value for y, or three values, for each vertex |

**What the hook does.** A hook at the start of each function (MinHook). A
mesh that can take the GPU path does what the function does after the loops:

1. The material, with the same calls as the function (the function of the
   type of the mesh, the tiling, the cull mode).
2. A look at the state of the device (`VkDdrawMeshState`): no blend, the
   depth test and the depth write on, and no texture on stage 1. The
   renderer draws such a mesh with stage 0 alone, and a mesh with blend sorts
   by depth in the engine. Else the function of the game runs from its start
   (it runs the material again, which changes nothing).
3. The data of the mesh go to the renderer one time (below).
4. One record of the stream for the draw: the matrix at `0x00EA9EA0`, the
   projection (`0x00E839E0`, `0x00E839F0`, `0x00E839F4`), the color, the
   specular color, the light at `0x00F03140` and the ambient value at
   `0x00C39264`, and the offsets of `fn_00859530`.

The hook reads the color, the specular color and the other values before
the material, as the loops of the game do. It does the rest of the work of
the function: the counter of the meshes at `0x00F05184`, and the pointer of
the offsets at `0x00F05180` (the game moves it past the offsets of each
mesh), and the two readings of `h_DrawTriangle` that a batch needs (the
interface panel goes back to the eye pictures, and the first vertex is in the
measure of the nearest geometry, `DrawHooksMeshBatch`).

**Which mesh takes the CPU path.** A mesh in a group (a matrix for each
group of vertices: the flag in the first word of the mesh group at
`0x00FA93BC`), with a shift of the texture (`0x00ECA628`: water and fire),
with a texture on stage 1, with blend or with no depth write (the glass, the
shadows and the effects), a mesh that `h_DrawTriangle` changes (the glow of a
target of the Hand, the advisor of a cut scene, the box of a speech bubble),
and a mesh with more than 20000 vertices or 40000 triangles. The log counts
each reason. `gpu_mesh_kinds=N` leaves a function on the CPU path: bit k of N
is the function k of the table (31 is the default).

**The data.** A mesh goes into the stream one time (`kMeshData`: the id is
its address, then the vertices of 32 bytes and the indices of 16 bits as the
game has them). A sum over the words (`MeshHash`, 6 SSE operations for 16
bytes) is checked at each draw, and the data go again when it changes, when
the frame has the whole state (a reader that comes late, `VkStreamMeshEpoch`),
and when the table of the hook has no entry. A record of data is not a record
of a walk (`stream_split.h`), so a frame that loses its walks keeps it. The
renderer keeps the data in an arena of 64 MB (`VkSceneMeshData`), and a range
that a new version replaces goes back after the walks that can read it are
done. `kMeshDraw` names the mesh and has the values of the draw. Both are
records of the stream of version 4. A draw of the CPU path has 32 bytes for
each vertex, a draw of the GPU path 152 bytes, and the offsets of
`fn_00859530` (12 bytes for each vertex).

**The shader.** `vk_mesh.vert` makes the camera point `c = M p` (with the
offset added to `p` first), the clip position of the pixel of the game, and
the lit color. The clip position is linear in `c`, so a point behind the
camera needs no division:

```
w   = cz / near
x   = w ax + cx / near kx        kx = half_x (2 / Width), ax = (half_x + 0.5 - X) (2 / Width) - 1
y   = w ay - cy / near ky        (the same for y, which goes down)
z   = 1 / (MaxZ - MinZ) (w (1 - MinZ) - 1)
```

`w = cz / near` is `1 / rhw` of the vertices of the game, so the GPU
interpolates the texture as it does for the vertices of the engine
(`vk_scene.vert`). The near plane of the GPU (`z = 0`) is the near plane of the
engine (`cz = near`), and the GPU clips the sides of the view. The cull is
the cull mode of the device (`D3DRENDERSTATE_CULLMODE`, 3 for a mesh with
the test of the back face, 1 for a mesh with the flag in the byte at `+5`),
and the screen turn of the CPU test is the turn that the renderer culls. The
right eye of one walk moves a point by `per_rhw * rhw` pixels. In clip space
that is the constant `per_rhw * 2 / Width`, so the shader needs no per eye
matrix: the matrix in a record is the camera of the pass (the left eye in one
walk, the eye of the pass in a walk for each eye). The light is the integer
math of `LitColor` (`fast_transform_math.h`): the level is the dot product
times 255, rounded to the nearest whole number (toward 0 for
`fn_00859530`), then `(255 - ambient) level >> 8 + ambient`, and each channel
`(channel level) >> 8`. The color of a lit mesh is the color of the part times
the color at `+0x0C` of the mesh when the word at `+8` is 0 (`fn_007ACF70`).

**The checks** (`test/controller_math/gpu_mesh_checks.h`) compare the C++ math
of `gpu_mesh_math.h` with the loops of the game (`fast_transform_math.h`, the
bits of the x87 code). For 300 random matrices with 64 points each (3295 points in the view): the pixel is the pixel of the
game to 0.0003, the depth to 1e-7 and the rhw to 4e-7 relative. The clip test
of the GPU is the clip flags of the game, except for a point less than 2 pixels
from an edge (the GPU map has the half pixel of Direct3D 9). The lit color is
`LitColor` bit for bit for 20000 random normals with each rounding. The checks also cover the sum, the
state test, the records and the loss of `kMeshDraw` in a frame with no walks.
The GLSL is not run by a check: the compare below runs it.

**The compare** (`gpu_mesh=2`, a test aid on the PC with `one_walk=1`). Each
mesh that can take the GPU path is drawn two times: the function of the game
draws it in the left eye layer (`kOneWalkLeftOnly`), and the GPU path draws
it in the right eye layer (`kOneWalkRightOnly`). The renderer does not move the
right eye of the other draws in this mode, so the layers of a walk are two
renderings of one picture, and the log has their difference:
`vk_scene_test_frame=N` gives it for that frame (and the file
`bwvr-vk-meshdiff-N.bmp`), and `gpu_mesh_compare_every=N` gives it for each
Nth frame, for the functions on the GPU path and then for each function alone.
The line is "vk scene     : mesh compare MASK, ...": the pixels that differ at
all, by more than 24 levels and by more than 80, the most, and the box. The
hand, the halos and the interface of the mod are drawn for each eye, so they
differ in the two layers. The log has the first 6 meshes of each function
with their values.

**Each 10 s** the log has the line "gpu mesh     : gpu_mesh=1, in 10 s": the meshes on
each path, the vertices, triangles and offsets on the GPU path, the CPU path by
function and by reason, the records of mesh data with their bytes, what the
renderer has, and the time on the frame thread for the GPU path (the checks,
the material and the record) and for the function of the game. With `test_keys=1`,
Ctrl Shift F10 steps `gpu_mesh` through 1, 0 and 2, and the debug page Render
has the row "GPU mesh".

- Not shown: the time of the draws of the meshes on the Frame
  (`object_timer=1`, the sites in `backend.md`, "The parts with SSE", with
  `gpu_mesh=0` and `gpu_mesh=1`), and the time of the GPU of the headset for
  these draws. Only a headset shows both.
- Open: the hook of the mesh functions and `walk_parts=1` hook the same
  start of three functions, and the second one fails. Use `walk_parts=0` with
  `gpu_mesh=1`.
- Open: the meshes in a group (a person, an animal, a building with parts that
  move) and the meshes with blend stay on the CPU path. A group needs a matrix
  for each group of vertices, and for `fn_0085AB10` and `fn_0084BA90` also the
  light of each group (`fn_007FB290`).
- Next: the own code of the objects (`fn_00815A70`, the test of the view and the
  choice of the detail) and the class draws on the GPU.

## Open questions

1. Where `DrawAndClip2D` gets its coordinates. It should not go through
   `ProjectPoint`, because it is the 2D and interface route.
2. How to make the projection center asymmetric: patch the two 1.0 constants
   in `ProjectPoint`, or absorb the shift into the matrix.
3. Every caller of `LH3DTech::UpdateCamera`, so that the VR camera can supply
   a position and a focus point instead of only editing the result.
4. Whether the cinematic camera drives the same function. If it does, cut
   scenes are reachable through the same hook.

## Tools

`bwvr/tools/disasm.py` disassembles any address with the bw1-decomp symbol
names attached, finds symbols by regex, and finds references to an address.
It answers a single question in under a second.

```
python bwvr/tools/disasm.py 0x0082F810        # one function
python bwvr/tools/disasm.py --find LH3DTech   # search symbols
python bwvr/tools/disasm.py --xref 0x00EA9E40 # who touches this
```

`bwvr/src/probe/frame_grab.cpp` saves the render target to a BMP from inside
the game. A capture from the desktop needs the window in front, and this does
not.
