# The headset app (Android)

This document covers the APK of the mod for a standalone headset: the Quest 3
and the other headsets of Meta, and other headsets with Android and OpenXR.
It covers the build, the install, the menu, the session, the tests and what
the app cannot do yet.

Summary:

- The APK is a native activity with no Java code (`bwvr/android`). It has
  `arm64-v8a` for the headsets and `x86_64` for the Android emulator, so the
  file that the tests install is the file that goes to a headset.
- In a headset the app starts an OpenXR session with Vulkan
  (`XR_KHR_vulkan_enable2`) and shows the menu of the mod as a quad of the
  compositor in front of the player. With no OpenXR runtime (the emulator)
  it shows the same menu in its window with a swapchain of Vulkan.
- The menu has all the settings of the launcher of the PC, in the form of
  the menu of the game: a main page with the commands and one entry for
  each group, and a page for each group. A save writes `bwvr.ini` by the
  rules of the launcher for the renderer Vulkan.
- The app finds the game folder on the headset and checks `runblack.exe`
  (the SHA-1 of the unofficial patch v1.42).
- The app has the Vulkan renderer of the mod (`vk_core.cpp`, `vk_scene.cpp`),
  the same source as the `ddraw.dll` of the PC. It draws the walks of a
  stream that the mod records on the PC (`vk_stream_record`). In the
  emulator its pictures of a walk of the game are the same as the pictures
  of the PC, to the pixel (see "The stream of the walks"). In a session the
  renderer draws on the Vulkan device of the session, and its eye pictures
  show on a stereo screen in front of the player (see "The picture of the
  renderer in the headset").
- **Only Vulkan.** The app draws with Vulkan alone: the menu, the stereo
  screen and the renderer of the mod. It has no fallback to Direct3D,
  DirectDraw or another graphics interface (see "Only Vulkan").
- **The app cannot start the game yet.** `runblack.exe` is a Windows
  program for x86, and the APK has no x86 runtime (see "What the app cannot
  do yet").

## Source files of this area

| File | Job |
| --- | --- |
| `android/AndroidManifest.xml` | The manifest: the native activity, the categories of an immersive app, the permissions and the queries of the OpenXR loader |
| `android/CMakeLists.txt` | `libbwvr_quest.so` with the toolchain of the NDK |
| `android/src/main.cpp` | `android_main`: the folder of the app, the event loop, the input of the controllers, of the keys and of a touch, the save |
| `android/src/xr_app.cpp` | The OpenXR session: the loader, the instance, Vulkan through the runtime, the actions, the two quads |
| `android/src/flat_view.cpp` | The menu in the window with no headset |
| `android/src/gpu.cpp` | The Vulkan device, the staging buffers and the copies |
| `android/src/canvas.cpp` | A picture of the CPU drawn with `android.graphics.Canvas` through JNI |
| `android/src/menu_model.h` | The menu: the pages, the rows, the values, the input, the save. No Android code |
| `android/src/menu_layout.h` | The layout of the menu picture, the quad, the hit of a controller ray. No Android code |
| `android/src/menu_paint.cpp` | The paint of the menu |
| `android/src/menu_painter.cpp` | The thread that paints the menu |
| `android/src/scene_host.cpp` | The functions of the mod that the renderer calls, with the state of the walk |
| `android/src/scene_player.cpp` | Gives each record of a stream of the walks to the renderer |
| `src/mod/scene_stream.h` | The format of the stream of the walks. No Windows code |
| `src/mod/vk_stream.cpp` | The record of the stream in the mod on the PC |
| `android/test_runtime/test_runtime.cpp` | The OpenXR test runtime for the emulator. Not in the APK |
| `src/mod/vk_platform_posix.h` | The parts of Windows that the renderer uses, for a build with no Windows |
| `src/mod/vk_d3d7_posix.h` | The parts of Direct3D 7 that the renderer uses, with the values of the SDK |
| `android/src/game_folder.cpp` | The game folder, the check of `runblack.exe`, the read and the write of a file |
| `src/common/settings_table.h` | The table of the settings, for the launcher and for the menu |
| `src/common/ini_text.h` | An ini file as text, with the rules of `GetPrivateProfileString` |
| `src/common/exe_sha1.h` | The SHA-1 of `runblack.exe` and the known builds, for the launcher, the mod and the app |
| `tools/android/build_apk.ps1` | The build of the APK |
| `tools/android/check_apk.ps1` | The checks of the APK with no device |
| `tools/android/emu_test.ps1` | The test in the Android emulator |
| `tools/android/emu_xr_test.ps1` | The test in the Android emulator with the OpenXR test runtime |
| `tools/android/emu_replay_test.ps1` | A stream of the game in the renderer of the app in the emulator, against the pictures of the PC |
| `tools/android/android_env.ps1` | Finds the SDK, the NDK and the JDK |

## Build the APK

The tools:

- The Android SDK with `platforms;android-34`, `build-tools;35.0.1`,
  `ndk;27.3.13750724`, `cmake;3.31.6` and `platform-tools`. For the test in
  the emulator also `emulator` and `system-images;android-34;google_apis;x86_64`.
- A JDK 17 or later (for example Microsoft OpenJDK 17). `apksigner` and
  `jar` need it.
- Git, for the build id.

The scripts find the SDK in `ANDROID_HOME`, in `ANDROID_SDK_ROOT`, in
`%LOCALAPPDATA%\Android\Sdk`, and in `%LOCALAPPDATA%\Packages\*\LocalCache\Local\Android\Sdk`.
A program from the Microsoft Store writes to `LOCALAPPDATA` in the folder
of its package, so an SDK that such a program installed is in that last
folder.

```
.\dev.ps1 apk
```

builds the APK and runs the checks of step 1 below. The steps alone:

```
.\bwvr\tools\android\build_apk.ps1          # bwvr\out\android\bwvr-quest.apk
.\bwvr\tools\android\check_apk.ps1          # step 1 of the tests
.\bwvr\tools\android\emu_test.ps1           # step 2 of the tests
```

`build_apk.ps1` builds `libbwvr_quest.so` for each ABI, links the manifest
with `aapt2` (`minSdkVersion` 29, `targetSdkVersion` 32), adds the libraries
with no compression (with the `jar` tool of the JDK, because the zip classes
of .NET Framework give a deflate entry for `NoCompression`), aligns the APK
for pages of 16 KB and signs it with the schemes v2 and v3.

The key is `local\android\bwvr-debug.keystore`. The script makes it at its
first run. It is not in the repo, and each person has a key of their own.
An APK with another key cannot install over an APK of this key: uninstall
the app first (`adb uninstall org.bwvr.quest`).

The version of the APK is the version of `src/common/version.h`. The
version code is major * 10000 + minor * 100 + patch.

The OpenXR loader is the loader of Khronos for Android, release 1.1.43
(`third_party/openxr-android`, see `third_party/README.md`). It needs no
`libc++_shared.so`, and the app links the C++ library statically.

## Install it on a headset

**Quest 3 (and Quest 2, Quest Pro, Quest 3S).** Turn on the developer mode
of the headset in the Meta Horizon app of the phone. Connect the headset to
the PC with a USB cable and allow the PC in the headset. Then:

```
adb install -r bwvr\out\android\bwvr-quest.apk
```

The app is in the library of the headset under "Unknown sources". SideQuest
can install the APK too.

**Steam Frame.** Valve says that the Steam Frame can install APKs. Not known
yet: if an APK there has an OpenXR runtime with Vulkan. If it has one, the
app starts its session there. If not, it shows the menu in a window.

## The game folder

The app does not contain the game. Start the app one time, then copy your
folder of Black & White (with the patches 1.10 and 1.20 and the unofficial
patch v1.42, as in `README.md`) to the folder `game` in the folder of the
app:

```
adb push "C:\Black & White\." /sdcard/Android/data/org.bwvr.quest/files/game/
```

The first start is necessary. Android gives the folder of the app to the
app when the app asks for it, and the app makes the folder `game` then. A
folder that `adb` makes before the first start belongs to the shell, and
the app cannot read or write in it. To repair that: uninstall the app,
delete `/sdcard/Android/data/org.bwvr.quest` with `adb shell rm -rf`,
install the app again, and start it one time. `bwvr.ini` is in the
game folder beside `runblack.exe`, as on the PC. At each start the app
reads `runblack.exe` and compares its SHA-1 with the value of
`bw_addresses.h` (`exe_sha1.h`). The status line of the menu says what it
found.

## The menu

The menu has the form of the Main Menu that the game shows over a loaded
game: a light panel that shows the world behind it, a thin light frame, tabs
at its top left ("Main Menu", and the group of a page), a line of white text
at the top, and dark brown buttons with a light frame and white text in the
middle. The font is the sans serif font of the system. The text has a dark
shadow, as the text of the game has.

- **The main page.** "Play in VR", "Play, new game", one entry for each
  group of the table ("Picture", "Performance", "Foveated rendering",
  "View", "Moving around", "Controllers", "Testing") and "Save Config".
- **A group page.** The rows of the group: the label at the left, the value
  at the right. A check box for a setting that is on or off, a slider with
  its value for a slider, the name of the item in a dark box for a list.
  "Back" is a button at the end.
- **The help.** Below the rows, the help text of the row with the highlight,
  the same text as the tooltip of the launcher. The help of a group names
  the settings on its page.
- **The status line.** At the bottom: the game folder, the runtime, "Saved."

The rows are the rows of `kSettings` in `settings_table.h` with the renderer
Vulkan. The rows that only the renderer Direct3D 9 uses (`d3d9_backend`,
`xr_vulkan`) are not shown, as in the launcher. The renderer is Vulkan, and
a step does not change it.

**A save** ("Save Config", and each start) writes each row, then the keys
with one right answer (`vr=1`, `stereo=1`, `head_camera=1`,
`backend=vulkan`, no `gpu_bridge`), and deletes the retired keys
(`kRetiredKeys`). The other lines of `bwvr.ini` stay. The ini of the app has
CRLF lines, as Windows writes it. The resolution of the game is in the
registry of the game, not in the ini: `Menu::RenderSize` gives `ScreenW` and
`ScreenH` for it.

At each start in a headset the app writes `headset_width` and
`headset_height` (what the headset asks for, for each eye) to `bwvr.ini`, as
the mod does on the PC. So the size of the render on the row "Resolution
scale" fits the headset.

**The controls in the headset.** Point at a row with a controller: the row
gets the highlight and the controller gives a short pulse. The trigger
selects: it turns a check box on or off, gives the next item of a list,
opens a group, or does a command. On a slider, press the trigger on the bar
and move the ray. The stick: up and down move the highlight, left and right
change the value. B or Y goes back. The hand that pressed its trigger last
has the ray. The cursor is a small glow on the menu where the ray hits it.

**The controls with no headset.** The arrow keys of a keyboard or a game pad
(or W, A, S, D) move the highlight and change the value. Enter, the centre
of the D-pad or A selects. Back, Esc or B goes back, and on the main page
Back closes the app. A touch selects the row under it, and a touch on the
bar of a slider sets its value.

## The session

- The Khronos loader for Android (`xrInitializeLoaderKHR` with the Java VM
  and the activity) finds the runtime through the broker of Khronos. Meta
  Horizon OS has one.
- The instance has `XR_KHR_vulkan_enable2` and
  `XR_KHR_android_create_instance`. The runtime makes the Vulkan instance
  (it asks for Vulkan 1.3) and the device (Vulkan 1.1 is enough for the
  menu). The log names the versions that the runtime was tested with
  (`minApiVersionSupported`, `maxApiVersionSupported`). The device decides
  if the renderer of the mod can draw.
- The reference space is `LOCAL`. The menu quad is 1.4 m wide, 1.4 m in
  front of the head and 0.15 m below it (`menu_layout.h`, `MenuQuad`). The
  cursor quad is 3 cm wide and 4 mm in front of the menu.
- The app copies the picture of the menu into a new image of the swapchain
  of a quad only when the picture changed (`xr_copy_on_change=1`, the
  default). In the other frames it takes no image, and the runtime shows
  the image that the app released last. `xr_copy_on_change=0` copies each
  layer in each frame. At its end the app logs the count of the copies and
  of the copies that it did not make. The images go to the runtime in the
  layout of a color attachment. The swapchain format is the first of
  R8G8B8A8_SRGB, B8G8R8A8_SRGB, R8G8B8A8_UNORM and B8G8R8A8_UNORM that the
  runtime has.
  The pixels of the canvas are sRGB with the color multiplied by the alpha,
  which is what a quad with `XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT`
  takes.
- The actions: an aim pose, select, back, the stick and a pulse for each
  hand, with bindings for `khr/simple_controller`,
  `oculus/touch_controller` and `valve/index_controller`.
- The app paints the menu again only when it changes (`Menu::Revision`). A
  thread of its own paints (`menu_painter.cpp`), so the frame loop does not
  wait: a paint takes 17 to 70 ms in the emulator, and more than 100 ms with
  the test runtime. The frames show the picture before until the new one is
  ready.

## With no headset

When `xrInitializeLoaderKHR`, the instance, the system or the session fails,
the log says why and the app shows the menu in its window: its own Vulkan
instance with `VK_KHR_android_surface`, a swapchain with FIFO, and one blit
of the menu picture at the largest size that fits. The app gives a frame
only after a change of the menu.

## The log

`bwvr-quest.log` in the folder of the app
(`/sdcard/Android/data/org.bwvr.quest/files`), and logcat with the tag
`bwvr`. The first line names the version and the build. Read it with

```
adb pull /sdcard/Android/data/org.bwvr.quest/files/bwvr-quest.log
adb logcat -s bwvr
```

**A test aid.** `menu_capture=1` in `bwvr.ini` writes each paint of the
menu to `bwvr-menu.bmp` in the game folder. A save keeps the key.

## Test the APK

1. **With no device** (`check_apk.ps1`, 31 checks): the signature, the
   manifest (the package, the SDK levels, the categories of an immersive app
   of Meta and of OpenXR, the permissions and the queries of the loader, the
   headsets of Meta, no Java code), the libraries of both ABIs (stored,
   on pages of 16 KB, LOAD segments of 16 KB, no `libc++_shared.so`), and
   that the APK has no other file than the license texts
   (`assets/licenses/`). So no file of the game can be in it.
2. **In the Android emulator** (`emu_test.ps1`, 19 checks): the same APK on
   an x86_64 image of Android 14 with no window, port 5584, a cold boot. The
   script deletes the folder of the app, installs the APK, starts it one
   time, puts a `bwvr.ini` and a stand in for `runblack.exe` (or with `-Exe`
   a real one) into the app folder, starts the app, drives the menu with key
   events, and checks the saved `bwvr.ini`, the log of this build, logcat
   (no JNI error, no crash) and three screenshots. The emulator has no OpenXR
   runtime, so this step tests the menu in the window.
3. **In the emulator with the OpenXR test runtime** (`emu_xr_test.ps1`, 29
   checks). The test runtime (`android/test_runtime`) is a headset with two
   Touch controllers that follow a script. The script puts it into an AVD
   with a system that it can write (`bwvr_xr_test`, port 5586) as
   `/system/etc/openxr/1/active_runtime.json`, where the Khronos loader looks
   when no runtime broker is there, and sets SELinux to permissive so the app
   can load it from `/data/local/tmp/bwvr-xr`. The right controller points at
   "Moving around", pulls the trigger, points at "Turn in steps", pulls the
   trigger, presses B, points at "Save Config" and pulls the trigger, then
   the runtime ends the session. The checks: the loader found the runtime,
   the instance, the Vulkan device of the runtime, the session, bindings with
   only paths of the Touch profile, the two swapchains, the state machine
   (begin at READY, end at STOPPING, EXITING), the place and the size of the
   menu quad, the cursor quad, a pulse at each new row, the saved
   `bwvr.ini`, no fault of the order of the calls (a swapchain image that is
   released with no wait, a layer of an acquired image), copies into the
   layers only when a picture changed (the count in the log), and four
   pictures of the quad that the runtime read back from the swapchain. The pictures
   are in `bwvr/out/android/emu-xr-test`. This step shows that the app makes
   the calls of OpenXR in the right order with the right structs. It does
   not show the runtime of Meta.
   With `-Stream <file> -Gpu host` (36 checks) the app also plays a stream
   of the walks on the device of the session, and the test checks the
   device (Vulkan 1.3 with the features of the renderer), the renderer on
   that device, the two quads of the stereo screen (one for each eye, 2.6
   m in front) and the pictures of both eyes that the runtime read back. Its
   results are in `bwvr/out/android/emu-xr-stream-test`.
4. **A stream of the game in the emulator** (`emu_replay_test.ps1`, 13
   checks). The stream and the pictures of the PC come from one desktop run
   (see "The stream of the walks"). The emulator has the GPU of the PC
   (`-gpu host`): the renderer needs Vulkan 1.3, and SwiftShader has 1.2. The
   script puts the stream and `stream_replay=1` into the game folder of the
   app, starts the app, and compares each picture of the renderer of the app
   with the picture of the same name of the PC. The pass is at most 2
   percent of the pixels with a difference of more than 24 in a channel.

   `emu_all.ps1 -Stream <file> -Reference <desk folder>` runs the tests 2,
   3 (plain and with `-Stream -Gpu host`) and 4 one after the other. It
   keeps the whole output of each test in `bwvr/out/android/emu-all`, and it
   stops at the first test that does not end with "N checks, 0 failed.",
   also when a test gives no such line.

   The emulator tests of all checkouts and worktrees use the same AVDs and
   ports, so each test takes a lock (`Lock-AndroidEmulator` in
   `android_env.ps1`). When a test of another process holds it, the test
   gives "NOT RUN" and the exit code 2. At its end each test waits until
   adb no longer lists its emulator.
5. **On the PC** (`.\dev.ps1 build`): the math test checks the ini text,
   the table, the menu (the pages, the steps, the help, the save rules), the
   hit of a ray on the quad (`test/controller_math/menu_checks.h`), each
   value and struct of `vk_d3d7_posix.h` against the Windows SDK
   (`vk_platform_checks.h`), and the writer and the reader of the stream
   (`stream_checks.h`).
6. **In a headset** (only a headset can show it): install the APK with
   `adb install -r`, start it from "Unknown sources", and read the log. Look
   for the lines `xr           : runtime`, `the headset asks for` and `the
   session is made`. Check that the menu is in front of you at a good
   height, that the ray and the cursor meet the rows, that the trigger, the
   stick, B and Y do what "The menu" says, and that the text is sharp.
   `menu_capture=1` gives the picture of the menu.

## The renderer in the app

The app compiles `vk_core.cpp` and `vk_scene.cpp` of the mod into
`libbwvr_quest.so` (`bwvr_renderer` in `android/CMakeLists.txt`). With no
`_WIN32` the two files include `vk_platform_posix.h` in place of
`windows.h`, `ddraw.h` and `d3d.h`:

- The calls of Windows that they make: the locks, the atomic counts, the
  clock, the secure string calls, `VirtualAlloc`, and the loader of Vulkan
  (`libvulkan.so` in place of `vulkan-1.dll`).
- The parts of Direct3D 7 that they use (`vk_d3d7_posix.h`): 55 enum values,
  20 constants and the structs `D3DVIEWPORT7`, `D3DRECT` and `DDSCAPS2`. The
  math test of the PC compares each one with the Windows SDK.
- `IDirectDrawSurface7` and `IDirect3DDevice7` are only the methods that the
  renderer calls. In the app a surface gives its pixels as RGBA8
  (`ReadRgba`), and the reads of the PC with `Lock` and SSE stay in the
  build of the PC.
- `scene_host.cpp` gives the functions of the other parts of the mod that the
  renderer calls (`StereoPassEye`, `IntroPassEye`, `OneWalkActive`,
  `OneWalkModeNow`, `OneWalkShiftPerRhw`, `OneWalkBoundTarget`,
  `GpuBridgeIsPanel`, `ModLog`, and the D3D7 thread, which the app does not
  have) with the state of the walk (`SceneHostState`).
- The renderer reads one value from the memory of the game: the angles of
  the camera (`bw::tech::InfoTransform`) for the foveation. In the app the
  stream gives them with the start of each walk (`VkPlatformTransform`).
- A picture of a test (`vk_scene_test_frame`, F12) goes into the folder that
  the app gives (`VkPlatformSetFolder`): the game folder. `fopen_s` of the
  app makes each backslash of a path a slash.

The code of the PC does not change: each part for the app is in `#ifndef
_WIN32`.

**Vulkan 1.3.** `vk_core.cpp` needs Vulkan 1.3 (dynamic rendering,
synchronization2, the extended dynamic state). SwiftShader of the emulator
has Vulkan 1.2. The emulator with `-gpu host` has the Vulkan 1.3 of the GPU
of the PC. Open: the version of Vulkan of the Quest 3 and of the Steam Frame
for an APK.

## The picture of the renderer in the headset

The session asks the runtime for a Vulkan 1.3 device with the features of
the renderer, as the session of the PC does (`xr_screen.cpp`): dynamic
rendering, synchronization2, multiview, the anisotropic filter, and a
shading rate attachment when the GPU has it. The log names the GPU and
says if the renderer can use it (`the renderer of the mod: yes`). With no
Vulkan 1.3 the session has Vulkan 1.1, and only the menu shows.

With `stream_replay=1` and a device of Vulkan 1.3, the app gives the device
of the session to the renderer (`vk::UseExternal`), as on the PC with
`xr_vulkan=1`, and plays the stream. Then the last eye picture of the
renderer (`VkSceneEyes`, an image with a layer for each eye) shows on a
**stereo screen**: two quads 2.6 m wide, 2.6 m in front of the head and
0.1 m above it, one for the left eye and one for the right eye, behind the
menu. Each layer goes with a blit to the size of the quads (half the size
of the picture) into an image of UNORM, and that image goes with a copy
into the swapchain of the quad, so the bytes of the game keep their values
also in a swapchain of sRGB. A quad gets a new image only when
`SetScreenSource` gave a new picture (`xr_copy_on_change`, as for the
menu and the cursor). When the renderer used the device of the session,
the app waits for the idle GPU at its end (`vk::Stop`) and keeps the device
to the end of the process, because the renderer has no stop for its images
and pipelines. So the renderer takes one device in a process: when the
glue code of Android starts `android_main` again in the same process, the
app logs it and plays no stream. This is the form of the "Stereoscopic
window" of the PC: the picture does not follow the head.

## The stream of the walks

The stream is a record of each call of the Vulkan renderer (`vk_scene.h`)
with `backend=vulkan`, in the format of `scene_stream.h`: the options of the
renderer, the frames, the start and the end of each walk with the state of
the device, the render states, the textures, the draws with their vertices
and indices, the targets, the viewports, the clears, and each surface of the
game with its pixels as RGBA8 before its first use and after the game writes
it. With each walk, draw and target the stream has the values of the other
parts of the mod that the renderer reads (`StreamState`).

**The record on the PC** (`vk_stream.cpp`, a test aid). `vk_stream_record=N`
in `bwvr.ini`: from the frame with the count N of the controllers (as
`vk_scene_test_frame`), the calls of `vk_stream_frames` frames (3 by
default) go to `bwvr-stream.bin` beside `runblack.exe`. Only with
`backend=vulkan`, where each call comes on the frame thread in the order of
the walk. A desktop run needs `fake_controller=1`: with no controller the
count of the frames does not move. The launcher removes both keys on each
save. A test key that changes a rule of the renderer during a record is not
in the stream. Each public call of `vk_scene.cpp` has one line
`BWVR_STREAM(...)` at its start, and each setter one line
`BWVR_STREAM_OPTION(...)`. In the build of the app these lines are empty.

```
desk_run.ps1 -Name stream -Dll <ddraw.dll> -Save pen -Seconds 60 -Extra @{
  one_walk = 1; fake_controller = 1; vk_stream_record = 3000;
  vk_stream_frames = 4; vk_scene_test_frame = 3002 }
```

A desktop run in the test save "pen" gives a stream of 4 frames of 16.8 MB:
1600 draws and 56 surfaces with 13.7 MB of pixels. **The file holds the
pixels of the textures of the game.** It stays on the PC of the person who
made it (`local\android-stream`), and never goes into the repo or into an
APK.

**The replay in the app** (`scene_player.cpp`, a test aid).
`stream_replay=1` in `bwvr.ini` of the game folder of the app: at its start
the app gives each record of `bwvr-stream.bin` of the game folder to its
renderer. A surface of the game is an object of the stream with its pixels
(`ReadRgba`), and the device at the start of a walk gives the states of the
stream. With `vk_scene_test_frame` in the options of the stream, the
renderer writes the pictures of that frame into the game folder.

**In a session** the same stream shows on the stereo screen ("The picture of
the renderer in the headset"). In the emulator with the test runtime and
`-gpu host`, the runtime reads back the pictures of both eyes from the
swapchains of the screen.

**What the replay shows.** The stream of the test save "pen" (frames 3000 to
3003, the Main Menu of the game over the island, `one_walk=1`,
`foveation=1`, `fovea_res=1`) in the emulator with `-gpu host`: the four
pictures of frame 3002 (the left eye, the right eye, and the outer and the
centre image of the foveated resolution) are the same as the pictures of the
PC, to the pixel. The four walks take 0.25 s with the first pipelines and
the uploads of the textures. The read back of the pictures takes about two
minutes in the emulator, because the GPU of the emulator gives an image of
40 MB to the CPU slowly.

## Only Vulkan

The app has one renderer: the Vulkan renderer of the mod. There is no
fallback to Direct3D, DirectDraw or another graphics interface, now or with
the x86 runtime.

- The menu, the cursor, the stereo screen and the menu in the window go to
  the screen with Vulkan. `android.graphics.Canvas` only paints the picture
  of the menu into memory of the CPU (the text and the shapes). It shows
  nothing itself.
- The renderer of the mod needs Vulkan 1.3 with its features ("The picture
  of the renderer in the headset"). With no Vulkan 1.3 the app shows the
  menu, and the game does not start: there is no other renderer.
- The menu writes `backend=vulkan` with each save, and the row "Renderer"
  has no other choice. "Play, no VR" of the launcher (dxwrapper) is not in
  the app.

**No fallback of the mod.** Each save of the menu writes `vulkan_strict=1`
(`backend.md`, "Only Vulkan"). With it the mod has none of the ways of the
PC to another graphics interface: the proxy loads no DirectDraw of the
system and no dxwrapper, whatever the key `backend` says, it gives the
enumeration of the displays itself, a `runblack.exe` that is not v1.42 gets
no DirectDraw, `vulkan_only` is 1, and the window of the game gets no GDI.
In a process of Wine "system DirectDraw" is the DirectDraw of Wine, which
draws with its own Direct3D, so these ways must not be there.

The runtime gets no `ddraw.dll`, `d3d*.dll`, `wined3d.dll` or `dxgi.dll` of
Wine, and Wine loads the `ddraw.dll` of the mod as a native DLL only, with
no builtin one behind it.

## What the app cannot do yet

**Start the game.** `runblack.exe` and the `ddraw.dll` of the mod are
Windows programs for x86 (32 bit), and the headsets have an ARM CPU with
Android. The plan, in steps:

1. **An x86 runtime in the app.** Box64 with Wine in the WoW64 mode runs a
   32 bit Windows program on ARM64 with no 32 bit libraries (the way of
   Winlator and similar apps). The app must start it in its own process,
   give it the game folder, and write `ScreenW`, `ScreenH`, `FullScreen`
   and `ScreenD` (`Menu::RenderSize`) into the registry of its Wine prefix.
   Wine also needs a display driver with no screen, and an audio driver.

   Box64 builds for the headsets with the NDK of the app:
   `bwvr/tools/android/build_box64.ps1 -Source <folder>` takes the folder
   of an unpacked source archive of Box64 (not in the repo, for example in
   `local/`), and writes `bwvr/out/android/box64/box64`. With the release
   v0.4.4 it is an AArch64 program for the linker of Android
   (`/system/bin/linker64`) of 21 MB that needs only `libc.so`, `libm.so`
   and `libdl.so`. The APK does not contain it yet, and it did not run yet:
   the emulator of the tests is x86_64, so a run needs a headset or another
   ARM64 device.

   The way for the 32 bit code is an ARM64 Wine for Android in the WoW64
   mode that loads `wowbox64.dll` (the option `WOW64` of Box64) as its CPU
   for x86. Then all of Wine runs as native ARM64 code, and only the 32 bit
   code (the game, the mod, the 32 bit DLLs of Wine) goes through the
   emulator. Wine needs no x86_64 libraries and no Linux program box64.
   `wowbox64.dll` is a DLL for Windows on ARM64, so its build needs a
   compiler for Windows (llvm-mingw): the NDK has no headers of Windows.
   The fall back is an x86_64 Wine in the WoW64 mode under the Linux
   program box64, with the x86_64 libraries of glibc that it needs.
2. **The walks for the renderer in the app, live.** The renderer is in the
   app, and it draws a stream of the walks of the game to the pixel ("The
   stream of the walks"). The stream goes through a file now, for a test.
   The plan: the same stream through shared memory between the process of
   Wine and the app, frame by frame, and the eye pictures of the renderer
   into the swapchains of the session of the app. So no draw goes through
   x86 code or through the Vulkan of Wine. The game already sends vertices
   in screen space, so the stream needs no matrices.
3. **The head and the controllers.** The mod reads the head pose and the
   controllers from OpenXR. Wine has no OpenXR for Android, so the app must
   give the mod its poses and its actions through the same shared memory.

Each step needs a check that a desktop run can make. Step 2 can be tested
on the PC: the game with `backend=remote` and the renderer of the app built
for Windows.

**The Steam Frame.** SteamOS on the Steam Frame has Proton and FEX (an x86
emulator). That way can run the PC build of the mod with no APK, if the
OpenXR of SteamOS takes a 32 bit program. Not tried.

**Open questions:** the frame rate of the menu quads on a Quest 3, the
readability of the text at 1600 by 1200 on a quad of 1.4 m, the bindings of
the controllers of the Steam Frame, and if the Steam Frame gives an APK an
OpenXR runtime.
