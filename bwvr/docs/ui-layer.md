# The interface on its own layer

The goal: the menus, the subtitles, the help text and the Creature panel are
on flat quads that stay at a fixed place in the room. OpenXR draws each quad
as its own layer, over the world. The tooltips and anything else attached to
the world stay in the eye pictures or go to the hint cards.

Marks, as in `tabletop.md`:

- [V] verified: read in the binary with `tools/disasm.py`, or in the source.
- [M] measured in a run.
- [I] inferred. It needs a test before anything depends on it.
- [G] a guess.

## Summary

- The game draws the screen interface in `LH3DRender::FinishFrame`, in the
  callbacks that run after the world, the screen quad and the cinema bars.
  FinishFrame runs inside the scene walk, so this happens once per eye pass.
  The tooltip list draws earlier, inside the walk.
- The mod can move the screen interface to a render target of its own. It
  switches the Direct3D 9 target around those draw calls in eye pass 0 and
  skips them in eye pass 1. dxwrapper's Dd7to9 does not see the switch, so
  the mod puts the target, the depth surface and its own states back.
- With the Vulkan renderer (`vk_panel=1`) the draws for the panel target
  also go into a panel image of the renderer, and the subtitle card and the
  panel layer of a session with Vulkan take that image (`backend.md`, "The
  Vulkan renderer").
- The target reaches OpenXR the way the eyes do: a shared texture, one copy,
  and a third swapchain. It goes out as `XrCompositionLayerQuad` layers in
  the LOCAL space, after the projection layer, with the source alpha flag.
- One target feeds three quads: a menu panel, a help panel and a Creature
  panel. Screens with no scene use an opaque quad for the picture behind the
  menus (planned, section 6).
- The subtitles and the help text have a card of their own (section 17). It
  works with `ui_panel=0`.
- The hints and the prompts of the controllers are on the hint cards
  (section 16 and `controller-hints.md`).
- A note of the game over a thing (the percent of a lesson over the
  Creature) stays in the eye pictures, at the depth of its place (section
  19).
- With `ui_panel=0` and `menu_room=1` (the default), the menus of the game
  are on a panel that is fixed in the room, and the ray of the controller
  points on it (section 20). While a text box has the focus, a keyboard
  shows below that panel (section 21).
- The item `places` of the ring opens a panel of the places of the game and
  its 7 bookmarks in front of the player, fixed in the room. The ray and
  the trigger press its buttons (section 22).
- With `ui_panel=1` the pointer on the panel is not built (section 5). The
  menus draw on the panel at their own pixels, but the pointer still comes
  from the eye view.

## 1. Where the interface is drawn

### 1.1 FinishFrame runs once per eye pass

`GGame::Process3dEngine` `0x0054DA80` walks the scene and ends in
`LH3DRender::FinishFrame` `0x0082F460` (call at `0x0054E41C`) [V]. The mod runs
`Process3dEngine` twice a frame, once per eye (`stereo.cpp`). So FinishFrame,
and all interface drawn inside it, runs once per eye pass [V].

The order inside FinishFrame [V, disassembly of `0x0082F460`]:

| Address | What |
| --- | --- |
| `0x0082F49C` to `0x0082F4C7` | The callbacks without the late flag `0x80000000` |
| `0x0082F4C9` | The sun glare, `fn_0086BB60` |
| `0x0082F5E4` to `0x0082F614` | `ZFUNC` set to `ALWAYS`, then a screen quad (`DrawIndexedPrimitive`, vtable `+0x68`). It sets the depth to 1.0 (`tabletop.md`, "Not built: the ring and the depth key"). |
| `0x0082F686` to `0x0082F6DD` | The cinema bars, only when `0x00EB9950` is not zero |
| `0x0082F6ED` to `0x0082F718` | The callbacks with the late flag. The screen interface draws here. |
| `0x0082F7D3` | `EndScene` (vtable `+0x18`) |

Both callback loops only run when `g_enable_callbacks` `0x00C386D0` is set
[V].

### 1.2 The FinishFrame callbacks

`LH3DRender::RegisterFinishFrameCallback` `0x0082F2C0` takes a priority, a
late flag, a `__stdcall` callback and its argument. It ORs `0x80000000` into
the priority when the flag is set, and keeps the list sorted by that value
[V]. So the late loop runs the late callbacks in order of priority.

| Priority | Late | Callback | Registered by | What it draws |
| --- | --- | --- | --- | --- |
| 100 | no | `0x005C2E30` | `HelpDudeControl::Init` | the advisors [I] |
| 1000 | [I] | `0x004562E0` | `CameraModeNew3::Initialise` | camera work [I] |
| 100 | yes | `0x005C2E10` | `HelpDudeControl::Init` | the advisors, calls `fn_005C3920` [I] |
| 10000 | yes | `0x0081E7D0` | `fn_0081E740` | the rect queue: the subtitle box. It is `call 0x0081E3C0`, then `ret 4` [V]. |
| 20000 | yes | `0x005CD020` | `HelpText` constructor | `fn_005CCAB0`, then `fn_005178D0` (the Creature panel) [V]. Between them: the subtitles, the help text, the hint beside the Hand and the second tooltip copy (`draw_hooks.cpp`). |
| 32768 | yes | `LHVideoPlayer::thedraw` `0x00844E30` | `GGame` constructor | video frames [I] |
| 655360 | yes [I] | `0x006424E0` | `start_system` | every `SetupBox` dialog: the Esc menu, options, load and save |

The late flag of the Setup callback comes from `edi`, which is 1 at
`0x0064326B`. Every path to `0x00643376` goes through `mov edi, 1` there, and
between it and the push at `0x006433D7` there are only calls, which keep `edi`
[V]. The callback is registered at priority `0xA0000` with the late flag.
`FallingSpell::Init` also registers a callback (`0x00526480`) with a flag from
`ebp` [I].

The rect queue callback runs before HelpText in the same pass. So the subtitle
box drawn in a pass is the one that HelpText queued in the pass before [I].

### 1.3 Each part of the interface

| Part | Drawn by | In the frame | Goes to |
| --- | --- | --- | --- |
| Setup dialogs | the Setup callback `0x006424E0` | late callbacks | menu panel (`ui_panel=1`), else the eyes |
| Subtitle letters | `fn_005CCAB0`, through `Draw3DScreenTriangle` | late callbacks | the subtitle card (section 17), else the help panel or the eyes |
| Subtitle box | the rect queue, caller `0x0081E3C0` to `0x0081E58C` | late callbacks | the same |
| Help text | `fn_005CCAB0` | late callbacks | the same |
| Creature panel | `fn_005178D0` | late callbacks | its own quad (section 13) |
| Hint beside the Hand | CameraHelp quads `0x00447450`, inside the HelpText callback | late callbacks | the hint card (section 16) |
| Tooltips | `HelpSystem::Draw3D` `0x005C59A0` (called only from `Process3dEngine` at `0x0054E2ED` [V]) and again from HelpText | in the walk, and late | the hint card, or the Hand with `hint_card=0` (section 15) |
| Screen quad, cinema bars | FinishFrame | before the late callbacks | the eyes. The bars are hidden. |
| Advisors | `HelpDude` code | in the walk and callbacks | the eyes (room camera, `spirit_anchor.cpp`) |
| Speech bubbles (a signpost, a villager that speaks, the Creature) | the bubble draw `0x005767D0`, an object of the Z sort | in the walk | the eyes, as one card at the depth of the sign (section 18) |
| Video in play | `LHVideoPlayer::thedraw` (`fn_00845740` through `DrawAndClip2D`), also from `FallingSpell::Draw` | late callbacks | the eyes. In the cut scene window, each eye moves it onto the screen plane (`cutscene-window.md`, section 7) |

The size, the placement and the shift of the interface in the eye pictures are
in `draw_hooks.cpp` and `backend.md` ("Interface size", "Subtitles",
"Tooltips").

`Creature::Draw` arms the Creature panel only while the Creature is
`GInterface::GetInteractObject` [V]. The box itself is fixed to the screen.

### 1.4 Screens with no scene

FinishFrame has 23 call sites, and only one is in `Process3dEngine` [V,
`disasm.py --callers 0x0082F460`]. The others are in `FrontEnd::JustDoABox`,
`LoadSaveBox` (from `GameLoadingBox` and `GameSavingBox`), `DrawLoading`,
`RenderLoadingFrame`, `DoCitadelDraw`, `DoLogo`, `MakeTipVideo`,
`PlayLogoScreens`, `PlayPreIntroVideo`, `RunAlexFX`, two text screens in
`GGame::Loop`, `fn_00640054`, and unnamed code at `0x0053CA71`, `0x0053CE0F`,
`0x00822779` and `0x00822997`. `JustDoABox` draws the plasma background and
then runs FinishFrame with the callbacks on, so the Setup callback draws its
dialog there too [V].

These frames do not reach `Process3dEngine`. After 150 ms with no stereo
frame, `StereoIdle` is true, and `h_FinishFrame` in `mod_main.cpp` sends the
whole render target through `SubmitTexture`, the same picture to both eyes
[V]. What the headset then shows depends on the session [V, `xr_screen.cpp`
`EndFrame`]:

- **Before the first stereo frame:** two quads, one per eye, 260 cm wide at
  200 cm (`screen_width_cm`, `screen_distance_cm`), in the LOCAL space. This
  screen is fixed in the room.
- **After the first stereo frame:** `stereo.cpp` sets the projection flag
  once and never clears it, and `have_render_pose_` stays true. So a screen
  with no scene goes out as a projection layer. It declares the eye poses of
  the last stereo frame and the render frustum of about 110 degrees. The
  runtime then shows the flat picture over the whole view, with no depth,
  fixed in the room where the head pointed at that last frame [I]. A player
  who turned since then sees the picture to one side and black past its edge.

The menu text is a fixed 20 to 35 px at any render size [M, `backend.md`],
which is about 0.8 to 1.3 degrees at 26 px per degree.

A frame with no scene that shows a menu does not take these routes: with
`menu_room=1` it goes out as a screen at the place of the menu panel
(section 20).

## 2. Drawing the interface to its own target

### 2.1 The choice

The mod switches the Direct3D 9 render target around the panel batches. It
already holds the Direct3D 9Ex device (`gpu_bridge.cpp`), makes shared render
target textures on it, and reads its render target. This depends only on the
three caches in 2.2, not on dxwrapper internals.

Two other ways do not fit. A Direct3D 7 surface with
`IDirect3DDevice7::SetRenderTarget` lets Dd7to9 know about the switch, but it
ties the mod to one dxwrapper version (it needs `IID_GetInterfaceX` to find
the Direct3D 9 surface behind it). A double draw, once into the eye and once
into the panel, only serves as a test mode (`ui_panel=2`), because the goal
is to take the interface out of the eyes.

### 2.2 What Dd7to9 caches

Read in the dxwrapper 1.8.8600.25 source, folder `ddraw` [V]:

| What Dd7to9 keeps | Where | What a switch behind its back does |
| --- | --- | --- |
| The render target, as its own surface objects (`RenderTargetSurface`, `lpCurrentRenderTargetX`) | `SetDrawStates` compares the two before every draw (`IDirect3DDeviceX.cpp`, about line 6713). `SetRenderTargetSurface` (`IDirectDrawX.cpp`, line 4286). | It compares its own pointers, not the device. So it does not see the switch and does not undo it. It also does not put the game's target back. The mod must do that. |
| The viewport | `SetDrawStates` calls `SetViewport` before every draw ("Need to always set viewport"). `Clear` does too (line 5713). | Direct3D 9 `SetRenderTarget` resets the viewport to the new target. Dd7to9 sets its own again at the next draw. A target smaller than that viewport makes the call invalid, so the panel target must be the render size. |
| Render, texture stage and sampler states | `SetD9RenderState` (line 5900) writes a shadow copy and a list of changes. `PrepDevice` (line 6360) sends only that list, at the next draw. | A state that the mod changes and does not put back stays wrong until the game sets it again. |
| The depth surface | set only together with the target (`SetRenderTargetSurface`, line 4329) | Direct3D 9 `SetRenderTarget` does not change it. The game's depth stays bound unless the mod binds its own. |
| Draws | `DrawIndexedPrimitive` calls `DrawIndexedPrimitiveUP` at once (from line 3035). Nothing is kept for later. | A switch around one engine draw call moves exactly that draw. |
| The present | `Flip` calls `SetCurrentRenderTarget` (`IDirectDrawSurfaceX.cpp`, line 1456). The present draw saves and restores the device target (`IDirectDrawX.cpp`, lines 5322 to 5343). | Both run outside FinishFrame, so they do not meet the switch. |

No file in the Dd7to9 source uses `D3DRS_SEPARATEALPHABLENDENABLE`,
`D3DRS_SRCBLENDALPHA` or `D3DRS_DESTBLENDALPHA` [V, a search of
`IDirect3DDeviceX.cpp` and `IDirectDrawX.cpp`]. The mod sets these three
without a conflict and puts them back.

The shipped `dxwrapper.ini` has `DdrawAllowMultiSampling = 1` [V]. The game's
render target is `X8R8G8B8` (format 22) with multisample type 4, quality 0
[M]. The panel target has its own depth surface and no multisampling, so this
does not matter for the panel.

### 2.3 Rules for the switch

1. The panel target has the same size as the game's render target. The bridge
   reads the size with `GetDesc` on the current target when it starts. The
   format is `A8R8G8B8` with no multisampling.
2. The panel target has its own depth surface (`D24S8`) of the same size, with
   no multisampling. The late callbacks draw after the screen quad at
   `0x0082F614` has set the game's depth to 1.0. So a panel depth cleared to
   1.0 gives the same depth test [I: the interface does not read the depth of
   the world].
3. The mod binds the pair when the first panel batch of a span arrives
   (`GpuBridgePanelBind`). It saves target 0, the depth surface, the viewport,
   the scissor rect and the three alpha states of 2.5, binds the panel pair
   and sets the three alpha states. `SetRenderTarget` sets the viewport and the
   scissor rect to the whole target, so both come back from the saved values.
4. At the first panel bind of a frame, the mod clears the panel target to 0
   with `ColorFill` and the panel depth to 1.0 (stencil 0) with `Clear`.
5. `GpuBridgePanelUnbind` puts back all seven saved values. This runs at the
   next eye batch, at the end of the Setup, HelpText, `HelpSystem::Draw3D` and
   rect queue scopes, at every `DrawTriangle` call, and after `FinishFrame`.
   A panel still bound after `FinishFrame` counts as a miss in the log.
6. Before each panel draw in a span, the bind reads target 0 and the depth
   surface again. If either changed, it binds again and counts it in the log.
   This guards against a Dd7to9 path that binds the game's target in the
   middle of a span.
7. The mod draws panel batches in eye pass 0 only. In eye pass 1, it does not
   call the engine draw for a panel batch. `DrawAndClip` and `DrawAndClip2D`
   clip and submit, so a skipped call only loses pixels [I].
8. Fail safe. If the GPU bridge is not ready, the projection layer is off, or
   any create fails, the interface stays in the eyes. With `play_mode` 1 (the
   stereoscopic window) the world is already a screen, so the interface stays
   in it.

The spans are short. The HelpText scope mixes panel batches and the hint, so a
frame can have a few binds. The log counts them.

### 2.4 Which batches go to the panel

`DrawInterface` in `draw_hooks.cpp` decides:

```
panel batch, when ui_panel is on and the rules of 2.3 allow it:
  inside the Setup callback 0x006424E0            (g_menu_depth > 0)
  inside the HelpText callback 0x005CD020 only    (g_helptext_depth, because
                                                   g_help_depth also counts
                                                   HelpSystem::Draw3D)
    and not a tooltip                             (g_in_tooltip)
    and not a kept hint copy                      (g_widget_hint, g_widget_scale,
                                                   g_widget_flat after
                                                   IsDuplicateWidget)
  the rect queue caller 0x0081E3C0 to 0x0081E58C  (the subtitle box)

eye batch: everything else

eye pass 0, or a FinishFrame outside the eye passes: draw a panel batch
  into the panel target
eye pass 1: do not draw a panel batch
```

A panel batch gets none of the eye steps: no interface shift, no interface
size, no subtitle lift, no subtitle placement. The panel has its own depth
and its own size in the room. So the menus sit exactly on their hit test. In
the eye pictures the shift moves them 8 px from it in each eye.

The HelpText scope has its own counter, `g_helptext_depth`. The rect queue callback `0x0081E7D0` (`__stdcall`, one argument, `ret 4` [V])
has a hook that closes the span after the subtitle box. The mod installs it
when `ui_panel` is not 0 or the subtitle card is on. `IsDuplicateWidget` runs
first. A batch it marks as a hint stays in the eyes. A panel batch still
records the subtitle band, because the hint rule reads it. A batch inside
`HelpSystem::Draw3D` stays in the eyes.

### 2.5 A clear background and the alpha

The panel target starts each frame at 0 in all four channels. The interface
then draws over it. With the game's usual blend (source alpha, inverse source
alpha) the color comes out premultiplied, which is what OpenXR wants. The
alpha would come out wrong: the same factors give `a * a` for a first layer.
The separate alpha blend fixes that:

```
D3DRS_SEPARATEALPHABLENDENABLE  TRUE
D3DRS_SRCBLENDALPHA             D3DBLEND_ONE
D3DRS_DESTBLENDALPHA            D3DBLEND_INVSRCALPHA
```

For each blend mode of the game:

| Game blend | Color on the panel | Alpha on the panel |
| --- | --- | --- |
| source alpha, inverse source alpha | premultiplied, exact | "over", exact |
| blend off | the source color | the source alpha. Opaque only when that alpha is 1 [I]. |
| one, one (additive) | added | coverage grows, so a glow shows as a faint shape [I] |

Every menu batch uses source alpha, inverse source alpha, with the depth test
on, the depth write off and the stencil off [M]. The alpha test is on for the
text (`Draw3DScreenTriangle`) and off for the boxes and lines. The help layer
is not measured. The mod reads the blend and the tests after each panel draw
for the census (`ui depth` and `ui layer` log lines), because Dd7to9 sends its
states at the draw.

The composite check proves the math on the desktop (`ui_panel=2`). On a
capture frame, the mod copies the eye target to a spare surface just before
the first panel batch, and draws the panel batches over it the game's way.
The panel composited over the spare (`color + spare * (1 - alpha)`) must equal
that check picture within rounding of 1 or 2 levels.

## 3. From the target to OpenXR

### 3.1 The share

`gpu_bridge.cpp` makes one more shared texture on the same device:
`A8R8G8B8`, `D3DUSAGE_RENDERTARGET`, with a shared handle, opened on the
Direct3D 11 device of the session. This is the eye path again [V,
`GpuBridgeStart`]. The panel target, its depth surface and the shared texture
live and die with the device, through the `Release` hook the bridge already
has.

After eye pass 0 returns, `stereo.cpp` calls `GpuBridgeCaptureUi`, then
`GpuBridgeCaptureEye(0)`. `GpuBridgeCaptureUi` does one `StretchRect` from the
panel target to the shared texture and no flush. The eye 0 capture then
issues its event query flush, and that flush drains every command before it,
the panel copy too [V, `backend.md`, "Keeping the two devices in step"]. So
the panel adds no wait. Then `SetEyeTexture(0)`, `SetPanelTexture` and
`SetPanelQuads` follow.

A frame with no panel batch makes no copy and submits no panel layer.

### 3.2 The quad layers

`XrScreen` has a panel swapchain: the same format as the eyes, the render
size, usage `TRANSFER_DST` and `SAMPLED`. It exists only with `ui_panel=1`.
`SetPanelTexture` acquires, waits, copies with `CopyResource` and releases,
as `SetEyeTexture` does. Then `EndFrame` submits:

```
layers[0] = the projection layer
layers[1] = the menu panel quad, when it has content
layers[2] = the help panel quad, when it has content
layers[3] = the Creature panel quad, when it has content

each quad:
  type          XR_TYPE_COMPOSITION_LAYER_QUAD
  layerFlags    XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT
  space         the LOCAL space the projection layer uses
  eyeVisibility XR_EYE_VISIBILITY_BOTH
  subImage      the panel swapchain, the rect of that panel (4.1), layer 0
  pose, size    from 4.3
```

OpenXR draws later layers over earlier ones. Without
`XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT` the runtime reads the color
as premultiplied, which matches 2.5. The quads read one swapchain image
through several rects, which `EndFrame` clamps to the image. If a runtime
refuses one image in several layers, use a second swapchain and
`CopySubresourceRegion` [I]. `stereo.cpp` passes the menu and the help quad to
`SetPanelQuads`. `ui_panel.cpp` gives the Creature quad to
`XrScreen::StagePanelQuad` before it, in the same frame. A frame that does not
stage it shows no Creature panel.

The size of a quad in meters, for a rect `w` px wide at `k` px per degree and
a distance `d` (`PanelWidth` in `controller_math.h`):

```
width  = 2 * d * tan(w / k / 2 degrees)
height = width * h / w
```

### 3.3 Color and alpha in the swapchain

The eye swapchains prefer `B8G8R8A8_UNORM_SRGB`, because the game's colors are
already gamma encoded [V, `xr_screen.cpp`]. The panel uses the same format.
The game blends in gamma space, so the panel color is premultiplied in gamma
space, and the compositor then blends in linear space. The result is exact for
opaque and fully clear pixels. On the soft edges of letters it is a little
darker than on a monitor [I]. If that shows in the headset, a pixel shader
pass on the Direct3D 11 side can convert the panel to linear premultiplied
color.

## 4. Placement

### 4.1 One panel or two

Three quads come from one target:

- **The menu panel** shows the Setup dialogs.
- **The help panel** shows the subtitles and the help text. Nothing on it
  takes a click.
- **The Creature panel** (section 13) is cut out of the help scope.

The kinds want a different size and a different place. The menus are small
text that a player reads and clicks, straight ahead. The subtitles are large
text that a player reads in a glance, low down. The subtitle bar sits at the
bottom of the render [V, `backend.md`, "Subtitles"]. The Setup dialogs sit
near the middle. A rect for each kind cuts them apart in one target. The rects
are fixed values, not fitted each frame, so a panel never changes size while
the player looks at it. If the kinds ever overlap in the target, use two
targets.

The default rects (`DefaultRect` in `ui_panel.cpp`), used when `ui_menu_rect`
and `ui_help_rect` are 0:

| Rect | Default | Why |
| --- | --- | --- |
| Menu | 1280 by 960 px, in the middle of the render, clamped to it | The menus are a fixed size in pixels and sit in the middle (question 5 of section 11). The front end menu, 780 by 570 px, fits with room for a larger page. |
| Help | the whole width, from 60 percent of the height to the bottom | The subtitle box sits at the bottom of the render. Not measured on the panel. |

At 16 px a degree the menu rect is 80 degrees wide, but most of it is clear.
The front end menu itself is about 49 degrees wide, 1.27 m at 140 cm.

### 4.2 Pixels per degree

The render is about 2880 px across about 110 degrees, which is 26 px per
degree [M, `backend.md`]. On the eye pictures the interface can only be the
angle its pixels give it. The quad size sets the angle on the panel.

- **Menu panel, 16 px per degree** (`ui_menu_px_per_deg`). Menu text of 20 to
  35 px becomes 1.25 to 2.2 degrees. The texture is magnified 1.6 times, so
  the letters are a little soft. The other way is to draw the menus larger in
  the target and give the pointer the inverse scale (5.4), which is sharper.
- **Help panel, 44 px per degree** (`ui_help_px_per_deg`, at a 2880 px tall
  render, scaled with the render height as the help text is). On the panel
  the help layer draws at the game's own size, 77 px letters at a 2880 px
  render [M], which is 1.75 degrees. The compositor then reduces the quad
  about 1.8 times with no mip levels, which can crawl [I]. The other way is
  to keep `ui_scale_pct` in the target and show the quad at about the display
  density.

The checks in a headset are in `headset-checks.md`, and the checks that a
desktop run can prove are in `roadmap.md`, R12.

### 4.3 Normal placement

The anchor is the recenter pose: the head position at recenter and its yaw.
`head_pose.cpp` keeps this pose, and it keeps only the yaw [V]. Each quad
faces the head position of the anchor.

`PanelPose` puts the center `d` meters from the recenter pose, at the pitch of
the panel, straight ahead of the recenter yaw. It turns the quad about its own
x axis by the same pitch, so it faces the recenter head position. The menu
density is fixed, because the menus draw at a fixed size in pixels. The log
prints both poses once, and again after each recenter.

| Quad | Center, from the anchor | Content |
| --- | --- | --- |
| Menu panel | 140 cm forward, 10 degrees below eye level | Setup dialogs |
| Help panel | 120 cm forward, 30 degrees below eye level | subtitles, help text |
| Creature panel | section 13 | the Creature panel |

The advisors float about 43 degrees to each side, 1 to 1.5 m away
(`tabletop.md`, "The spirits beside the table"), so they do not meet the panels.

A quad layer draws over the projection layer everywhere. A panel behind a
nearer object still covers it, and the two depth cues then disagree. In
normal play the land is tens of meters away, so a panel at 1.2 to 1.4 m is
nearer than the world behind it in nearly all views [I].

### 4.4 Tabletop placement (planned)

With the table on, the panels use the normal placement. The plan is to hang
them on the table anchor of `tabletop.md`, "The table camera" (the table center in the room,
80 cm forward and 40 cm down from the recenter pose):

| Quad | Center | Content |
| --- | --- | --- |
| Menu panel | above the far edge of the table, about 170 cm forward, at eye level | Setup dialogs |
| Help panel | at the far edge, 15 cm over the table surface, tilted back to face the head | subtitles, help text |

The island sits below the line of sight to both panels, so the panels cover
little of it. With the Virtual Desktop passthrough mask on, the panels are not
in the key color, so they float in the real room [I]. The planned keys are
`table_menu_up_cm` (40) and `table_help_up_cm` (15). Neither key exists in
the code yet.

### 4.5 Recenter

The panels never follow the head. They move only when the anchor moves. F11
and a recenter from the headset menu
(`XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING`) both clear the origin in
`head_pose.cpp`, and the next frame takes a new one [V]. The panels land in
front of the player on that frame, once. `HeadPoseOrigin` gives the origin
position and yaw in the LOCAL space.

## 5. Input on the panel (planned)

Not built for `ui_panel=1`. `ui_panel_pointer` is not in the code. The
menus in the room (section 20) use the ray against a quad of 5.3 and the
rules of 5.2 for their own panel.

### 5.1 One pointer, two surfaces

`pointer_map.cpp` keeps one pointer [V]. One owner, the mouse or a
controller, sets one float render pixel (`g_true_x`, `g_true_y`). The mod
writes it once a frame to `0x00E852C0` and `0x00E852C4` and to the Windows
cursor. `h_MouseMoveHandler` and `h_MouseSetPosition` convert the window
messages the same way, so the Hand and the interface agree.

The plan keeps this rule and adds one choice per frame: the surface of that
pixel. On the **world**, the pixel is a pixel of the eye view. On the **menu
panel**, the pixel is a pixel of the panel target. The panel target is the
render size. So a panel pixel is the exact pixel the game drew the dialog at,
and the exact pixel its hit test expects [I: the dialogs test the pixels they
draw]. The game still gets one pointer.

The game draws its own teal arrow at the pointer in the Setup dialogs [M],
inside the Setup callback. It lands on the panel with the dialog, at the pixel
that the hit test reads. So the plan needs no mark of its own for the menus.

### 5.2 When the pointer is on the menu panel

The pointer is on the menu panel while a Setup dialog is up, or while a screen
with no scene shows. `SetupBox::GetCurrentActiveBox` `0x00407ED0` returns
`[0x00C4CD20]`, and a value other than 0 means a dialog is up [V].

The game's state decides the surface, not the ray. A ray that leaves the panel
does not drop the pointer onto the island. The pointer keeps its last panel
pixel. A press keeps its surface until its release. A drag that starts on the
panel ends there. A land drag that is held when a dialog opens ends in the
world. The help panel never takes the pointer.

### 5.3 A ray against a quad

The controller's aim pose is in the LOCAL space, the same space as the quads
[V, `xr_screen.h` `HandInput`]. For a quad with pose `p`, `q`, size `w` by `h`
in meters, and a rect `x0`, `y0`, `cw`, `ch` in target pixels:

```
n  = q * (0, 0, 1)             the side that faces the player
s  = dot(d, n)                 below 0: the ray meets the front side
t  = dot(p - o, n) / s         above 0: the quad is in front of the hand
k  = conj(q) * (o + t*d - p)   the hit, in the quad's own frame
u  = k.x / w + 0.5             0 at the left edge, 1 at the right edge
v  = 0.5 - k.y / h             0 at the top, 1 at the bottom
pixel = (x0 + u * cw, y0 + v * ch)
```

It is a hit when `u` and `v` are both in 0 to 1. The function belongs in
`controller_math.h` with tests in `test/controller_math`: a ray at the center,
at a corner, from behind, parallel to the quad, and a turned quad. The ray
starts at the hand and ends on the panel, so there is no parallax, unlike the
world ray (`controllers.md` 3.2).

### 5.4 The mouse

On the menu panel the mouse pixel is a panel pixel with no conversion. The mod
clamps it to the menu rect and does not turn it by the head, because a panel
pixel is already fixed in the room. If the menus are drawn larger in the
target (4.2), the pointer takes the inverse: the game gets
`c + (pixel - c) / scale` about the same center `c`.

### 5.5 What changes in pointer_map.cpp

1. A test, `PanelOwnsPointer()`: `ui_panel` is 1, `ui_panel_pointer` is 1,
   and a dialog is up or `StereoIdle()` is true.
2. `ChooseOwner`: on the panel, a controller does not need the head camera or
   a scene. It needs only a hit on the panel.
3. `PointerMapApply`: on the panel, take a panel pixel from the ray instead of
   `ControllerRenderPixel`. Skip `LockToRoom`. Clamp to the menu rect.
4. When the pointer moves from the world to the panel, keep the last world
   pixel. When it moves back, restore that pixel, so the Hand is where the
   player left it.
5. `h_GCameraUpdate`: while the pointer is on the panel, give the camera code
   the last world pixel. A drag on a menu slider must not move the camera
   [I, test it].
6. `controllers.cpp`: record the surface at each press, for 5.2.

Today the menus draw on the panel at their own pixels and the hit test reads
those pixels. But the pointer still comes from the eye view: the mouse pixel
is turned with the head, and a controller ray goes through the world. So the
arrow moves on the panel when the head turns, and a controller does not point
at the panel.

## 6. Screens with no scene, with the panel (planned)

Not built. A frame with no scene still goes out as in 1.4.

- The picture (the plasma background, the loading art, a video) goes on the
  screen quad of 1.4. It is always a quad fixed in the room, and never a
  projection layer. This removes the stale pose of 1.4.
- The Setup dialogs of the front end and of the load and save boxes go to the
  menu panel, in front of the screen quad. A FinishFrame outside the eye
  passes counts as pass 0 for the routing of 2.4. Whether `LoadSaveBox` runs
  its FinishFrame with the callbacks on is not checked [I].
- The pointer is on the menu panel (5.2). A controller can then point in these
  screens, as it does on the menus in the room (section 20).

Only `ui_panel=2` routes a FinishFrame outside the eye passes (12.2).

## 7. Cost

At a 2880 by 2880 render one surface of 4 bytes a pixel is 31.6 MiB.

| Item | Cost |
| --- | --- |
| Memory, `ui_panel=1` | The panel target, its depth, the shared texture and the swapchain images (3 [I]): about 190 MiB. A swapchain of the rect size and `CopySubresourceRegion` bring it down. |
| Memory, `ui_panel=2` | The panel pair, and on the first capture frame two more targets, a depth and a readback surface |
| Clears | Two clears at the first bind of a frame |
| Draws | The panel batches draw once instead of twice. That is a small saving. |
| Copies | One `StretchRect` and one `CopyResource` of the target. The eye copy of the same size measures 0.00 to 0.01 ms [M, `backend.md`]. |
| Flush | None added. The eye 0 flush, 0.34 ms [M], covers it. |
| Binds | A few `SetRenderTarget` pairs a frame. With `ui_panel=2`, one bind and one unbind for each panel batch. |
| Compositor | One or two more quad layers |
| A frame with no interface | Nothing: no clear, no copy, no layer |
| `ui_panel=0` | Nothing. The route, the switch, the share and the swapchain are off. |

The biggest cost is memory, and it is small against a headset render.

## 8. Risks

1. **The switch behind dxwrapper's back.** This is the biggest risk. Dd7to9
   caches the render target, the viewport and the render states, and it does
   not check them against the device (2.2). If the mod leaves a state or the
   target wrong, the world or the next frame draws wrong. The rules of 2.3,
   the target check before each panel draw, and the composite check of 2.5
   cover it. All of it is testable on the desktop.
2. **A wrong route.** Other drawing in `HelpSystem::Draw3D` is not classified
   (1.3). A part that is fixed to the world but goes to the
   panel floats in the wrong place. The `ui depth` census names the caller of
   each batch.
3. **The skipped draw in pass 1.** If a panel caller draws only in pass 1, it
   is lost. `UiDepthReportPassMismatch` names every caller that draws a
   different count in each pass.
4. **Alpha.** Blend modes other than source alpha give a halo or a hole on
   the panel (2.5). The sRGB edges are a little dark (3.3).
5. **Device loss.** The surfaces go when the device goes, through the
   bridge's `Release` hook.
6. **The quad is always on top.** A world object nearer than a panel is still
   covered by it (4.3).
7. **Streaming.** Virtual Desktop sends one video stream, so its runtime
   probably composes all layers before the encode [I]. The panel is then less
   sharp than on a headset that composes layers itself. The fixed place and
   the free size still hold.
8. **The Hand while a dialog is up in play.** The Hand and the pick read the
   pointer as a pixel of the eye view (`hand.md` on the pick). On the panel
   that pixel would be a menu pixel, so the Hand could move in the world
   behind the menu [I]. The camera code would get the last world pixel (5.5),
   but the Hand would not.
9. **Readers of the screen.** `ScriptedScreenShot::Process`, the save
   picture, is skipped while VR runs [V, `snapshot_guard.cpp`].
   `CreatureRoom::TakeScreenShot`, `GScript::SnapShot` and
   `SaveCreatureSnapshot` also read the screen [V for the names, I for what
   they read]. Their pictures lose the interface, which is better. The copy to
   the monitor loses it too.
10. **Stencil.** The panel has its own depth surface, so the world's stencil,
    if the game uses one, is not touched. The panel batches may set a stencil
    test [I]. The census logs it.

## 9. What is built and what is planned

| Part | State |
| --- | --- |
| The panel target, the switch, the share, the census and the captures (`ui_panel=2`) | built |
| The panel layers in the headset (`ui_panel=1`): menu and help quads | built, never run with `ui_panel=1` on the desktop, never seen in a headset |
| The Creature panel quad | built (section 13) |
| The subtitle card | built (section 17) |
| Input on the panel (section 5) | planned for `ui_panel=1` |
| Screens with no scene on a screen quad (section 6) | planned. A screen with a menu goes to the place of the menu panel (section 20) |
| The menus in the room (section 20) | built, math checks. A desktop run shows the Main Menu of the start on the panel and a click through it, with Vulkan and with dxwrapper. Never seen in a headset |
| The keyboard in the headset (section 21) | built, math checks. A desktop run types into the box of a save name and closes it with Cancel, and saves the pictures of the parchment card and of the dark card. Never seen in a headset |
| Tabletop placement of the panels (4.4) | planned |
| A line from the controller to the panel hit | planned |
| The panel over the monitor copy, when the monitor copy is on | planned |
| The launcher: the interface size row changes meaning when `ui_panel` is 1, and says so | planned |

With `ui_panel=1` these are unchecked on the desktop: that eye pass 1 skips
the panel batches cleanly, that the eye pictures lose the menus, and that a
scripted click still works with the menu only on the panel. The code for the
hit test does not change, so it should.

The checks in a headset are in `headset-checks.md`, and the checks that a
desktop run can prove are in `roadmap.md`, R12.

## 10. Ini keys and the test key

| Key | Default | Meaning |
| --- | --- | --- |
| `ui_panel` | 0 | 0 the interface in the eyes. 1 the interface on its own layers. 2 the test mode: each panel batch also draws in the eye, and the captures of 12.5 are saved (desktop only). |
| `ui_menu_distance_cm` | 140 | menu panel distance |
| `ui_menu_pitch_deg` | -10 | menu panel angle to eye level |
| `ui_menu_px_per_deg` | 16 | menu panel scale |
| `ui_menu_rect` | 0 | the menu rect in render pixels, `x y w h`. 0 takes the default of 4.1. |
| `ui_help_distance_cm` | 120 | help panel distance |
| `ui_help_pitch_deg` | -30 | help panel angle to eye level |
| `ui_help_px_per_deg` | 44 | help panel scale at a 2880 px tall render. It scales with the render height, as the help text does. |
| `ui_help_rect` | 0 | the help rect in render pixels. 0 takes the default of 4.1. |
| `creature_panel` | 2 | the Creature panel with `ui_panel=1`: 0 off, 1 fixed in the room, 2 near the Creature (section 13) |
| `creature_panel_linger` | 1 | 1 the Creature panel shows while the Hand is at the player's Creature with no press, and for 3 s after the Hand left. 0 only while a press holds the Creature (section 13.2) |
| `menu_room`, `menu_room_test` | 1, 0 | the menus in the room with `ui_panel=0` (section 20) |
| `vr_keyboard` | 1 | the keyboard in the headset (section 21) |
| `screen_distance_cm`, `screen_width_cm` | 200, 260 | the screen quad of 1.4 |
| `ui_scale_pct`, `hint_scale_pct`, `hint_flat`, `ui_shift_max_px`, `subtitle_lift_pct`, `subtitle_deg` | 25, 20, 1, 24, 0, 35 | the interface in the eye pictures (`backend.md`) |
| `hint_hand_depth` | 1 | a hint of the game at the game's Hand at the depth of the Hand (section 15.6) |
| `subtitle_*` | section 17.5 | the subtitle card |
| `hint_card`, `prompts`, `hint_card_size_pct` | `controller-hints.md` | the hint cards |
| `bubble_card` | section 18.4 | a speech bubble of the game |

The test key: Ctrl Shift F11 with `test_keys=1` switches `ui_panel` between
its ini value and 0, at the next frame (`UiPanelToggle`, `PollHotkeys` in
`stereo.cpp`). With `ui_panel=0` it switches the hint cards and the subtitle
card between the card and the eye pictures. Ctrl Shift F12 is the audio key.

## 11. Open questions

1. Is the Creature panel `fn_005178D0` fixed to the screen or to the
   Creature? **Fixed to the screen [V].** See section 13.
2. What else does `HelpSystem::Draw3D` draw, besides the tooltip list?
   **Open.** The census counts `Draw3D` batches by caller ("Draw3D" in the
   caller table), but no run has drawn one.
3. Where do videos in play draw, and should they go on a panel? **In the late
   callbacks, in the cut scene window [V].** `PlayFullScreenMovie` puts the
   cinema bars up, so a video in the God view plays in the window, on the
   screen plane. No panel is necessary (`cutscene-window.md`, section 7).
4. Does the game draw a cursor in the Setup dialogs? **Yes [M].** A teal arrow
   at the pointer, drawn inside the Setup callback.
5. Do the Setup dialogs sit in the middle of the render at every render size?
   **In the middle at 2560 by 1440 [M].** The start dialog of a new game is
   406 by 316 px with its center at 1277, 727. The front end menu is 780 by
   570 px with its center at 1281, 729. Other render sizes are not tested.
6. Which blend modes, depth tests and stencil tests do the panel batches use?
   For the menus, see 2.5. The help layer is not measured.
7. Does an open dialog stop the Hand and the camera in play? **Open.**
8. Does Virtual Desktop's runtime compose quad layers before the encode?
   **Open.** It needs the headset.
9. Does `LoadSaveBox` run its FinishFrame with the callbacks on? **Open.**
10. No desktop run has shown a real subtitle, a help text of the game, or a
    tooltip beside a panel scope. So the help rect and the help half of the
    census are not measured.

## 12. The implementation

### 12.1 Files

| File | What it does |
| --- | --- |
| `src/mod/ui_panel.h` | The settings, the shared names, and the functions of the three files below. No Direct3D, so the Direct3D 7 files can include it. |
| `src/mod/draw_hooks.cpp` | The route of each batch (2.4), the census, and the hook on the rect queue callback `0x0081E7D0` |
| `src/mod/gpu_bridge.cpp` | The panel target and its depth, the switch, the clears, the share to Direct3D 11, the readback for a BMP, the half size copy of the subtitle card, and the composite check targets. All of it goes with the device, through the `Release` hook. |
| `src/mod/ui_panel.cpp` | The route of each frame, the test key, the captures, the placement of the quads, the Creature panel and the subtitle card |
| `src/mod/stereo.cpp` | Latches the route before the eye passes. After eye pass 0 it saves the capture and copies the panel before the eye 0 capture. |
| `src/mod/mod_main.cpp` | The ini keys. `FinishFrame` outside the eye passes, and the last release after it. |
| `src/vr/xr_screen.cpp` | The panel swapchain, `SetPanelTexture`, `SetPanelQuads`, `StagePanelQuad`, the cards, and the quad layers after the projection layer |
| `src/mod/head_pose.cpp` | `HeadPoseOrigin`, the recenter pose in the LOCAL space |
| `src/mod/controller_math.h` | `PanelPose`, `PanelWidth`, `WorldToRoom`, `RoomToLocal`, `FacePose`, `CreaturePanelPoint`, tested in `test/controller_math` |
| `src/mod/subtitle_card_math.h` | The pure math of the subtitle card |
| `src/mod/menu_room.cpp`, `menu_room_math.h` | The menus in the room: the place of the panel, the route of each frame, the pointer on the panel, the frames with no scene (section 20) |
| `src/mod/vr_keyboard.cpp`, `vr_keyboard_math.h` | The keyboard in the headset (section 21) |
| `src/mod/eye_capture.cpp` | A 32 bit BMP with a V4 header that keeps the alpha, and the panel over a checkerboard |

### 12.2 The route of a frame

`UiPanelBeginFrame` latches the route once a frame, before the eye passes:

| `ui_panel` | Eye pass 0 | Eye pass 1 | Outside the eye passes |
| --- | --- | --- | --- |
| 0 | the eyes | the eyes | the eyes |
| 1 | panel only | not drawn | the eyes |
| 2 | panel, then the eye | the eyes | panel, then the eye |

With `ui_panel=1` in the headset, the route needs the shared panel texture and
the projection layer. So the stereoscopic window (`play_mode` 1) keeps its
interface in its picture. With no headset, `ui_panel=1` routes anyway, as the
desktop test. A failed bind turns the route off for the rest of the frame,
both passes draw into the eyes, and no panel goes out in that frame.

### 12.3 The quads of a frame

After eye pass 0, `stereo.cpp` calls `GpuBridgeCaptureUi` (one `StretchRect`,
no flush), then `GpuBridgeCaptureEye(0)`, whose flush drains both copies, then
`SetEyeTexture(0)`, `SetPanelTexture` and `SetPanelQuads`. A frame that drew
nothing to the panel sends no copy and no layer. `EndFrame` adds one quad for
each kind that drew in that frame, after the projection layer.

### 12.4 The captures

On a capture frame (F12 or `capture_every_seconds`) with `ui_panel` not 0,
`UiPanelAfterFirstEye` writes, beside the eye pictures:

| File | What |
| --- | --- |
| `bwvr-ui-NNNNN.bmp` | The panel, 32 bit, with its alpha. The color is premultiplied. |
| `bwvr-ui-over-NNNNN.bmp` | The panel over a gray checkerboard |
| `bwvr-ui-spare-NNNNN.bmp` | The eye as it was just before the first panel batch |
| `bwvr-ui-check-NNNNN.bmp` | The spare, with the panel batches drawn over it the game's way |
| `bwvr-ui-frame-NNNNN.bmp` | Outside the eye passes (`ui_panel=2`): the finished frame |

The check picture, and not the finished eye picture, is the reference of the
composite check, because the eye gets the interface shift and the interface
size, and the panel does not. In the test mode each batch binds and unbinds,
because the eye draws it right after the panel. Outside the menu, the eye
picture must equal the check picture, which shows that the switch puts the
target, the depth, the viewport and the states back exactly.

### 12.5 The log

The tag `ui layer` and `ui panel`, per report window:

```
ui layer     : N panel batches (M menu, H help, S subtitle box), K kept in the eyes
ui layer     : B binds, R times the target was found changed
ui layer     : menu rect x y w h, help rect x y w h (render pixels)
ui layer     : blend modes: A source alpha, O off, D additive, X other
```

R must be 0. The Direct3D 9 states of each panel batch are read after its
draw. A desktop test is a headless stereo run with `ui_panel=2`, with the
Esc menu open, the Options page, and a subtitle.

## 13. The Creature panel

The camera part is in `creature.md` (the Creature by touch).

### 13.1 What the game draws [V]

- `fn_005178D0` (cdecl, no arguments, plain `ret`) runs from the `HelpText`
  callback at `0x005CD03C`. When the alpha at `0x00CC630C` is above 0, it
  calls `DrawCreatureStats` at its real entry `0x00517080`. The symbol file's
  `0x00517265` is inside that function. Then it clears the alpha and the
  layout at `0x00CC6310`. So the first eye pass draws the panel, and the
  second one does not.
- `Creature::Draw` (`0x00517910`) arms it with alpha 255 and layout 1 when the
  Creature has bit `0x10` of `Object+0x24` and is
  `GInterface::GetInteractObject`, the object of the next pick record. The
  locked select of a press sets bit `0x10` (`creature.md`, section 1.7). So
  the plain game shows the panel while a press holds the Creature, from the
  start of the locked select, and not only in the Creature state. A Hand over
  the Creature with no press shows no panel. `CameraModeFollow::Update` arms
  it with layout 0.
- The four values, at `0x00CC62EC` down to `0x00CC62E0`: a = 1 - (vtable
  `+0x11C`), b = 1 - `[+0x160]+0x1C`, c = `[+0x160]+0x30`, and d = the lesson
  total `+0xD8` of the Creature state. The labels are the help texts
  `0x1A52`, `0x1A66`, `0x1AAB` and `0x1A98`. What each one says is a guess
  [G].
- The box is fixed to the screen. It goes from W/40 across and is W/4 wide.
  Layout 1 goes from H/4 down and is H/3 high. Layout 0 goes from H/40 down
  and is H/4 high. W and H come from `0x00E85058`. The box takes no clicks.
- With `ui_panel=1` its batches go to the panel target, in the `HelpText`
  scope. The help rect is the bottom 40 percent of the target, so the box
  needs the quad of its own.

### 13.2 What the mod does

- **The panel with no press** (`creature_panel_linger=1`). In the first call
  of a frame the hook on `fn_005178D0` arms the panel itself when the game did
  not arm it, while the controller places the Hand at the player's Creature:
  the target at the Hand's point, or the Creature of the touch
  (`HandDirectCreatureAtHand`). It writes the alpha 255, layout 1 and the
  values a, b and c as `Creature::Draw` computes them. The value d (the lesson
  total) keeps the value of the last Creature state, as the game's own panel
  has it before a stroke. A Creature that does not live gets no panel. The
  hook is in with each value of `ui_panel`. With `ui_panel=0` the panel then
  draws where the game draws it: in the eye pictures, or with the help text
  on the subtitle card (section 17).
- **The panel stays.** After the Hand left the Creature, and after the end of
  a press, the hook arms the panel for 3 s more (`creature_touch_math.h`,
  `LingerStep`). So the panel does not go on and off at the edge of the body.
  A different Creature, or no Creature of the player, ends the stay at once.
  The mouse keeps the plain game: no panel with no press, and the 3 s after a
  press. `creature_panel_linger=0` keeps the plain game for the controller
  too. The debug page has the switch "Panel stays" (group Creature).
- A hook on `fn_005178D0` latches, once a frame, that the panel drew, its
  layout and its four values. For layout 1 it also reads the Creature of the
  pick (`GInterface+0x400`) and puts its head in the LOCAL space: the base
  `+0x78` of its `LH3DCreature`, raised by 15 times its size `+0x90`.
  `WorldToRoom` and `RoomToLocal` in `controller_math.h` undo the steps that
  put a controller in the world, on the table and off it.
- A third rect and quad (`kPanelCreature`) cut the box out of the panel
  target, 8 px wider on each side. The quad uses the density of the help
  panel. `ui_panel.cpp` gives it to `XrScreen::StagePanelQuad` before
  `SetPanelQuads`, in the same frame.
- `creature_panel` places the quad:

| Value | Place |
| --- | --- |
| 0 | No quad. The panel does not show in the headset. |
| 1 | Fixed in the room: 0.9 m out, 35 degrees to the left of the recenter yaw and 15 degrees down, facing the head. A recenter moves it. |
| 2 (default) | Near the Creature. When the panel shows, it goes 30 cm over the Creature's head and 15 cm to its left as the eyes see it, then 0.5 to 1.5 m from the eyes, facing them (`CreaturePanelPoint`, `FacePose`). It stays there while it shows and until it was hidden for 1 s, so it does not follow a Creature that walks. With no head (layout 0, or no head pose) it goes to the place of 1. |

The log, once each time the panel shows, and for the panel with no press
one line when the mod arms it, one when the Hand leaves and one when it goes:

```
ui panel     : the Creature panel shows (layout 1, values 0.62 0.80 0.45 0.10), near the Creature at 0.31 1.18 -0.74 m, 0.84 m from the eyes, 0.35 by 0.45 m, rect 64 712 736 976
ui panel     : the Hand is at your Creature 2925A6F0 with no press: the mod shows the Creature panel (creature_panel_linger=1), frame 931
ui panel     : the Hand left your Creature: the Creature panel stays for 3.0 s, frame 1800
ui panel     : the Creature panel goes, 3.00 s after the Hand left your Creature, frame 1980
```

The report window adds how many frames it drew, on how many of them the mod
armed it, and how many times it went to the room. `XrScreen` counts its quad layers.

### 13.3 Tests

The math test checks the face of the quad (its front points at the eyes, with
no roll), the place near the Creature (to the left, over the head, 0.5 to
1.5 m from the eyes), and the room to LOCAL against `head_pose.cpp`.

The math test (`creature_touch_checks.h`) checks the panel that stays: it
shows at the Hand, it stays 2.9 s and goes at 3 s after the Hand left, and
not for a different Creature. `creature_recipe.ps1 -Kind open` is the run of
the panel with no press: a picture at frame 1840 must have the box, and a
picture at frame 2060 must not. On the test laptop (2760 by 2208, Direct3D
9) the run shows the panel at the first touch of the open hand, the stay of
3.0 s after the hand left (the panel goes 2.99 s after it), the box in the
picture at frame 1840 and none at frame 2060. With
`creature_panel_linger=0` it shows no panel.

No desktop run has drawn the box. A run needs a game where the player owns a
Creature: the God view rig of `god-view.md` with `ui_panel=2`, the fake hand
on the Creature (`touchc`) and a capture (`shot`). The capture must show the
box at 2.5 to 27.5 percent across and 25 to 58 percent down, and the log must
show the values.

The checks in a headset are in `headset-checks.md`, and the checks that a
desktop run can prove are in `roadmap.md`, R12.

## 14. Sources

- `bwvr/src/mod/draw_hooks.cpp`: the caller ranges, the scopes, the hint rule,
  the tooltip dedup, the interface size, the subtitle placement, the shift,
  and `DrawInterface`.
- `bwvr/src/mod/stereo.cpp`: the eye loop, the projection flag, the render
  pose, `StereoIdle`.
- `bwvr/src/mod/mod_main.cpp`: `h_FinishFrame`, the path for screens with no
  scene, the screen keys.
- `bwvr/src/mod/gpu_bridge.cpp`: the shared textures, `StretchRect`, the
  flush, the device `Release` hook.
- `bwvr/src/vr/xr_screen.cpp`: the swapchains, `EndFrame`, the quad and
  projection layers, `SubmitTexture`.
- `bwvr/src/mod/pointer_map.cpp`: the owner, `LockToRoom`, the window message
  hooks, `PointerMapApply`.
- `bwvr/src/mod/ui_worldlock.cpp`: parked. It moved screen vertices to hang
  subtitles in the world, and that read worse than subtitles on the view. A
  real layer needs a second render target and control of the render target
  switch, which section 2 gives.
- `bwvr/src/mod/snapshot_guard.cpp`: the save picture is skipped in VR.
- `bwvr/src/mod/head_pose.cpp`: the recenter origin.
- `bwvr/docs/backend.md`, `tabletop.md`, `controllers.md`, `hand.md`.
- dxwrapper 1.8.8600.25, `ddraw/IDirect3DDeviceX.cpp`,
  `ddraw/IDirectDrawX.cpp`, `ddraw/IDirectDrawSurfaceX.cpp`, and the shipped
  `bwvr/installer/payload/dxwrapper.ini`.
- `tools/disasm.py` on `0x0082F460`, `0x0082F2C0` and its eight callers,
  `0x005CD020`, `0x005CCAB0`, `0x006424E0`, `0x00407ED0`, `0x0081E7D0`,
  `0x00544F90`, and `--callers` of `0x0082F460`, `0x0082F0E0`, `0x005C59A0`
  and `0x00557D10`.

## 15. The tooltip at the Hand

This path runs only with `hint_card=0`, or when the hint card cannot show.
With the default `hint_card=1`, a tooltip of a controller does not draw in the
eye pictures: it goes to the hint card (section 16).

### 15.1 The game's help options [V]

HelpSystem (`GGame+0x25005C`) reads four options of the player's profile at
`fn_005C6CF0`. The options menu of the game writes them to the profile
(`DialogBoxOptions` at `0x005148B0`). The mod never writes them.

| Profile value | Field | Values | Missing |
| --- | --- | --- | --- |
| `TOOLTIP_LEVEL` | `+0x45FC` | 0 No ToolTips, 1 Minimum, 2 Intelligent, 3 All ToolTips | 2 |
| `TEXT_DRAW` | `+0x4604` | 0 No Text, 1 Story Text, 2 All Text | 1 |
| `CREATURE_HELP` | `+0x4608` | 0 off, 1 on | 1 |
| `HELP_LEVEL` | `+0x45F4` | 0 No Help to 4 All Help | 3 |

With No ToolTips, `fn_005C9D00` makes no tooltip at all: no hint beside the
Hand, no tooltip of an object and no "Amount" of a bulk take. With No Text,
`fn_005C6E60` gives no subtitles. A profile with these options at 0 shows no
hints and no subtitles, in the headset and in a desktop run. The player
changes them in the options of the game, under "Help Options".

`controller_hints.cpp` logs the four values once a game, for example:
`hints : the game's help options are "No ToolTips", "No Text", "No Help" and
Creature help off (0 0 0 0)`.

The test key `help_test=1` sets All ToolTips, All Text, High Help and the
Creature help in the game's memory, never in the profile. It is for desktop
runs.

### 15.2 Why a flat copy does not work

The game draws a tooltip as flat interface. A flat copy in the two eye
pictures has the same pixel in both eyes, so it has no disparity and shows at
the far end of the view. The Hand has a disparity of tens of pixels (68 px at
1 m and 123 px at 0.55 m, at a 2880 px render of 110 degrees). A player who
looks at the hand sees a flat hint two times. So a tooltip must have the
Hand's disparity.

### 15.3 What the mod does

While the controller places the Hand (`HandGlowEyePoint` in `hand_glow.cpp`
gives the Hand's point in this eye), a tooltip goes to the Hand
(`TooltipAtHand` in `draw_hooks.cpp`, `HintAtHandScale` and `HintAtHandPoint`
in `controller_math.h`):

1. The size. The box of the call becomes `kHintHandCm` (2.5 cm) of the room
   high at the Hand's depth. The scale is 0.22 at 1 m and 0.40 at 0.55 m, and
   it stays between 0.1 and 3.
2. The left eye scales the tooltip about the Hand's point, at the game's
   layout, and moves it `kHintHandSideCm` (6 cm) to the right. It keeps that
   place as an offset from the Hand's point.
3. The right eye puts the tooltip at the same offset from its own Hand's
   point. So both eyes give the tooltip the Hand's disparity, and it shows at
   the Hand's depth, beside the hand.

`ui_scale_pct`, `hint_scale_pct` and `hint_flat` do not change a tooltip at
the Hand. While the game places the Hand, section 15.6 gives the depth.
The game's mouse picture hint (the widget band) is not changed. With the controller words on, the game's
hints become key caps and take the tooltip path.

The game draws its Hand after the help, so the mesh can hide a part of a
label. The offset of 6 cm keeps a label clear of the arm and of the bundle in
the Hand. That value is not yet checked in a desktop run.

### 15.4 The log

On a sample frame (every 150 frames, and when a tooltip with a new text
shows) the tag `ui hint` gives each tooltip, hand hint picture and subtitle
bar with the eye, its box as the game drew it, the pointer and the Hand's
point in that eye. A second line gives the scale and the place at the Hand.
The report of the `ui depth` tag counts the tooltips at the Hand.

### 15.5 Tests

The math test (`HintHandChecks`) covers the size at 1 m and at 0.55 m, the
limits, the scale about the Hand's point in the left eye, and the Hand's
disparity for a point of the tooltip in both eyes. The desktop rig is the bulk
take rig of `hand.md` with `help_test=1`.

The checks in a headset are in `headset-checks.md`, and the checks that a
desktop run can prove are in `roadmap.md`, R12.

### 15.6 A hint at the game's Hand

While the game places the Hand (with the mouse, or while the controllers are
not tracked), no controller gives a Hand's point, so 15.3 does not run. The
tooltip of the thing under the Hand ("Got 22 Belief", "Wood Required: 1000")
and the hint with a mouse picture ("Move") then draw in the eye pictures.

**What the game does [V].** `fn_005C9D00` makes the tooltip of the thing
under the Hand with the flags `0x16`. When the bits 2 and 4 of its flags are
set, `CameraHelp::DrawKeyOrMouse` (`0x00447EA0`) projects the point of the
Hand mesh (`CHand+0x482C`, its place at `+0x38`) with the camera of the pass,
and draws the hint at that pixel. So the game puts the hint at a point of the
world, but its batches have no depth of their own.

**Why it is double.** `hint_flat=1` puts the right eye of the hint at the
pixel of the left eye, and one walk draws the batch of the left eye again. A
hint with no disparity is at the far end of the view. In a desktop run the
point of the Hand was 163 units from the eye, with an eye difference of 52
px. The eyes look at the Hand or at the land under it, so they see the hint
two times.

**What the mod does (`hint_hand_depth=1`).** For a batch of a DrawKeyOrMouse
call with the bits 2 and 4, `HandHintMove` in `draw_hooks.cpp` gives the right
eye the eye difference of the point of the Hand (`hint_depth_math.h`):

- One walk: the move of the depth of the point
  (`one_walk_math::ShiftAtDepth`), with the camera of the left eye.
- Two walks: the pixel of the point in the right eye less its pixel in the
  left eye. With `hint_flat=0` the right eye pass keeps the pixel that the
  game gave, which has that eye difference already.

This applies to a tooltip with no picture (the tooltip path of
`h_DrawKeyOrMouse`) and to the kept copy of a hint with a mouse picture
(`FlattenIn`). The left eye does not change. A hint that the game does not
draw at the Hand (the flags without the bits 2 and 4, for example the hint
beside the help text, or the label of the hand demo) stays as before. When
the point is not in front of the eye, the hint stays flat. `hint_hand_depth=0`
keeps each such hint flat.

While a controller places the Hand, these hints go to the hint card (section
16), or to the Hand's point of 15.3 with `hint_card=0`. No other yellow text
of the game takes the flat path then.

**Keys, test keys and the log.**

| Key of `bwvr.ini` | Default | Meaning |
| --- | --- | --- |
| `hint_hand_depth` | 1 | 1: a tooltip or a hint of the game at the game's Hand has the depth of the Hand. 0: the same pixel in both eyes |

With `test_keys=1`, Ctrl, Shift and the slash key change `hint_hand_depth` in
play, with a long pulse for 1 and a short pulse for 0. Test keys for the
desktop runs:

- `hint_hand_depth_test_frame=<frame>`: the key changes at that frame.
- `mouse_owner_test_frame=<frame>`: from that frame the mouse owns the
  pointer, as with Ctrl F12 "mouse only" (`controllers.md`, section 3). The
  game then places the Hand at the pointer that the controller left.
- `hint_selftest=2`: the five hints of `hint_selftest=1`, and a yellow
  tooltip and a hint with a mouse picture at the game's Hand, with the flags
  `0x16`, two copies of each.

The frames of the test keys count as the steps of the scripted controller
count. On a sample frame of the log the tag `ui hint` gives the depth of the
point of the Hand, its eye difference and the move of the right eye. The
report of the tag `ui depth` counts the right eyes at the depth of the Hand,
the right eyes at the pixel of the left eye, and the frames with the point
of the Hand not in front.

**Tests.** The math test (`OneWalkChecks`) has a point of the Hand 55 units
from the eye: one walk puts the hint at the pixel of the point in the right
eye, and a flat hint is more than 50 px from it. The desktop run is
`creature_recipe.ps1 -Kind leash` with `hint_selftest=2`,
`mouse_owner_test_frame=860` and `hint_hand_depth_test_frame=925`, and
`test_frame_rate=60` for the Vulkan renderer: the pictures of frame 903 have
the key at 1, and the pictures of frame 953 have it at 0. The game's own
hint "Move" is at the Hand in that run.

**Limits.**

- No run had a real tooltip of a thing under the Hand while the game placed
  the Hand. The test key draws through the same call with the same flags.

The checks in a headset are in `headset-checks.md`, and the checks that a
desktop run can prove are in `roadmap.md`, R12.

## 16. The hint card

The hint beside the Hand and the prompts of the controller show on small quad
layers of their own: one card beside the wrist of each hand. The whole design
is in `controller-hints.md`, section 9. For the interface layer, these are the
facts:

- Each card is a quad layer. `XrScreen::EndFrame` adds the cards after the
  projection layer and the three panel quads, so they are over all of them.
  The order is the subtitle card (section 17), then the card of the hand that
  points, then the card of the other hand. Each uses
  `XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT` with the color
  multiplied by the alpha, as the panels do.
- Each card has its own swapchain, in the format of the eyes: 768 by 320
  pixels for a hint card. `XrScreen` makes it at the first layer of the card
  (`SendCard`). The picture comes from the CPU: GDI draws the words,
  `hint_card.cpp` adds the card and the alpha, and `UpdateSubresource` writes
  the part that shows into the swapchain image of the frame. There is no
  Direct3D 9 target and no share, so a hint card does not use the panel target
  of section 2.
- It does not need `ui_panel`. `UiPanelBeginFrame` gives it the frame with
  `ui_panel=0` too, and `UiPanelReport` adds its report.
- The mod stages a card with `XrScreen::StageCard` before `EndFrame`, as
  `StagePanelQuad` does for the Creature panel. It shows only in the frame that
  staged it.
- When the runtime refuses a frame with a card, `CardFailed` turns that kind
  of card off: first the subtitle card, and then the hint cards.
- With `ui_panel=1` the route of 12.2 does not change: a tooltip never goes to
  the panel. It goes to the card.
- With `ui_panel=0` the test key Ctrl Shift F11 switches the cards on and off:
  the hint cards and the subtitle card (`UiPanelToggle`).

## 17. The subtitle card

Code: the route in `src/mod/draw_hooks.cpp` (`kUiPanelSubtitles`), the frame,
the place and the picture in `src/mod/ui_panel.cpp` (`SubtitleBeginFrame`,
`SubtitleEndPasses`), the pure math in `src/mod/subtitle_card_math.h`, the
half size copy in `src/mod/gpu_bridge.cpp` (`GpuBridgePanelReadHalf`), and the
quad layer in `src/vr/xr_screen.cpp` (`kCardSubtitles`).

### 17.1 Why the subtitles are not in the eye pictures

Flat subtitles in the two eye pictures sit at one depth, and `ShiftIn` sets
that depth with a shift of `shift` pixels toward the nose in each eye. No
depth is right for flat text in the eye pictures of the God view. At a render
of 2880 pixels for 110 degrees (a focal length of 1008 pixels) and eyes 65 mm
apart:

| Reason | Detail |
| --- | --- |
| The subtitles are behind the things they cover | The interface depth is 1.37 m at the ceiling of 24 pixels (`ui_shift_max_px`), and 4.10 m at the resting shift of 8 pixels. In the God view the land is at the waist (0.5 to 1 m below the head), and the subtitles are 35 degrees below the middle of the view, on that land and on the hands. The Hand's point is 0.4 to 0.7 m away. A thing that covers a nearer thing gives two cues that do not agree. The eyes stay on the near thing, and they see the far text two times. With the eyes on the Hand, the two pictures of a subtitle at 4.10 m are 7.8 degrees apart. |
| The clamp | The adaptive rule follows the nearest world, and asks for up to 97 pixels at 100 units a meter, which is 0.34 m. That is too near to read, and the clamp of 24 pixels cannot put the text in front of the world. |
| The depth changes | The adaptive rule follows the nearest world, and the eye distance in world units follows the zoom. So the subtitles move between 1.37 m and 4.10 m in one session. |
| A low frame rate | Text in the eye pictures is fixed to the head. The runtime shows a late frame again, turned for the new head pose, so text that is fixed to the head steps against the head while the head turns [I]. |
| An icon in a line | A button icon in a subtitle draws through the camera help quads, so the hint rule takes it: the hint size, and no interface depth. |
| The first frame of a subtitle | The letters draw before the box that defines their band. In the left eye of that frame they have no band, so they are not moved to 35 degrees. In the right eye they are. |

The box (the rect queue caller) and its letters all go through `ShiftIn` with
one shift, left eye plus and right eye minus [V].

### 17.2 One card in the room

While the headset shows a projection layer and `subtitle_card` is 1, no help
text of the HelpText scope draws in an eye picture. This runs with
`ui_panel=0`, which is the default.

1. `UiPanelBeginFrame` latches the route for the frame (`SubtitleBeginFrame`):
   `UiPanelRouteFrame(kUiPanelSubtitles, on)`. Both eye passes agree.
2. In eye pass 0, `PanelTake` draws each batch of the HelpText scope and of
   the subtitle box into the panel target of section 2, at the pixels of the
   game, with no interface size and no depth. In eye pass 1 it draws them
   nowhere. This is the route of `ui_panel=1` for the help text only: the menus
   stay in the eye pictures. The hint beside the Hand and the tooltips go to
   the hint card.
3. After both eye passes (`UiDepthSetEye(-1)`), `SubtitleEndPasses` takes the
   bounds of what drew, with 8 pixels of room, on a grid of 16 pixels
   (`CardRect`). The copy of that rect at half the size is one `StretchRect`
   (linear, so each pixel is the mean of four) into a target that is only as
   large as the rect, on a grid of 128 pixels, and one `GetRenderTargetData`
   to system memory. A subtitle costs a copy of about 1 MB.
4. **The read ahead** (`subtitle_read_ahead=1`, the default). The GPU does the
   calls of the device in the order of its queue, so `GetRenderTargetData`
   waits until the GPU has done each call before it. After the eye passes that
   is both eye passes. With `frame_send` 1 and 2 the frame thread does not
   wait for the GPU at all, so that read would be a wait of the frame thread.
   So the copy goes into one of three slots, with an event query after it
   (`GpuBridgePanelQueueHalf`). At the start of a later frame, before the eye
   passes, `GpuBridgePanelFetchHalf` reads the newest slot whose query is done,
   when the query of the last frame end is done too. Then the GPU has no work
   left, and the read is only the copy. `GpuBridgePanelReadyHalf` gives those
   pixels and the rect that they hold, and the quad has that rect. So the card
   shows the help text of one or two frames before. A copy from before the
   frame in which the help text of now started does not show, so old text
   never comes back.
   With `subtitle_read_ahead=0`, `GpuBridgePanelReadHalf` reads the copy of
   the frame at once, and the frame thread waits for the GPU.
5. `XrScreen::StageCard(kCardSubtitles)` gives the pixels and the pose to
   `EndFrame`. That makes a swapchain of half the render size at the first
   subtitle, writes the rect with `UpdateSubresource`, and adds ONE quad layer
   in the LOCAL space. This is the path of the hint card.

A quad shows at one place of the room to both eyes, so the text cannot be
double. The runtime places the quad again for each displayed frame, so a slow
game frame does not move it against the room. The text is at half the size of
the render: the quad shows 1440 texels across 35 degrees, which is 1.7 texels
for each pixel of the headset, so the letters do not shimmer as they would at
the full size.

The card installs the hook of `ui_panel=1` on the rect queue callback
`0x0081E7D0`, which puts the game's target back after the subtitle box. The
binary has `call 0x81e3c0` and `ret 4` there [V], and `fn_0081E740` registers
it as a FinishFrame callback.

A picture of the camera help code inside the subtitle rows goes to the card
with its line, when it draws in the HelpText scope. The Creature panel is
drawn in the HelpText scope too, so it shows on the same screen, left of the
middle and near eye level.

### 17.3 The place

`subtitle_card_math.h`. The game's pixels show on a flat screen that hangs from
an anchor: a head position and a yaw.

- The row at 93 percent of the render height (the middle of the subtitle box)
  is 1.6 m from the anchor head, 17 degrees below eye level, straight ahead of
  the anchor yaw (`RectQuad`). The screen is square to that line of sight. The
  whole width of the render is 1 m, so a subtitle is 35 degrees wide and a
  letter of 77 pixels is 1 degree high.
- 17 degrees below eye level at 1.6 m is 47 cm below the head. So the card is
  above a land that is 0.6 m below the head, and in front of the land behind
  it. It is not in the place where the hands work.
- The quad of a frame is the rect that drew. A pixel has one place in the room
  in each rect, so a rect that grows moves no letter.
- **The anchor** (`AnchorStep`) starts at the recenter pose (`HeadPoseOrigin`),
  and each recenter puts it there again. The card does not follow the head: a
  head that turns, leans or looks down moves nothing.
- The head is away when it has turned more than 50 degrees from the anchor,
  moved more than 1 m along the floor, or more than 35 cm up or down. While the
  card shows, the anchor goes to the head only after the head was away for 1 s
  without a break. So the card is never lost behind the player. While no card
  shows, an away head takes the anchor at once, because nothing moves in the
  view.
- **The card never slides** (`ShownStep`). When the anchor changes while the
  card shows, the card fades out in 0.2 s at its old place, takes the new place
  while it is clear, and fades in. The yaw comes from the right axis of the
  head, so a head that looks straight down keeps its yaw.

### 17.4 The fallback

| Case | What the help text does |
| --- | --- |
| The headset with a projection layer | The card |
| The runtime refused the swapchain of the card or a frame with it (`CardFailed`), the panel target could not be made or bound, or the copy failed 3 times | The eye pictures, at one fixed depth of 0.75 m (`subtitle_fallback_cm`, 43 pixels in each eye at a 2880 px render). The depth does not follow the world, and it is in front of the land under the subtitles. The log says why. |
| No headset: the mouse player, and the desktop rig | The eye pictures at the interface depth |
| The stereoscopic window (a headset with no projection layer) | The interface is in its picture |
| A flat frame of the cut scene window | The card, in the plane of the screen under the scene (17.9). With no card there, the help text is in the picture of the window, on the screen plane. |
| `subtitle_card=0` | The eye pictures at the interface depth |
| `ui_panel=1` | The help panel of section 4 |

The fixed depth of 0.75 m is for the God view. In a flat frame of the cut scene
window the help text stays in the picture of the window.

### 17.5 Keys

| Key of `bwvr.ini` | Default | Meaning |
| --- | --- | --- |
| `subtitle_card` | 1 | 1 shows the subtitles and the help text on the card in the headset. 0 keeps them in the eye pictures |
| `subtitle_card_distance_cm` | 160 | The distance from the head to the subtitles, 60 to 400 |
| `subtitle_card_down_deg` | 17 | The angle of the subtitles below eye level, -20 to 60 |
| `subtitle_card_width_cm` | 100 | The width of the whole render on the card, 30 to 300. It sets the size of the letters |
| `subtitle_fallback_cm` | 75 | The fixed depth of the fallback, 30 to 400 |
| `subtitle_read_ahead` | 1 | 1 reads the copy of the card at the start of a later frame, when the GPU has done it, so the frame thread does not wait (17.2, step 4). 0 reads it after the eye passes of its frame, and the frame thread waits for the GPU |

Test keys: `subtitle_card_test=1` runs the route with no headset, for the desktop
rig. `subtitle_selftest=1` draws two lines of help text in the HelpText scope
with the game's own text code (`SubtitleSelfTest`), because a desktop run shows
no subtitle. With `test_keys=1` and `ui_panel=0`, Ctrl Shift F11 switches
between the cards and the eye pictures, for a live comparison. With
`test_keys=1`, Ctrl Shift Semicolon turns `subtitle_read_ahead` off and
on.
`subtitle_read_ahead_test_seconds=N` (5 to 600) does the same each N seconds
while the card is in use, with the same pulses, for a comparison in the
headset with no keyboard. A save of the launcher removes this key.

### 17.6 The log

The tag `subtitles`:

- The keys at the start.
- `the subtitles and the help text now go to the subtitle card, and none draws
  in an eye picture`, or the reason for the fallback or for the old path, at
  each change.
- `frame N, the subtitle card: the rect x y w h of the 2880 by 2880 render,
  1440 by 152 px on the card, at x y z m, turn ..., 1.00 by 0.11 m, 1.60 m from
  the head and 17 degrees below eye level. The head has a yaw of 3 degrees and
  the card of 0. Fade 1.00, the copy took 0.21 ms`: each time the card comes
  into the view (80 lines at most), and on a capture frame.
- `the subtitle card hangs from the recenter pose` and `goes to a new place,
  because the head was far away from the old one`.
- On a capture frame: `bwvr-subtitle-card-NNNNN.bmp` (with its alpha) and
  `bwvr-subtitle-card-over-NNNNN.bmp` (over a checkerboard).
- Each report window: the frames with the route, the frames with a card, the
  moves, the time and the size of the copy, the frames with help text and no
  card, and the batches of the fallback. `ui panel` gives the batches that went
  to the target and the half size copies. With the read ahead it gives the
  copies in the queue, the reads, the frame starts with a copy that the GPU
  had not done, and the copies that were never read. The time of the copy is
  then the time of the lock and of the copy in the CPU, with no wait for the
  GPU. `XrScreen` counts the quad layers of the card.

### 17.7 Cost

| Item | Cost |
| --- | --- |
| Memory | The panel target and its depth at the render size (66 MiB at 2880 by 2880), the copy targets (1 to 2 MiB each for a subtitle), a buffer and a swapchain of half the render size (8 MiB, and 24 MiB for 3 images) [I] |
| A frame with a subtitle | One clear of the panel, one bind for each span, one `StretchRect`, one event query, one `GetRenderTargetData`, one copy and one `UpdateSubresource` of the rect. The log gives the time of the copy. |
| The wait for the GPU at the copy | With `subtitle_read_ahead=1`: none. With 0 and `frame_send` 1 or 2: the frame thread waits until the GPU has done both eye passes. Headset sessions with `frame_send=2`, `one_walk=1` and the card in each frame had 4 to 8 ms in each frame that no part of the frame report had, and 45 fps in views that held 90 fps with no card. With `frame_send=0` the frame thread waits for the GPU after the eye passes anyway |
| The copy, with no game [M] | The same Direct3D 9 calls on an RTX 3090: a subtitle strip of 2112 by 304 pixels takes 0.41 ms, the Creature panel 0.32 ms, and a rect that holds both 2.2 ms |
| A frame with no help text | Nothing: no clear, no copy, no layer |
| `subtitle_card=0` | Nothing |

### 17.8 Tests

The math test (`SubtitleCardChecks` in `hint_checks.h`) covers:

- The depths of 17.1: 24 pixels are 1.37 m and 8 pixels are 4.10 m, and the
  angle between the two pictures of a subtitle with the eyes on the Hand.
- The yaw of the head, also for a head that looks straight down.
- The anchor: a head that turns 49 degrees and leans 40 cm moves no card. A
  look of 0.9 s at 80 degrees moves no card. A head that stays at 80 degrees
  moves the card one time, after 1 s. A turn of 180 degrees, a walk of 1.2 m
  and a player who stands up each move it after 1 s. A recenter puts it at the
  new recenter pose.
- The card is at the old place or at the new one in each frame of a move, and
  it takes the new place while it is clear.
- The screen: the strip is 1.6 m away, 17 degrees down, 1 m wide and above the
  land. It faces the head. A pixel has one place in the room in a small rect
  and in a large one.
- The rect of the card, and the fixed shift of the fallback.

The desktop route runs with `subtitle_card_test=1` and `subtitle_selftest=1`:
the two test lines must be on the card and not in the eye pictures, and with
the card off they must be in the eye pictures.

**Not tested:** a real subtitle of the game in a desktop run. The place of the
game's subtitle box in the render is not measured.

The checks in a headset are in `headset-checks.md`, and the checks that a
desktop run can prove are in `roadmap.md`, R12.

### 17.9 In the cut scene window

A flat frame of the cut scene window has the card too (`cutscene-window.md`,
section 7):

- `SubtitleBeginFrame` asks `CutsceneWindowSubtitles`. In a flat frame with
  `cutscene_subtitles=1` the mode is the card, with the same route: the
  HelpText scope and the subtitle box draw into the panel target in eye pass 0,
  and nowhere in eye pass 1.
- `SubtitleEndPasses` gives such a frame to `SubtitleWindowCard`. The card has
  no anchor of its own there and no fade. `CutsceneWindowSubtitleQuad` gives
  its pose: in the plane of the screen, at the yaw of the screen, with its top
  4 cm under the scene. The whole width of the render is
  `cutscene_subtitle_width_cm` (140) on the card.
- `SubtitleCopy` is the half size copy of 17.2, one function for the card of
  the room and the card of the window.
- `XrScreen::EndFrame` adds the card to the quad path of the window, as one
  quad layer over the two eye quads. If the runtime refuses that frame, the
  card is off (`CardFailed`) and the subtitles stay in the pictures.
- When the window is gone, the card has its place in the room again, and its
  anchor rule starts as for a new card.

The log has `the subtitle card hangs under the scene of the cut scene window,
and no subtitle draws in its picture` when the first flat frame takes the
route, and `the subtitle card under the cut scene window: the rect ...` with
the pose and the size each time the card comes into the view.

The math test has checks for the place (`window_checks.h`, "THE SUBTITLES OF
THE WINDOW").

## 18. A speech bubble as one card

### 18.1 How the game draws a bubble [V]

A bubble is a blue box with a tail and a text in it. Three things of the
game have one:

- A wooden signpost. It is a `ScriptHighlight` of the kind "did you know"
  (`ScriptHighlight::IsDidYouKnow` `0x0070AC20`: kind 1 of the table at
  `0x00D96390`). A tap on it (`ScriptHighlight::InterfaceTap` `0x0070AC70`)
  calls `HelpSystem::SetBubbleProperties` `0x005C8330`, which makes the
  signpost the object of the bubble of the help system (`HelpSystem + 0x20`).
  A tap on the same signpost takes the bubble away.
- A villager that speaks (`SpecialVillager::Draw`, call at `0x0071F439`).
- The Creature (`Creature::Draw`, call at `0x00517E78`).

One class draws all three, in three steps:

| Step | Function | What it does |
| --- | --- | --- |
| The layout | `fn_00576F20`. For a signpost `HelpSystem::Draw3D` calls it through `fn_005C83D0` (`0x005C8478`), in the scene walk | It gets the pixel of the sign. `across` is that pixel across the picture, from -1 at the left edge to 1 at the right edge. A sign in the right half gets the offset -10 pixels, and a sign in the left half +10. The anchor is `0.2 + 0.6 * abs(across)`: the part of the width of the box that is left of the sign |
| The place | `fn_00575EA0` (`__thiscall`: the point of the sign, the offset, the anchor, `ret 0xC`) | It takes the depth of the sign from the matrix with no field of view in it (`0x00EA1D28`). The size of the box goes from its full size at 3 units to one fourteenth at 450 units. The bubble fades in from 3 to 8 units and fades out from 300 to 450 units. The left edge of the box is the pixel of the sign, plus the offset, less the anchor times the width. The offset also gives the side of the tail |
| The draw | `0x005767D0` (the bubble in `ecx`), later in the frame: `fn_00576E90` gives the Z sort an object for it (`LH3DZSorter::NewZObject`) | `fn_005766E0` gives the depth of the bubble: the depth of the sign less 5 units, and less again while the camera looks down (times `1 - (2 * pitch / pi)^2`), and two times the near plane at the least. `GatheringText::DrawBubble` `0x008334C0` draws the box and the tail. `GatheringText::DrawText` draws the text through `LH3DTech::Draw3DScreenTriangle`. `SetupThing::DrawBox` draws two arrows when the text is longer than the box, and `fn_005760C0` draws an icon of the text |

`DrawBubble` does not draw the box at its pixels. The box has 16 corners
and the tail has 4. For each corner `DrawBubble` asks
`LH3DTech::Get3DPointFromScreen` `0x0081B370` for a point of the world: the
point at that pixel, at the depth of the bubble. Then it draws those points
through `LH3DTech::Draw3DWorldTriangle` `0x0081C090`, which ends in
`LH3DRender::DrawTriangle`. `Get3DPointFromScreen` makes the point from the
camera point of the game (`0x00EA1DB8`).

**The name of a villager** [V] is a box of `DrawBubble` with no bubble
object:

- The action Show Villager Details (the key S, action `0x1C`, the ring item
  `details`) changes bit `0x200` of the word at `+0xB4` of the villager
  under the Hand (`GGame::DoAction`, `0x0063FCDF`). With no villager under
  the Hand it changes bit `0x200000` of `g_game + 0x14`, which is for each
  villager. `Villager::Draw` calls `Villager::DrawVillagerInfo` `0x0051B510`
  in the scene walk, and it acts only with one of the two bits. A quest
  character (`SpecialVillager::DrawVillagerInfo` `0x0071F240`) shows its
  name while `SpecialVillager::CanShowName` is true and the bit is clear.
- `VillagerName::Add` `0x007629E0` projects the point over the head of the
  villager. It keeps the pixel (`+0x14`, `+0x18`) and the depth in the
  camera (`+0x1C`), and it puts the box above the pixel, in the picture and
  below the names that it made before in the pass.
  `VillagerName::AddDrawing` gives the Z sort an object for it.
- `VillagerName::Draw` `0x00762DC0` draws the box through `DrawBubble` at the
  depth of the bubble (`fn_005766E0` of the depth of the villager), and each
  line of the text through `GatheringText::DrawTextRaw`. Then it deletes the
  name. The game makes a new name in each pass.

### 18.2 What two eye passes do to it

Each step runs in each eye pass, with the camera of the eye. Three things
then put the parts of the bubble at three depths:

1. **The layout.** The pixel of the sign differs between the eyes by the eye
   difference of the sign. So `across` differs, and the two passes can get a
   different offset, a different side of the tail and a different anchor. A
   sign that the player reads is in the middle of the view, which is where
   the offset changes its sign.
2. **The box.** The camera point of the game does not move with the eye
   (`stereo-artifacts.md`, "Moving the camera point with the eye"). So a
   corner that `Get3DPointFromScreen` made comes back at a different pixel
   when the camera of the eye draws it. The box gets the eye difference of
   the depth of the bubble, on top of the eye difference of its pixels. At
   100 units for each meter, a bubble 34 units deep moves 98 pixels in each
   eye picture of 2880 pixels.
3. **The text.** The text code draws the text at its pixels. `draw_hooks.cpp`
   gives each flat batch of that code the interface depth: 8 to 24 pixels
   in each eye.

From far each of the three is small. From near the box is in front of the
text, and both are in front of the sign.

### 18.3 What the mod does (`bubble_card=1`)

The bubble is one flat card at the depth of the sign (`bubble_card.cpp`,
`bubble_card_math.h`):

- **One layout for a frame.** A hook on the place function keeps the layout
  that the game asks for in the first eye pass of a frame. Each later ask of
  that frame gets the same one (`Choose`). The box of the right eye is then
  the box of the left eye, moved by the eye difference of the sign.
- **The side near the middle.** The middle band is 6 percent of the half
  width of the picture, to each side. While the sign is in that band, the
  offset of the frame before stays. So a small turn of the head does not
  change the side of the tail.
- **The box at its pixels.** A hook on the draw of the bubble marks its
  batches. In `h_DrawTriangle` each corner of the box goes back to the pixel
  that the game made it from (`CornerToItsPixel`). The mod has the camera
  point of the game in the camera space of the eye pass. It takes that
  point away from the corner and projects the corner again. The Z value of
  the corner stays, so the land near the sign does not cut the card, as in
  the plain game.
- **The text at its pixels.** `DrawInterface` draws each flat batch of a
  bubble as the game made it: the text, the arrows and the icon. Such a
  batch gets no interface depth and no interface size.
- **One walk for both eyes.** With `one_walk=1` the game places and draws
  the bubble one time, for the left eye, and no batch of the bubble has the
  depth of the sign in it. So the mod draws each batch of the card for the
  left eye only, and then for the right eye only, moved by the eye difference
  of the sign (`BubbleCardWalkMove`, `one_walk_marks=1`). The depth of the
  sign is the one that the place function wrote in this frame. The card of
  the right eye is then the card that the place function makes in a pass of
  the right eye. `backend.md`, "The cards and the marks of the mod", has the
  rule and the key.

With `bubble_card=0` the mod changes nothing of the bubble.

**The name of a villager** (`name_card=1`) is a card by the same rule, at
the depth of the villager. A hook on `VillagerName::Draw` reads the pixel and
the depth of the villager, and makes the draw a draw of a card:

- Each corner of the box goes back to its pixel (`CornerToItsPixel`).
- The text is drawn at the pixels of the game, with no interface depth.
- In one walk the card goes to the right eye moved by the eye difference of
  the villager (`one_walk_marks=1`).

The name has no layout of its own, so it needs no `Choose`. With
`name_card=0` the mod changes nothing of the name, and the log has its
numbers.

### 18.4 Keys, the test key and the log

| Key | Default | Meaning |
| --- | --- | --- |
| `bubble_card` | 1 | 1: a speech bubble of the game is one flat card at the depth of its sign. 0: the bubble is drawn as the game and the interface depth make it in each eye pass |
| `name_card` | 1 | 1: the name of a villager is one flat card at the depth of the villager. 0: the name is drawn as the game and the interface depth make it in each eye pass |
| `bubble_card_test` | 0 | A test key of the desktop runs. 1: one line of the log each 30 frames while a bubble is in view |
| `bubble_card_test_frame` | 0 | A test key of the desktop runs: the frame at which `bubble_card` changes, as the test key does |

With `test_keys=1`, Shift and Delete change `bubble_card` in play. The
pointing hand gets a long pulse for 1 and a short pulse for 0.

The log has the tag `bubble card`. One line comes when a bubble comes into
view. It has these numbers:

- the depth of the sign, and the pixel of the sign in each eye picture
- the layout that the game asks for in each pass, and the layout that each
  pass gets
- the box of the game, the box as drawn and the flat parts as drawn, each
  with how far apart the two eyes have it

In one walk the line has the depth of the sign, its pixel in the left
picture, the eye difference of that depth, the value of `one_walk_marks`, and
the box and the flat parts as drawn in each eye. On a frame of
`one_walk_check=1` (`backend.md`) one more line has the box and the flat
parts of the right eye in its own walk beside those that one walk drew.

The name of a villager has the tag `name card`, with the same lines: the
depth and the pixel of the villager in the place of those of the sign. With
more than one name in a pass, the numbers are those of the first name that
the pass draws. `bubble_card_test=1` gives a line each 30 frames for a name
too.

Each report has the counts.

### 18.5 Tests

The math test (`bubble_card_checks.h`) has the rule of the game for the
layout, and it shows that a near sign in the middle of the view gets a
different side in each eye pass. It checks `Choose`: the first frame, the
side near the middle, and the side far from the middle. For the box it has
`Get3DPointFromScreen` as its listing has it. With eyes 6.6 units apart and
a bubble 34 units deep, the game draws the box 97.9 pixels to the side in
each eye picture. `CornerToItsPixel` puts each corner back within 0.01
pixels, also for a camera of an eye with its own turn and a roll.

A desktop run shows the card. It needs a tap on a signpost.
`bwvr/tools/desk/hand_run.ps1 -Kind fixed -Save temple` makes one, with
`-Steps "seek@4640+1 seek@4700+1 shot@4730+1 grip@4760+10 shot@4840+1
shot@4900+1 shot@4990+1 shot@5050+1"` and these test keys in `-Set`:

- `seek_plan=go,scrolls` and `seek_go=1977.94,2338.25`. The first word
  `seek` puts the head over that place of the land, where the test save has
  a signpost. The second one writes each scroll in view to the log and puts
  the Hand's point on the one nearest to the eye. The log names a signpost a
  silver scroll (type 1). A scroll of a challenge is a gold scroll (type 2):
  a tap on it makes no bubble. The `grip` after that taps the signpost.
- `fake_head_pitch=-60`, so the land under the head is in the pictures.
- `bubble_card_test=1`, and `bubble_card_test_frame=<frame>` for the old
  value in the same run. Each line `bubble card` of the log then has the
  pixel of the sign in each eye, and how far apart the two eyes have the
  box and the text. With 1 the three numbers must be the same. A word
  `shot` after each line saves the two eye pictures.
- `one_walk=0` for two walks. For one walk: `one_walk=1`, `one_walk_check=1`
  and `one_walk_marks_test_frame=4930`, which gives the last two pictures
  `one_walk_marks=0`. A desktop run takes `one_walk` from the installed
  `bwvr.ini` when the run does not set it.

What such a run shows, with a render of 3600 by 2880 and the sign 49.6 units
deep:

| | Sign | Box | Text |
| --- | --- | --- | --- |
| Two walks: how far apart the two eyes have it | 169.9 px | 170.0 px | 169.9 px |
| One walk, `one_walk_marks=1`: how far apart | 169.9 px | 169.9 px | 169.9 px |
| One walk, `one_walk_marks=1`: the right eye, from its place in its own walk | | 0.1 px | 0.0 px |
| One walk, `one_walk_marks=0`: how far apart | | 302.2 px | 0.0 px |
| One walk, `one_walk_marks=0`: the right eye, from its place in its own walk | | 132.2 px | 169.9 px |

The game makes the corners of the box whole pixels in each pass, which is
the 0.1 px. With `one_walk_marks=0` the text of the right eye is beside the
box, not in it. In the pictures of the right eye, the card of one walk with
`one_walk_marks=1` is the card of its own walk.

A desktop run shows the name of a villager in one walk.
`bwvr/tools/desk/village_run.ps1 -Kind job` makes one, with `-Plan
'gostore,store,villager'`, `-Steps "stick_up@800+10 stick_up@830+10
look@860+5000 point@900+1 seek@920+1 seek@1000+1 seek@1040+1 y@1060+30
lpoint@1066+1 lpoint@1096+1 shot@1120+1 shot@1160+1 shot@1220+1
shot@1290+1"` and these keys in `-Set`:

- `radial_menu_items=details,creature,town,leash_learning,more,leash_compassion,leash_aggression,enter`
  and `fake_controller_left_points=-23,-22,40 -20,-30,35`. The third word
  `seek` puts the Hand's point on a villager at the Village Store, and the
  ring of the left hand chooses `details` (`Get-BwvrRingChoice` in
  `game.ps1`, slot 0).
- `one_walk=1`, `one_walk_check=1`, `bubble_card_test=1`, and `name_card` 0
  or 1.

What such a run shows, with a render of 3600 by 2880 and the villager 70 to
80 units deep (an eye difference of 76 to 86 px):

| | Box | Text |
| --- | --- | --- |
| `name_card=0`: how far apart the two eyes have it | 85 to 99 px | 48 px |
| `name_card=0`: the right eye, from its place in its own walk | 77 to 83 px | 77 to 83 px |
| `name_card=1`: how far apart the two eyes have it | the eye difference of the villager | the same |
| `name_card=1`: the right eye, from its place in its own walk | 0.1 to 0.4 px | 0.1 to 0.4 px |

The pixel of the villager in the right eye of its own walk is the pixel of
the left eye with the eye difference of its depth, to 0.4 px. So the depth at
`+0x1C` is the depth in the camera. In the pictures of the right eye, the
name of one walk with `name_card=1` is the name of its own walk.

The same run with `one_walk=0` and `name_card=1` shows the card in two walks.
There the details were on for each villager, and the first name of a pass
was a villager 615 units deep: the villager, the box and the text are each
10.0 px apart in the two eye pictures.

### 18.6 Limits

- `VillagerName::Add` keeps the box of a name in the picture and below the
  other names of the pass. A name at the edge of an eye picture, or one that
  meets another name, can get a different place in each pass of two walks.
- The size of a bubble comes from the depth of the sign in units (full size
  at 3 units, gone at 450 units). The mod does not change that for the scale
  of the God view.
- `fn_00576F20` tests the pointer against the box in each eye pass
  (`0x005772AE` to `0x005773A0`). The pointer in the box keeps the bubble in
  view, and a drag moves a long text. No run has tested that with the box of
  two eye passes.
- `Draw3DWorldTriangle` clips the box at the edge of the picture before the
  corners go to their pixels. So a box at the edge of an eye picture can
  lose a strip, as wide as the move of its corners.
- In one walk the game clips the box and the text at the right edge of the
  picture of the left eye. So a card at the right edge of the picture of the
  right eye can lose a strip there, as wide as the eye difference of the
  sign (`backend.md`, "The edge of the right eye").
- In one walk with `bubble_card=0`, the text has the pixels of the left eye
  in both eyes, with the interface depth. It has no eye difference of the
  sign. No run has shown it. A run shows the same for the text of a name
  with `name_card=0`.
- No run had the name of a quest character.

## 19. A note over a thing

Code: `src/mod/note_depth.cpp` (the hook, the test key and the log), and the
step in `DrawInterface` of `src/mod/draw_hooks.cpp`.

### 19.1 What the game draws [V]

The game shows a short yellow text over a thing in the world for 6 s, with a
sprite. `fn_00827A50` puts such a note in a list (the first note is at
`0x00EB9A74`). The only code that adds a note is the note of a Creature that
learns: `CreatureMental::ConsiderLearningAction` calls `fn_0047CDA0`, which
shows the percent of a lesson (`"%.0f%%"`) over the head of the Creature, at
most each 100 game turns.

Each frame `fn_00827B90` draws the list in the eye pass, from
`fn_0x005e5cd0`. `fn_00827BD0` draws one note:

1. It projects the point over the thing with the camera of the pass
   (`fn_00819490`).
2. It draws the text two times with `GatheringText::DrawTextRaw`: a black
   shadow 1 px down and to the right, then the text in its colour (yellow,
   `0xFFFFFF00`, by default). The depth that it gives is the camera depth of
   the point.

`DrawTextRaw` draws through `LH3DTech::Draw3DScreenTriangle`, which writes
`rhw = near / depth`. So each vertex of the text has the rhw of the place of
the note.

### 19.2 Why the interface depth makes it double

`draw_hooks.cpp` takes each batch of `Draw3DScreenTriangle` as interface. The
interface depth moves a batch to the depth of the nearest world, and in one
walk the right eye gets the pixel of the left eye with that depth. Neither is
the depth of the thing. In a desktop run at a render of 3600 by 2880, the
note over the Creature was 50 units from the eye. The eye difference of its
place was 168 px, and the interface depth gave the note 48 px. The eyes look
at the Creature, so they see the note two times, 120 px apart.

### 19.3 What the mod does (`note_depth=1`)

`h_NotesDraw` marks the draw of the list in an eye pass. A batch in that draw
gets none of the steps of the interface: no interface depth, no interface
size, no hint rule and no panel.

- Two walks: each eye pass draws the note where the game put it for that
  eye. The camera of each eye gives the eye difference of the place.
- One walk: the batch goes to both eyes as a batch of the world. The right
  eye gets the move of its rhw (`one_walk_math::CopyForRightEye`), with
  dxwrapper and with the Vulkan renderer. The rhw is that of the place, so
  the move is the eye difference of the place.

So the note is at the depth of the thing in both eyes, beside it, at the
size of the game. The depth test of the game stays: a part of the note
behind the head of the Creature does not show, as in the plain game.

`note_depth=0` gives a note the steps of the interface.

### 19.4 Keys, the test keys and the log

| Key of `bwvr.ini` | Default | Meaning |
| --- | --- | --- |
| `note_depth` | 1 | 1: a note of the game over a thing has the depth of its place. 0: the interface depth |

With `test_keys=1`, Ctrl, Shift and the apostrophe key change `note_depth` in
play. The pointing hand gets a long pulse for 1 and a short pulse for 0.

Test keys for the desktop runs:

- `note_test=<frames>` (up to 4 frames, with commas): at each frame the mod
  asks the game for the note of a Creature that learns over the player's
  Creature, with 37 percent (`fn_0047CDA0`). The game shows a note of a
  Creature at most each 100 game turns, and a test save starts soon after
  its own start. So the test puts the turn of the last note of the Creature
  101 game turns back. When the game shows the note, the mod saves the eye
  pictures of a frame 20 frames later.
- `note_depth_test_frame=<frame>`: `note_depth` changes at that frame, as
  the key does.

The frames of both keys count as the steps of the scripted controller count
(`fake_controller_play_frame`).

The tag `note depth`:

- At the start: `note_depth`, and the test keys.
- `the game shows a note over a thing: "37%", colour 0xFFFFFF00, ...`: each
  new note, with its text, its colour and its place.
- `a batch of a note in one walk: x ... in the left eye as the game put it,
  rhw ... The right eye has it N px from the left eye`: the first batch of a
  new note, and then each 150 frames while a note shows. With two walks the
  line is for each eye pass.
- Each report window: the new notes, and the batches at the depth of their
  place and at the interface depth.

### 19.5 Tests

The math test (`OneWalkChecks`) has a note over a Creature 300 units away:
the right eye of one walk has it at the pixel of the point in that eye, with
its layout. The interface depth puts the eyes more than 20 px from the eye
difference of the place.

The desktop run is `creature_recipe.ps1 -Kind leash` in the test save
`temple`, with `note_test=870,1700` and `note_depth_test_frame=1300`: a note
over the Creature with `note_depth=1`, and one with 0. Add
`test_frame_rate=60` for the Vulkan renderer, and
`backend=dxwrapper\ddraw.dll` for Direct3D 9.

With one walk, the Vulkan renderer and dxwrapper gave the same result. The
note was 51 units from the eye. With `note_depth=1` the right eye had it 166
px to the left of the left eye, which is the eye difference of its place,
and the eye pictures show it over the head of the Creature in both eyes.
With 0 the right eye had it 48 px to the left, about 120 px to the right of
the Creature. With two walks (`one_walk=0`) and the Vulkan renderer, the
camera of each eye pass gave the note 166 px between the eyes. With
`note_depth=1` that stays. With 0 the interface depth adds its move in each
eye, so the note is not at the depth of the Creature there either.

### 19.6 Limits

- No run has shown a note that the game made by itself: a Creature that
  learns from a miracle of the player. The test key uses the same function
  of the game.

The checks in a headset are in `headset-checks.md`, and the checks that a
desktop run can prove are in `roadmap.md`, R12.

## 20. The menus in the room

The game draws each menu as a `SetupBox` dialog in screen space. In the eye
pictures a menu is then fixed to the head: it turns with the head, and the
player cannot look around it. With `menu_room=1` and a headset, the menus
are on a panel that is fixed in the room. The code is in `menu_room.cpp`
and `menu_room_math.h`. It works with `ui_panel=0`. With `ui_panel=1` the
interface panel of that key has the menus.

### 20.1 The paths that draw a menu [V]

Each menu is a dialog of the Setup callback `0x006424E0` in
`LH3DRender::FinishFrame` (1.2). `SetupBox::GetCurrentActiveBox`
`0x00407ED0` returns the box that is up (`0x00C4CD20`). Two kinds of frame
draw a menu:

| Menu | The frame | Without the menus in the room |
| --- | --- | --- |
| The Main Menu over the loaded game at the start (`backend.md`, "A test save") | A frame with the scene: `GGame::Loop` runs `Process3dEngine`, so the eye passes run [R] | In the eye pictures, with the interface shift: fixed to the head |
| The game menu of Esc (`MainMenu`), the options, the load and save boxes in play | A frame with the scene | The same |
| The boxes of the front end: the profile, the name of a new god, the land, the options of the front end | A frame with no scene: `FrontEnd::JustDoABox` `0x00544F90` draws the plasma, runs `FinishFrame` with the callbacks on, and then `LHScreen::Flip` | The whole frame, flat: on the projection layer at the eye poses of the last stereo frame, or on the screen of 1.4 before the first stereo frame |
| `DoCitadelDraw`, the loop at `0x0053CD1F` and the other loops of 1.4 that show a box | A frame with no scene | The same |

The panel covers each of them. A loop with no eye passes that shows no menu
(a loading screen, a video) keeps the route of 1.4.

### 20.2 What the mod does

**A frame with the scene.** `UiPanelBeginFrame` latches the route one time,
before the eye passes (`MenuRoomBeginFrame`). When a box is up, the draws of
the Setup callback go to the panel target in eye pass 0, and not to the eye
pictures (`UiPanelRouteMenu` in `draw_hooks.cpp`). They draw at the pixels
that the game gave them, and these are the pixels that the hit test of the
box reads. The help text keeps the route of the subtitle card. After eye pass
0 the menu rect of the target (4.1) goes out as one quad layer over the
projection layer, as with `ui_panel=1` (`UiPanelQuads`, `MenuRoomQuad`). With
`backend=vulkan` the quad takes the panel image of the Vulkan renderer
(`vk_panel=1`).

**A frame with no scene.** `h_FinishFrame` in `mod_main.cpp` calls
`MenuRoomFlatFrame` before the frame goes to the headset. When a box is up
and the Setup callback drew it, the frame goes out as a screen at the place
of the panel: the menu rect of the flat picture, the same picture for both
eyes (`XrScreen::StageWindow` with `WindowLayer::rect`). No eye pass runs in
such a frame, so this function also runs the hands of the frame
(`ControllersFrame`), the pointer (`PointerMapApply`) and the keyboard
(section 21). `JustDoABox` makes one frame with no callbacks when a box
opens. A frame with no draw of a menu less than 200 ms after one with a
draw keeps the screen.

The route goes back to the eye pictures, and the log says why, in these
cases: the frame has no projection layer (the cut scene window, play mode
1), the panel target is not shared with the headset, the session has no
swapchain for the panel, or the runtime refused a frame with the panel
(`XrScreen::PanelFailed`).

### 20.3 The place

The panel goes in front of the head in the first frame of a menu
(`menu_room_math::PlaceMenu`):

- The anchor is the head of that frame: the middle of the two eyes in the
  LOCAL space, and the yaw of the head. The yaw comes from the right axis of
  the head, so a head that looks down has a yaw. The panel has no roll.
- The center of the panel is `ui_menu_distance_cm` (140) from the anchor,
  `ui_menu_pitch_deg` (-10) below eye level. The panel turns about its own x
  axis by the same angle, so it faces the head.
- The panel shows the menu rect (4.1) at `ui_menu_px_per_deg` (16). The
  default rect of 1280 by 960 pixels is then 2.35 by 1.76 m at 1.4 m. The
  front end menu of 780 by 570 pixels is 49 degrees across.

After that the panel stays at its place in the room. It never follows the
head. A box that leads to the next box keeps the place: the menus must be
closed for 1 s before the next menu gets a new place. A recenter (F11, or
the headset menu) puts the panel in front of the head of that moment, as
with the cut scene window (`cutscene-window.md`, 5.1).

### 20.4 The pointer on the panel

The aim ray of the pointing hand meets the panel in the LOCAL space
(`menu_room_math::Hit`, the math of 5.3). The pixel under the ray is a
pixel of the menu rect, so it is the pixel of the render that the game drew
the dialog at (`Compute` in `controllers.cpp`, `MenuRoomRayPixel`). After
that the pointer takes the path of a dialog box (`controllers.md`, 3.6): the
trigger is the left button, and the grip is the right button. The game
draws its own arrow at that pixel, on the panel.

- A ray past the edge of the panel puts the pointer at the edge. The pointer
  is then off the view, and a press does not reach the game.
- A ray that does not meet the plane of the panel gives no pixel. The
  pointer keeps its last pixel.
- In a frame with the scene the thin ray of the hand ends on the panel. A
  frame with no scene draws no ray, and the arrow of the game shows the
  pointer.
- In a frame with no scene the controller can own the pointer
  (`ChooseOwner` in `pointer_map.cpp`, `Usable` in `controllers.cpp`). The
  front end calls no `LHMouse::UpdateDeltaPos`, so `MenuRoomFlatFrame` calls
  `PointerMapApply` itself. The buttons go to the game through
  `LHScreen::Flip`, which calls `AnyWindowMessages`, the hook of the pump.
- The head does not turn the pixel of the mouse while the menus are on the
  panel (`LockToRoom`), because a pixel of the panel is fixed in the room.

### 20.5 Keys and the log

| Key | Default | Meaning |
| --- | --- | --- |
| `menu_room` | 1 | 1: in the headset the menus are on a panel that is fixed in the room. 0: the menus are in the eye pictures, fixed to the head |
| `menu_room_test` | 0 | A test key: the route of a frame with the scene runs with no headset. The eye pictures lose the menus, the pointer of the scripted controller meets the panel, and a capture saves the panel (`bwvr-ui-NNNNN.bmp`, 12.4) |
| `ui_menu_distance_cm`, `ui_menu_pitch_deg`, `ui_menu_px_per_deg`, `ui_menu_rect` | 140, -10, 16, 0 | The place, the size and the rect of the panel (section 10) |

`menu_room` has no keyboard test key. With `menu_room=1` the session makes
the panel swapchain, and the bridge makes a shared panel texture of the
render size, also with `ui_panel=0`.

The log has the tag `menu room`:

```
menu room    : menu_room=1, in the headset the menus show on a panel that is fixed in the room. vr_keyboard=1, ...
menu room    : frame N, the menus go to the panel in the room, and not to the eye pictures
menu room    : frame N, the menu panel goes in front of the head: 2.35 by 1.76 m, its center at x y z m, the head at x y z m with a yaw of Y degrees. It shows the rect x y w h of the W by H render
menu room    : frame N, a frame with no scene shows a menu, so the part x y w h of its picture goes to the panel in the room, and the controller points on it
menu room    : the ray meets the menu panel T m from the hand at U V of the panel, and the pixel is X Y
menu room    : frame N, the menus stay in the eye pictures: <the reason>
  openxr: a frame with no scene goes out as 2 quad layer(s) of the menus in the room, ...
```

### 20.6 Tests

The math checks are in `test/controller_math/menu_room_checks.h`: the place
for a head that looks ahead and for a head that is turned and looks down, a
ray at the center, at a corner and past the edge, a ray from a hand that is
not at the eye, and three rays that do not meet the panel.

A desktop run in a test save with `menu_room_test=1` and
`menu_skip_test_frame=0` keeps the Main Menu of the start up. The render is
3600 by 2880. With `backend=vulkan` and with `backend=dxwrapper` the run
shows the same numbers:

- In frame 1 the panel goes in front of the head: 2.35 by 1.76 m, its
  center at 0.00 -0.24 -1.38 m, the rect 1160 960 1280 960. The panel gets
  one place.
- 611 frames with the scene have the menu on the panel, and 610 of them
  have the ray on the panel. The eye pictures have no Main Menu. The panel
  capture (`bwvr-vk-panel-NNNNN.bmp` with Vulkan, `bwvr-ui-NNNNN.bmp` with
  dxwrapper) has the menu, with the arrow of the game on "Continue Game".
- The ray of the scripted hand meets the panel 1.12 m from the hand at
  0.500 0.371 of the panel. That is the pixel 1800.0 1316.6, on "Continue
  Game". The scripted trigger is down from frame 600 to frame 608. In frame
  611 the game plays its menu click and closes the dialog. In frame 612 the
  route goes back to the eye pictures, and the Hand comes back.

With `menu_room=0` the eye pictures have the Main Menu, fixed to the head.
The same hand then points at the pixel 1800.0 1454.4 of the view, and that
is "Join Online Game".

The box "Save Game Message" of the Save Game Room goes to the panel in the
same way, and a click of the ray on its Cancel closes it (21.5). The desktop
runs do not show a frame with no scene that shows a menu (the front end),
the game menu of Esc, the options, or the load box. The invented head does
not turn in them.

The checks in a headset are in `headset-checks.md`, and the checks that a
desktop run can prove are in `roadmap.md`, R12.

## 21. The keyboard in the headset

Some boxes of the game ask for text. While a text box has the focus, a
keyboard shows below the menu panel, and the ray of the pointing hand
presses its keys. The code is in `vr_keyboard.cpp` and
`vr_keyboard_math.h`.

### 21.1 The text boxes of the game [V]

A text box is a `SetupEdit` control (the table of its virtual functions is
at `0x008AB3A0`). The control that has the focus of a box is at box +
`0x70` (`SetupBox::SetFocusControl` `0x00409140`). These boxes make a
`SetupEdit` (the callers of its constructor `0x0040C220`):

| Box | What it asks for |
| --- | --- |
| `NewProfileBox` | The name of the god of a new profile, in the first start |
| `ProfileEditor` | The name of a profile |
| `DialogBoxSaveMessage` | The name of a saved game |
| `LoginBox`, `RegisterBox`, `MPFELogin` | A name and a password for the online game |
| `LobbyBox`, `GSLobbyBox`, `GatheringBox`, `ChannelBox`, `MPFEChannelSelector`, `MPFECreateGame`, `MPFEChooseConnection`, `IPSpecialDialog`, `SetupMultiplayerMain` | The games for more players: a name, a message, an address |
| `SetupLandscapeBox`, `SetupOnlineLandscapes`, `SpellSetupBox` | The setup of a land and of the miracles of a game for more players |
| `EditingDebugBox` | A box of the debug code |

No box asks for the name of the Creature.

### 21.2 How the game reads a key [V]

The window procedure `GameWindowProc` `0x007DBF30` puts the character of
each `WM_CHAR` in a queue of 16 characters at `0x00E855A0` (at `0x007DC199`,
after `TbIME_ConvertCHAR8toCHAR16`). The box gives one character of the
queue to the control that has the focus in each pass (`0x00408EC8`, then
`SetupEdit::Char` `0x0040B5F0`). `WM_KEYDOWN` and `WM_KEYUP` go to
`LHKeyboard::ProcessKeyboard` `0x007DCAF0`, which sets the last key. The
loops of the menus give the last key to the box: `FrontEnd::JustDoABox` at
`0x00545125` and `GGame::ProcessKey` at `0x0063F222` call `fn_00408F80`,
which calls `SetupEdit::KeyDown` `0x0040BAF0`. `ProcessWindowMessages` calls
`TranslateMessage`, so a key of a computer gives `WM_KEYDOWN`, `WM_CHAR` and
`WM_KEYUP`.

### 21.3 What the mod does

- **When.** `vr_keyboard=1`, the menus are on the panel in the room, and the
  control that has the focus of the active box is a `SetupEdit`. The
  keyboard goes away when the focus goes to a different control or the box
  closes.
- **The place.** The keyboard card hangs from the anchor of the panel
  (20.3): 1 m from the head, 35 degrees below eye level, tilted to face the
  head. It is 70 cm wide (38 degrees) and 26 cm high. It is nearer than the
  panel and below the dialog. In a frame with the scene it is a card over
  the projection layer. In a frame with no scene it is a card over the
  screen of the menu.
- **The look** follows `hint_card_style`, the key of the hint cards (the
  launcher row "Card look", and the switch "Card look" of the debug page).
  With 1 the card is a page of the parchment of the hint cards
  (`controller-hints.md`, 9.11), with a torn and burnt edge. Past the torn
  edge the card is clear. Each key is a frame of dark ink round a face that
  is pressed into the paper: the wall at the top and the left of the face is
  in shade, and the wall at the bottom and the right has the light. The
  letter is in dark ink in Alegreya Bold, with a capital of 30 px, and the
  words Space, Back, Enter and Shift are in Alegreya SC with a capital of 19
  px. Alegreya has old style digits only, and its zero looks like a small o.
  So the digits are in Palatino Linotype Bold (then Book Antiqua, then
  Cambria, all fonts of Windows), as high as a capital letter, and the zero
  is narrower than the O. The key under the ray has a gold face, a frame of
  dark gold and a rim of bright gold 3 px wide. The key that the trigger
  pressed has a face of bright yellow gold and a pale rim for 120 ms. While
  Shift is on, the Shift key has a red wax seal with its word in light
  letters. With 0 the card is dark and opaque, with grey keys in Segoe UI:
  the key under the ray is gold, the pressed key is pale gold, and Shift on
  is blue.
- **The keys.** Five rows: the digits and Backspace, three rows of letters
  with Enter and Shift, and the space bar. Shift makes the next letter a
  capital. It is on when the keyboard opens, so the first letter of a name
  is a capital.
- **The ray.** The keyboard is nearer than the panel, so the ray of the
  pointing hand meets it first. The key under the ray has a highlight, and
  the hand gets a light pulse at each new key. With the ray on the keyboard
  the pointer of the game is off the view, so the trigger makes no click in
  the game.
- **A press.** A pull of the trigger past 60 percent presses the key under
  the ray, with a pulse of 20 ms at 0.3 and a short flash of the key. The
  trigger must go below 40 percent before the next press. A pull that
  starts off the keys presses nothing.
- **The key to the game.** The mod sends `WM_KEYDOWN` with the scan code of
  the key, and then `WM_CHAR` with the character, to the window of the game
  with `SendMessageA` on the game thread. `WM_KEYUP` follows 60 ms later. So
  the text box gets the key as from a keyboard of a computer. Backspace
  sends `VK_BACK` and the character 8, and Enter sends `VK_RETURN` and 13.

### 21.4 Keys and the log

| Key | Default | Meaning |
| --- | --- | --- |
| `vr_keyboard` | 1 | 1: the keyboard shows while a text box has the focus. 0: no keyboard |
| `hint_card_style` | 1 | 1: the keyboard is a page of parchment with keys of ink. 0: the dark keyboard. The key also sets the look of the hint cards and of the spell book |
| `vr_keyboard_test_save` | 0 | Test key. N saves the first N new pictures of the keyboard as `bwvr-keyboard-FFFFF.bmp` over a checkerboard (FFFFF is the frame), each with a log line that names the key under the ray, the key of the flash and the Shift |

The log has the tag `keyboard`: one line when the keyboard shows, with its
place, one line for each key that goes to the game, and one line when it
goes away.

```
keyboard     : frame N, a text box of the game has the focus, so the keyboard shows below the menu panel, 0.70 by 0.26 m at x y z m
keyboard     : frame N, the key A goes to the game (virtual key 0x41, character 0x41)
keyboard     : the keyboard goes away: no text box has the focus
```

### 21.5 Tests and limits

The math checks are in `test/controller_math/menu_room_checks.h`: each key
is on the card, no two keys overlap, no two boxes of a key with its rim of
3 px overlap, the center of each key finds that key,
what each kind of key sends, the shift, and one key for each pull of the
trigger.

A desktop run shows the keyboard in the box "Save Game Message" of the Save
Game Room, with `backend=vulkan`, the test save `temple` and
`menu_room_test=1`:

```
temple_run.ps1 -Name kbd -Dll <dll> -Rooms "room_save@3400" -Points "0,-2,30"
  -Set "menu_room_test=1;test_frame_rate=60;capture_every_seconds=4;aim_preset=0;temple_test_scan=10;temple_test_targets=e0 e0;fake_controller_turns=-2,-50,1 22,-40,1 -27,-36,1 -10,-18,1"
  -Steps "point@5700+1 aimtemple@5800+1 trigger@5840+6 aimtemple@6000+1 trigger@6040+6
          aimturn@6150+1 trigger@6190+6 aimturn@6240+1 trigger@6280+6 trigger@6320+6
          aimturn@6370+1 trigger@6410+6 aimturn@6550+1 trigger@6600+6"
```

- Two clicks of the ray on the picture of an empty slot open the box. In
  the same frame the panel goes in front of the head, and the keyboard shows
  at 0.00 -0.57 -0.82 m, 0.70 by 0.26 m. The text line has the focus when
  the box opens.
- From the point `0,-2,30` the aims `-2,-50`, `22,-40` and `-27,-36` are
  on B, W and Backspace. Four presses (B, W, W, Backspace) send `0x42`,
  `0x77`, `0x77` and `0x08`. The panel capture shows "Bw" in the text line:
  the first letter is a capital, and Backspace removes a letter.
- Each press on the keyboard has the line `the left button press started
  off view, so it is dropped`, so no press on a key reaches the game.
- The aim `-10,-18` meets the panel at 0.579 0.572, the pixel 1901.5
  1509.4, on Cancel. At the press the keyboard goes away (the text line
  loses the focus), and the box closes 8 frames later. The run writes no
  file in the folder of the slot.

The run does not use Esc to close the box. A desktop capture does not save
the keyboard card. The log and the text in the panel capture show the keys.
With `vr_keyboard_test_save=24` in `-Set` the run saves each new picture of
the card.

- With `hint_card_style=1` the pictures show the page, the red seal of
  Shift on, the gold key under the ray (B, W, Backspace), the bright yellow
  flash of each press, and the small letters after the B.
- The page takes 1.2 ms one time. The first picture draws the 40 keys in
  17 ms, most of it the first draw of each letter by GDI. The first change
  of Shift draws the 26 letters and Shift again in 7 ms. Each other new
  picture draws 1 or 2 keys in 0.2 to 0.3 ms, or copies the boxes of the
  keys in less than 0.01 ms.
- With `hint_card_style=0` the pictures show the dark card. Each new
  picture of the dark card draws all of it, in 0.9 to 1.2 ms (7.7 ms for
  the first picture and 3 ms for the first small letters).

- The hand does not press a key by touch. Only the ray and the trigger do.
- The text box of a frame with no scene (the name of a new god) needs the
  first start of a profile. The profile guard of the desktop scripts takes
  a new profile away after a run, but no run has done it yet.

The checks in a headset are in `headset-checks.md`, and the checks that a
desktop run can prove are in `roadmap.md`, R12.

### 21.6 The bake of the parchment

The page is drawn one time, into a picture of its own. A key in a look
(plain, under the ray, pressed) is drawn the first time that it shows, over
the page, and the mod keeps a copy of its box: the key and its rim of 3 px
(`card_scroll::kKeyRim`). The letters and Shift have a copy for Shift on and
one for Shift off. A new picture of the card copies the box of each key
whose look changed, and draws only a key that has no copy yet. The card
needs two pictures of 1.5 MB (the page and the card) and at most 240 copies
of about 26 KB.

## 22. The panel of places

The item `places` of the ring (`controllers.md`, section 10) opens a page
of buttons for the places of the game and its 7 bookmarks. The ray and the
trigger of the pointing hand press its buttons, as on the keyboard in the
headset (section 21). The code is in `places_panel.cpp` and
`places_panel_math.h`.

### 22.1 What the panel has

| Row | Buttons | What a press does |
| --- | --- | --- |
| The places | Temple, Creature, Town, Land | The items `temple`, `creature`, `town` and `land` of the ring: the Temple key two times, one teleport to the Creature in the God view, Tab, and the key of the whole land |
| Bookmarks 1 to 7 | A wax seal with the number, then Go, Set here, Follow, Remove | The key of the number with no modifier, with Ctrl, with Shift, and with Ctrl and Shift (`controls-audit.md`, section 3) |
| The bottom row | Back, the note, Close | Back is the item `mark_back` of the ring (Backspace). Close closes the panel |

- **The keys go the way of the ring.** Each key goes into the key buffer of
  the game at the next pass of the pump (`controllers.md`, 10.3). So each
  gate of the game holds: a dialog box, a cut scene, the tutorial.
- **A button that cannot act now is dim.** Go, Follow and Remove of a
  bookmark that is not set are dim. All the bookmark buttons are dim while
  the game takes no bookmark key (`0x009CD384`, the two flags of the control
  map). A place is dim for the reasons of its item on the ring, for example
  "You have no Creature". The seal of a bookmark that is not set is pale.
- **The note** between Back and Close says what the button under the ray
  does ("Set bookmark 3 at the Hand"), or why it is dim ("Bookmark 3 is not
  set"). After a press it says what the press did for 2 s.
- **What closes the panel.** Go, Follow, the places and Back move the view,
  so they close the panel. Set here and Remove keep it open, so the player
  sees the seal change. Close, the ring, the spell book, a dialog box, a cut
  scene and the Temple also close it. With `places_close_buttons=1` B of the
  pointing hand and the menu button close it too, as they close the debug
  window: B then draws no gesture, and the menu button sends no Esc.

### 22.2 The place

The panel opens in the frame of the choice on the ring, in front of the
head of that frame (`places_math::PlaceAt`):

- The anchor is the head of the frame and its yaw, as for the menu panel
  (20.3). The middle of the panel is 1 m from the head, 12 degrees below eye
  level, and the panel turns to face the head. It has no roll.
- It is 56 by 56 cm, 31 degrees at 1 m: from about 3 degrees above eye
  level to 27 degrees below it. A button is 11 by 4 cm, about the size of a
  key of the keyboard.
- After that the panel stays at its place in the room. The player moves
  and the panel does not. A recenter puts it in front of the head again.

Why there. The God view has no menu panel in the room, so the panel needs
a place of its own. 1 m is out of the reach of the hand, so the ray, and not
a touch, presses a button. The panel is a quad layer over the world, and a
quad layer covers each thing of the eye pictures behind it, also the hand.
A hand that points at the panel is lower than its bottom edge, so the panel
does not cover that hand.

### 22.3 The ray and the trigger

- The aim ray of the pointing hand shows while the panel is open
  (`ControllersMenuRay`). It ends at the panel, or 2 m from the hand when
  it is not on the panel. The hand has the pose of a pointing finger.
- The button under the ray is gold, and the hand gets a light pulse at each
  new button (8 ms at 0.1), as on the keyboard.
- A pull of the trigger past 60 percent presses the button under the ray,
  with a pulse of 20 ms at 0.3 and a short flash of the button. The trigger
  must go below 40 percent before the next press. A pull that starts off
  the buttons presses nothing. A press of a dim button gives the pulse
  `refused`.
- While the panel is open, the trigger belongs to the panel, and no press
  goes to the game (the pump of `controllers.cpp`). After a press that
  closed the panel, the trigger stays with the panel until it goes up. So
  the press that closed the panel is no click in the game.

### 22.4 The look

The look follows `hint_card_style`, as the keyboard does (21.3).

- With 1 the panel is a page of the parchment of the cards (`card_scroll.h`)
  with a torn edge. The title "Places" is in Alegreya Bold with a capital of
  38 px, and the header "Bookmarks" in Alegreya SC. Each button is a key of
  ink with its word in dark ink (a capital of 26 px). The button under the
  ray has a gold face and a gold rim, and the pressed button is bright
  yellow gold for 120 ms. A dim button and the pale seal go 65 percent of
  the way back to the paper. Each bookmark has a red wax seal with its
  number in the digits of the keyboard (Palatino Linotype).
- With 0 the panel is dark and opaque, with grey buttons in Segoe UI: gold
  under the ray, pale gold for a press, dark grey for a dim button.

**The bake.** The page with the title and the header is drawn one time. A
button in each look (plain, under the ray, pressed, dim) and a seal in each
look (set, not set) are drawn the first time that they show, and the mod
keeps a copy of the box. A new picture copies the box of each part whose
look changed. The note is drawn again each time that it changes.

The panel uses the quad layer of the ring (`kCardRing`), 1024 by 1024
pixels, as the ring does. The ring and the panel never show at one time.

### 22.5 Keys and the log

| Key | Default | Meaning |
| --- | --- | --- |
| `places_panel` | 1 | 1: page 2 of the ring as made has the item `places`, and it opens the panel. 0: page 2 has `mark_set`, `mark_go` and `mark_back`, and the word `places` is an empty slot |
| `places_close_buttons` | 1 | 1: B of the pointing hand and the menu button close the panel. 0: they do not, and B can start a gesture |
| `hint_card_style` | 1 | 1: the panel is a page of parchment. 0: the dark panel |
| `places_panel_test_save` | 0 | Test key. N saves the first N new pictures of the panel as `bwvr-places-FFFFF.bmp` over a checkerboard (FFFFF is the frame) |
| `places_test_buttons` | | Test key. The buttons for the word `aimplaces` of the scripted controller, in order: `temple`, `creature`, `town`, `land`, `back`, `close`, and `go`, `set`, `follow`, `remove` with the number, for example `set3,land,go3` |

The log has the tag `places`:

```
places       : the panel opens, 0.56 by 0.56 m, its middle at x y z m, the head at x y z m with a yaw of Y degrees, frame N
places       : the ray is on set 3, frame N
places       : the trigger presses set 3, frame N
ring         : the panel of places: the key Ctrl and 3, frame N
places       : the trigger presses go 3, and it does not act: Bookmark 3 is not set, frame N
places       : the panel closes: the player pressed land, frame N
places       : the panel closes: the player pressed B, frame N
places       : the panel closes: the player pressed the menu button, frame N
```

### 22.6 The parts for other panels

The parts of the panel that are not places are in `panel_kit.h`,
`panel_kit.cpp` and the pure `panel_kit_math.h`, so the next panels in the
room can use them:

- `panel_kit::Room`: `Open` places a panel in front of the head (a
  distance, a pitch and a size), `FollowRecenter` places it again after a
  recenter, and `Step` once a frame gives the part under the ray (a hit
  function of the caller, a light pulse at each new part) and the part that
  a pull of the trigger pressed. `Pressed` gives the stronger pulse and the
  flash, and `StageLayer` sends the picture as the quad layer of the ring.
- `panel_kit::Canvas`: the GDI picture, and the parchment. `StartParchment`
  draws the page one time, with a function of the caller for the parts of
  the page that never change. `InkWords`, `PageToCard` and `Fade` draw on
  it.
- `panel_kit::Tiles` and `ShowPart`: a copy of the box of each part in each
  look, so a new picture copies the parts that changed.
- `panel_kit::Button`: a button of the parchment in the looks plain, under
  the ray, pressed and dim. `PlainButton` is the same button in the dark
  look.
- `panel_kit_math`: `PlaceAt`, `HitRects`, `RectBox`, and the trigger with
  its band (`Press`, `PressStep`, `PressOpen`).
- The widgets of a panel of settings (the debug window, section 23):
  `Toggle` (a slot with a knob of wax), `ArrowKey` (a key with a triangle of
  ink), `ScrollBar`, `InkWordsIn` (words at the left, in the middle or at
  the right of a box), `InkLines` (words that wrap), and `Note` with `Over`
  (a small card with rolled ends, laid over the card). They draw into a
  picture of the caller, so a part can be drawn one time and copied to each
  place. The look `kOn` is a button that is on now, for example a tab.
- `panel_kit_math`, the list that scrolls: `MaxOffset`, `ClampOffset`,
  `Thumb`, `OffsetOfThumb` (the drag of the thumb), and `StickScroll` (the
  speed of a scroll by the stick).

The caller keeps the trigger from the game while its panel is open
(`PlacesPanelHoldsTrigger` and `DebugWindowHoldsTrigger` in the pump of
`controllers.cpp`), and it decides what closes its panel.

### 22.7 Tests and limits

The math checks are in `test/controller_math/places_checks.h`: the buttons
and their kinds, each button on the paper, no two boxes with the rim that
overlap, the middle of each button finds it, the keys of a bookmark, the
items of the places, what closes the panel and what is dim, one press for
each pull of the trigger, the place 1 m out and 12 degrees down, a ray from
the head to each button, and the test key.

A desktop run (the test save `temple`, dxwrapper, `test_frame_rate=60`,
`aim_preset=0`, `places_panel_test_save=60`,
`places_test_buttons=set3,land,go3,follow3,remove3,go3,close`) opens the
panel from page 2 of the ring five times: Y, the left hand to More, then to
`places`. The word `aimplaces` aims the ray at each button, and the word
`trigger` presses it.

- The panel opens at 0.00 -0.21 -0.98 m from a head at 0 0 0.
- Set here 3 sends Ctrl and 3, and bookmark 3 is set in the next frame.
  The panel stays open, and its seal turns red.
- Land sends F3, the panel closes, and the God view goes to the jump of
  the game camera.
- Go 3 sends 3, the panel closes, and the view goes back to the Temple,
  where bookmark 3 is.
- Follow 3 sends Shift and 3. Bookmark 3 is on the land, not on an object,
  so the game camera stays where it is.
- Remove 3 sends Ctrl, Shift and 3, and bookmark 3 is not set in the next
  frame. Go 3 is then dim, and its press does not act, with the note
  "Bookmark 3 is not set". Close closes the panel.
- The log has no line of a press of the left button of the game.
- With `backend=vulkan` and `hint_card_style=0` the dark panel shows, the
  press of the dim Go 1 does not act, and Close closes the panel.
- With `places_panel=0` and `snapshot_item=0` page 2 is `mark_set`,
  `mark_go`, `mark_back`, `pause`, `more`, `debug`, `names`, `save` (the
  owner's ini has `debug_menu=1`), and the three bookmark items send Ctrl
  and 1, Ctrl and Tab, and Backspace, as before.
- The first picture of the panel draws its 42 parts in 18 ms, and the page
  takes 8 ms one time. Each other new picture takes 0.4 to 1.6 ms, or 0.01
  ms when it only copies.

Only a headset shows the panel, its place and its size, whether the ray
finds a button with ease, whether a hand that points stays below the
panel, and what Follow does with a bookmark on an object in the God view.

## 23. The debug window

The item `debug` of the ring opens a window with the switches of the debug
page (`controllers.md`, 10.8), with `debug_menu=1` and `debug_window=1`.
It is built on the parts of section 22.6. The code is in `debug_window.cpp`
and `debug_window_math.h`, and the notes of the switches are in `kTips` of
`debug_menu.cpp`.

### 23.1 What the window has

The picture is 1024 by 1024 pixels:

| Part | Where | What it is |
| --- | --- | --- |
| The title | The top | "Debug switches", or "Settings" for the settings window (section 24) |
| The tabs | A column at the left, 196 by 62 px each | One for each group. The tab of the group that shows is bright gold (`kOn`) |
| The list | 692 by 760 px, 10 rows of 76 px | The switches of the group, one row for each: the label at the left and the control at the right. An ink rule is at the foot of each row, and above and below the list |
| The scroll bar | At the right of the list | Only for a group with more than 10 rows. The thumb is the share of the list in the view, at least 56 px |
| The bottom row | Under the list | The words of the last press at the left, and Close at the right |
| The note | Over the list | After a rest of 0.35 s on a row: the name of the switch, its note in one to three sentences, and its ini key. It goes below the row, or above the row when it does not fit below. It does not catch the ray |

The control of a row:

- **A toggle** for a switch with two values: the word of the value, and a
  slot (a key of ink) with a knob of wax. The knob is green at the right for
  the value 1, and red at the left for 0. A press anywhere on the row changes
  it.
- **Two arrows** for a switch with more values or a number: a key with a
  triangle at each side of the word of the value. The left arrow makes the
  step back of the switch when the table has one, and else the step of the
  right arrow (`controllers.md`, 10.8).
- **A dim row** for a switch that cannot change now: the row goes 65 percent
  of the way back to the paper, and the word is "not now".

### 23.2 The place, the ray and the stick

The place, the ray and the trigger are those of the panel of places (22.2,
22.3): 1 m in front of the head, 12 degrees below eye level, 56 cm across,
fixed in the room. A row is 4.2 cm high and an arrow 3.3 cm across.

- The part under the ray is gold, with a light pulse at each new part. For a
  toggle the part is the whole row.
- A pull of the trigger presses the part under the ray. A press of a tab,
  Close, the thumb or the track gives the stronger pulse of the kit. A step
  of a switch gives the pulse of its test key only, and the control flashes.
- The thumb: a pull on it takes it, the ray drags it while the trigger is
  down, and the point of the thumb that the ray took stays under the ray.
  A pull on the track goes one view less one row up or down.
- The stick of the pointing hand: while the ray is on the window, a push
  forward or back past 25 percent scrolls the list, up to 1400 px a second
  at a full push (`StickScroll`). Forward goes up the list. The pump of
  `controllers.cpp` then sends no zoom and no wheel (`DebugWindowTakesStick`),
  and a push that the window took must come back before it zooms.
- B of the pointing hand and the menu button close the window. B then draws
  no gesture, and the menu button sends no Esc. Close, the ring, the spell
  book, a dialog box and a cut scene close it too.

### 23.3 The look and the bake

The look follows `hint_card_style`, as the panel of places does. With 1 it
is a page of parchment, with the words in Alegreya (a capital of 24 px for a
label) and the values in Alegreya SC. With 0 it is dark, in Segoe UI, and
the mod draws all of it each time that it changes.

**The bake.** The page with the rules is drawn one time. The title is drawn
on the card when the list changes, so each list has its own title. Each
tab and Close in each look is drawn the first time that it shows, and the
mod keeps a copy (`panel_kit::Tiles`), apart for each list. Each row of the group shows the paper
of the first row of the list, so a copy of a row can go to each place of the
list. A row is drawn into a picture of its own when its value or the look of
its control changes, and a new picture copies the rows that show. So a
scroll copies rows, and a change under the ray draws one or two rows. The
note is drawn one time for each switch and laid over the rows again after a
change. The scroll bar and the bottom row are drawn when they change.

Costs in a desktop run: the first picture 31 ms (the first draw of each
letter by GDI), a change under the ray 0.4 to 0.9 ms, and 1.8 to 1.9 ms
while a note shows (the note is laid over the rows again), a frame of a
scroll by the stick 0.6 to 1.8 ms, a new group 5 ms, the first draw of a
note 4 to 6 ms. The report of each window gives
the average, the most, and the most for a change under the ray only.

The window uses the quad layer of the ring (`kCardRing`), 1024 by 1024
pixels. The ring, the panel of places and the window never show at one
time. Memory: the page and the card (4 MB each), a picture of 692 by 76 px
for each row of the group (210 KB each, 24 rows in the group Render), and
the note.

### 23.4 Keys and the log

| Key | Default | Meaning |
| --- | --- | --- |
| `debug_window` | 1 | With `debug_menu=1`: 1 the item `debug` opens the window. 0 it goes to the debug pages of the ring |
| `hint_card_style` | 1 | 1: the parchment. 0: the dark window |
| `debug_window_test_save` | 0 | Test key. N saves the first N pictures of the window with a new group, a new value, a note or a scroll of 3 rows as `bwvr-debugwin-FFFFF.bmp` over a checkerboard |
| `debug_test_parts` | | Test key. The parts for the word `aimdebug` of the scripted controller: `tab3`, `row5`, `left5`, `right5` (from 0), `thumb`, `trackend` and `close` |

The log has the tag `debug window`:

```
debug window : the window opens, 0.56 by 0.56 m, its middle at 0.00 -0.21 -0.98 m, the group View with 14 switches, frame 371
debug window : the note of "Zoom point" shows (row 4), frame 543
debug window : the trigger presses the tab Render: 24 switches, frame 731
debug window : the trigger takes the thumb of the scroll bar at offset 933, frame 841
debug window : the trigger lets go of the thumb at offset 0 of 1064, frame 901
debug window : frame 641, a new picture of the parchment window took 0.92 ms: 2 parts drawn and 9 parts copied (a change under the ray)
debug window : 67 new pictures, 1.34 ms each on average and 5.69 ms at the most. 10 were a change under the ray only, 1.92 ms at the most
```

### 23.5 Tests and limits

The math checks are in `test/controller_math/debug_window_checks.h`
(`controllers.md`, 10.8). A desktop run opens the window from page 2 of the
ring and presses a toggle, both arrows of a switch of three values, the
arrows of "Throw speed", a tab, the stick, the thumb, the track and Close,
and closes the window with B and with the menu button.

The check in a headset is in `headset-checks.md`, check 23.

## 24. The settings window and the welcome page

The settings window is the debug window of section 23 with the settings of
the player as its rows. The item `settings` of page 2 of the ring opens it,
and so does the button Settings of the welcome page. The welcome page shows
one time, at the first start of the mod. The code is in
`settings_panel.cpp` and `settings_panel_math.h`. The window code in
`debug_window.cpp` takes its rows from a table of functions (`WindowRows`
in `debug_window.h`): one for the debug switches, one for the settings.

### 24.1 The rows

The place, the parts, the look, the bake, the stick and the closes are those
of section 23. The title is "Settings", and the bottom row says "Point at a
setting to read what it does". The words are the words of the launcher
(`launcher.md`), so a player knows them. The note of a row has the launcher
label, what the setting does, and its key with "A change acts at once" or
"A change acts at the next start of the game".

| Tab | Row | Key | Values |
| --- | --- | --- | --- |
| Moving | Turn in steps | `snap_turn` | Off, 30, 45, 90 degrees |
| Moving | Teleport | `teleport` | Off, On |
| Moving | Stick walk | `move_stick` | Off, Head, Controller |
| Moving | Walk speed | `move_stick_speed_cm` | Slow 75, Normal 150, Fast 250, Very fast 400 |
| Moving | Smooth zoom | `god_zoom_smooth` | In steps, Smooth |
| Moving | Comfort vignette | `comfort_vignette` | Off, Gentle, Strong |
| Moving | Blink | `blink` | Off, Short, Long |
| Moving | Recenter hold | `recenter_hold` | Off, Other hand, Hand |
| Hands | Hand | `left_hand` | Game, Right, Left |
| Hands | Grab the world | `grab_hand` | Off, Other grip, Either grip. At the next start |
| Hands | Gesture trail | `gesture_trail` | Off, On |
| Hands | Open hand stroke | `creature_stroke_open` | Off, On |
| Cards | Hints | `hint_learn` | Always, Until learned |
| Cards | Card look | `hint_card_style` | Plain, Scroll |
| Cards | Subtitle card | `subtitle_card` | Off, On. At the next start |
| Controls | The page of 24.6 | `button_map_hand`, `button_map_other` | A job for each button |

A row with two values is a toggle, with the "off" value first, so the knob
is at the left for it. A row with more values has two arrows, and a step
goes round the list. A value in `bwvr.ini` that the list does not have shows
as its number, and the right arrow goes to the first value of the list. A
row that acts at the next start has the small words "at the next start"
under its label, and the bottom row adds them after a change.

### 24.2 A change

A change writes the key into `bwvr.ini` at once, as the launcher writes it:
`WritePrivateProfileString` in the section `[bwvr]`, with the value as a
number. Then a row that acts at once calls the change function of its area,
which writes its own log line:

| Key | The change function |
| --- | --- |
| `snap_turn`, `blink` | `ComfortCameraSetSnap`, `ComfortCameraSetBlink` |
| `teleport` | `TeleportSetOn` |
| `move_stick`, `move_stick_speed_cm`, `god_zoom_smooth` | `TabletopGodSetMoveStick`, `TabletopGodSetMoveSpeed`, `TabletopGodSetZoomSmooth`. A walk or a hold of the zoom that runs stops |
| `comfort_vignette` | `VignetteSet` |
| `recenter_hold`, `left_hand`, `creature_stroke_open` | `ControllersSetRecenterHold`, `ControllersSetLeftHand`, `ControllersSetOpenStroke`. The Hand changes to the other controller when each button is up |
| `hint_learn`, `hint_card_style` | `HintCardSetLearn`, `HintCardSetStyle`. The cards, the ring, the spell book and the windows take the new look in the next frame |
| `gesture_trail` | `OverlaySetTrail` |

These functions do not need `test_keys=1`, and they give no pulse: the press
of the row gives the pulse of the kit. The window never writes the registry
or a file of the game. When Windows refuses the write, the change acts for
this game only, and the log says so.

### 24.3 The welcome page

The page shows when `welcome_page=1` and `bwvr.ini` has no
`welcome_done=1`. It waits until the controllers can act for 3 s, with no
dialog box, no cut scene, no ring, no spell book and no other panel. It is
a parchment page of 1024 by 640 pixels, 56 by 35 cm, at the place of the
settings window:

- the title "Welcome to Black & White VR"
- "Your hands are the Hand of your god, at their true size." and "The land
  is at the height of your waist."
- three controls: the grip of the Hand, the grip of the other hand, and the
  button of the ring with "Settings is on its page 2". The button is Y, X, B
  or A, as `button_layout`, the hand of the ring and the map of the buttons
  (24.6) give it
- the buttons Settings and Play

Settings closes the page and opens the settings window. Play closes the
page. A press of either button, B of the pointing hand and the menu button
write `welcome_done=1`. The ring, the spell book, a dialog box and a cut
scene close the page with no write, and it shows again when they are gone.
The words are drawn one time into the page, and each button in each look is
copied, so a change under the ray copies one button.

### 24.4 Keys and the log

| Key | Default | Meaning |
| --- | --- | --- |
| `settings_panel` | 1 | 1: page 2 of the ring as made has the item `settings` in the slot of `names` (`controllers.md`, 10.2). 0: page 2 has `names`, and the word `settings` is an empty slot |
| `welcome_page` | 1 | 0: the welcome page never shows |
| `welcome_done` | 0 | The mod writes 1 after the welcome page. The desktop runs set 1 (`tools/desk/desk_test.ps1`) |
| `welcome_test_save` | 0 | Test key. N saves the first N pictures of the welcome page as `bwvr-welcome-FFFFF.bmp`. The pictures of the settings window come from `debug_window_test_save` |
| `welcome_test_parts` | | Test key. The buttons for the word `aimdebug` while the page is open: `settings`, `play` |

The log has the tag `settings`, and the window lines of the settings window
have it too:

```
settings     : settings_panel=1: page 2 of the ring as made has the item "settings". welcome_page=1, welcome_done=0: the welcome page shows at this start
settings     : the welcome page opens, 0.56 by 0.35 m, its middle at 0.00 -0.21 -0.98 m, frame 412
settings     : the welcome page closes: the player pressed Settings. welcome_done=1 is in bwvr.ini, so the page does not show again, frame 530
settings     : the window opens, 0.56 by 0.56 m, its middle at 0.00 -0.21 -0.98 m, the group Moving with 8 switches, frame 531
settings     : snap_turn goes from 2 to 3 (90 degrees), written to bwvr.ini. It acts at once, frame 650
comfort      : the settings window: turn in steps of 90 degrees now
```

### 24.5 Tests and limits

The math checks are in `test/controller_math/settings_panel_checks.h`: the
table (each line in its group, a key of its own, the default among its
values, labels and value words that fit), the words and numbers of the
values, the step round a list, the text for `bwvr.ini`, the ring as made
with `settings_panel` 1 and 0, and the layout of the welcome page.

A setting that the area reads only at its start (`grab_hand`,
`subtitle_card`) acts at the next start. The other settings of the launcher
stay in the launcher.

### 24.6 The page Controls

The tab Controls of the settings window shows the controllers of the player
and gives a job of a controller to a different button of the same
controller. The code is in `controls_page.cpp`, `button_map_math.h` (the
kinds, the buttons, the map and the jobs) and `controller_art_math.h` (the
shapes of the drawing). `controllers.md`, 5.7, has the map.

**The rows**, from the top:

| Rows | What they are |
| --- | --- |
| The drawing | 4 rows, 692 by 304 pixels: the kind of the controllers at the top, the left and the right controller in ink, and a line of words for each control at the outer side: its name and its job, for example "B: Gesture". Under each controller: its side and "the Hand" or "other hand". The control of the row under the ray, and its line of words, are gold |
| The buttons | One row for each button of the left and then of the right controller that a job can use: the first face button, the second face button, the stick click, the menu button. The label is the side and the button ("Left Y"), and the value is its job. The arrows give the button the next or the last job of the same controller |
| Button defaults | A toggle. "In use" with the knob at the right when each button has its job as made. A press at "Changed" gives each button its job as made. A press at "In use" does nothing |
| The fixed rows | The trigger, the grip and the stick of the left and of the right controller, dim, with their jobs in the place of "not now". The note of each row says why it stays |

A controller that has a button that the map cannot move, or only one
button, shows that button as a dim row with its job: the pad click of a Vive
wand and the menu button of a simple controller. The rows follow the kind of
the controller at the press of the tab.

**A change.** The arrow of a button gives it the slot of the next button of
the list, and that button takes the old slot of the first one. So each job
keeps one button. The page writes `button_map_hand` or `button_map_other`
into `bwvr.ini` at once, and `controllers.cpp` uses the new map when each of
the four buttons of both controllers is up. The hint cards, the help words
and the welcome page take the new buttons in the next frame
(`controller-hints.md`, 8.3).

**The drawing** is our own art: circles, capsules, rounded boxes and rings
with a distance test, in the ink of the cards, with no logo and no outline of
a product. Each kind has its own shapes (`ArtOf`): Touch (a round face with
the stick, two face buttons and a menu button on the left), Index (a long
body with a stick, A, B and a pad with no job), Vive wand (a ring, a pad
and a menu button), Windows Mixed Reality (a ring, a stick, a pad with no
job and a menu button) and simple (select and menu). The left controller is
the mirror of the right one. A button with no job is pale. The trigger and
the grip are tabs at the edge of the body. The page draws the picture one
time for each look, and again when a job, a kind or the lit control
changes: 1.2 to 1.9 ms, and 13 to 22 ms for the first picture with the first
draw of each letter. Each row of the drawing is a band of that picture
(`WindowRows::picture`).

**The window** has two parts for the page: a row whose `value_count` is -1
is a band of a picture, with no control, no note and no press. A dim row can
have a word of its own (`WindowRows::fixed_word`), and then the bottom row
says "Right grip stays on its job".

| Key | Default | Meaning |
| --- | --- | --- |
| `controls_page` | 1 | 0: the settings window has no tab Controls. The keys of the map still act |
| `button_map_hand`, `button_map_other` | 1234 | The map of the buttons (`controllers.md`, 5.7). 1234 is the layout as made |
| `hint_profile` | | Test key (`controller-hints.md`, 8). The page draws the kind of this profile |

The log has the tag `controls`, and the map has lines of the tag
`controllers`:

```
controllers  : the right controller is a Quest Touch controller (profile "/interaction_profiles/oculus/touch_controller"). Its buttons for the jobs: A, B, Stick click, frame 2
controls     : the drawing of the Quest Touch and Quest Touch controllers took 1.88 ms (parchment), frame 731
controls     : Left Y goes from Ring to Recenter, and Left stick click takes Ring. button_map_other=1324 is in bwvr.ini. It acts when each button is up, frame 760
controllers  : the page Controls: button_map_other=1324: the page Controls moved jobs to other buttons, frame 760
controllers  : the new map of the buttons acts now: button_map_hand=1234, button_map_other=1324, frame 761
controls     : Button defaults: each button has its job as made. button_map_hand=1234 and button_map_other=1234 are in bwvr.ini, frame 902
```

The math checks are in `test/controller_math/button_map_checks.h`: the kind
of each profile, the buttons of each kind, the 24 maps and their numbers, a
map that does not fit a controller, the steps round the buttons, the jobs as
made and with `button_layout=0`, the words and the labels, the place of the
two controllers and of their words, the shapes of each kind in the frame and
on the body, and the lines of words that keep apart. The desktop run is
`village_run.ps1 -Kind controls` (`tools/desk/README.md`). A desktop run has
the fake controller, which is a Touch controller: `hint_profile` draws the
other kinds, but only a headset gives the profile of a real Index, wand or
Mixed Reality controller.
