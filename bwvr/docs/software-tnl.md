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
log says so. With `gpu_mesh=0` the game runs as it did before: no hook, no
shader module and no arena of the meshes.

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

No caller reads the value that the functions return. The 40 call sites
overwrite `eax` at once, or read `al` after a new load, so the hook is a
function with no value.

**What the hook does.** A hook at the start of each function (MinHook). A
mesh that can take the GPU path does what the function does after the loops:

1. The checks of the mesh, and the reads of the inputs that the function
   reads before its material: the matrix at `0x00EA9EA0`, the projection
   (`0x00E839E0`, `0x00E839F0`, `0x00E839F4`), the color, the specular color,
   the light at `0x00F03140` and the ambient value at `0x00C39264`.
2. The material, with the same calls as the function (the function of the
   type of the mesh, the tiling, the cull mode). It runs one time for each
   mesh. The state of the device after it (`VkDdrawMeshState`) says which
   parts the mesh needs (below).
3. The data of the mesh go to the renderer one time (below).
4. One record of the stream for the draw: the values of step 1, the shift of
   the texture, the offsets of `fn_00859530`, and the matrices and lights of
   the groups.

The hook does the rest of the work of the function: the counter of the meshes
at `0x00F05184`, and the pointer of the offsets at `0x00F05180` (the game
moves it past the offsets of each mesh), and the two readings of
`h_DrawTriangle` that a batch needs (the interface panel goes back to the eye
pictures, and the first vertex is in the measure of the nearest geometry,
`DrawHooksMeshBatch`).

**The order of the draws.** The five functions draw a mesh at once through
`DrawTriangle`, in the order of their calls. The z sorter of the engine
(`LH3DZSorter::NewZObject`) holds whole objects with a callback, and the
callback calls the function of the mesh later, in the order of the depth. The
functions have no sort of their own, and the triangles of a mesh go to the
device in the order of its indices. So a mesh with blend or with no depth
write needs nothing but its states: the record of the GPU path comes at the
place of the draw of the CPU path, the renderer reads the same states, and the
queue of the renderer moves only draws that may move (`vk_draw_math.h`:
no blend, a depth write, a depth test). A mesh draw is never in a draw of the
merge.

**The parts.** The state of the device after the material gives the reasons
that a mesh needs a part of the path. Each part has a key (the value 0 leaves
that kind of mesh on the CPU path), and a switch for a live test:

| Part | Key | What it takes |
| --- | --- | --- |
| Blend | `gpu_mesh_blend` (0 by default) | a mesh with blend, with no depth write or with no depth test (the glass, the shadows, the effects, the leaves of a tree). Off by default. Known limit with 1: on the Frame the roofs of the houses with snow flicker, and the pictures are old at the game menu when the head turns. Open: the cause |
| Stage 1 | `gpu_mesh_stage1` | a mesh with a texture on stage 1. The vertex shader gives stage 1 the texture coordinates of stage 0, as the vertex format of the game (FVF `0x1C4`) does |
| Shift of the texture | `gpu_mesh_uvshift` | a mesh that `DrawTriangle` shifts (`0x00ECA628`, the values at `0x00ECA62C` and `0x00ECA630`: water and fire). The shift is part of the record and the shader adds it to u and v |
| Groups | `gpu_mesh_group` | a mesh with a matrix for each group of vertices (below) |
| Position of the game | `gpu_mesh_depth` | not a kind of mesh: 1 the vertex shader makes the pixel, the depth and the w as the loops of the game do: the camera point with the sums in the order of the loop (no fused operation), `inv = 1 / cz`, `rhw = near * inv`, `z = 1 - rhw`, `x = (inv cx + 1) half_x`, `y = half_y - (product of inv, cy and half_y in the order of the function)`, each rounded to a float, then the map of the renderer (`vk_scene.vert`) with `w = 1 / rhw`. All with `precise` values and `invariant gl_Position`. A batch on the CPU path and a mesh on the GPU path then get the same pixel and the same depth where the geometry is the same, up to the arithmetic of the GPU. 0 the other vertex shader (the `LEGACY` variants of `vk_mesh.vert`): the linear form `z = w - 1`, with neither `invariant` nor `precise`. See "The layers" |

With `test_keys=1`, Ctrl Shift 6 turns all five parts off and on (the debug
page has the row "GPU mesh parts"), Ctrl Shift 5 turns the position of the game
alone off and on (the other parts stay), Ctrl Shift 4 turns the batches of
vertices (below) off and on, and Ctrl Shift 7 turns `gpu_mesh` between 1 and 0
(the row "GPU mesh"). The default control map of the game has
no action on the digits 5, 6 and 7 (`controls-audit.md`, section 2). With `gpu_mesh=0` in `bwvr.ini` and `test_keys=1`
the hooks go in at the first press of Ctrl Shift 7.

**Which mesh takes the CPU path.** A draw with the clamp of the screen
(`0x00FA9398`), a mesh that `h_DrawTriangle` changes (the glow of a target of
the Hand, the advisor of a cut scene, the box of a speech bubble), a mesh with
more than 20000 vertices or 40000 triangles, a mesh with groups whose counts
are not its vertex count, a mesh of a function that `gpu_mesh_kinds=N` leaves
out (bit k of N is the function k of the table, 31 is the default), and a mesh
of a part that a key turned off. A mesh that a key turned off keeps the CPU
path for 30 frames after its first test, so the material does not run twice
for it in each frame. The log counts each reason apart.

**The groups.** A person or an animal has a matrix for each group of vertices
(the flag in the first word of the mesh group at `0x00FA93BC`). The mesh has
the count of its groups at `+0x20` and a list at `+0x24`, and each entry is a
count of vertices and the index of a matrix in the list of the part at
`0x00E9FE48` (12 floats each). The vertices of the groups follow each other.
The data of the mesh have the ordinal of the group of each vertex (16 bit).
Each draw has the matrices of the groups in the order of the list (in the rows
of the shader) and, for a lit mesh, the light of each group. The renderer keeps
them in a buffer of its own for each slot (a storage buffer, set 2 of the
pipeline layout of the groups, which is made at the first such mesh). The light
of a group is made on the CPU as the game makes it (`fn_0084BA90`,
`fn_0085AB10`): the inverse of the matrix of the group (`LHMatrix::SetInverse`),
the position of the light at `0x00EA9E80` in the space of the group, and the
length 1 from the fast root of `fn_00841170` (a table at the pointer
`0x00EEA394`). The port has the float operations in the order of the game. In
the compare (`gpu_mesh=2`) the hook also calls the two functions of the game
for a light and counts the lights with other bits (the log line). The shader
makes the dot product of the normal and the light in one order for all lit
functions. The game has other orders in `fn_0084BA90`, `fn_0085AB10` and
`fn_00859530`, so the level can differ by 1 for a normal on the edge of a
rounding.

**The data.** A mesh goes into the stream one time (`kMeshData`: the id is
its address, then the vertices of 32 bytes, the indices of 16 bits and the
ordinals of the groups, as the game has them). The words of a mesh
can change at the same address: a new mesh made in the place of an old one, and
other changes that the log shows. A draw tests the pointers, the counts and a
sample over the first, the middle and the last vertex and the first and the
last triangle (`MeshSample`, 8 steps of the sum), once in a frame for each mesh.
A full sum over all words (`MeshHash`, 6 SSE operations for 16 bytes, with the
groups) comes for each mesh every `gpu_mesh_check_frames` frames (30 by default,
spread over the meshes by their address). A change that the sample sees goes
to the renderer at the draw. A change that only the full sum sees (a change of a
few words in the middle) goes after up to that many frames. When a full sum
finds a change, that mesh has a full sum at each draw from then on.
`gpu_mesh_check_frames=0` is a full sum at each draw. The log counts the changes
that the sample found apart from the changes that only the full sum found (with
the longest time since the last full sum), and for the first 12 changes of each kind it says
the mesh, its type and counts, how it was found and the first changed vertex or
block of indices (the hook keeps the sums of the blocks of 32 bytes of each
mesh for this). In the desk run of the Temple save (45 changes in
the first window, none later) the sample found 42, and the full sum found 3, all
of one animated mesh of 60 vertices where a few vertices move: the full sum found
the first change 148 frames late, and the mesh then had a full sum at each draw.
So the default is 30 frames (a change that only the full sum sees is up to half a
second late). The data also go again
when the frame has the whole state (a reader that comes late,
`VkStreamMeshEpoch`) and when the table of the hook has no entry. A record of
data is not a record of a walk (`stream_split.h`), so a frame that loses its
walks keeps it, and `KeepKind` of `stream_ring.h` includes it, so a frame
with mesh data is neither dropped by the writer nor skipped by the reader.

The renderer keeps the data in an arena of 64 MB (`VkSceneMeshData`), made at
the first data of a mesh. A range that a new version replaces goes back after
the walks that can read it are done. The renderer takes no range back from a
mesh that is in use: the game sends the data one time, the renderer cannot ask
for them again, and a mesh with no data would stay out of the picture. When the
arena is full the log says so, and the data wait (one entry for each mesh, up
to 16 MB, `mesh_store_math.h`) for a range that a new version of a mesh frees.
A direct store of a mesh removes its entry, so a try never puts an older
version over a newer one, and the try runs once in a walk. A new version of a
mesh that has no room keeps the old version in use. A renderer in the game
process that cannot take the data (not ready) says so, and the hook gives them
again after 30 frames. A draw with groups needs at least as many matrices as
the data have groups (a smaller draw is left out and counted), because the
shader reads the matrix of the group of each vertex. A failed
shader module or arena stops the meshes only. `kMeshDraw` names the mesh and
has the values of the draw. Both are records of the stream of version 4. The
writer sends no mesh record with `gpu_mesh=0`. A draw of the CPU path has 32
bytes for each vertex, a draw of the GPU path 152 bytes (the record, with the
state of the stream), the offsets of `fn_00859530` (12 bytes for each vertex)
and the groups.

**The shader.** `vk_mesh.vert` makes the camera point `c = M p` (with the
offset added to `p` first, and `M` the matrix of the group for a vertex in a
group), the clip position of the pixel of the game, and the lit color. The
clip position is linear in `c`, so a point behind the camera needs no division:

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
`tools/mesh_shaders.ps1` makes `vk_mesh_vert.h` (the SPIR-V of the three
variants: plain, `OFFSETS` and `GROUPS`) with the glslc of the NDK. Its first
line has the hash of the source, and a check of the math tests fails when the
header is not the header of the source.

**The layers.** A layer is a draw with blend or with no depth write and a depth
test of EQUAL or LESSEQUAL over a mesh (the snow over a roof). Two draws on one
surface give the same depth only when their vertices and their math are the
same. What was checked: both layers take the GPU path in the Frame logs (the
CPU path has only the clamp of the screen, which is the icons of the spells, and
the glow of a target), so the same data go through the same shader.
`DrawTriangle` and `DrawAndClip` change x and y (the clamp) and u and v, and never
z, and no function of the game sets the state ZBIAS to another value than 0
(one call in the code, with the value 0). The line each 10 s counts: the layers
on the GPU path, the layers right after a layer, the CPU batches that are
layers right after a mesh on the GPU path (with the first 8 return addresses of
the callers in `DrawTriangle` and `DrawAndClip`), and the log has the first 16
pairs of layers with the mesh, the type, the function, the vertices, the
matrix and the groups of both. The causes that are left: one layer is a batch
that does not come through the five functions, one layer has groups and the
other has not (the shader reads the matrices from another place), the rounding
of the depth of the GPU (the part `gpu_mesh_depth` and its key Ctrl Shift 5),
and the vertices of the two layers. At a house with snow, compare
`gpu_mesh_depth` 1 and 0 (Ctrl Shift 5), `gpu_mesh_group=0`, and
`gpu_mesh=0` (Ctrl Shift 7). In the desk run of the Temple save about
90000 CPU batches in 10 s are layers right after a mesh on the GPU path. They
come through `DrawTriangle` from `fn_0081C090` (60000), `fn_007A8DB0` (26000) and
`fn_0085C7F0` (7000), not from the five functions. With `gpu_mesh_vertices=1` the
first group goes to the GPU path, and the layers after a mesh on the GPU path
are 82000 in 10 s, 70000 of them from `fn_007A8DB0`.

**Layers on two paths.** The layers that the log names (`fn_00851870`,
`fn_00859D90`, `fn_0084E200`) have blend, a depth write and the depth test EQUAL
(zfunc 3), so a layer passes only where its depth is the depth of the mesh under
it. On the CPU path both come from the x87 code of the game, with the same bits.
A mesh on the GPU path has the depth of the vertex shader, and a layer on the CPU
path over it can differ by a step or two of the depth image. The renderer draws
a draw with the test EQUAL as LESS_OR_EQUAL with a small depth bias
(`vk_equal_depth`, `backend.md`, "The depth test EQUAL"), so such a layer passes.
The checks in float math give a depth that is the same bit for bit for 99.5 to
99.9 percent of the vertices of the two paths, so the bias is a safety margin, and
no run in the game has shown yet that it brings the snow back. With all on the CPU
path (`gpu_mesh=0`) the snow is right. Open: the cause of the missing snow with
`gpu_mesh_blend=0` and of the flicker with `gpu_mesh_blend=1`. The latch below puts
the meshes under such a layer on the CPU path.

**The latch** (`gpu_mesh_latch`, 1 by default, Ctrl Shift 3 with `test_keys=1`).
A mesh class of the engine has a draw function of its own on the CPU path, and
such a function can draw a layer (blend or no depth write, a depth test of
EQUAL or LESSEQUAL) over the meshes of a 3D part. The callers that the Frame log
names (the return address of `DrawTriangle`) are, with their functions:

| Return address | Function | What it is |
| --- | --- | --- |
| `0x00840C5C` | `fn_00840530`, `LH3DSprite::Draw` | a sprite (a billboard, 31 callers). Not a mesh |
| `0x00852BA7` | `fn_00851870` | a mesh class draw, from the draws of the part at `0x0080F061`, `0x0080FC91`, `0x00814A74`, `0x00814B6B`. It makes the texture coordinates from the light and the normal (the fraction of a product, a sheen) |
| `0x0085CED5` | `fn_0085C7F0` | `fn_00859530` with a water plane, from `fn_00816AD0` |
| `0x0085A6D9` | `fn_00859D90` | a mesh class draw (sheen), from `fn_0080AF80` |
| `0x00875627` | `fn_00874AA0` | the land: a patch, from `fn_007FF4F0` and `LH3DIsland::Draw` |
| `0x0081CCB7` | `fn_0081C780` | a draw of an array of vertices like `fn_0081C090`, from `fn_007F7ED0`, `fn_0083E8C0`, the icons of the spells `fn_0067C150` and `fn_0067B3F0` |
| `0x0084E8F6` | `fn_0084E200` | a mesh class draw, from `fn_0080B050` |
| `0x008573E7` | `fn_00856D40` | the CPU path of the mesh function itself (clamp) |
| `0x008517F7` | `fn_00850FC0` | a mesh class draw, from `fn_00810E20` and `fn_00811010` |
| `0x0085AA7A` | `fn_0085A6F0` | a mesh class draw, from `fn_0080AE40` |
| `0x00841159` | `fn_00840CC0`, `LH3DSprite::DrawSpecial1` | a sprite |
| `0x0081C5B6` | `fn_0081C090` | the batches of vertices (below) |
| `0x78E0C2A0` | outside the exe | not named |

The snow of a roof is a layer over a house mesh. The land of Lethys shows
`fn_00851870` and `fn_00859D90` in the log, which the Temple save does not show, so
they are the candidates. The latch does not need the name: when a layer from a
function of a mesh class (`fn_00851870`, `fn_00859D90`, `fn_0084E200`,
`fn_0085A6F0`, `fn_00850FC0`, `fn_0085C7F0`) comes right after a mesh of the five
functions (on the GPU path or on the CPU path), the meshes of the 3D part of that
mesh (`0x00EA1AE8`) take the CPU path for the next 90 frames, and each new layer
of the part keeps the latch. The table of the latched parts has 4096 places (`latch_math.h`): the
first version had 64, and in a snow land more parts than that have a layer at
the same time, so a latch was lost before the next layer could keep it (tens of
thousands of new latches in 10 s, and the flicker stayed). The layer and the
meshes under it then have the same path, as with the glow of a target. A sprite is no layer over a mesh for the
latch, because a snow land has sprites between nearly all meshes. The log has
the first 12 latches (the part, the function of the layer, its state, the mesh)
and each 10 s the new latches by function, the layers that kept a latch, and the
meshes that took the CPU path because of a latch.

**The material.** `RunMaterial` calls the function of the type of the mesh (a
table at `0x00ECA618`), the tiling (`fn_0082FF10` or `fn_0082FF50`) and the
cull mode. Each of them asks the device only when its own cache of the game has
another value, so a run that finds the device as it left it changes nothing.
On the Frame the material takes 2.0 us of 3.2 us for a mesh, against 0.08 us on the
PC. The calls go through indirect calls and the device of the mod (the
reference count of a surface at a change of the texture, the stream record of a
state). `gpu_mesh_timer=1` counts the runs, the runs that changed a state of the
device (a counter in the device), the runs of the mesh of the run before with
no change, and the time of the function of the type, of the tiling and of the
cull mode. `gpu_mesh_material_skip=1` leaves a run out for the mesh of the run
before when the device had no change since and the mesh has no dynamic texture
(the word at `+0x138`).

**The batches of vertices** (`gpu_mesh_vertices`, 0 by default, Ctrl Shift 4 with
`test_keys=1`). `fn_0081C090` draws a batch of vertices that the caller gives
in each call: positions (3 floats), a color for each vertex (a dword), texture
coordinates (2 floats), the triangles as 6 words each (an index every other
word), a material mesh (or none) and a flag for the camera matrix (with 0 it
copies the camera matrix to `0x00EA9E10` and points `0x00EA9EA0` at the copy).
It has 18 callers (text in the world, icons, effects) and no light. It
transforms with the sums in the order `((z + y) + x) + t` for x and
`((y + z) + x) + t` for y and z, tests the clip flags and the back face,
runs the material and calls `DrawTriangle`. The vertices change in each call,
so the data go with each draw (a `kMeshDraw` with the id `kInlineMesh`: the
vertices of 32 bytes with the color in the first word of the normal, and the
indices as 16 bit, three for each triangle). The vertex shader makes the
position of the game with the order of the sums of this function (flag 64) and
takes the color of the vertex (flag 128). The hook takes the batch when it has
no clamp of the screen (`0x00FA9398`), no glow, advisor or bubble, all indices
inside the batch and a state that the parts allow. Else the function runs. The
hook does what the function does before its loops: the copy of the camera
matrix. The log counts the batches on each path and the time.
`fn_007A8DB0` (a sprite, 4 vertices that it makes with its own trigonometry and
`rcpss`, from `fn_007ACD20`) and `fn_0085C7F0` (a mesh with offsets that the
water plane cuts: the flag `0x800`, `fn_0081D4F0` for the cut, and
`fn_0085C0E0` after the draw reads the vertex table) stay on the CPU path. The
first has nothing on the CPU that the GPU could save and its depth is the depth
of the game, the second needs the vertex table and a clip plane.

**The checks** (`test/controller_math/gpu_mesh_checks.h`) compare the C++ math
of `gpu_mesh_math.h` with the loops of the game (`fast_transform_math.h`, the
bits of the x87 code). For 300 random matrices with 64 points each (3295
points in the view): the pixel is the pixel of the game to 0.0003, the depth to
1e-7 and the rhw to 4e-7 relative. The same holds for a viewport with an offset,
a smaller size and a MinZ above 0, and for a render of 4080 by 3264. The clip
test of the GPU is the clip flags of the game, except for a point less than 2
pixels from an edge (the GPU map has the half pixel of Direct3D 9). The lit
color is `LitColor` bit for bit for 20000 random normals with each rounding.
The checks also cover the sum and the sample (the sample sees the first, middle
and last vertex and the first and last triangle, the full sum sees any other
change), the reasons of a state and the parts that take them, the order of the
sort around a mesh with blend, the shift of the texture, the light of a group
(the inverse of the part and the fast root), the groups of a mesh, the records,
the single growth of a record in the writer, the loss of `kMeshDraw` in a frame
with no walks, and the hash of the shader source. The GLSL is not run by a check:
the compare below runs it.

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

**Each 10 s** the log has the line "gpu mesh     : gpu_mesh=1, parts 0xF, in 10
s": the meshes on each path with the share of the GPU path, the vertices,
triangles and offsets on the GPU path, the GPU path by function, what the
meshes on the GPU path have (blend, no depth write, no depth test, stage 1,
shift of the texture), the CPU path by reason and by function, the meshes with
groups, the layers (a draw with blend or no depth write and a depth test of EQUAL
or LESSEQUAL, and how many of them draw the mesh of the draw before), the records of mesh data with their bytes, the full sums, the meshes
whose words changed, what the renderer has, and the time on the frame thread:
the GPU path (the checks, the material, the sum and the record, and in a
process with a renderer also the draw of the renderer), the function of the
game, and the checks of the meshes that went to the CPU path. With
`gpu_mesh_timer=1` a second line has the time of each part of the hook for a
mesh (checks, material, state, consts, data, record), at the cost of a clock
read for each part.

- Not shown: the time of the draws of the meshes on the Frame
  (`object_timer=1`, the sites in `backend.md`, "The parts with SSE", with
  `gpu_mesh=0` and `gpu_mesh=1`), and the time of the GPU of the headset for
  these draws. Only a headset shows both.
- Open: the hook of the mesh functions and `walk_parts=1` hook the same
  start of three functions, and the second one fails. Use `walk_parts=0` with
  `gpu_mesh=1`.
- Open: the snow roofs. On the Frame, the roofs of the houses with snow
  flicker with `gpu_mesh=1`, and the flicker stops when the glow of a target
  sends the meshes of the target to the CPU path. The cause is not known.
  "The layers" lists what was checked.
- Open: the order of the products in the light of a mesh with groups. The
  shader uses one order, the game uses three. The compare gives the pixels that
  differ.

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
