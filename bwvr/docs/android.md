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
- The stream can be live: the mod writes each frame of its walks into a
  ring file (`vk_stream_live`), and a thread of the app reads the frames
  from the same file while the game runs (`stream_live`, see "The live
  stream"). In a desktop run the frames of the ring have the same bytes as
  the record of the same run. In the emulator the app takes the frames from
  a second process through the file, and its pictures are the same as the
  pictures of the PC.
- The input goes the other way through a second file, the **input
  channel**: at each frame of its session the app writes the views of the
  eyes, the hands, the display time, the focus and the recenter
  (`input_live`), and the mod reads them in place of a session of its own
  (`xr_input`). The pulses of the hands go back to the app (see "The input
  channel").
- The mod gives the app the values of the projection layer of each frame in
  the frame of the live stream: the pose that each eye was drawn from, the
  angles of view that the game drew and the size of the pictures (a record
  `kView`). The app shows the eye pictures of its renderer in a projection
  layer with these values (`stream_projection=1`), as the mod does on the PC
  (see "The projection layer").
- The mod also gives the app the quad layers of its session for each frame:
  the hint cards, the subtitle card, the ring, the panel quads and the cut
  scene window (records `kCard`, `kCardPicture`, `kPanel` and `kWindow`). The
  app shows them over the projection layer in the order of the session of
  the mod (`stream_layers=1`, see "The quad layers").
- The mod gives the app the frames with no scene too: the logos, the
  videos, the front end and the dialog boxes go into the live stream as
  screen frames with a record `kScreen`, and the app shows them on a quad
  fixed in the room, as the screen of the PC (`vk_stream_screens=1`,
  `stream_screen=1`, see "The screens with no scene").
- With `vk_device=0` the mod makes no Vulkan instance and no device of its
  own, and its walks only go into the live stream: only the app draws them
  (see "No Vulkan device of the mod").
- **Only Vulkan.** The app draws with Vulkan alone: the menu, the stereo
  screen and the renderer of the mod. It has no fallback to Direct3D,
  DirectDraw or another graphics interface (see "Only Vulkan").
- **Wine.** The APK carries Wine 11.0 in the WoW64 mode for each ABI, with
  `wowbox64.dll` of Box64 and `libwow64fex.dll` of FEX as the CPUs of the 32
  bit x86 code on ARM64. At its
  start the app unpacks the tree of its ABI into its internal folder, makes
  a Wine prefix and links the drive `G:` to the game folder. In the emulator
  it runs a 32 bit test program in Wine (see "Wine in the app").
- **Play.** "Play in VR" and "Play, new game" write the keys of a run into
  `bwvr.ini`, put the `ddraw.dll` of the mod from the APK into the prefix,
  write the registry of the game into the prefix and start `runblack.exe`
  in Wine. The mod has no Vulkan device, gives its frames through the live
  stream and takes the head and the hands from the input channel. The menu
  button shows the menu over the game with "Quit game" (see "Play in the
  app").
- **The size of the eye pictures** has a cap: the panel of a Quest 3, 2064
  by 2208, with the aspect of the runtime (`eye_size_max=1`). So a runtime
  that asks for 4096 by 4291 does not make the render, the swapchains and
  the panel image 4096 by 4096 (see "The size of the eye pictures").
- **The sharpness of the middle.** One line of the log gives the pixels of
  each step from the render to the panel. With `fovea_res=1` and
  `fovea_res_layer=1` the centre image of foveated resolution is a
  projection layer of its own, so a centre of about 200 percent reaches the
  panel in the middle of the lens (see "The sharpness of the middle").
- **The time log.** While the game runs, the app and the mod log each 10 s
  where the time of a frame goes: the frames and the pictures, the CPU and
  the GPU, each thread of the game and of the app, and each core
  (`time_log=1`, see "The time log of the app"). Under Box64 no way gives
  the x86 address of the hot code of the game yet (see "The hot code under
  Box64").
- **A stop of the game.** When no frame of the game ends for 5 s, the mod
  writes the region and the mapped file of each address of `stall_addrs`
  and the 32 bit modules of the game process to its log (`stall_modules=1`,
  `backend.md`, "The module list at a stall"), so an address of a lock line
  of Wine can be named.
- **The variables of Box64.** The row "Strict memory order of the game
  (Box64, slower)" of the page "Testing" (`box64_strongmem`) and the key
  `box64_env` give variables of Box64 to the game alone (see "The variables
  of Box64").
- **FEX.** The row "Translator of the game (Box64 or FEX)" of the page
  "Testing" (`wow64_cpu`, `box64` by default) chooses the CPU DLL of the 32
  bit code on arm64-v8a: `wowbox64.dll` of Box64 or `libwow64fex.dll` of
  FEX-2610. The key `fex_env` gives variables of FEX to the game. On x86_64
  the row does nothing (see "FEX as the CPU of the 32 bit code").

## Source files of this area

| File | Job |
| --- | --- |
| `android/AndroidManifest.xml` | The manifest: the native activity, the categories of an immersive app, the permissions and the queries of the OpenXR loader |
| `android/CMakeLists.txt` | `libbwvr_quest.so` with the toolchain of the NDK |
| `android/src/main.cpp` | `android_main`: the folder of the app, the event loop, the input of the controllers, of the keys and of a touch, the save, the play of the live stream and the play thread of the frame loop |
| `android/src/xr_app.cpp` | The OpenXR session: the loader, the instance, Vulkan through the runtime, the actions, the two quads |
| `android/src/flat_view.cpp` | The menu in the window with no headset |
| `android/src/gpu.cpp` | The Vulkan device, the staging buffers, the copies, and the ring of the command buffers of the session (`frame_loop=1`) |
| `android/src/picture_ring.cpp` | The ring of the pictures of the game of the frame loop: the images of the slots, the copies of the play thread, the fences, the newest finished slot for the session |
| `src/mod/eye_copy_math.h`, `android/src/copy_time.cpp` | The copies of the eye pictures: the part of an atlas that the layers show, the size of the swapchains of a slot (`xr_eye_direct`), the even rate (`frame_loop_hold`), and the timestamps of each copy for the time log. No Android code in the header |
| `test/controller_math/eye_copy_checks.h` | The math checks of `eye_copy_math.h` |
| `android/src/frame_loop.h` | The rules of the frame loop: the slots of the ring of the pictures, the drop of a picture, the ring of the command buffers, the serials of the submits, the age of the shown picture, the queue at the GPU, the direct writes and the rect of a copy of the pixels of the CPU. No Android code |
| `test/controller_math/frame_loop_checks.h` | The math checks of `frame_loop.h` and of the row "The headset gets each frame (no wait for the GPU)" |
| `android/src/motion_comp.cpp` | The motion of the stick in the projection layer (`motion_comp`), and the parts of the age of a new picture for the time log |
| `src/mod/motion_comp_math.h` | The map of the room to the world of a frame, the difference of two maps, the pose of a view with the difference, the prediction of the walk. No Windows code |
| `src/mod/motion_world.cpp` | The place of the player in the world of each frame in the mod, into the `kView` of the frame and into the input channel |
| `test/controller_math/motion_comp_checks.h` | The math checks of `motion_comp_math.h`, of the slot of the place of the player in the input channel, of the place of a picture and of the parts of the age |
| `android/src/canvas.cpp` | A picture of the CPU drawn with `android.graphics.Canvas` through JNI |
| `android/src/menu_model.h` | The menu: the pages, the rows, the values, the input, the save. No Android code |
| `android/src/display_rate.h` | The display rate: the rates of the row from the rates of the runtime, the fall back of `display_hz`, the steps of the row. No Android code |
| `test/controller_math/display_rate_checks.h` | The math checks of `display_rate.h` and of the row "Display rate" |
| `android/src/menu_layout.h` | The layout of the menu picture, the quad, the hit of a controller ray. No Android code |
| `android/src/menu_paint.cpp` | The paint of the menu |
| `android/src/menu_painter.cpp` | The thread that paints the menu |
| `android/src/scene_host.cpp` | The functions of the mod that the renderer calls, with the state of the walk |
| `android/src/scene_player.cpp` | Gives each record of a stream of the walks to the renderer |
| `src/mod/scene_stream.h` | The format of the stream of the walks. No Windows code |
| `src/mod/vk_stream.cpp` | The record of the stream and the writer of the live stream in the mod on the PC |
| `src/mod/stream_ring.h` | The ring of the live stream: the head, the indices, the wrap, the choice of the writer for a full ring, the frames of the reader. No Windows code |
| `android/src/stream_reader.cpp` | The reader thread of the live stream in the app, and the feed process of a test |
| `test/stream_reader/main.cpp` | The test reader of the live stream on the PC (`bwvr_stream_reader.exe`) |
| `src/common/input_channel.h` | The input channel: the file, the slot of the newest input, the ring of the events, the ring of the keys, the record. No Windows code |
| `android/src/keyboard_layout.h` | The keyboard of the app: the layout of its picture, its quad under the screen quad, the hit of a controller ray. No Android code |
| `test/controller_math/screen_pointer_checks.h` | The math checks of the ray against a quad, of the pointer on the screen quad, of the keyboard and of the ring of the keys |
| `tools/desk/screen_ray_run.ps1` | A desktop run with a click of the ray on the start dialog of a new game on the screen quad |
| `src/mod/xr_input.cpp` | The reader of the input channel in the mod (`xr_input`), and the record of the input of a run (`xr_input_record`) |
| `src/mod/xr_input_math.h` | The values of the input channel as the values of `XrScreen`, the recenter, the lockstep of a test. No Windows code |
| `src/mod/frame_pace_math.h` | The pace of the frames of the game with no Vulkan device of the mod (`vk_pace`): the end of a wait at a new write of the slot or at its time limit. No Windows code |
| `src/mod/view_layer_math.h` | The projection layer of a frame for the app (the record `kView`): the values that the mod gives its session, the view of a picture in the player of the app, the record of a test. No Windows code |
| `src/mod/quad_layer_math.h` | The quad layers of a frame for the app (the records `kCard`, `kCardPicture`, `kPanel`, `kWindow`): the values of the calls of the session, the hash and the change of a picture of a card, the order and the quads of `XrScreen::BuildFrame`, the layers of a picture in the player of the app. No Windows code |
| `src/mod/xr_layers.cpp` | The quad layers of a frame with no session in the mod (`xr_input_layers`), and the record of the layers of a run (`xr_layer_record`) |
| `src/mod/screen_quad_math.h` | The screens with no scene (the record `kScreen`): the number of a screen frame, the route of a FinishFrame, the source of the picture, the quad of the screen of the PC, the screen in the player of the app. No Windows code |
| `src/mod/setup_box.cpp`, `src/mod/setup_box_math.h` | The line of the log that names the dialog box that is up, with the class from the RTTI of the game |
| `tools/desk/screens_run.ps1` | A desktop run of a new game with the screens with no scene through the live stream |
| `android/src/input_live.cpp` | The writer of the input channel in the app (`input_live`), and the play of the pulses of the mod |
| `test/input_writer/main.cpp` | The test writer of the input channel on the PC (`bwvr_input_writer.exe`) |
| `tools/desk/input_channel_run.ps1` | A desktop run with the input channel and the test writer of the PC |
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
| `tools/android/emu_live_test.ps1` | The live stream through the ring file in the emulator, against the pictures of the PC |
| `tools/desk/live_stream_run.ps1` | A desktop run with the live stream and the test reader of the PC |
| `tools/android/android_env.ps1` | Finds the SDK, the NDK and the JDK |
| `android/src/wine_runtime.cpp` | Wine in the app: the thread of the setup (the unpack of the tree, the prefix, the drive of the game folder), the start of a program in Wine, `wineserver -k` at the end |
| `android/src/wine_rules.h` | The rules of Wine in the app: the DLLs that stay out of the tree, the paths, the CPU DLLs and the choice of `wow64_cpu`, the environment, the build id, the seed of `user.reg`, the reader of the tar file. No Android code |
| `test/wine_probe/main.cpp` | The test program of Wine (`bwvr_wine_probe.exe`), a 32 bit console program with no game code |
| `tools/android/build_wine.sh` | The build of FreeType and Wine for each ABI, in a Linux of WSL2 |
| `tools/android/wine-patches/` | The patches of the source of Wine |
| `tools/android/pack_wine.sh` | The Wine tree of the APK for each ABI, with its checks |
| `tools/android/build_box64.ps1` | The build of Box64 and of `wowbox64.dll` |
| `tools/android/build_fex.ps1` | The build of `libwow64fex.dll` of FEX, with its license texts |
| `tools/android/emu_wine_test.ps1` | The test of Wine in the app in the Android emulator |
| `android/src/play_rules.h` | The rules of "Play": the keys of a run, the files of the mod, the registry of the game with the CPU DLL, the environment and the command line of the game, the variables of Box64 and of FEX for the game (`Box64Vars`, `FexVars`, `CpuVars`), the input of the mod while the menu shows over the game. No Android code |
| `test/controller_math/play_rules_checks.h` | The math checks of `play_rules.h` and of "Quit game" in the menu |
| `test/controller_math/stall_checks.h` | The math checks of the filter of `box64_env`, of the environment of the game with the variables of Box64, of the row of the memory order of Box64, and of `stall_math.h` of the mod |
| `test/controller_math/wow64_cpu_checks.h` | The math checks of the choice of the CPU DLL (`wow64_cpu`), of its value in the registry of the game, of the filter of `fex_env`, of the code cache of FEX (`fex_disk_cache`), and of the row "Translator of the game (Box64 or FEX)" |
| `android/src/thread_cores.cpp`, `android/src/core_rules.h` | The cores of the headset (`thread_cores`, `thread_bind`): each second the core and the CPU time of each thread, and the binding of threads to the cores. `core_rules.h` has the rules and no Android code |
| `test/controller_math/core_checks.h` | The math checks of `core_rules.h` |
| `tools/android/emu_game_test.ps1` | The game from the menu of the app in the Android emulator, with the test runtime |
| `android/src/eye_size.h` | The cap of the size of the eyes that the app takes from the runtime (`eye_size_max`), and the pixels of the panel in the middle of the lens for the line of the sharpness. No Android code |
| `test/controller_math/sharp_checks.h` | The math checks of the middle as its own layer (`fovea_res_math.h`: `PackAtlas`, `CenterFov`, `OuterRect`) and of the sharpness of the middle (`eye_size.h`) |
| `android/src/time_log.cpp`, `android/src/proc_stat.h` | The time log of the app (`time_log`): the times of a frame, of the GPU, of each thread and of each core, each 10 s. `proc_stat.h` reads the lines of `/proc` and has no Android code |
| `src/mod/time_log.cpp`, `src/mod/time_log_math.h` | The time log of the mod, and the stats and the rules that the app uses too. No Windows code in the header |
| `test/controller_math/time_log_checks.h` | The math checks of the cap, of the lines of `/proc` and of the parts of a frame |
| `src/mod/long_frame_math.h`, `android/src/long_frames.cpp` | The long frames of the game (`slow_frame_ms`): the rules of the lines, of the calls of the frame loop, of schedstat and of the late pictures, the clock of the frames of the game in the reader and the log of the takes of the session. No Windows code and no Android code in the header |
| `test/controller_math/long_frame_checks.h` | The math checks of `long_frame_math.h` |
| `android/src/walk_probe.cpp`, `android/src/walk_parts.h` | The GPU parts of the walk (`gpu_parts`): the functions of the app in the table of the renderer, the timestamps at the bounds of the passes, and the counts. `walk_parts.h` has the rules of the parts and no Vulkan code |
| `test/controller_math/gpu_parts_checks.h` | The math checks of `walk_parts.h` |

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
with `aapt2` (`minSdkVersion` 29, `targetSdkVersion` 28 with a Wine tree and
32 with none, see "Wine in the app"), adds the Wine tree of each ABI that
has one in `bwvr/out/android/wine` (compressed), adds the libraries
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

**Steam Frame.** Turn on the developer mode (Steam Settings, System), and
set the user password (Steam Settings, Developer, Set User Password). Then
the PC gets SSH to `steamos@frame`. Start the Lepton container "dev" over
SSH, connect `adb` to its port, and install the APK:

```
ssh steamos@frame "~/.local/share/Steam/steamapps/common/Lepton/lepton start"
adb connect frame:5555
adb -s frame:5555 install -r bwvr\out\android\bwvr-quest.apk
adb -s frame:5555 shell am start -n org.bwvr.quest/android.app.NativeActivity
```

`lepton ps` gives the port of `adb`. The container has no launcher, so
`am start` starts the app. Lepton gives Android 11 (arm64-v8a only) and the
OpenXR runtime of SteamVR (`/vendor/etc/openxr/1/active_runtime.json`, no
runtime broker): the app gets its session, 1728 by 1728 for each eye at
90 Hz. The game folder goes in with `push_game.ps1 -Serial frame:5555`.
Lepton shows each file with the owner 0 and traps `membarrier`, so Wine
needs `wine_owner_check=2` and `wine_sigsys=2`, the defaults (see "The owner
of the folders" and "A system call that the container traps"). Lepton also
has the
layer `VkLayer_VALVE_fdm_injection` of Valve, which the container "dev"
does not turn on.

## The game folder

The app does not contain the game. Start the app one time, then copy your
folder of Black & White (with the patches 1.10 and 1.20 and the unofficial
patch v1.42, as in `README.md`) to the folder `game` in the folder of the
app:

```
.\bwvr\tools\android\push_game.ps1 -Game "C:\Black & White"
```

`push_game.ps1` makes each folder with `mkdir -p` first and then pushes each
file. A push of the whole folder (`adb push <folder>`) does not work in the
folder of an app on a Quest 3: adb makes the first folder, and the make of
each folder in it fails ("remote secure_mkdirs failed: Operation not
permitted"). The ini and the logs of the mod of the PC and
`MemoryLeaks.txt` stay on the PC. The copy has no folder `Profiles` and no
Creature of a profile (each `*.erc` of `Scripts`), so the game starts with
the dialog of a new profile. To play on with the profile of the PC, copy it
with `push_profile.ps1` (see "The profile of the PC"): it also carries the
values of the profile in the registry of the PC. `-WithProfile` of
`push_game.ps1` copies only the folders, with no values. Start the app
again after the copy, so it checks `runblack.exe`.

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
  "View", "Moving around", "Controllers", "Testing"), "Save Config" and
  "Exit". "Exit" ends the app (and a game that runs, with no save).
- **A group page.** The rows of the group: the label at the left, the value
  at the right. A check box for a setting that is on or off, a slider with
  its value for a slider, the name of the item in a dark box for a list.
  "Back" is a button at the end. The page "Performance" also has the row
  "Display rate" of the app when the runtime gives display rates (see "The
  display rate"). The page "Testing" also has the rows "Translator of the
  game (Box64 or FEX)" (see "FEX as the CPU of the 32 bit code") and
  "Strict memory order of the game (Box64, slower)" (see "The variables of
  Box64") of the app.
- **The help.** Below the rows, the help text of the row with the highlight,
  the same text as the tooltip of the launcher. The help of a group names
  the settings on its page.
- **The status line.** At the bottom: the game folder, the runtime, "Saved.",
  the setup of Wine, and the game: "Starting the game (14:03:22)...", "The
  game runs (started 14:03:22).", "The game ended with the exit code N
  (14:10:05, Quit game)." or why "Play" did not start the game.
- **While the game runs** the main page has no "Play in VR" and no "Play,
  new game", and "Quit game" is above "Exit" at the bottom (see "Play in the
  app"). The rows that end something are at the bottom.

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
`headset_height` (what the headset asks for, for each eye, after the cap of
`eye_size_max`, see "The size of the eye pictures") to `bwvr.ini`, as the
mod does on the PC. So the size of the render on the row "Resolution scale"
fits the headset.

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

## Play in the app

"Play in VR" and "Play, new game" start the game in Wine with the mod of the
APK as its DirectDraw (`main.cpp`, `Play`, the rules in `play_rules.h`, the
thread in `wine_runtime.cpp`, `RunGame`).

**What "Play" needs.** `runblack.exe` of v1.42 in the game folder, an APK
with Wine for the ABI and `wine_setup=1`, a setup of Wine that ended with
"Wine is ready", an OpenXR session, and a device of the session with Vulkan
1.3 for the renderer. With one of them missing "Play" saves, starts
nothing, and the status line says why. `play_game=0` in `bwvr.ini`: "Play"
saves and starts nothing (no game in Wine).

**The steps of "Play":**

1. The save of the menu, as on the PC.
2. The keys of a run go into `bwvr.ini`, after the save, at each start: a
   save deletes the retired keys (`kRetiredKeys`), and `vk_stream_live` is
   one. Only the keys whose default is not the value of a run:
   `vk_stream_live=1`, `xr_input=1`, `vk_device=0`, `ui_panel=1`,
   `vk_pace=1` (the mod) and `stream_live=1`, `input_live=1` (the app). The save writes
   `vulkan_strict=1` and `backend=vulkan` itself. `stream_projection` and
   `stream_layers` have 1 as the default, and "Play" does not write them.
   `ui_panel=1`: with `vk_device=0` the help text needs the panel quads (see
   "No Vulkan device of the mod"). `vk_pace=1`: each frame of the game
   waits for a frame of the session (see "The pace of the frames").
3. The live stream and the input channel start in this process, when the
   start of the app did not start them. A session takes its actions one
   time, so with Wine in the APK (and `play_game=1`) the session has the
   actions of the mod from its start (`SetInputLive`).
4. The thread of the game puts `ddraw.dll` and `openxr_loader.dll` of the
   APK (`assets/mod`, the build of `.\dev.ps1 build`, with their notices in
   `assets/licenses/mod_notices.txt`) into
   `drive_c/windows/syswow64` of the prefix, the 32 bit system folder. The
   mod needs no other file: with `vulkan_strict=1` it loads no dxwrapper,
   and it loads `d3d11.dll` and `dxgi.dll` only at a first call, which the
   renderer Vulkan does not make. Not in the game folder: the DLL search of
   Windows looks first in the folder of the program, so a `ddraw.dll` there
   (the mod of a PC install) would come first. The registry makes both
   files known DLLs, and Wine maps a known DLL from the system folder
   whatever the folder of the program has (`create_known_dlls` of wineboot,
   `open_known_dll` of ntdll).
5. The registry of the game, at each start, so a change of the menu
   counts: `reg import` of `C:\bwvr\game.reg` (`GameRegistry`). Only what
   the game needs: `GameDir` = `G:\` (the game makes it its current folder
   after its start) under `HKCU\Software\Lionhead Studios Ltd\Black &
   White`, and under `...\BWSetup` `ScreenW` and `ScreenH`
   (`Menu::RenderSize`), `FullScreen` 0 and `ScreenD` 32, each a REG_DWORD as
   the install of the game writes it. Then the values `bwvr_ddraw` and
   `bwvr_openxr_loader` of `HKLM\System\CurrentControlSet\Control\Session
   Manager\KnownDLLs`. Then `HKCU\Software\Wine\Drivers`, `Audio` =
   `aaudio`, the audio driver of the tree (`wine_audio=0`: `reg import`
   deletes the value, and the game has no sound), so a prefix of an older
   tree gets it too. These are the values of the launcher of the PC. With
   `pc_profile=1` and a new `bwvr-profile.reg` in the game folder, the
   values of that profile go after them in the same import (see "The
   profile of the PC"). The app never reads the registry of the PC.
   Then `wineserver -w`: wineboot makes the sections of the known DLLs one
   time for each start of the wine server, from the registry of that time,
   and the server of `reg import` made them before the import. So the game
   starts a new server.
6. `wine G:\runblack.exe` (with `-NEWGAME` for "Play, new game"), with the
   game folder as the current folder and
   `WINEDLLOVERRIDES=mscoree,mshtml=;ddraw=n`: the DirectDraw of the game is
   a native DLL, and the tree has no builtin one.

**The notice** (`play_notice=1`, the default). From "Play" to the first
frame of the game the quad of the menu shows a notice in place of the rows:
the game starts in Wine, its picture comes with its first frame, and a hold
of the left menu button shows the menu with "Quit game". The headset shows
nothing of the game before its first frame, so with no notice the player
sees only black. `play_notice=0`: no quad until the first frame.

**While the game runs** the menu quad and the cursor do not go out, and the
app shows the frames of the live stream in the projection layer and the
quad layers (with `frame_loop=1`, the default, through the play thread and
the ring of the pictures, "The frame loop"). A hold of the menu button of the left controller for
`play_menu_hold_ms` (1000 by default) shows the menu over the game, and a
press of the menu button, or B or Y on the main page, hides it. With
`button_layout=1` the menu button is the one button of the menu of the game
(`controllers.md`, "Esc"), so the app holds the menu bit of the input
channel back while the button is down (`play_menu_press=1`, the default,
`StepMenuButton` of `play_rules.h`): a press shorter than the hold goes to
the mod at its release, down for two slots and then up, on the hand that
pressed. A hold and the press that hides the menu never reach the mod.
`play_menu_press=0`: the mod gets each press at once, and a hold opens the
menu of the game under the menu of the app too. While the menu shows over
the game, the mod gets no focus and no active hand through the input
channel, as with no input focus of the session (`MaskInput`). With no
headset the key Menu shows and hides the menu, and Back shows it.
`play_menu_hold_ms=0`: no menu of the app over the game (the menu button,
the key Menu and Back do not show it), each press of the menu button goes
to the mod at once, and the menu comes back only when the game ends.

**The two sticks** (`play_menu_sticks`, `StepStickChord` of
`play_rules.h`). The Steam Frame keeps the menu button of the left
controller for the system: a hold of it centres the view. So with
`play_menu_sticks=1` (the default) a hold of the clicks of both sticks for
`play_menu_hold_ms` also shows the menu over the game, on each headset. The
time starts when the second stick goes down. While the menu shows, the
next press of both sticks hides it. The clicks of the sticks still go to
the mod, so a stick click that the mod uses (`gesture_button=2`,
`radial_menu_button=2`) also acts. The log has "the clicks of both sticks
show the menu over the game". `play_menu_sticks=0`: only the menu button.
The process that the app starts is the loader of Wine. It becomes
`start.exe` of Wine, which runs `runblack.exe` as a process of its own (its
parent is 1, `emu_game_test.ps1` logs the tree) and gives its exit code
back. "Quit game" sends SIGKILL to `start.exe`, and then `wineserver -k`
ends `runblack.exe` and each other process of Wine. The game does not save.
When the game ends, the menu comes back with the exit code in the status line, and the last
picture of the game goes out of the layers. At the end of the app a game
that runs ends with it.

**The log.** Each step has a line `game` in `bwvr-quest.log`: the files of
the mod, the end of `reg import`, the command line, the first frame of the
game and its time after "Play", the frame rate of the game each 10 s (from
the frame numbers of the mod in the live stream), the menu over the game,
Quit game, and the exit code. With `time_log=1` the lines `time` each 10 s
say where the time of a frame goes ("The time log of the app"). The log of
the mod is `bwvr.log` in the game folder, as on the PC, and `wine.log` in
the folder of the app has the output of Wine.

**The test in the emulator** (`emu_game_test.ps1`). The AVD `bwvr_xr_test`
with the test runtime (a head that stands still), `-gpu host` and 4096 MB.
The test copies the game folder of `local/game-path.txt` into the game
folder of the app (1032 files, 548.8 MB): with no `Profiles`, no Creature of
a profile (`Scripts\CreatureMind\*.erc`), no `MemoryLeaks.txt`, no
`bwvr.ini` or log of the PC and no input channel or ring of a desk run
that runs at the same time (with a copy of them the open of
`bwvr-input.chan` in the app gives errno 13), and with the `ddraw.dll` of the mod of the PC
in it. It writes `resolution_pct=50` (a render of 1380 by 1104) and
`wine_debug=warn+virtual`. The runtime points at "Play, new game" when the
log says "Wine is ready". After the measure it holds the menu button for 90
frames, presses it for 6 frames, holds it again, points at "Quit game" and
pulls the trigger. Then "Play in VR": a press of 6 frames, a hold, "Quit
game". At its end the test deletes the app and the copy. After the measure
the runtime reads back the quads (the word `quads`): the picture of the
screen quad. `-SceneEyes`: the runtime reads back the eye pictures of the
projection layer too, for a game that comes to a frame of the scene. What
it shows (`-StartMinutes 15 -PlaySeconds 360`, 113 checks, 0 failed):

- "Play" wrote the keys of the run, and `bwvr.ini` at the start of the game
  has `vk_stream_live=1`, `xr_input=1`, `vk_device=0`, `ui_panel=1`,
  `vk_pace=1`, `stream_live=1`, `input_live=1`, `vulkan_strict=1` and
  `backend=vulkan`.
- `reg import` took 2.0 to 2.2 s, and `wineserver -w` 4.4 to 4.5 s, so the
  game starts about 6.5 s after "Play".
- The log of the mod has the build of the APK ("1.2.0 (build ...)"), not
  the build of the `ddraw.dll` of the game folder: the known DLL comes
  first. It has "vk_device=0. The mod makes no Vulkan instance and no
  device", `vulkan_strict=1`, "xr_input=1" with the channel of the app,
  and the first slot of the app.
- With `-NEWGAME` and with no word the same: the logo screens (479 and
  484 frames of the game), the video before the intro (2515 frames, about
  100 s), then the game waits in the dialog of a new profile at frame 0 of
  the controllers: the log of the mod has "setup box : the dialog box
  NewProfileBox is up now" (the SetupBox with the vtable `0x008D85F0`, kept
  by the `NewProfileBox` with the vtable `0x008DEC1C`, the callback
  `0x0053D210`, at FinishFrame 3068 and 3072). Each of these frames is a
  screen frame of the live stream (see "The screens with no scene"). The
  first frame of the game came 13.4 and 12.4 s after "Play", a screen
  frame of the logos (`game : the first frame of the game (frame
  1000000004, a screen with no scene)`). The frame rate of the app each
  10 s: 54 in the logos, 25 in the video (9 lines), then 68 in the
  dialog, with `vk_pace=1` ("The pace of the frames"). The app showed each
  picture on the screen quad: one quad for both eyes, 2.60 by 2.08 m at 0,
  0, -2 m, opaque, with the whole render of 1380 by 1104, no stereo screen
  and no projection layer. The
  quad that the runtime read back after 360 s has the dialog of a new
  profile over the plasma of the front end (`new-0.png`, `play-0.png`).
  The processes of Wine take 24 percent of one CPU in 361 s in each game
  (`runblack.exe` 19 percent, the app 14 and 15 percent).
- The tree of the processes: the process that the app started is
  `start.exe`, and `runblack.exe` has the parent 1.
- Game 1: the hold showed the menu over the game, the press of 6 frames
  hid it ("the menu goes"), and the second hold showed it again. The app
  gave no press of the menu button to the mod. Game 2: the app gave the
  press of 6 frames to the mod at its release ("a press of the menu button
  goes to the mod at its release"). The mod reads the buttons only in a
  frame of the scene (`ControllersFrame` of `stereo.cpp`), so in the dialog
  its log cannot show a press, and it has no line of Esc.
- The trigger on "Quit game" sent SIGKILL to `start.exe`, which ended with
  -1 after 440.6 s and 400.8 s. `wineserver -k` ended with 0, the menu came
  back with "The game ended with the exit code -1 (..., Quit game).", and
  no process of Wine stayed.

**The sound.** The audio driver of Wine is `wineaaudio` of patch 0003
(`audio.md`, "In Wine on Android"). The test checks for each game: the
audio driver in the registry of the game, an output stream of
`wineaaudio` in `wine.log`, no line "No driver ... could be initialized"
of `mmdevapi`, the start of QMixer and its stream in the log of the mod,
and no line "QMixer gave no block". In the emulator (`-Audio none`, each
game in the logos, the video and the dialog): QMixer starts ("22050 Hz, 22
channels, output 2"), uses DirectSound ("QMixer unlocks the DirectSound
buffer ...: 2 channels, 22050 Hz, 16 bit"), and the mod knows its stream
("the stream of QMixer is 22050 Hz, 16 bit stereo PCM"), so the mixer of
the mod can work. `dsound` of Wine gives one stream to `wineaaudio`: 48000
Hz, 32 bit float, the mode none, a burst of 960 frames, a buffer of 4360
frames (90.8 ms), the period of `mmdevapi` 2208 frames (46 ms). The
latency from the timestamp of AAudio after 0.5 s: 161.5 to 162.0 ms.
After 481984 frames (10 s) of output: 0 underruns of AAudio, and 3840
frames of silence from the callback. No later line of the log has more
underruns or more frames of silence. `user.reg` of the prefix after Play
has `"Audio"="aaudio"`.
With `-WineAudio 0` (`wine_audio=0`, no audio driver and no sound) the
log of the app has "wine_audio=0, wine_audio_low_latency=1. No audio
driver of Wine", the line of `reg import` says "no audio driver
(wine_audio=0)", `user.reg` of the prefix after Play has no `Audio` value,
`wine.log` has no stream of `wineaaudio` and the line "No driver from
L"pulse,alsa,oss,coreaudio" could be initialized" of `mmdevapi`, QMixer
starts, and the log of the mod has "QMixer gave no block to the wave
device in 15 s". Only a headset can show the sound itself.

**The null display driver.** With `Graphics` = `null` Wine runs
`explorer.exe` for the desktop. In the source of Wine 11.0
(`dlls/win32u/driver.c`) the null driver makes each window
(`nulldrv_CreateWindow` gives TRUE), gives it no surface of its own
(`nulldrv_CreateWindowSurface` gives FALSE), and has no events of its own
(`nulldrv_ProcessEvents` gives FALSE): no input of a mouse or a keyboard
comes through it. The messages of activation, focus and size come from
`win32u` itself, as with each driver. The game makes its window and draws
through the mod, which shows nothing in the window with `vk_device=0`. The
game does not pause: the logos and the video ran to their end, and the game
takes a whole CPU in the dialog. The messages that the window of the game
gets are not in a log yet.

**The noexec storage:** see "Wine in the app", the patch 0002.

**The dialog boxes by the controllers** (`emu_game_test.ps1 -NewProfile
-SceneEyes -SkipClick 547,575 -TestSave pen`): game 1 makes the profile of
the emulator with the keyboard of the app, the click on the start dialog
ends the Lionhead logo, and game 2 continues the test save "pen" in slot 20
of that profile. The facts are in "The dialog boxes by the controllers".

**Not shown yet:** each step in a headset (the screen quad, the keyboard
and the pace too).

**Known limits:**

- The mod reads the buttons of the input channel only in a frame of the
  scene, so before the first such frame a press of the menu button that
  the app gives to the mod does nothing.
- "Quit game" does not save the game.

## The profile of the PC

The game in the headset can play on with the profile of the player of the
PC: the name, the Creature, the saved games and the options of the game.
One command on the PC copies it to the headset:

```
.\bwvr\tools\android\push_profile.ps1 -Serial <adb serial of the headset>
```

**What a profile is** (the disassembly of `runblack.exe` and of
`LHMultiplayerR.dll` of v1.42). The game keeps a profile in two places:

- The registry, under `HKCU\Software\Lionhead Studios Ltd\Black &
  White\LHMultiplayer`. The value `Profile` is the name of the current
  player (REG_BINARY, UTF-16 with a zero at its end): the game reads it at
  its start (`LHNetGetCurrentProfileNameFromRegistry`), and with no such
  value or no key of that profile it shows the dialog of a new profile. The
  key `Profiles\<folder>` has the values of that player. `<folder>` is the
  name with a `_` before each character (`ProfileNameToKey`, `Kim` gives
  `_K_i_m`). `PlayerProfile` reads and writes `creature name`, `login
  name`, `login password`, `stuff`, `player symbol`, `timestarted`,
  `numberofsaves`, `numberofloads`, `pitchaboutcentre`, `pushscroll`,
  `IMMERSION_KEY` and `HAND_ORIENTATION` (`0x0066BB00` to `0x0066BCD0`,
  through `LHNetGetCurrentProfile*`). The options of the game (`pushscroll`,
  `AUTOSAVE` and others, `0x00514726` to `0x0051494C`) are values of that
  key too. The key bindings (`Key 0 Primary`
  to `Key 32 Mouse Button`), `CircleSlot(n)` and `LoveBuckets` (the
  alignment) are values of that key. The value `file` names the Creature.
- The files, from the current folder of the game, which is the folder of
  `GameDir` after the start (`0x0078E946`): `Profiles\<folder>` (`Saved
  Games`, `Pictures`, `Current`, `creature.lhp`), and the Creature
  `Scripts\CreatureMind\<file>` (`0x0066BF70`) with
  `Scripts\CreatureMind\Physique<file>`.

The values of `BWSetup` (`ScreenW`, `ScreenH`, `ScreenD`, `FullScreen`,
`MaxVRAM`, `detailidx`, `maxdetailidx`, `AudioSampleMasterVolume`,
`AudioMusicMasterVolume`) and `GameDir` are values of the PC, not of a
profile. The app writes its own values of `BWSetup` and `GameDir` ("Play in
the app", step 5), and the volumes stay at the default of the game.

**`push_profile.ps1`** reads the values of the current profile from the
registry of the PC, read only, and writes them into `bwvr-profile.reg`: the
value `Profile` and each value of the key of that profile (REG_DWORD,
REG_BINARY and REG_SZ) but `login password`, nothing else. `login password`
holds the password of the online login of the player, encrypted with a key
of the game that each copy of the game can undo: a secret does not go into
a file on the headset. The line after the header is `; copy <guid>`, new
for each run of the script, so each copy is a new file for the app. It
takes each file of `Profiles\<folder>` and the two Creature files from the
folders of the game:
when `GameDir` is in Program Files, from the VirtualStore of `GameDir`
first (a game with no administrator rights reads a file there before the
file of `GameDir`), then from `GameDir`. `-From` names other folders. It
makes each folder in the game folder of the app with `mkdir -p` and pushes
each file, as `push_game.ps1` does, then `bwvr-profile.reg` last. So the app
has no `bwvr-profile.reg` while the copy is not whole. The script never
writes to the registry of the PC, and it never changes, moves or deletes a
file on the PC. It does nothing with no `-Serial`.

- When the game folder of the app has the folder of that profile, the script
  stops: the game in the headset can have newer saves there. With
  `-Replace` it first moves that folder and the two Creature files of the
  app into a backup folder of the game folder of the app,
  `bwvr-profile-backup/<folder>-<date>-<time>` (with `-2`, `-3` and so on
  when that name is there), and then copies into an empty folder. So no
  slot mixes the files of two games. The script names the backup folder. To
  go back, move the folder from there to `Profiles` again.
- `-DryRun -Out <folder>` writes `bwvr-profile.reg` and `files.txt` (each
  file, its size, its time and the folder that it comes from) into that
  folder, and pushes nothing. The script stops when that folder is in a
  folder of the profile or in `GameDir`. It also lists the files of each
  slot of the saved games, so a slot with no whole save shows (a `.sav`, a
  `Script_<n>.sav` and `SaveGame.inf`). The script copies the files as they
  are: a slot that the game on the PC cannot load does not load in the
  headset either.
- `-WineUserReg <user.reg> -From <folder>` takes the values from a
  `user.reg` of a Wine prefix in place of the registry of the PC: the test
  of the emulator copies a profile that the game made in the emulator with
  it.

**The import** (`pc_profile=1`, the default, `play_rules.h`). At "Play" the
app reads `bwvr-profile.reg` of the game folder, keeps only the value
`Profile` of `LHMultiplayer` and the values of the key of that profile, and
adds them after the registry of the game in `C:\bwvr\game.reg`: one `reg
import`. It never imports `GameDir`, a value of `BWSetup`, another key,
`login password`, a delete or the default value of a key. The file must
have the form that the script writes: the first line `Windows Registry
Editor Version 5.00`, one
key or value on each line, only the printable characters of ASCII, the data
as `dword:`, `hex:`, `hex(N):` or a quoted text. The export of regedit
(UTF-16, long values on more lines) does not have this form. A file that is
not right gives no import, and the log and the help of "Play" say why. A
line that starts with `;` is a comment. The app makes one import for each
copy: after a good import it writes the stamp of the file (FNV-1a of its
bytes, and its size) into `C:\bwvr\pc-profile.done` of the prefix. So the
values that the game in the headset writes later (the saves, the
alignment, the options) stay, and only a new copy of `push_profile.ps1`
goes in again, also a copy with the same values: its line `; copy <guid>`
gives it a new stamp. `pc_profile=0`: no import, and the game
has the profile that it made in the headset, or none.

**The menu.** The help of "Play in VR" and "Play, new game" names the
profile of the game: "The profile of the game: Kim, from the PC. Play puts
it into the game." before the import, "..., from the PC." after it, the
profile in `user.reg` of the prefix with no file of the PC, or "No profile:
the game asks for a new one." The app reads it at its start and at the end
of each game. The log has a line `profile` for each step: the key, the help,
the import at "Play" with the count of the values, the folder of the profile
in the game folder, and the end of the import.

**The tests.**

- `.\dev.ps1 build`: the checks of `play_rules_checks.h`: a right file and
  what the filter keeps and drops (`login password` too), the import after
  the registry of the game with one `GameDir`, 21 files that are not right,
  the name in the value `Profile`, the stamp and the one import for each
  copy (two copies with the same values), the profile in `user.reg` of a
  prefix, and the help of "Play".
- `push_profile.ps1 -DryRun` against the registry and the profile of a PC:
  an export of the key of the game and a hash of each file of the profile
  folders before and after the run are the same.
- `emu_game_test.ps1 -NewProfile ... -TestSave pen -PullProfile <folder>`
  makes the profile `bwvrtest` in the emulator and pulls it (the folder of
  the profile with the test save in slot 20, and `user.reg` of the prefix).
  `emu_game_test.ps1 -ProfileFrom <folder>` copies it into a new game folder
  of the app with `push_profile.ps1 -WineUserReg`, then plays: game 1
  imports the values (no `login password`). After game 1 a copy with no
  `-Replace` stops, and a copy with `-Replace` moves the folder of the
  profile into the backup folder. Game 2 imports that new copy one time. No
  game shows the dialog of a new profile, and game 2 loads slot 20 of the
  profile. A profile of the emulator, never a profile of the player.

**Known limits:**

- A name of a profile with a character outside the printable characters of
  ASCII: the script stops, and the app takes no such file.
- The script takes the VirtualStore first when `GameDir` is in Program
  Files. A game that runs with administrator rights reads only `GameDir`:
  give `-From` with that folder.
- The game on the PC reads `20.sav` of slot 20 from the game folder at its
  start, and the other files of the slot from the folder of `GameDir`
  (`tools/desk/README.md`). The script takes slot 20 from the folder of
  `GameDir` and its VirtualStore only.

**Not shown yet:** the profile of a player in a headset.

## The session

- The Khronos loader for Android (`xrInitializeLoaderKHR` with the Java VM
  and the activity) finds the runtime through the broker of Khronos. Meta
  Horizon OS has one.
- The instance has `XR_KHR_vulkan_enable2` and
  `XR_KHR_android_create_instance`, and `XR_FB_display_refresh_rate` when the
  runtime lists it (see "The display rate"). The runtime makes the Vulkan instance
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

1. **With no device** (`check_apk.ps1`, 79 checks with both Wine trees):
   the signature, the
   manifest (the package, the SDK levels, the categories of an immersive app
   of Meta and of OpenXR, the permissions and the queries of the loader, the
   headsets of Meta, no Java code), the libraries of both ABIs (stored,
   on pages of 16 KB, LOAD segments of 16 KB, no `libc++_shared.so`), and
   that the APK has no other file than the license texts
   (`assets/licenses/`), the Wine trees (`assets/wine/`) and the files of
   the mod (`assets/mod/ddraw.dll` and `openxr_loader.dll`). So no file of
   the game can be in it. The files of the mod have the SHA-256 of the
   files of `bwvr/build/src/mod/Release` (`.\dev.ps1 build`), and each is a
   PE file for 32 bit x86. The OpenXR loader has the pinned SHA-256 of
   `package.ps1`, and the files of the mod never go without their notices
   (`assets/licenses/mod_notices.txt`, the `Notices.txt` of the installer of
   the PC). An APK with no Wine has none of them. For each Wine tree: a
   build id, the compression,
   the loader, `wineserver`, `ntdll` of i386 and of the ABI, the CPU DLL of
   the ABI in the tree and in `wine.inf`, `libfreetype.so`, `tahoma.ttf`,
   the audio driver (`wineaaudio.drv` of i386 and of the ABI, and
   `wineaaudio.so`), none of the DLLs that stay out, and the license texts of Wine, FreeType
   and Box64. The tree of arm64-v8a has both CPU DLLs (`wowbox64.dll` and
   `libwow64fex.dll`), each a PE file for ARM64 with the name
   `BTCpuProcessInit`, and the license texts of FEX (`fex*.txt`) are in the
   APK. With a tree `targetSdkVersion` must be 28, with none 32.
2. **In the Android emulator** (`emu_test.ps1`, 19 checks): the same APK on
   an x86_64 image of Android 14 with no window, port 5584, a cold boot. The
   script deletes the folder of the app, installs the APK, starts it one
   time, puts a `bwvr.ini` and a stand in for `runblack.exe` (or with `-Exe`
   a real one) into the app folder, starts the app, drives the menu with key
   events, and checks the saved `bwvr.ini`, the log of this build, logcat
   (no JNI error, no crash) and three screenshots. The emulator has no OpenXR
   runtime, so this step tests the menu in the window.
3. **In the emulator with the OpenXR test runtime** (`emu_xr_test.ps1`, 47
   checks, with the checks of "The display rate"; `-DisplayRate` and
   `-NoRateExtension` are in that section). The test runtime (`android/test_runtime`) is a headset with two
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
   pictures of the quad that the runtime read back from the swapchain. The
   run has `input_live=1`: the runtime recenters its LOCAL space one time
   (the script word `recenter`), and the app puts one pulse into the ring of
   the events itself (`input_live_test_pulse`). The test reads
   `bwvr-input.chan` back with `bwvr_input_writer.exe dump` and checks the
   views of the runtime (`xrLocateViews`, each eye 32 mm to its side, the
   four angles), the display period of 13888888 ns, one recenter, the size
   of the eyes, a last slot with no focus and no active hand, the aim poses
   of the right hand of the script in the slots of the log, the profile of
   the Touch controller, and the pulse of 80 ms at 0.50 at the runtime.
   The runtime also has an eye tracker (the script word `gaze`), and the
   run has `foveation=1`: the test checks that the app asks for
   `XR_EXT_eye_gaze_interaction`, binds the gaze, finds the eye tracker,
   and that the slots have the gaze of the script. The pictures
   are in `bwvr/out/android/emu-xr-test`. This step shows that the app makes
   the calls of OpenXR in the right order with the right structs. It does
   not show the runtime of Meta.
   With `-Stream <file> -Gpu host` (55 checks) the app also plays a stream
   of the walks on the device of the session, and the test checks the
   device (Vulkan 1.3 with the features of the renderer), the renderer on
   that device, the two quads of the stereo screen (one for each eye, 2.6
   m in front), the pictures of both eyes that the runtime read back, and
   that the log says one time that a picture with no values of the
   projection layer of the mod shows on the stereo screen. Its
   results are in `bwvr/out/android/emu-xr-stream-test`.
   With `-Live <desk folder> -Gpu host` (59 checks, 60 with `-FeedMs 0`)
   the app plays the live stream of `ring-capture.bin` of a desktop run of
   `live_stream_run.ps1` on the device of the session: a child process of
   the app writes one frame into the ring each `-FeedMs` ms (14 by
   default). The script of the controllers starts 200 frames later, after
   the first picture. The test checks the device, the stereo screen and the
   pictures of both eyes as with `-Stream`, the feed process, the answer to
   the ask, the first frame with the whole state, the frames that the app
   played and skipped, that each eye of the stereo screen got a copy for
   each new picture of the live stream, and the four pictures of the test
   frame of the stream against the pictures of the PC (the pass of step 4).
   With `-FeedMs 0` the feed is faster than the session, and the test also
   checks that the app skipped the walks of frames. Its results are in
   `bwvr/out/android/emu-xr-live-test`.
   When the feed has records `kView` for a projection layer (a desk run of
   `input_channel_run.ps1 -Kind channel -Live`, see "The projection
   layer"), the app shows the pictures in a projection layer, and the test
   checks it in place of the stereo screen (with the quad layers of the
   desk runs of "The quad layers": 73 checks for the hint card, 68 with
   `-NoLayers`, 68 for a fade of the card with `-QuadFrames`, 64 for a feed
   of the cut scene window alone): the two swapchains, no quad of the stereo
   screen, that the pose and
   the angles of view of each eye of each projection layer that the runtime
   reports are the values of a record `kView` of the feed, that the layers
   have the values of the `kView` of the frame of each picture in the order
   of the pictures (the app logs the frame of each new picture), that the
   layers have two poses or more (not in a feed with frames of the cut
   scene window), that the rect of each eye is its whole
   picture, the pictures of both eyes that the runtime read back, a copy for
   each new picture, and a projection layer in each frame with a picture.
   The feed needs another pose in its last frames than in its first frames,
   because the app plays the first frame and the newest frame.
   When the feed has the quad layers of the mod (`bwvr_stream_reader
   layers`, see "The quad layers"), the test also checks that each line of
   the quads of the runtime, with no menu and no cursor, is the quads of the
   layers of the frame of a picture of the app (the pose, the size, the rect,
   the eye and the flags, in the order of the session of the mod), that they
   come in the order of the pictures, that the log names each card of the
   feed as a quad layer, and that the rect of each card in the pictures that
   the runtime read back has the bytes of the picture of that card in the
   feed. The menu quad is the quad of 1600 by 1200 pixels over the layers of
   the mod. A feed with frames of the cut scene window: the test also checks
   that the log names the window and each picture in it, that each picture
   in the window is a frame of the window in the feed, and the four walls.
   When the newest frame of the feed is a frame of the window, the runtime
   reads back no eye of a projection layer. `-WindowFrom N -WindowTo M` makes
   a test feed from the capture (`bwvr_stream_reader window-feed`): the
   frames N to M become frames of the window as a screen, with values of the
   test (2.6 m wide, 2 m in front, a crop of 120 and an edge of 2 columns),
   and the test checks the two quads of the eyes with those columns. The
   app plays only some frames of a feed at a pace of 14 ms, so the range must
   cover about half of the frames of the feed: else no frame that the app
   shows can be a frame of the window.
   `-NoLayers` gives the app `stream_layers=0`, and the test checks that
   each line of the quads of the runtime has the menu and the cursor alone:
   no quad of the mod, and no quad of the stereo screen over the projection
   layer. `-NoProjection` gives the app `stream_projection=0`, and the test
   checks that the app had no frame with the projection layer and no frame
   with the cut scene window, the stereo screen in each frame with a
   picture, and that each line of the quads of the runtime has the stereo
   screen, the menu and the cursor, and no quad of the mod. `-QuadFrames` (a
   feed over a fade of a card) reads back the quads of the mod in each of the
   first 60 frames of the runtime (the word `quads`) and checks that they
   have two different pictures of a card or more, each with the bytes of a
   new picture that the app logs.
   A feed of the screens with no scene (a desk run of `screens_run.ps1`, 60
   checks, see "The screens with no scene"): the test checks the screen quad
   in place of the stereo screen and the projection layer. `-NoScreen` (56
   checks) gives the app `stream_screen=0`, and the test checks that each
   line of the quads of the runtime has the menu and the cursor alone.
4. **A stream of the game in the emulator** (`emu_replay_test.ps1`, 13
   checks). The stream and the pictures of the PC come from one desktop run
   (see "The stream of the walks"). The emulator has the GPU of the PC
   (`-gpu host`): the renderer needs Vulkan 1.3, and SwiftShader has 1.2. The
   script puts the stream and `stream_replay=1` into the game folder of the
   app, starts the app, and compares each picture of the renderer of the app
   with the picture of the same name of the PC. The pass is at most 2
   percent of the pixels with a difference of more than 24 in a channel.
5. **The live stream in the emulator** (`emu_live_test.ps1 -Run <folder>`,
   23 checks). The feed (`ring-capture.bin`) and the pictures of the PC come
   from one desktop run of `live_stream_run.ps1` (see "The live stream").
   The emulator has the GPU of the PC (`-gpu host`). The script puts the
   feed, `stream_live=1` and `stream_live_feed=1` into the game folder of
   the app and starts the app. A child process of the app makes
   `bwvr-stream.ring` in the game folder and writes the frames of the feed
   into it as the mod does, and the reader thread of the app takes them
   from the same file. The checks: the ring file and its head, the answer
   to the ask, the first frame with the whole state, each frame of the feed
   played, no record of an unknown kind, the end of the child process with
   the status 0 (the reader took each byte), the queue of the app under its
   limit with no session, no lost place, Vulkan 1.3, no
   crash, and each picture against the picture of the PC (the pass of step
   4). The results are in `bwvr/out/android/emu-live-test`.

   **Wine in the emulator** (`emu_wine_test.ps1`): see "Wine in the app".
   The results are in `bwvr/out/android/emu-wine-test`.

   **The game from the menu in the emulator** (`emu_game_test.ps1`, it
   needs the game): see "Play in the app". The results are in
   `bwvr/out/android/emu-game-test`, with pictures of the game, so they stay
   on the PC. `emu_all.ps1` does not run it.

   `emu_all.ps1 -Stream <file> -Reference <desk folder> -Live <desk folder>`
   runs the tests 2, Wine, 3 (plain and with `-Stream -Gpu host`), 4 and 5
   one after the other. It
   keeps the whole output of each test in `bwvr/out/android/emu-all`, and it
   stops at the first test that does not end with "N checks, 0 failed.",
   also when a test gives no such line.

   The emulator tests of all checkouts and worktrees use the same AVDs and
   ports, so each test takes a lock (`Lock-AndroidEmulator` in
   `android_env.ps1`). When a test of another process holds it, the test
   gives "NOT RUN" and the exit code 2. At its end each test waits until
   adb no longer lists its emulator. A fault of a test script (an
   exception) is a failed check with the text of the fault, so a test that
   stops early does not end with "0 failed".
6. **On the PC** (`.\dev.ps1 build`): the math test checks the ini text,
   the table, the menu (the pages, the steps, the help, the save rules), the
   hit of a ray on the quad (`test/controller_math/menu_checks.h`), the
   display rate (`display_rate_checks.h`: the rates of the row from the
   rates of the runtime, the fall back of `display_hz`, the row, its steps
   and its save), each
   value and struct of `vk_d3d7_posix.h` against the Windows SDK
   (`vk_platform_checks.h`), the writer and the reader of the stream
   (`stream_checks.h`), and the ring of the live stream
   (`stream_ring_checks.h`: the wrap at the end of the data and at 2^32, a
   full ring, the choice of the writer, the frames of the reader, a reader
   that comes late, and 3000 frames through a ring of 4 KB in two threads),
   and the input channel (`input_channel_checks.h`: the layout of the file,
   a torn read of the slot, the wrap of the ring of the events at its end and
   at 2^32, a full ring, the values of `XrScreen` through the channel, the
   recenter, the lockstep, 200000 slots and 100000 events in two threads),
   and the projection layer (`view_layer_checks.h`: the layout of the record
   `kView`, the angles of view of `LayerFov`, the values of `MakeView`, when
   the app shows a projection layer, `kView` in a frame of the ring and in
   the records of state, and the order in the player: the view goes with the
   picture at the walk that ends it, and a frame with no walks gives its
   view to no picture), and the quad layers (`quad_layer_checks.h`: the
   layout of the records, the hash of a picture, a picture only when the
   reader does not have it, the order of the records of a frame, the values
   of the calls, the layers of a picture in the player, the state of a frame
   with no walks, the rules of `SendCard`, of a panel quad, of the walls and
   of the columns of the cut scene window, and the order of the layers of
   `BuildFrame`), and the screens with no scene (`screen_quad_checks.h`),
   and the time log (`time_log_checks.h`: the cap of the size of the eyes
   and the render from it, the lines of `/proc/<pid>/task/<tid>/stat` and
   of `/proc/stat`, the first 8 threads, the parts of a frame of the mod and
   the cause of a frame that did not show),
   and the rules of Wine (`wine_rules_checks.h`: the DLLs
   that stay out, the paths of each ABI, the build id, the environment of a
   program, the seed of `user.reg`, the marker of the prefix and the copy
   of the CPU DLL, the headers of the tar file and the names that stay in
   the tree), and the variables of Box64 and the stall (`stall_checks.h`:
   the filter of `box64_env`, the environment of the game, the row of the
   memory order of Box64, the parse of `stall_addrs`, the watch of a stall
   and the time of the test spin).
7. **In a headset** (only a headset can show it): install the APK with
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
and pipelines. So the renderer takes one device in a process. A headset
keeps the process of an app when the app goes to the back or the headset
sleeps, and the next start of the app is then a new `android_main` in the
same process. So the process ends with `android_main` (`exit_at_end=1`, the
default), and each start of the app is a new process. `exit_at_end=0`: the
process stays, and a new `android_main` in the same process logs that the
renderer ran in it already and plays no stream (and "Play" does not start
the game). This is the form of the "Stereoscopic
window" of the PC: the picture does not follow the head.

With the values of the projection layer of the mod for the frame of the
picture (`stream_projection=1`, see "The projection layer"), the picture
shows in a projection layer in place of the stereo screen.

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

## The live stream

The live stream gives each frame of the walks of the game to the app while
the game runs, through a real file that both map: the **ring**
(`stream_ring.h`). On the PC and in Wine the mod maps the file with
`CreateFileMapping`, and the app maps it with `mmap` and `MAP_SHARED`. A
named section of Wine is not visible to the app, so it must be a file.

**The ring.** `bwvr-stream.ring` beside `runblack.exe`: a head of 4 KB and
32 MB of data. The data is a pipe of bytes: the records of the stream
(`scene_stream.h`) with no head of a stream, one frame after the other. A
frame is the records from one `kFrame` of the game to its `kFrameEnd`. The
head has the indices `write` (of the writer) and `read` (of the reader).
They count the bytes since the start and wrap at 2^32. Each side stores its
index with a release after its copy, and loads the other index with an
acquire before its copy: `InterlockedExchange` and
`InterlockedCompareExchange` in the mod, the `__atomic` builtins in the app.
In the headset the mod runs in Box64 on ARM, which has a weak memory model,
and Box64 gives the interlocked instructions their barriers.

**The writer** (`vk_stream_live=1`, `vk_stream.cpp`, only with
`backend=vulkan`). At its start the mod makes the file and a new head. The
records of a frame collect in the memory of the mod. At the end of the walk
of the frame with both eyes (`vk_stream_live_end=1`, the default) they go
into the ring, so the reader can have the frame in the game frame of its walks. The
frame ends at the first walk that has both eyes: the one walk of
`one_walk=1`, the walk of the right eye, or a walk with no eye pass
(`WalkEndsFrame`, `stream_split.h`), the walk that `vk_scene.cpp` takes for
the picture of a test. With the quad layers (`xr_input=1`,
`xr_input_layers=1`) the layers at the end of the frame end it (see "The
quad layers"). A record after that walk and before the next frame,
also a later walk of the same frame, goes at the start of the next frame
(the tail), in its order. The walks of "pen" have no record after that
walk.
`vk_stream_live_end=0`: a frame goes at the start of the next frame of the
game. With `vk_stream_worker=1` (the default) a thread of the stream makes
the copy into the ring and its waits for room. The frame thread waits for
that thread only at the start of the next frame of the game and before it
gives the next frame (`backend.md`, "The way of a draw to the headset
app"). A record (`vk_stream_record`) and the live stream get the same bytes
for a frame, and the first frame of a record has the whole state for both.
With no reader the mod puts no frame into the ring and reads no surface for
it.

**A reader that comes late** adds one to `ask` in the head. At the start of
its next frame the writer sets `sync` to its write index, then `answer` to
that ask, and that frame starts with `kStart` (the options) and has each
surface with its pixels before its first use. The reader sets `read` to
`sync` and reads from there. A reader that leaves adds one to `bye`, and
the writer stops until the next ask. The same holds for the first frame
after a frame with no stream.

**A full ring** (a slow reader). A frame waits up to
`vk_stream_live_drop_us` microseconds (0 by default: no wait) for room for
the whole frame. Then the writer drops its walks (`vk_stream_live_drop=1`,
the default): the frame keeps its records of state, `kStart`, `kFrame`,
`kGaze`, `kOnlyFrame`, `kTarget`, `kPanelBind`, `kSurface`, `kWritten` and
`kFrameEnd`, and loses `kWalkBegin` to `kWalkEnd` with the draws, the
clears, the render states, the stage states, the textures and the viewports
(`StateOnly`, `stream_split.h`). A walk needs nothing of an earlier walk,
because `kWalkBegin` has the whole state of the device (`WalkBeginBody`, and
`BeginOnWorker` of `vk_scene.cpp` takes each render state, each stage state,
both textures and the viewport from it). The records of state go into the
ring in parts as the reader makes room. A whole frame leaves 4 MB of the
ring free (`state_reserve`), so the state of a dropped frame goes in with
no wait. The log has one line when the reader becomes slow, one for each
10000 dropped frames, and one when 1 s went with no drop.
`vk_stream_live_drop=0`: a frame with a `kStart`, `kSurface` or `kWritten`
record never drops and waits for room, and a frame with none drops whole.
The frame thread spins no longer than `vk_stream_live_drop_us`, and a longer
wait sleeps. When the reader takes no byte for 2 s, the writer gives up on
it (`lost` in the head), says so in the log, and stops until the next ask.

**The reader in the app** (`stream_live=1` in `bwvr.ini` of the game folder
of the app, `stream_reader.cpp`). A thread waits for the ring file with a
whole head, asks for the whole state, and takes each frame of the ring into
a queue at once. The queue keeps the newest `stream_live_frames` frames
whole (3 by default), and each older frame keeps only its records of state
(`KeepWalks`, `stream_split.h`). The thread that plays takes the whole queue
(a batch) and gives its frames to the renderer (the renderer is not thread
safe, so one thread plays), through the same player as the file
(`PlayStreamRecords`, `scene_player.cpp`): from Play to the end of a game
with `frame_loop=1` the play thread, as soon as frames come ("The frame
loop"), else the loop of the session at each of its frames. It plays the
walks of the newest
frame and of each frame with a picture of a test (`vk_scene_test_frame` of
the stream, or F12), and only the records of state of the other frames (the
log counts them as skipped). So the picture is at most one batch late, and
no surface is lost. Then the eye picture goes to the stereo screen with a
new revision, one for each batch. With `stream_live_frames` above 0 the
queue has a limit of `stream_live_queue_mb` (64 by default): when a frame
would pass it, for example while the session does not run and takes no
frame, the reader clears the queue and leaves the ring, so the writer stops
at its next frame with no wait. When the session takes frames again, the
reader asks for the whole state, as a reader that comes late
(`QueueFull`, `stream_split.h`). `stream_live_queue_mb=0`: no limit.
`stream_live_frames=0`: the queue takes
up to 64 MB of whole frames, and a batch plays each frame with a `kStart`,
`kSurface` or `kWritten` record, each frame with a picture of a test and the
newest frame (`PlayInBatch`, `stream_ring.h`).
With no session the loop runs each 5 ms. A new head (a new start of the game), a `lost` of
the writer or a record that is not of the stream makes the reader ask
again. The log has one line for each change of state, and a line with the
counts when no frame came for 1 s.

**The test reader of the PC** (`bwvr_stream_reader.exe`,
`test/stream_reader`). `live_stream_run.ps1` starts it with a desktop run in
the test save "pen" (`one_walk=1`, `fake_controller=1`, `vk_stream_live=1`,
a record of the frames 3000 to 3003, the pictures of frame 2996). The reader
asks at frame 2990, so it comes late, takes the frames to frame 3003, writes
them to `ring-capture.bin` and the size and the wait of the writer of each
frame to `ring-frames.txt`, and leaves. Then the script compares each frame
of the record with the frame of the same number of the ring.

```
live_stream_run.ps1 -Name live -Dll <ddraw.dll>
```

**What the desktop run shows** (a Radeon RX 7900 XTX, about 500 frames a
second): the answer comes 1 to 2 ms after the ask, at the next frame. The
frame of the answer and the first frame of the record have 14.4 MB (59
surfaces with 13.6 MB of pixels), and the other frames 0.79 MB. Each of the
frames 3000 to 3003 of the ring has the same bytes as the frame in
`bwvr-stream.bin`. The writer waited for room for no frame. The copy of a
frame into the ring took 4 to 6 ms for a frame of 14.4 MB and 0.05 to 0.5
ms for the others. Each of the 997 frames 3004 to 4000 was at the test
reader in its own game frame. After a frame of 14.4 MB the next frames are
1 to 4 game frames late, because the test reader takes some milliseconds
for the large frame.

**With a slow reader** (`live_stream_run.ps1 -SlowMs 10`: the test reader
takes 1 MB and then sleeps 10 ms): the writer dropped the walks of 12487 of
16890 frames, each in at most 0.17 ms with no wait for room, and the game
ran at 467 frames a second against 502 and 509 with no reader. Each frame
of the record was whole in the ring.

**A TEST AID of the app** (`stream_live_feed=1`): a child process of the app
makes the ring and writes the frames of `bwvr-stream-feed.bin` of the game
folder into it as the mod does, after the ask, with no drop, one frame each
`stream_live_feed_ms` (0 by default: no wait). So the reader
takes the frames from a second process through the file, as from Wine.
`emu_live_test.ps1` uses it with the capture of a desktop run (see "Test the
APK", step 5), and `emu_xr_test.ps1 -Live` in a session of the test runtime
with one frame each 14 ms (step 3). In the emulator the four pictures of frame 2996 are the same
as the pictures of the PC, to the byte.

**Not shown yet:** the ring under Box64 (the barriers of Box64 in a
headset), and the frame rate of the live stream in a headset. With the game
in Wine in the emulator the ring of the mod (a real file of the game folder
that Wine and the app map) carries the screen frames of the logos, the video
and the dialog of a new profile: the app took 20749 and 20609 frames whole
and skipped the walks of 137922 and 120925 ("Play in the app", "The screens
with no scene").

**Known limits:**

- With `vk_keep_picture=1` a walk with no clear of the whole target starts
  with the picture of the walk before (the trails of the intro). After a
  frame that lost its walks, such a walk starts with an older picture. The
  walks of "pen" start with a clear.
- A frame with the whole state (14.4 MB in "pen") takes some milliseconds in
  the writer and in the reader, so the next frames come 1 to 4 game frames
  late. Its 13.6 MB of pixels can wait in parts for room in the ring, also
  with the room that the walks leave free (4 MB).
- No run had a tail or more than one walk in a frame, and the move of the
  tail at the start of the next frame (`VkStreamFrame`) has no math check.
- A frame with `kStart` drops the tail before it, also the `kGaze` and the
  `kOnlyFrame` of the tail.
- No desk run compared a frame of state of the writer (a frame with no
  walks) with the record: the slow reader of the desk run took each frame
  of the record whole.
- The picture of frame 3002 of the test save changes with the time of the
  run (the same build gave other pictures 8 minutes later). A comparison of
  pictures of two builds needs runs close in time, in the order A, B, A.
- The limit of the queue of the app (`stream_live_queue_mb`) ran in no
  test: no test stops the session while frames come.
- With `vk_stream_live_drop=0` and `stream_live_frames=0` (the first way) a
  slow app holds the frame thread of the game, and the picture can come
  more than 1.5 s late (about 126 frames a second in place of 536 with the
  slow test reader).

## The input channel

In the headset the game runs in Wine, and Wine has no OpenXR. The app has
the OpenXR session. So the app gives the mod what the mod reads from a
session of its own (`XrScreen`, `src/vr/xr_screen.h`), and the mod gives
the app the pulses of the hands. Both map one real file of 4 KB,
`bwvr-input.chan` beside `runblack.exe` (`src/common/input_channel.h`): the
mod with `CreateFileMapping`, the app with `mmap` and `MAP_SHARED`, as the
ring of the live stream.

**The file.** A head (a magic, a version, the size, the offsets, the
sessions of the two sides and the indices), at 256 the slot of the newest
input, at 1024 a ring of 64 events, at 2048 a ring of 32 keys, and at 2304
the slot of the place of the player. Each part has one layout on 32 bit
x86 and on 64 bit ARM (`static_assert` of each size and offset).

**The slot.** One writer (the app) and one reader (the mod). The writer
makes the sequence odd, writes the data and makes the sequence even. The
reader loads the sequence, copies the data and loads the sequence again.
When the first is odd, or the two are not the same, the copy is torn and
the reader reads again (up to 64 times, then it keeps the values of the
frame before). A full barrier is between the steps: the interlocked calls
of MSVC in the mod, which Box64 translates with barriers, and the
`__atomic` builtins in the app. The slot holds:

- the two eyes at the display time: position, orientation and the four
  angles of view of `xrLocateViews`, in the LOCAL space;
- the two hands after `xrSyncActions`: active, the aim and the grip poses
  with valid and tracked, trigger, squeeze, the stick, the stick click, the
  face buttons, the menu button, the serial and the profile;
- the predicted display time and the display period of the runtime, the
  focus, the count of the recenters of the LOCAL space, the size of an eye
  that the headset asks for, and the gaze of an eye tracker (below).

**The gaze of an eye tracker** (`XR_EXT_eye_gaze_interaction`). The app
asks for the extension when the runtime lists it, `foveation` or
`fovea_res` is on (or `test_keys=1`) and `foveation_gaze` reads OpenXR (1
or 2, `backend.md`, "Foveated rendering"). When the headset has an eye
tracker, the session has the action `eye_gaze` on
`/interaction_profiles/ext/eye_gaze_interaction`, and each slot with the
focus gets the gaze in the VIEW space (the space of the head) at the
display time, with `kGazeValid`. The mod makes the centre of the fovea from
it. The log of the app says if the headset has an eye tracker, names the
first gaze, and counts the slots with a gaze. On Meta Horizon OS the gaze
needs the permission `com.oculus.permission.EYE_TRACKING`: the manifest has
it, and the app asks the player at the start when the headset has an eye
tracker. The answer counts from the next start of the app. The menu of the
app has no row of `foveation_gaze` (a page has room for 10 rows), so
the app takes its value from `bwvr.ini` (1, the default, reads OpenXR).

The values go across as the runtime gave them: no smoothing, no prediction
of the app, the clock of the runtime.

**The events.** The mod puts a pulse (the frame, the hand, the amplitude
and the length) into the ring, and the app takes each one and plays it with
`xrApplyHapticFeedback`. Each side stores its index with a release after
its copy. A full ring drops the new pulse and counts it. At its start the
app drops the pulses of an earlier run.

**The keys.** The ring of the keys goes the other way: the app puts each
key of its keyboard into it (a character, 8 for back space or 13 for Enter,
with the number of the newest slot), and the mod takes the keys at each
frame with no scene ("The dialog boxes by the controllers"). The rules are
those of the ring of the events. A full ring drops the new key and counts
it. At its start the mod drops the keys of an earlier run. The ring and its
indices are in fields of the head and of the tail that were reserved, so
the version stays 1: a mod that reads no keys and an app that writes none
use the file as before.

**The place of the player.** A second slot goes the other way: in each
frame the mod writes the place of the player in the world
(`input_channel::World`, "The motion of the stick") with the rules of the
slot, the mod as the writer, and the session of the app reads the newest
one. It is in the tail of the file, so the version stays 1, and a slot that
no mod wrote has no map. A writer that stopped in a write leaves an odd
sequence: the next write makes it odd again and then even. At its start the
app writes a slot with no map, so the map of an earlier run does not count.

**The mod** (`xr_input=1`, `src/mod/xr_input.cpp`). The mod starts no OpenXR
session, and the frame runs as with `stereo_headless=1`: it waits for no
session. The channel takes the place of the session at each place where the
mod reads it, and it wins over `fake_head` and `fake_controller`:

| What the mod reads | Where |
| --- | --- |
| The views of the eyes (`XrScreen::eye`) | `stereo.cpp`: the angle of view (`PrepareHeadView`), the eye separation, the head of the frame, the pose of each eye. `intro_vr.cpp`: the head of the intro. `controllers.cpp`: the head of the hint cards and the ring |
| The hands (`XrScreen::hand`) | `controllers.cpp` (`ControllersFrame`), `intro_skip.cpp`, the menu button of the intro in `intro_vr.cpp` |
| The profiles (`HandProfile`) | `controllers.cpp`: the pointer, the aim presets, the grab of the other hand, the hints |
| The focus (`InputFocused`) | `controllers.cpp` (`LatchFront`, the game window behind) |
| The recenter (`TakeRecenter`) | `stereo.cpp`, `intro_vr.cpp` |
| The display time (`LastDisplayTime`, `DisplaySeconds`) | `stereo.cpp` (`StereoDisplayTime`), `mod_main.cpp` (`ModDisplaySeconds`) for the walk and the smooth zoom of the God view and the grips |
| The gaze (`EyeGaze`) | `stereo.cpp`, the fovea of the foveated rendering |
| A pulse (`Haptic`) | `controllers.cpp` (`PulseWith`): into the ring of the events |

The first reader of a frame of the game reads the slot, and each later
reader of that frame gets the same values (`XrInputLatch`). With
`vk_pace=1` a read before the wait of the frame (the intro skip in
FinishFrame) reads only the newest slot, and the first read after the wait
latches the frame ("The pace of the frames"). A recenter of
the app recenters the mod one time. When the app writes no new slot for
`xr_input_stale_ms` (500 by default, 0: never), the hands read as not
active and the focus as off, and the head keeps its last pose. The lockstep
of a test has no such rule. The mod does
not write `headset_width` and `headset_height` to `bwvr.ini` from the
channel: the app writes them itself. `xr_input=0`: the session, or the fake
sources on the desktop.

**The app** (`input_live=1`, `android/src/input_live.cpp`). The session
also has the actions of the mod, with the bindings of `xr_screen.cpp` for
the Touch controller, the simple controller and the Index controller. At
each frame of the session, after `xrWaitFrame`, `xrBeginFrame` and
`xrSyncActions`, the app locates the views and the poses at the predicted
display time, writes the slot and plays the pulses that wait. While the
session does not run, the app writes one slot with no focus and no active
hand. `input_live_test_pulse=N` (a test aid, with no mod): at the slot N
the app puts one pulse into the ring itself, as the mod does.
`input_live=0`: the app reads only the input of its menu.

**The test aids of the mod.** `xr_input_record=N`: the input of the first
N frames of the controllers goes to `bwvr-input.rec` beside `runblack.exe`,
from the source of the run (the session, the fake sources or the channel).
`xr_input_lockstep_ms=N` (with `xr_input=1`): each latch waits up to N ms
for the slot of its frame of the controllers, so a test writer can give
each frame its input. After one wait that runs out the lockstep stops.
With `vk_pace=1` and `vk_device=0` the writes of the slot pace the frames
of the game ("The pace of the frames").

**The test writer of the PC** (`bwvr_input_writer.exe`,
`test/input_writer`). `play` writes the input of a record into the slot,
frame by frame, as the app does, and writes each pulse of the mod.
With a rate (`input_channel_run.ps1 -WriterHz`) it also writes the last
slot again at that rate, as the app writes a slot at each frame of its
session. With the word `app` (`screens_run.ps1 -WriterApp`) it writes
only at its rate, with a new frame of the writer in each write and no
answer to a latch, as the app does, and `screens_run.ps1 -WriterSeconds`
stops it early, for the stale rule.
`compare` compares two records frame by frame, and `dump` writes a channel
file as text. `input_channel_run.ps1 -Kind fake` makes a run in the test
save "pen" with the scripted controller and a record. `-Kind channel -From
<the fake run>` makes the same run with `xr_input=1`: the writer plays the
record of the fake run into the channel, and the script compares the
records, the pulses and the pictures of the test frame.

A channel run gives the mod the input of the fake run in each frame of the
record (the two records have the same bytes), the same pulses at the same
frames, and pictures of the test frame within the noise of two fake runs
close in time. With `xr_input=0` the pictures are within the same noise as
the build before. `xr_input` and `input_live` have no test key: the source
of the input is chosen at the start, and with `xr_input=1` the mod starts
no session.

In the emulator with the game in Wine (x86_64, no Box64, `emu_game_test.ps1`)
the mod maps the channel that the app made and reads the slots of the test
runtime ("xr input : the first slot of the app ..., eyes valid, focus yes,
profiles "oculus/touch_controller" and "oculus/touch_controller", the
headset asks for 2064 by 2208, display period 13.89 ms").

**Not shown yet:** the channel under Box64 in the headset (the barriers of
Box64), and the input of a real headset through the channel.

**Known limits:**

- Not run in a headset: the gaze of an eye tracker (Quest Pro, Steam
  Frame). Open: if the runtime of the Steam Frame for an APK has
  `XR_EXT_eye_gaze_interaction`, and if it needs a permission of its own.
- The test aid `input_live_test_pulse` writes into the ring of the mod.
  With a mod that sends pulses at the same time, the ring has two writers.

## The projection layer

On the PC the mod shows the God view in a projection layer of its session
(`XrCompositionLayerProjection`, `XrScreen::BuildFrame`): each eye with the
pose that the game drew it from, and the angles of view that the game drew.
The runtime reprojects the picture to the pose of the head at the display
time. In the headset the app has the session, so the mod gives the app the
same values for each frame, and the app makes the layer.

**The values of a frame** (`src/mod/view_layer_math.h`, `MakeView`). These
are the values of the projection layer that the mod gives its session in a
frame of the God view:

- `SetProjection`: a projection layer, or the screen of two quads. The head
  view setup of the first frame turns it on.
- `SetRenderHalfAngle`: the half angle of the symmetric frustum that the game
  draws. The four angles of each eye come from it and from the size of the
  picture (`LayerFov` in `src/vr/xr_screen.h`, the rule of `BuildFrame`).
  With no half angle, the angles of the views of the frame.
- `SetRenderPose`: the pose of each eye that the picture was drawn from, one
  frame older than the newest pose, because the camera of the engine is
  built before the frame begins. In the first frame, the views of the frame.
  With no roll of the camera the pose has no roll.
- The size of each eye picture. The rect of each eye is the whole picture,
  layer 0 of its swapchain, in the LOCAL space.
- The display time of the input of the frame, and the frame of the input
  channel (`SlotData::frame`).
- A frame of the cut scene window: the mod shows it as a screen.

**The back channel** (`xr_input=1`). The values go with the frame of the
live stream, as a record `kView` of `scene_stream.h` (128 bytes), after
`kFrame` and before the first walk. So the values belong to the eye pictures
of the same frame with no frame number to match, and a skip, a drop and the
queue of the app keep them with their frame. The layout is the same on 32
bit x86 and on 64 bit ARM (`static_assert` of each size and offset). With
`xr_input=0` the stream has no `kView`, and the session gets the values.

**The app** (`stream_projection=1`, the default). The player of the app
(`scene_player.cpp`) takes the `kView` of a frame, and the view goes with the
picture at the end of the walk that ends the picture of that frame
(`PictureView`, the rule of `WalkEndsFrame`). A frame that a newer frame
replaced keeps its records of state with no walks, so its view never goes
with a picture. A picture with a view for a projection layer (a projection
layer, valid views, no frame of the cut scene window) shows in a projection
layer: two swapchains of the size of the picture of the renderer, each eye
with the pose and the angles of view of the mod, the menu and the cursor as
quads over it. A swapchain gets a new image only for a new picture
(`xr_copy_on_change`), and the values go with the image that an eye
released. The app adds no smoothing. With `motion_comp=0` each view has the
pose of its picture. With 1 and 2 (the default) the pose of each view has the
difference of the place of the player since the frame of its picture ("The
motion of the stick"). A picture with no view shows on the stereo screen, and the log says so
one time. `stream_projection=0`: each picture shows on the stereo screen.
`stream_projection` has no test key: the app reads it at its start, and the
app has no hotkeys. When the app cannot make the swapchains of the layer, it
frees what it made, says why in the log one time, does not try again, and
the pictures show on the stereo screen.

**The test aids of the mod.** `xr_view_record=N`: the values of the first N
frames of the scene go to `bwvr-view.rec` beside `runblack.exe`, from the
source of the run (the session, the invented head, or the back channel).
`fake_head_fov=1`: the invented head has the angles of view of the OpenXR
test runtime of the app, so a desk run with no headset has the view setup of
a headset and a projection layer (`backend.md`, "Testing without a
headset"). `bwvr_stream_reader views` writes the `kView` of a stream and
compares them with a record of the views, and `bwvr_stream_reader
view-compare` compares two records of the views.

**The desk runs** (`input_channel_run.ps1`). A fake run with
`-More @{ fake_head_fov = 1 }` records the input and the views. A channel
run `-Kind channel -From <the fake run> -Live` plays that input into the
channel, has the live stream with the test reader, and checks that the
views of the back channel are the views of the fake run (`view-compare`)
and that each `kView` of the ring and of the record of the stream is the
view of the same frame. In the test save "pen" (a render of 4080 by 3264,
`one_walk=1`) the 3100 views of the channel run are the 3100 views of the
fake run, each with a projection layer: the half angle 1.0602 radians
(60.74 degrees), the angles -1.0602, 1.0602, 0.9599 and -0.9599 for each
eye, the eyes at -0.0334 and 0.0334 m. The first frame has the views of the
frame, and each later frame the pose of `SetRenderPose`. Each of the 13
`kView` of the ring (frames 2991 to 3003) and the 4 of the record of the
stream (frames 3000 to 3003) has the bytes of the view of its frame. The
pictures of frame 2996 of the channel run differ from the pictures of the
fake run by 0.000 to 0.028 percent of their pixels by more than 24.

A head in steps: the fake run has the words `look@3004+4 look@3016+40` of
the scripted controller (the head turns 30 degrees left and 20 degrees
down, still between the steps, with no motion), and the channel run has
`-Live -LiveRecord 3030`, so the capture has the frames 2991 to 3033. Its
`kView` have the straight pose in the frames 2991 to 3005 and 3010 to 3017,
and the turned pose in the frames 3006 to 3009 and 3018 to 3033, one frame
after each step of the head: the pose that each picture was drawn from.
The 3100 views of the two runs are the same, and each of the 43 `kView` of
the ring and the 4 of the record of the stream has the bytes of the view of
its frame.

**In the emulator** (`emu_xr_test.ps1 -Live <the channel run> -Gpu host`,
"Test the APK", step 3). With the capture of the head in steps (43 frames,
each with a `kView`): the app made the two swapchains of 4080 by 3264, and
each of the 397 frames of the session with a picture had the projection
layer and no quad of the stereo screen. The app showed the pictures of the
frames 2992, 3013 and 3033, and the layers that the runtime reported have
the values of the `kView` of those frames in that order: the straight pose,
then the turned pose. The rect of each eye is its whole picture, and the
pictures of both eyes that the runtime read back have the scene. Each eye
got a copy for each of the 3 new pictures. The four pictures of frame 2996
are the same as the pictures of the PC, to the pixel. With `-FeedMs 0` the
app showed the frames 2991 and 3033, with the two poses in that order.

**Not shown yet:** a real headset (the reprojection of the runtime of Meta
with the poses of the mod, and the comfort of the God view in the app), and
the run with the game in Wine.

**Known limits:**

- The intro (`intro_vr.cpp`) gives no frame of the live stream, so its
  pictures and its poses do not go to the app.
- After a recenter of the LOCAL space the frames that the mod drew before
  the recenter go out with the poses of the space before, until the frame of
  the new input comes.

## The motion of the stick

When the game gives no new picture in time, the headset shows the picture
before again. The runtime turns that picture to the head, but it does not
move it with the walk of the stick, the turn of the view or the grab of the
world. So a walk with the stick stutters: the view stops for a refresh and
jumps at the next one. The app shows each picture at the place in the world
where the game drew it, seen from the place of the player now
(`motion_comp`, `android/src/motion_comp.cpp`, the math in
`src/mod/motion_comp_math.h`).

**The place of the player** (`input_channel::World`). For each frame the mod
gives the map of the LOCAL space to the world of the game that the camera of
the frame used (`src/mod/motion_world.cpp`, `HeadCameraWorldMap` of
`head_camera.cpp`):

```
world = camera + units * L * F * R(origin)^-1 * (local - origin)
```

| Symbol | Meaning |
| --- | --- |
| `camera` | the camera of the main camera call with no head: in the God view and on the table the recenter pose in the world, else the camera of the game |
| `L` | the levelled basis of the room: world up and the heading of the camera |
| `F` | diag(1, 1, -1), from the right handed LOCAL space to the left handed world |
| `origin` | the recenter pose of `head_pose.cpp` in the LOCAL space at the main call |
| `units` | game units for a meter of the room: the lean gain of the main call times `HeadPoseScale` |

The camera of the frame is the map of the head of the frame, so the eye
pictures of a frame show the world as its map puts it in the room. The map
goes after the `ViewBody` of the `kView` of its frame in the live stream
("The projection layer"), and the newest map goes into the input channel
("The input channel", "The place of the player"). It also has the walk of
the frame: the speed of the move of the waist point in game units a second
(`TabletopGodWalkRate` of `tabletop.cpp`), the hand of the stick of the
walk, its push, and the dead zone and the full push of that stick
(`ControllersWalkStick`). A frame with no projection layer, a flat frame of
the cut scene window and a frame with no recenter pose have no map.

**The map now.** At each frame of the session, after the take of the
picture, the app reads the newest map of the input channel. When the channel
is behind the newest picture (the frame of the picture came after the read)
or more than 600 frames ahead of it (a channel of an earlier game), the map
of the newest picture is the map now. With `motion_comp=2` the map now goes
on with the walk of its frame over the slots of the input channel that the
app wrote after the slot of the map (`motion_comp_math::AheadSeconds`). Each
frame of the mod walks by the push of the slot that it took, for the time
from the display time of the slot that it took before, so those slots are
the walk in the pipe. For each of them the map goes on by the speed of the
walk of its frame times the push of that slot over the push of the frame,
for the time from the slot before, at most `motion_comp_predict_ms` after
the display time of the map. The push is the push of the stick of the walk
that the mod gets, with the dead zone and the curve of the mod
(`walk_math::Push`). A slot counts only when the slot after it pushes the
stick too: the mod can skip a slot, and a later slot with the stick back
then stops the walk for the time of the skipped slot. So while the mod
skips no two slots in a row, the prediction never goes past the place where
the mod stops, and the view does not step back at a stop. When the mod took each slot up to the stop, its map comes
one slot ahead of the prediction: one step forward of the push of the last
slot.

**The pose of a view.** The difference of the map now and the map of the
picture of each eye takes a point of the LOCAL space of the picture to the
LOCAL space now. The eye of the picture goes to its place with the
difference, and its orientation turns by the turn of the difference. A
runtime with no depth of the picture turns a projection layer to the head
and does not move it: for it a picture is as far as the horizon. So the view
also turns so that the point `motion_comp_depth_cm` (100) in front of the eye
shows in its direction now. A step of 2 cm to the side then turns the view by
1.15 degrees. The land at that distance shows at its place, nearer land
moves a little less than it must, and the horizon moves a little. With 0 only
the eye moves, for a runtime with the depth of the picture.

**What stays.** Two maps with the same bytes give no difference, and the
views keep the poses of the picture to the bit: a still stick gives the same
layer as `motion_comp=0`. A difference over a limit is a cut of the view (a
teleport, a snap turn, a zoom step, a recenter, a new place at a camera of
the game), and the picture shows as the game drew it: a step at the head of
`motion_comp_most_cm` (50), a turn of `motion_comp_most_deg` (10), or a
change of the scale of `motion_comp_most_scale_pct` (25). The panel quads,
the cards and the cut scene window hang in the room or at the hands, not in
the world, so they keep their poses. The screen quad and the stereo screen
show no world.

| Key | Default | Meaning |
| --- | --- | --- |
| `motion_comp` | 2 | 0: each view has the pose of its picture. 1: the difference to the newest map of the mod. 2: the same, and the newest map goes on with the walk of the stick to the display time. The row "Picture follows the stick (headset app)" of the page "Testing". A Save of the menu while the game runs changes it at once |
| `motion_comp_depth_cm` | 100 | The distance of the turn of a view for a step. 0: no turn for a step |
| `motion_comp_most_cm` | 50 | The limit of the step at the head |
| `motion_comp_most_deg` | 10 | The limit of the turn |
| `motion_comp_most_scale_pct` | 25 | The limit of the change of the scale |
| `motion_comp_predict_ms` | 50 | The longest prediction of the walk |

The keys are in `bwvr.ini` of the game folder, read at the start of the app.
The app has no test keys: the row of the menu is the A/B of `motion_comp`.

**The log.** At the start "motion : motion_comp=2: ...", and at a change in
the menu "motion : motion_comp is now 0 (it was 2): ...". The line of each
new picture of the projection layer has the difference of the left eye in
that frame:

```
xr           : the projection layer shows the picture of frame 3012, with a difference of 1.952 cm and 0.000 degrees at the head and a scale of 1.0000 to the place of the player in frame 3013, 13.9 ms of the walk ahead (motion_comp=2)
xr           : the projection layer shows the picture of frame 3101, with a difference of 0.000 cm and 0.000 degrees to the place of the player in frame 3101 (motion_comp=2)
```

With `time_log=1` each 10 s: "motion : 10.0 s, motion_comp=2: N frames of
the projection layer ...": the frames with the map now from the input
channel, with no difference, with a difference (the step at the head and the
turn, the mean and the most), over the limits, with no map, and with the
walk ahead (the mean and the most in ms).

**The tests.** `.\dev.ps1 build`: the checks of `motion_comp_checks.h`. The
map of the head is the camera of the main call with the real
`head_pose.cpp`, and the turn of the map times the head is the basis of the
camera. A still player gives the identity, and the pose of a view keeps its
bits. A step of the stick of 12.4 cm gives a step of 12.4 cm at the head and
no turn, and the head goes back by the step in the LOCAL space. A turn of 4
degrees about the head gives no step at the head and a turn of 4 degrees. A
snap turn of 30 degrees, a teleport of 3 m and a zoom step of 1.5 times are
cuts. The prediction over slots of 10 ms at a full push of 150 units a
second: 4 of 5 pushed slots count (6 units), half the push 3 units, at most
50 ms, the slots before a slot with the stick back, and none with a gap in
the slots or with `motion_comp=1`. A stop with a full push up to slot 30
and a mod 3 slots late, which takes each slot, each second or each third
slot: the view never goes back, never goes past the place where the mod
stops, and after the pipe it is that place. `emu_game_test.ps1
-StickFrames N`: in game 2, after its pictures, the left stick pushes forward
for N frames of the script and is then still for 80 frames. The test checks
that pictures in the walk have a difference, and that each new picture of
game 2 before the walk and after its stop (with 10 frames of the pipe, the
frames from the lines of the mod) has a difference of 0.000.

**What the emulator shows** (the command of "The frame loop" with
`-StickFrames 144 -MenuClick 690,430`). The click on "Continue Game" closes
the Main Menu of game 2, because a dialog box stops the walk. With
`motion_comp` 1 and 2 the pictures of the walk show with a difference of
some cm at the head and no turn, and each picture before the walk and after
its stop has no difference. With `motion_comp=0` no picture has a
difference.

**Not shown yet:** a headset. Only a headset shows the stutter of a walk
that the player sees with each value of `motion_comp`, the distance of
`motion_comp_depth_cm` that keeps the land still in a step, how the runtime
of Meta shows a projection layer with a pose that is not at the head, the
feel of the stop of a walk, and how the Hand looks while the player walks
(see the known limits).

**Known limits:**

- The runtime gets no depth of the picture (`XR_KHR_composition_layer_depth`):
  the app asks for no such extension, makes no swapchain of the depth, and
  the ring has no copy of the depth. So a step shows exactly at one distance
  only.
- The Hand, the leash ring and each other thing that the game draws in the
  eye pictures move with the world by the difference. While the player
  walks, the Hand that shows is behind the real hand by the difference of
  its picture (some cm at a full push). Check it in a headset.
- The prediction follows only the walk with the stick, and only its move
  over the land. A grab of the world, the smooth zoom, a turn of two hands
  and the zoom of a climb (`controls=1`) show from the newest map of the
  mod, with no prediction.
- When the mod skips two slots or more in a row just before a stop (a frame
  of the game longer than two frames of the session), the place where the
  mod stops can be one slot behind the prediction, and the view steps back
  by the push of one slot.

## The quad layers

Over its projection layer a session of the mod on the PC has quad layers
(`XrScreen`, `src/vr/xr_screen.h`, `BuildFrame`):

- **The cards** (`StageCard`): the hint card of the hand that points, the
  hint card of the other hand, the subtitle card and the ring. Each card is a
  quad in the LOCAL space with a picture of its own that the mod draws on the
  CPU (`hint_card.cpp`, `ui_panel.cpp`, `radial_menu.cpp`): a texture of BGRA
  with the color multiplied by the alpha, and the rect of it that shows.
- **The panel quads** (`SetPanelQuads`, `StagePanelQuad`, `ui_panel=1`): the
  menu, the help and the Creature panel, rects of the panel image of the
  renderer (`vk_panel=1`).
- **The cut scene window** (`StageWindow`, `cutscene_window.cpp`): a frame of
  the window as a screen (the eye pictures on a quad of each eye at the pose
  of the window, with the crop and the edge columns, and the subtitle card),
  or the four black walls around the opening while the window opens and
  closes.

In the headset the app has the session, so the mod gives the app the same
layers for each frame, and the app shows them.

**The mod** (`xr_input_layers=1`, the default, with `xr_input=1`,
`src/mod/xr_layers.cpp`). The interface of each frame goes as in a session
with the projection layer of the frame: the hint cards and the subtitle card
take the routes of a headset (`UiPanelBeginFrame`), the panel quads come
after the one walk with the panel image of the renderer, and the cut scene
window stages its layer. At the place where the session takes the layers
(the end of the frame of `stereo.cpp`, and the place of `SetPanelQuads`), the
mod takes what was staged (`XrScreen::TakeStaged`, `TakeStagedPanelQuad`)
and writes it into the frame of the live stream: one record `kPanel`, one
`kCard` for each staged card and one `kWindow`, the values of the calls
(`src/mod/quad_layer_math.h`, `MakeCard`, `MakePanel`, `MakeWindow`). A
`kCard` has the hash of the pixels of its rect (`PictureHash`). The pixels go
in a record `kCardPicture` only when the picture is new for the reader
(`CardPictures`, `SentPictures`): after a change, and in a frame with the
whole state (`kStart`). The picture of the panel quads is the panel image of
the renderer, which the app draws itself, so it does not go. The first walk
with both eyes does not end the frame of the stream then: the layers end it
(`VkStreamLayers`), so they are in the frame of their eye pictures, after the
walk and before `kFrameEnd`. A `kCardPicture` is state as the pixels of a
surface: a frame that the writer or the app strips keeps it, and the writer
does not drop a frame with one (`KeepKind`). `xr_input_layers=0`: the
interface goes as with no headset, the stream has no layers, and a frame ends
at its walk. With `xr_input=0` nothing changes. `xr_input_layers` has no
test key (see below).

The records (`scene_stream.h`) have one layout on 32 bit x86 and on 64 bit
ARM (`static_assert` of each size and offset): `kCard` 80 bytes,
`kCardPicture` 24 bytes and the pixels, `kPanel` 176 bytes, `kWindow` 136
bytes. The field `visible` of a quad of `kPanel` has the bits
`kPanelQuadShows` and `kPanelQuadText` (a dialog box on the menu panel quad
takes text, "The dialog boxes by the controllers"). The stream has version
2: a stream with no layer records plays with no quad layer of the mod.

**The app** (`stream_layers=1`, the default). The player takes the layer
records of a frame, and they go with the picture of the frame at its
`kFrameEnd`, when a walk of the frame ended the picture (`PictureLayers`). A
frame that lost its walks gives its layers to no picture, but its
`kCardPicture` still gives the newest picture of the card (`AppPictures`).
The app shows the layers as `BuildFrame` makes them (`Order`, `Spec`):

- a frame of the world: the projection layer, the panel quads, the subtitle
  card, the hint card of the hand that points, the hint card of the other
  hand, the ring, then the four walls.
- a frame of the cut scene window as a screen (`kViewFlat` and a `kWindow`
  with a screen): a quad of each eye of the swapchains of the projection
  layer, at the pose of the window with the columns of the crop and the
  edge, then the subtitle card. No panel and no other card.

The menu and the cursor of the app stay over each layer of the mod. Each
quad has the pose, the size and the rect that the mod gave for that frame,
and the app adds no motion. A card has a swapchain of the size of its
texture, as on the PC. Its pixels go into its staging buffer only when its
picture, its rect or the size of its texture changed, and a frame copies
them into a new image of the swapchain only then (`xr_copy_on_change`). The
panel image of the renderer (`VkScenePanelFor`) goes into a swapchain of its
size with each new picture. The walls are a swapchain of 16 by 16 pixels,
black with the alpha of the frame. The rules of `SendCard`, of the rect of a
panel quad and of the walls are the rules of the PC (`CardShows`,
`PanelQuadRect`, `WallsShow`). `stream_layers=0`: no quad layer of the mod,
the pictures of the world show in the projection layer alone, and a frame
of the cut scene window shows on the stereo screen. `stream_projection=0`:
each picture shows on the stereo screen, a frame of the cut scene window
too, and no quad layer of the mod goes out, with each value of
`stream_layers`. The stereo screen shows only a picture that went out in no
projection layer and in no cut scene window. `xr_input_layers` and
`stream_layers` have no test key: the mod with `xr_input=1` has no session
(the test key of the cards, Ctrl Shift F11, still moves them into the eye
pictures and back), and the app reads its keys at its start and has no
hotkeys.

**The test aids.** `xr_layer_record=N` (the mod): the layers of the first N
frames of the scene go to `bwvr-layer.rec` beside `runblack.exe`, a stream
with one frame for each frame of the scene, from the source of the run (the
session, the invented head or the input channel). `fake_head_layers=1`: the
invented head routes the interface as a session does (`backend.md`,
"Testing without a headset"). `bwvr_stream_reader layers <stream> [<record>]
[<folder>]` writes the layers of each frame of a stream with the quads as
the test runtime reports them, checks the hash of each picture, compares the
layer records of each frame with a record, and writes the pictures of the
cards as BMP files. `bwvr_stream_reader layer-compare` compares two records
of the layers frame by frame. The test runtime of the app reports each quad
in full (`frame N quads`: the pose, the size, the rect, the eye and the
flags), and the word `point <hand> menu` points at the menu quad of the app
over the layers of the mod.

**The tests.**

- `.\dev.ps1 build`: the math checks of `quad_layer_checks.h` ("Test the
  APK", step 6).
- A pair of desk runs (`input_channel_run.ps1`): a fake run with `-More @{
  fake_head_fov = 1; fake_head_layers = 1 }` and a channel run with the same
  keys and `-Kind channel -From <the fake run> -Live`. `-LiveAttach` and
  `-LiveRecord` choose the frames of the capture. The channel run checks that
  the layer records of each frame of the ring and of the record of the
  stream are the bytes of the record of the layers of the channel run, that
  each picture of a card has the hash of its `kCard` and that each card has
  its picture, and it compares the layers of the two runs
  (`layer-compare`): a frame differs only in the pictures or in the cards
  that show only when a picture of a card or the set of the cards changes in
  one run at that frame or at the frame after it (a fade by the clock of the
  PC). Each other frame that differs is a fault. `ui_panel = 1` in both runs
  gives the panel quads. `cutscene_window = 1`, `cutscene_immersive = 0` and
  the test key `cutscene_window_test_bars` give the cut scene window in the
  test save "pen". The window opens and closes by the clock of the PC
  (`cutscene_window_ms`), so `cutscene_window_ms = 0` gives both runs the
  same frames of the window.
- `emu_xr_test.ps1 -Live <the channel run> -Gpu host` (step 3 of "Test the
  APK"): the quads of the runtime against the layers of the frames of the
  pictures of the app, the pictures of the cards that the runtime read back
  against the new pictures that the app logs (each one a picture of the
  feed), the panel quads, the walls and the window. `-NoLayers` runs the app
  with `stream_layers=0` and checks that the runtime has no quad but the
  menu and the cursor. `-NoProjection` runs it with `stream_projection=0`:
  the stereo screen in each frame with a picture, and no quad of the mod.
  `-WindowFrom` and `-WindowTo` make a test feed of the cut scene window.

The commit messages have the numbers of the runs.

**Not shown yet:** a real headset (the cards, the panel quads and the window
over the reprojection of the runtime of Meta), the subtitle card and the
ring through the live stream (in the test save "pen" no frame has help
text, and the ring does not open while a dialog box is up), and the run with
the game in Wine.

**Known limits:**

- The app gives no fault of a card back to the mod. On the PC a card that
  the runtime refuses goes back into the eye pictures (`CardFailed`). With
  `xr_input=1` the mod takes each card as sent, and a card that the app
  cannot show (no swapchain) does not show at all.
- The fades of the cards and the opening and the closing of the cut scene
  window go by the clock of the PC, so two runs with the same input differ in
  those frames.
- A card picture goes whole when it changes: the subtitle card of a large
  render can be some MB in a frame of its fade.
- Two desk runs with the same input give eye pictures of a frame of the cut
  scene window that differ in about 22 percent of the pixels, up to 0.4
  percent by more than 24, while the views and the layers of the frame are
  the same bytes. The cause is not known.

## The screens with no scene

The logos, the videos, the front end and the dialog boxes of the game
(SetupBox) do not walk the scene: the game calls `LH3DRender::FinishFrame`
outside `GGame::Process3dEngine` (`ui-layer.md`, section 1.4). On the PC
the StereoIdle path of `h_FinishFrame` (`mod_main.cpp`) shows such a frame
on a screen in front of the player: the picture of the last walk, or the
pixels that the game wrote into the render target with the CPU. With
`xr_input=1` the mod has no session, and the frames of the live stream start
in the frame of the scene. So the mod gives the app these frames too, and
the app shows them on a quad fixed in the room.

**The mod** (`vk_stream_screens=1`, the default, only with `xr_input=1`,
`vk_stream.cpp`, the rules in `src/mod/screen_quad_math.h`). Each
FinishFrame outside the eye passes of the scene and of the intro is a
**screen frame** of the stream (`ScreenRoute`): the next screen frame starts
at the end of a FinishFrame, so its walk and each record before it (a new
picture of a video texture, `kWritten`) are in it, and it ends at the end
of its FinishFrame. A frame that the PC shows (`StereoIdle`, 150 ms after
the last frame of the scene) ends with a record `kScreen` (72 bytes, one
layout on 32 bit x86 and on 64 bit ARM):

- the source of the picture (`ScreenSource`, the rule of `h_FinishFrame` and
  `VkSceneLastPicture`): the picture of the last walk, or, when the game
  wrote the render target with the CPU after that walk or the walk is
  smaller than the render target, the pixels of the render target. Then a
  `kSurface` with its pixels comes before the record, when the stream does
  not have them since the last write.
- the size of the render target.
- the quad of the screen of the PC (`MakeScreen`, the screen of
  `XrScreen::BuildFrame` with no projection layer): the LOCAL space, no
  turn, `screen_distance_cm` in front of the origin at its height (200),
  `screen_width_cm` wide (260), and as high as the shape of the picture.

The frame of the controllers does not move in these screens (it is 0 before
the first game), so a screen frame has the number 1000000000 + its count in
`kFrame` (`ScreenFrame`). A record (`vk_stream_record`), a picture of a test
(`vk_scene_test_frame`) and the test reader of the PC can name it.
`vk_stream_screen_test=N` (a test aid): each Nth screen frame saves the
pictures of its walk, as F12 does (`bwvr-vk-left-<frame>.bmp`, with a device
of the mod). A frame of the scene after a screen frame ends it. In an eye
pass of the intro (`intro_vr=1`) the open screen frame ends, and the stream
takes no record until its next frame, which has the whole state: the intro
gives no frame to the stream. The log has a line for the first screen in the
stream, for each change of the source, for the end of a run of screen
frames, and the rate of the screen frames each 30 s. The stream has version
3, and the reader plays a stream of version 2 (no `kScreen`) too.
`vk_stream_screens=0`: no screen frame, as with `xr_input=0`. The key is not
a row of `settings_table.h` and not a retired key, so a save keeps it.

**The dialog box** (`setup_box.cpp`): the log names each dialog box that
comes up, with the class of its dialog object from the RTTI of the game (for
example `setup box : the dialog box NewProfileBox is up now`), its
SetupBox, the callback of its controls, the FinishFrame and the frame of
the controllers (`ui-layer.md`, section 5.2). It only reads, and it runs in
each mode: the log of the PC with `xr_input=0` has these lines too.

**The app** (`stream_screen=1`, the default). The player takes the
`kScreen` of a frame at its `kFrameEnd` (`ScreenPicture`): a screen of the
picture of the last walk only when a walk of the frame ended, a screen of
the pixels of the CPU always (they are state). The newest picture of a
batch decides: a screen shows on the **screen quad**, a picture of the world
in the projection layer, on the stereo screen or in the cut scene window.
The screen quad is one quad layer for both eyes, opaque, with the pose and
the size of the `kScreen` and the whole picture: the picture of the last
walk of the renderer (`VkSceneLastPicture`) goes with a blit and a copy into
a swapchain of its size, the pixels of the CPU go into a swapchain with a
staging buffer. Each swapchain gets a new image only for a new screen
picture (`xr_copy_on_change`). It is never a projection layer, and the menu
and the cursor stay over it. With `ui_panel=1` the dialog boxes of these
frames are in the picture, as on the PC. The log has a line for the first
quad and for each change of its source, its size or its quad, and the
counts at the end. The first frame of the game after "Play" is a screen
frame of the logos (`game : the first frame of the game (frame N, a screen
with no scene)`). When the numbers of the frames go from the screen frames
to the frames of the scene or back, the log has one line (`live : the frames
of the scene start at frame N`, `live : the screens with no scene start at
frame N`), and the rate of the frames of a game starts again.
`stream_screen=0`: no screen quad, the app shows no frame with no scene.
`stream_screen` and `vk_stream_screens` have no test key: the mod with
`xr_input=1` has no session, and the app reads its keys at its start and has
no hotkeys.

**The tests.**

- `.\dev.ps1 build`: the checks of `screen_quad_checks.h` (the layout of
  `kScreen`, the version, the numbers of the screen frames, the route, the
  source, the quad of the PC, the screen in the player, a stripped frame,
  the name of a class from the RTTI).
- `screens_run.ps1` (a desk run): a new game with the logos (no skip) and
  `intro_vr=0`, `xr_input=1` with the test writer, the live stream with the
  test reader in screen frames, a record of the screen frames and the
  pictures of each Nth screen frame. Then `bwvr_stream_reader screens` of the
  capture and of the record. `-SkipAt`, `-Most` and `-TestFrame` give a
  capture over the switch to the frames of the scene: the reader leaves
  after a count of frames (`bwvr_stream_reader read`, the argument "most
  frames"), because the numbers of the frames go down at the switch.
- `emu_replay_test.ps1` with that record and the pictures of the PC of its
  screen frames, and `emu_xr_test.ps1 -Live <the run> -Gpu host`: the screen
  quad against the `kScreen` of the feed, no stereo screen and no projection
  layer, the pictures of the screen frames of the feed against the pictures
  of the PC, and the quad that the runtime read back at the end against the
  picture of the PC of the last frame of the feed. `-NoScreen`: the app with
  `stream_screen=0`.
- `emu_game_test.ps1`: the screen quad in the game in Wine (see "Play in the
  app"). With `-NewProfile` the dialog boxes by the controllers ("The dialog
  boxes by the controllers").

**What the desk runs show** (`screens_run.ps1`, a render of 4080 by 3264,
`intro_vr=0`, the profile of the PC): the logos are screen frames 1 to about
6700, the game puts up the dialog box `SkipBox` at about screen frame 7700
(the log names it), and the Lionhead logo with that box plays as screen
frames to about 27300, at 742 screen frames a second. Then the frames of the
scene start. Each `kScreen` of the runs has the picture of the last walk
(the game wrote no render target with the CPU after a walk), and the quad
2.60 by 2.08 m at 0, 0, -2 m. A screen frame of a logo has 2.6 KB with no new
texture. A frame with the whole state has 1.5 MB. The flow of a game folder
with no profile is another one: the logos, the video before the intro and
the dialog of a new profile (see "Play in the app"). With `intro_vr=1` the
Lionhead logo is an intro in stereo, and its frames are not in the stream:
in a run with the test reader in the stream from screen frame 7401, the log
has "the screens with no scene end at screen frame 7802, in an eye pass of
the intro" at the first eye pass of the intro (`VkStreamScreenStop`).

**The switch to the frames of the scene** (`screens_run.ps1 -SkipAt 8000
-Attach 8800 -Last 999999 -Most 350 -TestFrame 60`, `intro_vr=0`): a press
of the test at frame 8000 of the intro skips the Lionhead logo, and the
frames of the scene
start after screen frame 8965. The capture has 165 screen frames and 185
frames of the scene (0 to 184, each with a `kView` for a projection layer
and the panel quads). One screen frame has no `kScreen` and no walk: the
screen frame that the first frame of the scene ends. The run has no screen
frame of `kSilent` (150 ms after a frame of the scene), because the game does
not go back to a screen with no scene. The writer dropped the walks of 17
frames of the scene for the slow test reader.
In the A, B, A runs with `xr_input=0` (the build before, this build, the
build before, the test save of a new game) the frame time is 1.9 ms in each
run and the scene 1.73, 1.75 and 1.725 ms: the same within the noise.

**In the emulator** (`-gpu host`): the replay of a record of 4 screen frames
of the logos gives the pictures of the PC (0.00 percent of the pixels differ
by more than 24). `emu_xr_test.ps1 -Live` with 40 screen frames of the
Lionhead logo (60 checks): the app showed 24 screen pictures on the screen
quad in 398 frames of the session, each quad of the runtime is the quad of a
`kScreen`, the runtime had no projection layer and no stereo screen, and the
picture of the quad that the runtime read back at the end differs from the
picture of the PC of the last frame of the feed in 0.20 percent of the pixels
by more than 24. The app skipped the walks of 16 frames, and a walk of the
Lionhead logo draws over the picture of the walk before (`vk_keep_picture`),
so that is the likely cause.
`-NoScreen` (56 checks): the menu and the cursor alone.
`emu_xr_test.ps1 -Live` with the capture of the switch (76 checks): the app
showed the screen quad first, then the projection layer. The log has "live :
the frames of the scene start at frame 0", the runtime had the screen quad
before its first projection layer and no screen quad and no stereo screen
in the lines of its quads after it, the panel quads of the frames of the
scene show over the projection layer, each eye of the projection layer got
a copy for each new picture of the scene (47), and the five pictures of the
test frame (`bwvr-vk-*-00061.bmp`, the left eye, the right eye, the outer
and the centre image and the panel) are the pictures of the PC (0.00
percent of the pixels differ by more than 24).

**The game in Wine** (`emu_game_test.ps1`, see "Play in the app"): the
logos, the video before the intro and the dialog of a new profile show on
the screen quad, and the first frame of the game comes 12.4 to 13.4 s
after "Play".

**Not shown yet:** the screen quad in a headset (its place, its size and the
sharpness of the text of a dialog), a screen of the pixels of the CPU (no
run had one), a screen frame of `kSilent` (the app shows no new picture for
it, and its walk with no eye pass gives no picture to the eyes, but no run
had one), the switch from the frames of the scene back to the screens (a
save or a load of the game), and the screens with no scene with a session
of the mod on the PC.

**Known limits:**

- The intro in stereo (`intro_vr=1`) gives no frame to the stream, so the
  headset shows its last screen picture while it plays.
- A frame of the Lionhead logo draws over the walk before it
  (`vk_keep_picture`), so a picture after walks that the app skipped differs
  a little from the picture of the PC.

## The dialog boxes by the controllers

A dialog box of the game in a frame with no scene (the dialog of a new
profile, `NewProfileBox`, and each box of the front end) shows on the screen
quad. The player points at it with a controller and clicks with the trigger,
and a box that takes text gets the keys of a keyboard of the app. The game
then takes them as the mouse and the keys of a PC.

**The pointer** (`screen_pointer=1` of the mod, `controllers.md`, section
12). The mod reads the hands of the input channel one time for each frame
with no scene, after the wait of the pace. While a dialog box is up, the aim
ray of the hand that pulled its trigger last meets the quad of the `kScreen`
of the frame, and the hit is the pointer of the game, as a pixel of the
render. The trigger is the left button of the mouse of the game. A press
starts only with the ray on the quad. The mod computes the hit, not the app:
the mod makes the quad and has the hands, and the pixel goes into the game in
the frame of the read. The cursor is the cursor of the game: the front end
draws it at the pointer in the picture, so the app draws no cursor on the
screen quad.

**The keyboard of the app** (`screen_keyboard=1` of the app). While a box of
the game takes text (its control with the focus is a text field), the mod
sets the flag `kScreenText` in the `kScreen` of the frame (`screen_keys=1`),
and the app shows its keyboard on a quad under the screen quad
(`keyboard_layout.h`): the digits, the lower case letters, Space, Back and
Enter, in the look of the menu of the app. The quad is 0.6 of the width of
the screen quad, its top is 5 cm under the bottom of the screen quad, and it
turns about its x axis so that its front faces the origin of the LOCAL space.
The hand that pulled its trigger last points, as on the menu, the key under
the ray is lit (a short pulse at each new key), the cursor of the app shows
at the hit (with the turn of the keyboard, 4 mm in front of it along its
normal), and the trigger types the key. The key goes to the mod through
the ring of the keys of the input channel ("The input channel"), and the mod
posts it to the game window as `WM_CHAR`, as Windows gives the game a key of
a keyboard. The mod takes the trigger only on the screen quad, so a press on
the keyboard clicks nothing in the game. `screen_keyboard=0`: no keyboard.

**A dialog box in a frame of the scene** (`panel_pointer=1` and
`panel_keys=1` of the mod, `panel_keyboard=1` of the app, `controllers.md`,
section 13). The start dialog of a new game after the Lionhead logo, the
Main Menu over a saved game, the Esc menu, a load and a save come in frames
of the scene. They draw on the panel image, and the app shows them on the
menu panel quad of the `kPanel` of the frame ("The quad layers"). The mod
reads the hands in each frame of the scene. While a box is up, the aim ray
of the hand that pulled its trigger last meets the panel quads of the last
`kPanel`, the nearest hit wins, and the hit is the pointer of the game as a
pixel of the panel image, which has the size of the render. The trigger is
the left button. A press starts only with the ray on a panel quad. When the
ray leaves the panel quads, the pointer is the pointer of the eye view
again. While the box takes text, the mod sets `kPanelQuadText` on the menu
panel quad (a bit of `PanelQuadBody::visible` beside `kPanelQuadShows`, so
`kPanel` keeps its 176 bytes), and the app shows its keyboard in front of the
lower part of that quad (`QuadOnPanel` of `keyboard_layout.h`): half as wide
as the quad, its top 0.35 of the height of the quad under its center, 3 cm
in front of it toward the head, with the turn of the quad. The menu panel
quad is about 80 by 60 degrees (the menu rect at 16 pixels a degree), and the
box is a small part of it in its middle, so the keyboard is not under the
whole quad. The mod knows that quad too, and a ray on the keyboard is no hit
on the panel. `panel_keyboard=0`: no keyboard in a frame of the scene.
`panel_keyboard` of the app and `panel_keys` of the mod go together: with
`panel_keys=1` and `panel_keyboard=0` the mod stops the ray at a keyboard
that the app does not show. Set both to 1 or both to 0.

The user chose a keyboard of the app over `XR_META_virtual_keyboard`, so it
works on each runtime, the Quest 3 and the Steam Frame alike.

**The log.** The app: `keyboard     : screen_keyboard=1 ...` at the start,
one line when the keyboard shows (its size, its place and its turn) and when
it goes (the count of the keys), each key that goes to the mod, the time of
the first paints, and `xr           : N frames with the keyboard` at the end.
`keyboard     : panel_keyboard=1 ...` at the start, and the line of the
keyboard says whether it shows under the screen quad or at the menu panel
quad. The mod: `controllers  :` and `vk stream    :` lines of
`controllers.md`, 12.4, and `controllers  : panel quads:` and
`xr layers    :` lines of 13.4.

**The tests.**

- `.\dev.ps1 build`: the checks of `screen_pointer_checks.h`: the ray
  against a quad, the hand that points, the press on the quad, the layout of
  the keyboard (39 keys, none over another, the middle of each key gives
  that key), its quad under the screen quad and facing the head, the ray at
  a key, and the ring of the keys (the order, a full ring, the keys of an
  earlier run, indices that are not valid, the layout of the file). The
  checks of `panel_pointer_checks.h`: the ray against the panel quads (the
  pixel of a hit, the nearest quad, a quad that does not show), the
  keyboard at the menu panel quad and that it stops the ray, the surface of
  the pointer, and the text flag of `kPanel`.
- `screen_ray_run.ps1` (a desk run, `controllers.md`, 12.5): a click of the
  ray of the test writer on the start dialog over the Lionhead logo.
- `panel_ray_run.ps1` (a desk run, `controllers.md`, 13.5): the clicks of the
  ray of the test writer on a row and on OK of the start dialog on the menu
  panel quad in a frame of the scene.
- `emu_game_test.ps1 -NewProfile` (the emulator, the test runtime, the
  copy of the game folder with no `Profiles`): when the log of the mod
  names `NewProfileBox` and the `kScreen` asks for the keyboard, the
  runtime points at the keys of `bwvrtest` and at Enter, and pulls the
  trigger on each (`point <hand> key <word>` of the runtime). With
  `-ProfileOk x,y` it then clicks OK of the dialog on the screen quad in
  place of Enter. With `-SkipClick x,y` it then clicks the start dialog on
  the screen quad. With `-PanelClicks x,y,...` it then clicks each pixel of
  the start dialog on the menu panel quad in the frames of the scene (`point
  <hand> panel x y` of the runtime: the first quad with the source alpha,
  and the pixels of the panel image). With `-TestSave <name>` the test puts
  the test save into slot 20 of the new profile before game 2, and with
  `-MenuClick x,y` the runtime clicks that pixel of the Main Menu of game 2
  on the menu panel quad. With `-SaveName <name>` game 2 then goes into the
  Save Game Room of the Temple by the ring (`move <hand> dx dy dz` and the
  button Y of the runtime), the runtime clicks a picture of an empty slot two
  times along the ray of the eye (`pointfile <hand> <file>` with `view x y`),
  the keyboard of the app at the menu panel quad types the name and Enter,
  and the ray clicks OK of the box.

**What the emulator shows** (`-NewProfile -ProfileOk 480,670 -SceneEyes
-SkipClick 547,575 -TestSave pen -PlaySeconds 60`, a render of 1380 by
1104, 137 checks, 0 failed, and the same run with Enter in place of OK, 135
checks, 0 failed):

- The dialog of a new profile came at FinishFrame 3066, after the logos and
  the video before the intro. The `kScreen` asked for the keyboard, and the
  app showed it, 1.56 by 0.62 m at 0, -1.40, -2 m, turned 35 degrees to the
  head. A paint of the keyboard takes 2.7 to 5 ms.
- The game draws the dialog and its cursor in the picture of the screen
  quad, and the cursor is at the hit of the ray.
- The keyboard gave `b w v r t e s t` to the mod (8 keys, one for each pull
  of the trigger, about 0.3 s apart), the mod posted each one as `WM_CHAR`,
  and the name is in the field of the dialog in the picture. Each pull of
  the trigger on the keyboard was "a press with the ray off the quad" for
  the mod: no click in the game.
- The ray pressed at the pixel 480 670 (OK of the dialog), the left button
  of the game went down and up, and the dialog went ("setup box : no dialog
  box is up now"): a control of the box took the click of the ray. In the
  run with Enter the dialog went after the key Enter of the keyboard.
- The game made the folder `Profiles\_b_w_v_r_t_e_s_t` in the game folder
  of the app, and `user.reg` of the prefix has the value `Profile` after
  the game.
- The start dialog of a new game (`SkipBox`) came over the Lionhead logo.
  The ray pressed at the pixel 547 575, the left button of the game went
  down, the logo ended, and the frames of the scene came 166 to 182 s after
  Play (most of it the video before the intro and the typing), with the
  start dialog over the land. The runtime read back the eye pictures of the
  projection layer with the land in them.
- Game 2 ("Play in VR") continued the test save "pen" in slot 20 of the
  profile: the Main Menu of the game over the land of the save, the frames
  of the scene 57 s after Play, and the eye pictures of the land.
- The frame rates and the pace: see "The pace of the frames". The
  processes of Wine took 97 percent of one CPU in game 1 (the scene with
  the start dialog) and 68 percent in game 2.

**What the emulator shows in a frame of the scene** (the same run with
`-PanelClicks 547,532,543,665 -MenuClick 690,430`, 146 checks, 0 failed):

- Game 1: the start dialog stayed over the frames of the scene after the
  click on the screen quad, on the menu panel quad, 2.35 by 1.76 m at 0,
  -0.24, -1.38 m with the rect 50 72 1280 960 of the panel image of 1380 by
  1104. The picture of the panel quad that the runtime read back has the
  dialog. The ray pressed at the pixel 547 532 (the row "Skip straight to
  the Creature select") and 543 665 (OK) of the panel image, the left
  button of the game went down after each press, and the dialog went at
  frame 987 of the scene. Then the game played a cut scene: the runtime had
  the two quads of the cut scene window.
- Game 2: the Main Menu over the land of the test save "pen" on the menu
  panel quad. The ray pressed at the pixel 690 430 ("Continue Game"), the
  left button of the game went down, and the Main Menu went at frame 676 of
  the scene. The eye pictures of the projection layer after the measure have
  the land.
- Game 2 with `-SaveName vrsave`: the ring of the left hand went into the
  Temple and to the Save Game Room, and two clicks of the ray on a picture of
  an empty slot opened the box "Save Game Message" on the menu panel quad.
  The mod set `kPanelQuadText`, and the app showed its keyboard at the menu
  panel quad, 1.17 by 0.47 m at 0, -1.08, -1.20 m, turned 10 degrees. The keys
  `v r s a v e` and Enter went to the game as `WM_CHAR`, and no pull of the
  trigger on the keyboard was a press on the panel. The ray pressed OK at the
  pixel 591 627, 212 pixels of the panel image above the top of the keyboard,
  the box went, and the game wrote slot 0 of the test profile in the
  emulator. 156 checks, 0 failed.

**Not shown yet:** the dialog boxes in a headset (the aim at the screen
quad, at the panel quads and at the keyboard, the size of the cursor of the
game, the keyboard at the menu panel quad), and a double click in a list of
the front end.

**Keys.** `screen_pointer`, `screen_keys`, `panel_pointer` and `panel_keys`
of the mod and `screen_keyboard` and `panel_keyboard` of the app have 1 as
the default, the value of a run, so "Play" does not write them. None has a
test key: the mod with `xr_input=1` has no hotkeys, and the app reads its
keys at its start.

## No Vulkan device of the mod

In the headset the game runs in Wine, and the app draws the frames of the
live stream on the device of its session. A Vulkan device of the mod in the
process of Wine would draw each frame a second time on the same GPU,
through the Vulkan of Wine. So the mod has the key `vk_device` in
`bwvr.ini`:

- `vk_device=1` (the default): the renderer of the mod draws on a Vulkan
  device of its own, as on the PC.
- `vk_device=0`: the mod makes no Vulkan instance and no device, and it does
  not load `vulkan-1.dll`. The calls of each walk still go into the stream,
  and a walk keeps only what the panel quads need (`VkScenePanelDrew`). The
  window of the game gets no picture and no GDI, and the present does not
  wait. It needs `backend=vulkan` and `xr_input=1`. With another setting the
  log says why, and the mod keeps `vk_device=1`.

The key is not a row of `settings_table.h` and not a retired key, so a save
of the launcher and of the menu keeps it as it is (the math test checks the
save of the menu). "Play" writes `vk_device=0` ("Play in the app").
`backend.md`, "No Vulkan device of the mod", has what each
reader of a picture of the renderer gets with `vk_device=0`.

**The stream is the same.** `vk_device` is not an option of the stream, and
the panel quads (`kPanel`) and the binds of the panel (`kPanelBind`) come
from the same tests as with a device. In desk runs of `input_channel_run.ps1
-Kind channel -Live` with the same input (`-RingFrom`, `-NoiseFrom`), each
record of the run with 0 has the bytes of the run with 1, but for 28 draws
of each frame. Those draws differ between two runs with 1 too: their
vertices follow the clock of the PC (`backend.md`). The ids of the surfaces
are addresses of the process, so `bwvr_stream_reader run-compare` takes the
ids of one run as the ids of the other, one to one.

**In the emulator** (`emu_xr_test.ps1 -Live <the run> -Gpu host`): the
capture of the run with 0 passes 68 checks, and the capture of the run with
1 passes 74 (the 6 more checks compare with the pictures of the PC, and a
run with 0 has none). The pictures of frame 2996 that the app drew from the
two captures differ by at most 0.003 percent of their pixels by more than
24, the noise of two desk runs.

**Known limits:**

- The subtitle card of `ui_panel=0` has no pixels with `vk_device=0`: the
  mod reads them from its own panel image. The subtitles and the help text
  then draw in the eye pictures at a fixed depth, so a frame with help text
  differs from the stream of `vk_device=1`. For the card the mod must give
  the rect of the panel in place of the pixels, and the app must cut the
  card from its own panel image. Not made yet. With `ui_panel=1` the help
  text is in the panel quads, which the app draws.
- A draw for the panel that the device of the mod would not take (no
  pipeline, no descriptor set) counts as drawn into the panel with
  `vk_device=0`.
- With `vk_device=0` and `vk_pace=0` nothing paces the frames of the game:
  the mod waits for no GPU and for no present, so the game makes frames as
  fast as the CPU can, faster than the app shows them. The live stream then
  has more frames than the app plays, and the app skips the walks of the
  others. `vk_pace=1` ("Play" writes it) paces them ("The pace of the
  frames").

In Wine in the emulator (`emu_game_test.ps1`, "Play in the app") the mod
logs `vk_device=0` and makes no Vulkan instance and no device. Wine has no
`vulkan-1.dll`.

The frames of the scene of the game in Wine come with `vk_device=0` in the
emulator ("The dialog boxes by the controllers"): the projection layer has
the eye pictures of the game. The screen frames of the logos, the video and
the dialog come at 25 to 711 frames a second with `vk_pace=0`, and at 25 to
68 with `vk_pace=1` ("The pace of the frames").

## The pace of the frames

With `vk_device=0` the mod waits for no GPU and for no present, so the game
makes frames as fast as the CPU can. The app shows at most one frame of the
game for each frame of its session, and the frames that it does not show
cost CPU time that Box64 and the app need, and heat and battery in a
headset. So the mod has the key `vk_pace` in `bwvr.ini`:

- `vk_pace=0` (the default): no wait. The app skips the walks of the frames
  that it does not show.
- `vk_pace=1` (only with `vk_device=0` and `xr_input=1`): each frame of the
  game waits for the next frame of the session of the app. "Play" writes it
  ("Play in the app"). With `vk_device=1` the log says that `vk_pace=1`
  needs `vk_device=0`, and no frame waits.

The key is not a row of `settings_table.h` and not a retired key, so a save
keeps it. It has no test key: the mod with `xr_input=1` has no hotkeys in
the headset, and the headset is tested through its `bwvr.ini`.

**The rule** (`src/mod/frame_pace_math.h`, `XrInputPace` of
`xr_input.cpp`). The app writes the slot of the input channel one time for
each frame of its session, after `xrWaitFrame` ("The input channel"). The
wait of a frame ends at the first write of the slot that no frame took, or
at its time limit. The mod counts the writes by the sequence of the slot
(two more for each write), and a frame takes the newest write at the end of
its wait and at its latch. The time limit counts from the end of the wait
before:

- A fresh slot (a write in the last 4 display periods, with a display period
  of 1 to 100 ms): 5/4 of the display period of the slot. So a frame of the
  session that comes a little late still ends the wait, and a frame that the
  session does not make costs the game at most one period.
- No slot yet, a slot with no write for 4 periods, or no display period (a
  record of a fake run has 0): one period of 72 Hz, the lowest rate of a
  Quest 3. With no write of the app no frame of the session shows the game,
  but the game must go on: its logic, its sound, the stale rule of the
  input channel, and the next start of the session. The next write of the
  app ends the wait.

A frame that is longer than the limit does not wait. The wait sleeps with a
timer of high resolution (`ThreadWaitSleep`, `Sleep(1)` when there is
none): with a fresh slot one sleep to 1 ms before the write that the period
gives and then steps of 0.5 ms, else steps of 1 ms. After each sleep it
looks at the sequence of the slot, and it reads the display period from a
whole copy of each new write.

**The clock.** The mod does not compare the display time of a slot with its
own clock. The display time is the `XrTime` of the runtime of the headset,
in ns. Wine gives `QueryPerformanceCounter` from `CLOCK_BOOTTIME`
(`dlls/ntdll/unix/sync.c`, `monotonic_counter`), which counts the time that
a headset slept, and `CLOCK_MONOTONIC` does not. The test writer of the PC
writes the display times of a record. So the mod paces by the count of the
writes and takes only the display period from the slot. The times of the
rule are of the clock of the mod alone.

**Where the wait is.** One wait for each FinishFrame of the game: the
first call after a FinishFrame waits, and a second call before the next
FinishFrame does not.

| The frame | The place of the wait | What the game does after it |
| --- | --- | --- |
| A frame of the scene | The start of `Process3dEngine` (`stereo.cpp`), before the first read of the input channel of the frame, where a session of the PC waits in `xrWaitFrame` | The latch reads the slot that ended the wait, then the frame of the controllers, the eye passes with FinishFrame, the layers into the live stream, and after the hook the logic of the game with the input of that slot |
| A frame of the intro (`intro_vr=1`) | Before the read of the input channel of the intro (`intro_vr.cpp`) | The eye passes of the intro |
| A frame with no scene (the logos, the videos, the front end, a dialog box, a load) | The end of `h_FinishFrame` (`mod_main.cpp`), after its screen frame went into the live stream | The logic of the game, the walk of the next screen, and its FinishFrame. On the PC with a session these frames wait in `xrWaitFrame` of `XrScreen::SubmitVulkan` |

So the wait holds no input of the scene: in a frame of the scene and of the
intro the read of the input channel that latches the input of the frame
comes after the wait, and the frame of the controllers reads the slot at
its own place in the frame. In a frame with no scene, and in the
FinishFrame of an eye pass, the intro skip reads the hands before the wait
of the next frame. With the pace on that read takes the newest slot only
(`ReadKind` of `frame_pace_math.h`): it does not ask the writer for the
slot of a frame of the controllers, it takes no write for the pace, and it
leaves the latch of the frame to the first read after the wait. So a write
that came in the work of the frame ends the wait at once, and the eye
passes of the intro and the logic of the game use the slot that ended the
wait. The log of each 450 frames of the scene has the count of these reads
and of the latches of a frame after them, and one line names the first such
latch. The present of `vk_ddraw.cpp` does not wait: it runs in FinishFrame,
inside the eye pass of a frame of the scene, before the layers of the frame
go into the live stream. A wait there would hold the frame from the app,
and the logic of the game after it would use input that is one wait older.
The wait of a frame of the scene is the "wait" of the frame time in the
line `stereo` of the log.

**The log.** At the start: "xr input : vk_pace=1 with vk_device=0. Each
frame of the game waits for the next write of the slot of the app, ...".
Each 30 s: "xr input : the pace: N frames in S s (R a second), A started at
a new write of the slot and B at the time limit, C with no fresh slot. A
frame waited ... ms as a mean and ... ms at most. The display period of the
slot is ... ms".

**The tests.**

- `.\dev.ps1 build`: the checks of `frame_pace_checks.h` (the limit with no
  slot, with a fresh slot, after a frame that the app missed, with a slot
  that does not change and with a period of 0, the end at a new write, the
  write that a latch took, the lockstep of the test writer, the clock of the
  mod).
- `input_channel_run.ps1 -Kind channel -Live -WriterHz <hz>` (a desk run):
  the test writer writes the last slot again at `<hz>`, as the app writes a
  slot at each frame of its session, and still answers each latch of the
  lockstep at once. The script gives the frames a second and the cores that
  `runblack.exe` used in the windows of 450 frames, and the lines of the
  pace. `screens_run.ps1 -WriterHz <hz>` does the same in a new game with
  the logos (with `xr_input_lockstep_ms` in `-More`, else the writer
  answers each read of a frame with a new write, and no frame waits).
- `emu_game_test.ps1`: `vk_pace=1` from "Play", the lines of the pace and
  the rate of the paced frames in the game in Wine.

**What the desk runs show** (`input_channel_run.ps1 -Kind channel -Live
-WriterHz 90`, the test save "pen", `vk_device=0`, `ui_panel=1`, the input
of a fake run). The test reader asks for the first frame of the ring with a
number from its attach frame up, and a screen frame has a number above each
frame of the scene, so these runs have `vk_stream_screens=0`.

- `vk_pace=1`: 90.0 frames a second in each window of 450 frames after the
  load, and `runblack.exe` used 0.08 cores. In a window of 30 s each of the
  2701 frames started at a new write of the slot, after a wait of 10.2 ms
  as a mean.
- `vk_pace=0`: 1398 frames a second as a mean (1160 to 1465), and
  `runblack.exe` used 0.97 cores.
- In both runs the record of the input (3100 frames), the views (3100) and
  the pulses (frames 1801 and 2601) are the same as in the fake run, and
  the 4 frames of the record of the stream have the same bytes in the ring.
  The quad layers differ from the fake run only in the hint cards, which
  follow the clock of the PC, with `vk_pace=0` too.

**In the emulator** (`emu_game_test.ps1 -StartMinutes 15 -PlaySeconds 360`,
Wine of x86_64, the test runtime at 72 Hz, "Play" with `vk_pace=1`): the
slots have a display period of 13.89 ms. In the logos 1042 and 1049 of the
frames of the first 30 s started at a new write of the slot (2 and 0 at the
time limit), at 34.8 and 34.9 frames a second, after a wait of 6.3 and 6.4
ms as a mean. In the video before the intro the game makes 25 frames a
second by itself: a frame of 40 ms is longer than a period, so each frame
starts at a write that came in its work, with no wait. In the dialog of a
new profile each frame started at a new write, after a wait of 11.6 to 12.0
ms as a mean, and the game made 68.0 to 68.4 frames a second, the rate of
the frames of the session of the app. The processes of Wine took 24 percent
of one CPU in 361 s in each game, with the sound (`runblack.exe` 19
percent, the app 14 and 15 percent). With `vk_pace=0` the same screens come
at 368 to 711 frames a second, and the processes of Wine take three
quarters of one CPU. The first frame came 13.4 and 12.4 s after "Play", the
logo screens took 479 and 484 frames and the video 2515 frames, and the
dialog of a new profile came at FinishFrame 3068 and 3072.

**In the scene in the emulator** (`emu_game_test.ps1 -NewProfile`, the
test runtime at 72 Hz): with the start dialog of a new game over the land,
the frame of the scene takes 22 ms (the two eye passes 18 ms on the CPU of
the emulator), and the game makes 41 to 55 frames a second with no wait. In
the test save "pen" (the Main Menu of the game over the land) a frame takes
15 ms with a wait of 5.7 to 7.7 ms, and the game makes 66 to 68 frames a
second: in one window of 30 s 1549 of 1550 frames started at a new write of
the slot. While the menu of the app shows over the game, most frames start
at the time limit with no fresh slot.

**Not shown yet:** the pace in a headset (the timers of Wine under Box64,
the CPU load and the heat of a Quest 3). Other rates than 72 Hz: see "The
display rate".

**Known limits:**

- A record of a fake run has no display period, so in a desk run with the
  test writer the time limit is one period of 72 Hz. A writer above 72 Hz
  shows the pace by the writes, a writer below it does not.
- No event of the app wakes the wait: it looks at the slot after each
  sleep, so a frame starts up to 0.5 ms after the write (1 ms with no fresh
  slot), plus the lateness of the timer.
- The layer compare of `input_channel_run.ps1` fails when the frame rate of
  the channel run differs from the frame rate of the fake run, with
  `vk_pace=1` and with `vk_pace=0`: the hint cards follow the clock of the
  PC, so they show at other frames.

## The long frames

The time log gives the mean and the most of the parts of a frame each 10 s
("The time log of the app", `backend.md`, "The time log of the mod"). A
frame of the game that takes 25 to 38 ms in a period of 13.89 ms shows an
old picture again in the headset. The key `slow_frame_ms` gives one line for
each such frame, in the mod and in the app, so a log says where the time of
each long frame went. It is a measurement and changes no behavior of the
game or of the picture.

**The keys** (`bwvr.ini`, not rows of `settings_table.h`, so a save keeps
them, and no test key):

- `slow_frame_ms=0` (the default): no line, no more clock read, no read of
  `/proc`.
- `slow_frame_ms=N` (1 to 1000): each frame of the game longer than N ms
  gives one line, at most 5 lines in a second. 20 fits a display of 72 Hz.
  The lines of the mod need `time_log=1` (the default). With `time_log=0`
  the mod logs "slow_frame_ms=N needs time_log=1" and gives no line.
- `slow_frame_cpu=1` (0 by default, only with `slow_frame_ms` above 0): the
  lines of the mod have the CPU time of the frame thread, from one call of
  `GetThreadTimes` at each present. In Wine that call goes to the
  wineserver and reads `/proc`, on the frame thread, in the scene walk. The
  lines `game frame` of the app give the same time in ns with no call on
  the thread of the game, so the call is off by default.
- `profile_sample_slow=N` (100 to 5000, 0 by default, a test aid, only with
  `slow_frame_ms` above 0): the sampler of `profile_sample` (`backend.md`,
  "Where the time of the scene walk goes") takes a sample each N
  microseconds in the whole frame, and its report of each 450 frames of the
  scene has only the samples of the long frames (at most the newest 8192
  samples of one frame). The hold of the frame thread in that report is the
  mean of all frames. The sampler stops the frame thread for each sample.
  Under FEX that can hang the game ("FEX as the CPU of the 32 bit code",
  "SuspendThread under FEX in Wine"), and under FEX and Box64 a sample of a
  thread that runs translated code has the address of its last system call
  ("The hot code under Box64"). For FEX this comes from its source:
  `BTCpuGetContext` of `Module.cpp` reads the context of the CPU area
  (`RtlWow64GetThreadContext`), and FEX writes that context only when the
  thread leaves its JIT (a system call, an exception). So on a headset the
  samples name only the waits in system calls.

**The lines wait.** Each line of one frame waits in memory until its window
of 10 s ends (at most 64 lines in a window, the others are lost and the
count line says how many lines it has). Then the lines go to the log one at
each frame: in the mod one at each present, in the reader of the app one
after each frame that went into its queue, in the session one at each frame
of the session. So the log writes at most one such line on a thread in a
frame, and not in the frame just after a long frame. A write of the log
(with `fflush`) can make a frame long, so each line of a long frame of the
mod says how many lines this log wrote in the frame. The lines of the last
window before the end of the game do not go to the log.

**The lines of the mod** (`time_log.cpp`, on the frame thread):

```
long frame   : present P, the frame of the stream S, at T s, G ms after the long frame before. It took F ms: the scene walk A, the write of the stream B, the wait of the pace C, the rest D. The calls of the frame loop: the input I, the game turn U, ProcessGraphicsEngine E, FlipScreen L, the network flush N, outside them O. The CPU time of the frame thread: X ms. In the frame: K opens of a file of the game (W to write), R reads of the game, M surfaces with pixels into the stream (B KB), J lines of the time log
long frame   : L frames longer than N ms in 10.0 s, K of them with a line. They took F ms. The CPU time of the frame thread in them: X ms (GetThreadTimes: the least step of the clock is S ms). Since the line of the window before, the writes of that line and of the lines of the long frames took W ms. The lines of this window go to the log now, one at each present
```

- A frame goes from one present to the next, as in the 10 s line of the
  time log. Its parts (the scene walk, the write of the stream, the wait of
  the pace, the rest) are the parts of that line for this frame alone.
- The frame of the stream S is the frame of the live stream that this
  present belongs to. At the present it is still open: it goes to the ring
  after the present, at the end of the walk with both eyes
  (`vk_stream_live_end=1` with no layers), at the layers of the frame
  (`xr_input_layers=1`), or at the end of a screen frame. So the app gets S
  as the frame whose gap is the time of this frame. With no live stream S
  is 0.
- The calls of the frame loop are the calls of `GGame::Loop` that the mod
  times already (`frame_timers.cpp`, `pass_guard.cpp`): the input
  (`ProcessActionsPerformed`, `ProcessBufferedKeys`, `ProcessFrameInputs`),
  the game turn (`GGame::ProcessNetworkPackets`, which runs a turn each 100
  ms of game time), `GGame::ProcessGraphicsEngine`, `GGame::FlipScreen` and
  the network flush (`0x005525E0`). `ProcessGraphicsEngine` holds the scene
  walk, the write of the stream and the wait of the pace of a frame of the
  scene, and the work of the mod in the stereo hook. A present in a call
  splits it: each frame gets its part. "Outside them" is the frame less
  these calls: the rest of the loop and the time between two turns of it.
- The CPU time of the frame thread (`slow_frame_cpu=1`) is the step of
  `GetThreadTimes` (kernel and user) from the present before. "Not read
  (slow_frame_cpu=0)" with the key at 0, and "not known" before the first
  read. The clock has steps: 15.625 ms on Windows, and 10 ms in Wine, which
  reads `/proc/<pid>/task/<tid>/stat` of Linux in clock ticks. The count
  line gives the least step that the clock made in the run. With steps of
  10 ms one frame says only "about 0", "about 10" or "about 20", but the
  sum of the long frames of 10 s says which share of their time the thread
  had a core.
- The counts of the frame: the calls of `LHFile::Open` (all modes, of them
  the opens to write: a save, the mind of the Creature), the reads of the
  game from files that LHFile opened to read (`file_read_ahead.cpp`, 0
  with `file_read_ahead=0` and `test_keys=0`), the surfaces whose pixels
  went into the live stream in the frame (a new texture or a texture that
  the game wrote, `vk_stream.cpp`), and the lines that `time_log.cpp` wrote
  since the present before.
- `G ms after the long frame before` gives the rhythm: a steady G (for
  example near 1000 ms) is a work of the game on a timer, a G of one frame
  is a burst, and long frames that come with opens of files or new surfaces
  in new places are loads.
- The line `time log` stays as with `slow_frame_ms=0`. The count line comes
  just after it.

**The lines of the app** (`long_frames.cpp`):

```
game frame   : the frame F of the game came G ms after the frame before. In that time the main thread of the game ran R ms on a core, waited Q ms for a core, and was off a core O ms (K time slices)
game frame   : 10.0 s, N gaps between frames of the game, ms: the mean A and the most M. In T ms of them with schedstat the main thread of the game ran R ms on a core and waited Q ms for a core. L gaps were longer than X ms (K with a line): G ms, and in T2 ms of them with schedstat the main thread ran R2 ms and waited Q2 ms for a core
late picture : N frames of the session had no new picture before the frame F of the game: <cause>. It came L ms after the first take that missed it, and its copies ended R ms after that take. Its frame of the game came G ms after the frame before
late picture : 10.0 s, T takes of the session, M of them with no new picture: a by the GPU, b by the play of the app, c by a long frame of the game (the gap A ms as a mean, B at most), d by a frame of normal length. Those frames of normal length came after the take: e up to 1 ms, f up to 2 ms, g up to 4 ms, h more (C ms as a mean, D at most). The pictures in time ended their copies E ms before the take as a mean, F at the least (P pictures)
```

- `game frame` comes from the reader of the live stream: at each frame of
  the game that it takes from the ring (within 0.5 ms of the write of the
  mod) it reads `/proc/<pid>/task/<pid>/schedstat` of the main thread of
  the game: the time on a core and the time in the queue for a core, in ns.
  The gap is the time from the frame before. So "ran" close to the gap says
  that the thread worked, "waited for a core" says that the thread could
  run and had no core, and "off a core" says that it slept or waited for an
  event (a lock, a file, a timer, the wait of the pace). The main thread is
  the thread with the id of the process. With `vk_device=0` it runs the
  frame loop of the game. The app finds the process with `time_log=1`, so
  with `time_log=0` the lines have no time of the main thread. At each Play
  the reader starts a new window, and the first frame of the new game has
  no gap.
- `late picture` comes from the thread of the session with `frame_loop=1`:
  at each take of a picture (`TakePicture`, before `EndFrame`). A take with
  no new picture is a miss. The next new picture gives the cause of the
  misses before it:
  - by the GPU: its copies went to the GPU before the first take that
    missed it, and the GPU had not finished them.
  - by the play of the app: its frame of the game came before that take,
    and the play of the app ended its copies after it.
  - by a long frame of the game: its frame came after the take, more than
    1.5 display periods after the frame of the game before it.
  - by a frame of normal length: its frame came after the take, and it was
    not long. The line says by how much it missed the take.
- The misses before a picture count in the window of that picture.
- The room of a shift of the phase of the pace (`frame_pace_math.h`): with
  `vk_pace=1` a frame of the game starts at the write of the slot of a
  frame of the session, so the time from the end of the frame of the game to
  the next take is fixed by the work of the frame. The pictures in time
  ended their copies E ms before their take. When most misses are frames of
  normal length that came up to 1 or 2 ms late, a start of the frame of the
  game 1 or 2 ms earlier in the period catches them, and costs the same ms
  of age of the input. When most misses are long frames, a shift of the
  phase does not help.

**Read the logs together.** For a long frame of the mod, take its frame of
the stream S. The line `game frame` with the frame S of the app gives the
time of the main thread on a core in the same gap, in ns, and the line `late
picture` with the frame S says if the session missed it. The app gives a
line only for its own long gaps, at most 5 a second, so not each long frame
of the mod has a line of the app. A long frame with a long "rest" or
"outside them" and a time on a core near its length is work on the frame
thread outside the calls of the game that the mod times: Wine, the mod, or
the translation of new code by FEX. A long frame with much time off a core
is a wait.

**The phase of the take** (`src/mod/take_phase_math.h`, `TakePhase` of
`long_frames.cpp`, the loop of the session in `main.cpp`). With
`frame_loop=1` the session takes the newest finished picture of the ring one
time in each of its frames, just after the write of the slot of the input
channel. With `vk_pace=1` a frame of the game starts at the write of a slot,
and its picture must be finished before the take of the next frame of the
session. With `slow_frame_ms` above 0 or `time_log=1` the session gives one
line each 10 s that says where the time between the write and the next take
goes. It is a measurement and changes no behavior.

```
take phase   : 10.0 s. THE WRITE OF A SLOT TO THE START OF THE FRAME OF THE GAME that took it: ... THE START OF A FRAME OF THE GAME TO THE END OF THE GPU WORK OF ITS PICTURE: ... THE FRAME OF THE SESSION, F frames: ...
```

- The start of a frame of the game: the reader of the live stream reads the
  world of the input channel at each turn of its loop (about each 0.5 ms
  while the ring is empty, the line gives the turns a second). The mod
  writes the world at the start of each frame of the scene, after the wait
  of the pace and the latch of the slot. A new world is the start of a frame
  of the game, and its slot gives the write. The line splits the frames by
  the frame before: when its picture came to the app before the write, the
  game was free (at the end of that frame or in the wait of the pace), else
  it was still in the frame before.
- The start to the end of the GPU work: the frame and the way to the app
  (the start to the arrival in the app), the wait for the play (the arrival
  to the start of the play of its batch), the play (to the submit of the
  copies into the ring), and the GPU (the submit to the end of its fence).
  The play thread sees the end of the fence in its wait before the next play
  (`frame_loop_gpu_depth=1`). With another depth the line has no GPU time.
  Each part has the mean, the standard deviation and the most.
- The frame of the session: the end of `xrWaitFrame` to the take and the
  take to the end of `xrEndFrame` (frames with a take), the wait in
  `xrWaitFrame`, the rest of the display period after the end of
  `xrEndFrame`, the display time after the end of `xrEndFrame` (only with
  `XR_KHR_convert_timespec_time`), and the display periods that the runtime
  skipped: the display time of a frame moved more than 1.5 display periods
  after a frame of the session that ended in less than a period.

**What the phase of the take shows** (the test save `pen`,
`slow_frame_ms=20`, the windows of 10 s from the second window with frames
of the scene to the last whole window before the menu over the game). The
column of the Frame comes from the one session with this line so far, which
had a wait at the take of up to 4 ms in the session (1.6 to 2.4 ms as a
mean). The app has no wait at the take.

| | Emulator, 72 Hz | Emulator, 90 Hz | Steam Frame, 72 Hz, a session with a wait at the take |
| --- | --- | --- | --- |
| The write to the start, ms (mean, deviation, most) | 2.38 to 2.52, 0.95 to 1.32, 5.4 to 12.7 | 2.46 to 2.60, 0.91 to 1.09, 5.4 to 7.0 | 0.99 to 1.19, 0.74 to 1.06, 8.9 to 12.9 |
| The start to the end of the GPU work, ms (mean, deviation) | 7.00 to 8.14, 0.66 to 3.18 | 6.99 to 7.62, 0.67 to 2.19 | 13.1 to 14.8, 2.4 to 5.1 |
| Of it the frame and the way to the app, the play, the GPU, ms | 4.3 to 5.1, 2.2 to 2.4, 0.4 to 0.5 | 4.3 to 4.7, 2.2 to 2.3, 0.4 | 7.8 to 9.0, 1.5 to 1.7, 3.7 to 4.7 |
| The end of `xrWaitFrame` to the take, ms (mean) | 0.10 to 0.13 | 0.10 to 0.11 | 1.1 to 1.3 in the windows with no wait |
| The take to the end of `xrEndFrame`, ms (mean) | 1.0 to 1.1 | 1.0 | 0.26 to 0.31 |
| The rest of the period after `xrEndFrame`, ms (mean, least) | 12.7 to 12.8, 8.6 | 10.0 to 10.1, 2.5 | 11.5 to 12.3, 6.7 |
| Takes with no new picture in 10 s | 0 to 28 of 684 | 13 to 46 of 855 | 42 to 123 of 721 |

- On the Frame the frame of the game starts about 1 ms after the write (the
  reader sees a new world each 0.6 ms). So the pace does not start the game
  late there. But a picture needs 13 to 15 ms from that start to the end of
  its GPU work, with a deviation of 2.4 to 5.1 ms, and the next take comes
  about 15 ms after the write. So the end of the pictures falls on the take,
  and the pictures are in time and late by turns. The frame of the game and
  the GPU vary most.
- In the emulator the start comes 2.4 to 2.6 ms after the write, and a
  picture needs about 7 ms. At 72 Hz one window has 28 takes with no new
  picture (a window with frames of the game longer than a period), the
  others have none. At 90 Hz 13 to 46 of 855 takes in 10 s have no new
  picture.
- With SteamVR the session has no room for more work between the end of
  `xrWaitFrame` and `xrEndFrame`. About 2 ms more of it in a frame makes the
  compositor of SteamVR time out presents ("Timed out" in
  `vrcompositor.txt`), and the player sees double pictures and a world that
  moves partly with the head. The line of the app sees no skipped display
  period then. A later take in the same frame of the session gains only the
  time of the controllers and of the menu, less than 0.2 ms on the Frame.

**The cost.** With `slow_frame_ms` above 0 the mod reads the clock two more
times for each timed call of the frame loop, and with `slow_frame_cpu=1` it
calls `GetThreadTimes` once a frame. The app reads the clock and
`schedstat` once for each frame of the game on the thread of the reader,
and the clock once for each take.

**What the desk runs show** (`desk_run.ps1`, the test save `pen`, 90 s,
dxwrapper, about 560 to 750 frames a second, `slow_frame_ms=10`):

- The load of the save is a row of frames that are long outside the calls
  of the frame loop, for example 382.50 ms with 6 opens of a file and
  339425 reads of the game, and 375.0 ms of CPU time (`slow_frame_cpu=1`).
  The first frames of the scene after the load have scene walks of 50 to 80
  ms.
- In the play a long frame is a scene walk of 10 to 16 ms, also with the
  stick of the scripted controller. Its CPU time is 0.0 ms with steps of
  15.625 ms.
- The frame rate in the windows of 10 s is the same with the key at 10 and
  at 0, within the noise between two runs.

**What the emulator shows** (`emu_game_test.ps1 -NewProfile -ProfileOk
480,670 -SceneEyes -SkipClick 547,575 -TestSave pen -PlaySeconds 90
-FrameLoop 1`, Wine of x86_64, the test runtime at 72 Hz,
`slow_frame_ms=20`):

- The frames of the start of a new game are near 20 ms, so nearly each
  window has 50 lines of long frames. In the test save `pen` the frames are
  paced, and most long frames are in the load and the screens before it.
- `GetThreadTimes` of the mod and `schedstat` of the app give the same time
  of the frame thread: 20.0 ms in a frame of the mod, and 19.78 to 20.50 ms
  in the lines `game frame` of the same frames. So the main thread of the
  process is the frame thread of the game in Wine.
- In the video before the intro (25 frames a second) the main thread is
  about 12 percent of the time on a core: it sleeps in the rest. In `pen`
  it is 15 to 25 percent of the time on a core: it waits in the pace.
- In the start of a new game (about 55 frames a second, with no wait of the
  pace) about 1 in 5 takes has no new picture. Most of these misses are by
  the play of the app. The pictures in time end their copies about 6 ms
  before the take as a mean.
- The scene walk of the start of a new game is 14 to 17 ms as a mean, with
  and with no `GetThreadTimes`: no cost of that call above the noise between
  runs.

**What the Steam Frame shows** (the SteamVR runtime at 72 Hz, `wow64_cpu=fex`,
`frame_loop=1`, `fovea_res_layer=0`, `slow_frame_ms=20`, a strafe with the
stick in a saved game):

- After the load, the game makes 71.2 to 71.6 frames a second, and 4 to 8
  frames in 10 s are longer than 20 ms. Each is 28 to 39 ms long. In nearly
  each one the game turn takes 19 to 33 ms, and the scene walk is 4 to 10
  ms as in a frame of normal length. The game opens no file in them, and
  reads from a file in only one of them. They
  come 0.2 to 5 s apart, with no steady rhythm.
- Each long frame of the mod after the load has a line `game frame` of the
  app at the same frame of the stream, with a gap within 2 ms of the length
  of the long frame. The pairs that do not match are in the load.
- 46 to 145 of 721 takes in 10 s have no new picture. The GPU had not
  finished the copies at the take in 31 to 76 of them, a frame of normal
  length came 7 to 9 ms after the take in 9 to 70, and a long frame of the
  game caused 2 to 10. A newer picture replaced about 1 picture in 8 before
  a take. So most takes with an old picture come from the phase of the
  frames of the game to the takes, not from the long frames.
- The pictures in time end their copies 5 to 15 ms before the take as a
  mean, 2.4 ms at the least.
- `schedstat` of the app gives no time of the main thread of the game.

**Known limits:**

- While the menu of the app shows over the game, and in a load, the app
  plays no picture for frames that come. A take after such a time can count
  as "a frame of normal length" that came seconds late.
- The clock of `GetThreadTimes` has steps of 10 ms in Wine and 15.625 ms on
  Windows. `schedstat` of the app has ns.
- The arrival of a frame in the app is up to 0.5 ms after its write (the
  reader sleeps 0.5 ms when the ring is empty). Two frames in one read of
  the reader have a gap of about 0.
- FEX gives no time of its translation in a frame. Its statistics
  (`FEX_PROFILESTATS`, `SHMStats`) need the Unix library of FEX, which is
  not in the tree.
- The mod counts no sound, no change of the camera, and no new object.
- The start of a frame of the game in the line `take phase` is the write of
  the world, after the wait of the pace and the latch of the slot, as the
  reader sees it at its next turn (0.6 to 1.1 ms apart). The line cannot
  split the lateness of the timer of the pace from the work before the
  write of the world.
- The line `take phase` counts a skipped display period only when the
  display time of the runtime moves more than 1.5 periods. A present that
  the compositor of SteamVR gets too late ("Timed out" in `vrcompositor.txt`)
  does not show there.
- Some runs of `emu_game_test.ps1` stop in game 1 after the dialog of a
  new profile, when the game loads the land: the log of the app ends about
  147 s after Play, the mod gets no new slot after that (all frames of the
  pace at the time limit), the frames of the scene start in the log of the
  mod, and the app takes about 125 percent of one CPU in the measure (42 to
  44 percent in a run that passes). Then the menu over the game and "Quit
  game" fail, and the test aborts before it gets the log of the app and the
  logcat ("StartIndex" in `WaitLog`). In the runs that pass, the play of
  one picture in this phase takes up to 9 s. The cause is not known.

## The display rate

A Quest 3 runs an app at 72 Hz when the app asks for no rate. The player
can choose the rate in the menu of the app: the row "Display rate" on the
page "Performance" (`main.cpp`, `TakeDisplayRates` and `ApplyDisplayRate`,
the rules in `display_rate.h`, the row in `menu_model.h`).

**The extension.** The instance has `XR_FB_display_refresh_rate` only when
the runtime lists it. After the session is made, the app reads the rates
of the runtime (`xrEnumerateDisplayRefreshRatesFB`) and the rate that it
uses (`xrGetDisplayRefreshRateFB`). A runtime with no such extension (for
example Lepton of the Steam Frame, not tried) gives no rates: the menu has
no row, the app asks for no rate, and the headset sets the rate.

**The row.** A list row, in the look of the other list rows: "Headset
default" and each of 72, 80, 90 and 120 Hz that the runtime lists (a rate
of the runtime within 0.5 Hz counts). The help text: a higher rate gives
smoother head motion, it costs more power and heat, and the game itself
can make fewer frames than the rate. The trigger and the stick step the
value, and the steps go round.

**The ini key.** `display_hz` in `bwvr.ini`: 0 (the default) is "Headset
default", and the app asks for no rate. A value that the row does not offer
counts as 0, with a line in the log. A save writes the value of the row,
and only when the row is there: with no row the key stays as it is.
`display_hz` is not a row of `settings_table.h`, so the launcher of the PC
does not show it, and a save of the launcher keeps it.

**The request.** The app asks the runtime for the rate of the row
(`xrRequestDisplayRefreshRateFB`, with the float of the list of the
runtime) when the session runs (its start), and at each change of the row,
with no new start of the app or of the game. A change back to "Headset
default" after a request asks for 0, "no preference" of the extension.
With `display_hz=0` from the start the app makes no request.

**The log.** One line for each step: the extension in the line of the
extensions, "the runtime gives 4 display rates: 72.00 80.00 90.00 120.00
Hz. It uses 72.00 Hz now", the rates of the row, the value of `display_hz`
or its fall back, each request with its result and the rate that the
runtime uses after it, each event `XrEventDataDisplayRefreshRateChangedFB`
("the display rate changed from 80.00 to 90.00 Hz"), and in the lines
`input` each change of the display period that the input channel writes.

**The pace at each rate.** The mod paces the game by the display period
of the slot of the input channel ("The pace of the frames"), and the app
writes the `predictedDisplayPeriod` of each frame of its session there.
So after a change of the rate the slot has the new period, and the mod
follows it with no change of the mod:

| Rate | Period | A frame of the game starts at | A frame that the app missed waits | A slot is fresh for |
| --- | --- | --- | --- | --- |
| 72 Hz | 13.89 ms | the next write of the slot, at most 72 a second | 17.36 ms | 55.6 ms |
| 80 Hz | 12.50 ms | the next write of the slot, at most 80 a second | 15.63 ms | 50.0 ms |
| 90 Hz | 11.11 ms | the next write of the slot, at most 90 a second | 13.89 ms | 44.4 ms |
| 120 Hz | 8.33 ms | the next write of the slot, at most 120 a second | 10.42 ms | 33.3 ms |

With no fresh slot the limit is one period of 72 Hz at each rate. A frame of
the game that takes longer than the period does not wait, so the game can
make fewer frames than the rate.

**The test runtime** lists the extension with 72, 80, 90 and 120 Hz and
starts at 72 Hz. A request counts from its next `xrWaitFrame`: the period
of its frames follows the rate, and the app gets the event. `rates.txt` in
the folder of the runtime gives other rates, or "none": then the runtime
does not list the extension.

**The tests.**

- `.\dev.ps1 build`: the checks of `display_rate_checks.h` (the rates of
  the row from the rates of the runtime, the fall back of `display_hz`, the
  steps, the row on the page "Performance", its look, its help, its save)
  and the checks of the pace at 72, 80, 90 and 120 Hz and after a change
  from 72 to 90 Hz (`frame_pace_checks.h`).
- `emu_xr_test.ps1` with no word (47 checks): the runtime lists the
  extension, the app reads its 4 rates, the row offers 72, 80, 90 and 120
  Hz, `display_hz=0` asks for no rate (no request), and the slot has the
  period of 72 Hz (13888888 ns).
- `emu_xr_test.ps1 -DisplayRate` (52 checks): the app starts with
  `display_hz=80` and asks for 80 Hz at frame 0 of the session. The trigger
  on the row at frame 100 gives 90 Hz, and the app asks for 90 Hz in the
  same frame (no other request). The runtime changes at frame 101, and the
  log of the app has both events (72 to 80 Hz, 80 to 90 Hz). The input
  channel has the period of 90 Hz from slot 102 ("the display period is
  11111111 ns now (90.00 Hz), it was 12500000 ns"), and the last slot has
  11111111 ns. The save writes `display_hz=90`. The picture of the page
  that the runtime read back has the row with "90 Hz" and its help.
- `emu_xr_test.ps1 -NoRateExtension` (47 checks): the runtime lists no
  extension, the instance does not have it, the app makes no call of the
  extension, the menu has no row, the slot has 13888888 ns, and the save
  keeps `display_hz=90`.
- `emu_game_test.ps1 -NewProfile -ProfileOk 480,670 -SceneEyes -SkipClick
  547,575 -TestSave pen -PlaySeconds 60 -DisplayHz 90` (143 checks): the
  app asks for 90 Hz at the start of the session, the first slot that the
  mod reads has 11.11 ms, and each line of the pace of both games has "The
  display period of the slot is 11.11 ms". The paced frames of the game
  came at 25 to 64 frames a second in game 1 (the logos, the video, the
  dialog of a new profile, the land) and at 56.8 to 71.0 in game 2 (the
  test save), all below 90. In the test save 1868 of 1870 frames of one
  window of 30 s started at a new write of the slot. Some windows have
  many frames at the time limit with no fresh slot (1514 of 1922 in one
  window of game 1), as the windows of a run at 72 Hz have (1283 of 1790).
  The emulator does not show the frame rate of a headset.

**Not shown yet:** a rate in a headset: the feel of the head motion, the
power and the heat of a Quest 3 at each rate, and the frames of the game
in Wine under Box64 at each rate.

**Known limits:**

- On the Steam Frame an APK runs at the rate of SteamVR for each app, 72 Hz
  by default (`/opt/steamvr/drivers/frame_hmd/resources/frame_hmd_additional.vrsettings`
  has "defaultPerAppRefreshRate": 72.0 and "enablePerAppFPS": true, and the
  key of an app is `steam.app.<id>`). `xrEnumerateDisplayRefreshRatesFB`
  gives only the current rate, and `xrRequestDisplayRefreshRateFB` with 90
  Hz returns `XR_ERROR_DISPLAY_REFRESH_RATE_UNSUPPORTED_FB`. So the row
  "Display rate" cannot change the rate there. The display has the modes 72,
  80, 90, 96, 108, 120 and 144 Hz. Another rate for the app is a setting of
  the device.

## The size of the eye pictures

The runtime gives the size of an eye that it asks for
(`recommendedImageRectWidth` and `recommendedImageRectHeight`). Each size of
the pictures of the game comes from that value: the app writes it as
`headset_width` and `headset_height`, the render of the game is
`resolution_pct` of its larger side (`Menu::RenderSize`, `ScaleToPixels` of
`settings_table.h`), and the swapchains of the projection layer, of the
panel image and of the screen quad have the size of the picture of the
renderer, which is the size of the render. A Quest 3 with the profile of
the player can ask for 4096 by 4291 (and at other starts for 1680 by
1760), with no value in `debug.oculus.textureWidth` and `textureHeight`.
The cause is not known. With no cap that size gives a render of 4096 by
4096 (the most of `ScaleToPixels`), two swapchains of 4096 by 4096 and a
panel image of 4096 by 4096.

**The cap** (`eye_size_max`, `eye_size.h`, `XrApp::MakeInstance`).
`eye_size_max=1` (the default): the app takes the largest size with the
aspect of the runtime that fits in the panel of a Quest 3, 2064 by 2208 for
each eye. A size that fits stays as it is, and the cap never makes a size
larger. 4096 by 4291 gives 2064 by 2162. Each headset gets this cap, not
only a Quest 3: the app does not read the size of the panel of the
headset. A headset with a panel larger than 2064 by 2208 gets a smaller
size than its panel, and needs `eye_size_max=0` or a higher
`resolution_pct`. `eye_size_max=0`: the size of the
runtime, with no cap. The size after the cap goes to `headset_width` and
`headset_height`, to the menu ("Resolution scale") and to the size of an
eye in the slot of the input channel. `resolution_pct` is a percentage of
the size after the cap: 100 percent of 2064 by 2162 is a render of 2702 by
2162 with one walk, 50 percent is 1350 by 1080. The key is not a row of
`settings_table.h` and not a retired key, so a save keeps it. There is no
test key: the app reads it at its start and has no hotkeys.

**No second cap.** The swapchains of the projection layer (`MakeEyes`), of
the panel image and of the screen quad have the size of the picture of the
renderer, and the stereo screen has half of it. The picture has the size of
the render of the game. So the cap of the size of the eyes caps each of
them, and a cap of a swapchain would only make a picture of the render
smaller. A render larger than the panel comes only from a `resolution_pct`
above 100.

**The log.** One line with the value of the runtime and the value after the
cap:

```
xr           : the headset asks for 4096 by 4291 for each eye (at most 4096 by 4096). eye_size_max=1: the app takes 2064 by 2162, the panel of a Quest 3 (2064 by 2208) with the aspect of the headset
```

**The values that can explain the size.** A Quest 3 with the same build and
the same runtime (Oculus 209.147.0) asked for 1680 by 1760 at four starts
and for 4096 by 4291 at one, each time with at most 8192 by 8192. The
cause is not known, so the app logs at its start:

- the system: the largest swapchain, the most layers, and the tracking,
- each view of the runtime: the size that it asks for, the largest size,
  and the samples,
- the view configurations of the runtime, and the properties of Android
  that a tool of the headset can set (`debug.oculus.textureWidth`,
  `textureHeight`, `refreshRate`, `foveation.level`, `gpuLevel`,
  `cpuLevel`, "(empty)" when not set),

and at the first picture of the projection layer, in the session, the size
that the runtime asks for then, if it is the size of the start, and the
display rate then. The cap does not change until a log shows the cause.

**The test runtime** asks for 2064 by 2208, or for the size in `eyes.txt`
of its folder (`emu_game_test.ps1 -EyeSize 4096,4291`). `emu_game_test.ps1`
checks the line of the log, `headset_width` and `headset_height` in
`bwvr.ini`, the render of each game and its registry, the swapchains of the
projection layer, of the panel image and of the screen quad, and that the
runtime made no swapchain larger than the render or the menu.

**What the emulator shows** (`emu_game_test.ps1 -ProfileFrom <a test
profile of the emulator> -EyeSize 4096,4291 -PlaySeconds 60`, 168 checks, 0
failed): the line of the log with 4096 by 4291 and 2064 by 2162,
`headset_width=2064` and `headset_height=2162` in `bwvr.ini` at the start of
each game, a render of 1350 by 1080 in both games (and `ScreenW`, `ScreenH`
of the registry), the swapchains of the projection layer, of the panel
image and of the screen quad of 1350 by 1080, and no swapchain larger than
the menu (1600 by 1200). The first slot that the mod reads has "the headset
asks for 2064 by 2162". With `-EyeSizeMax 0` (168 checks, 0 failed) the app
takes 4096 by 4291, the render and the three swapchains are 2682 by 2146,
and the slot has 4096 by 4291. With the size of the panel
(`-EyeSize` not given) the render stays 1380 by 1104 (170 checks, 0 failed).
The GPU of the emulator (the GPU of the PC) does not show what the size
costs in a headset: a walk of the renderer in the test save took 0.23 ms
with the cap and 0.60 ms with no cap.

**Not shown yet:** the cap in a headset, and why the Quest 3 asks for 4096
by 4291.

## The sharpness of the middle

A projection layer maps its pixels linearly onto the tangents of its angles
of view, so the sharpness of each image on the way to the panel is its
pixels for a unit of tangent. The lens makes the middle of the panel denser
than its mean: in the middle of the lens a unit of tangent has about 1.3
times the mean pixels of the panel on each side (`eye_size::kLensMiddle`).
The table has the values of a Quest 3 that asks for 1680 by 1760 (the view
of the left eye -0.942 to 0.698 radians across, the render 2200 by 1760 of
one walk, the half angle 60.74 degrees of the game):

| Step | Pixels for a unit of tangent across | Pixels of the panel for one pixel, in the middle |
| --- | --- | --- |
| The panel in the middle of the lens | 1212 (the mean 934) | 1 |
| The render of the game and the eye picture | 616 | 1.97 |
| `fovea_res`: the outer image at 20 percent | 123 | 9.8 |
| `fovea_res`: the centre image at 200 percent | 1232 | 0.98 |
| `fovea_res_layer=0`: the centre in the eye picture, with a linear filter | 616 | 1.97 |
| `fovea_res_layer=1`: the centre layer, pixel for pixel | 1232 | 0.98 |
| The copies into the ring and into the swapchains | the same as the eye picture, no filter | |

A start that asks for 4096 by 4291 has a render of 2702 by 2162: 757
pixels, 1.60 pixels of the panel for one pixel.

So the eye picture limits the middle. With `fovea_res_layer=0` a centre of
200 percent goes into the eye picture at the size of the render
(`ComposeMres` of `vk_scene.cpp`), and the headset gets no more pixels than
the render has: the player sees a smoother middle, not a sharper one. With
`fovea_res_layer=1` the centre image is a projection layer of its own
(`backend.md`, "The middle as its own layer"), and a centre of about 200
percent reaches the panel in the middle of the lens.

**Why a layer of its own.** A swapchain larger than the panel, with fewer
pixels at the edge, needs an eye picture of the size of the middle, about 2
times the render on each side: 4 times the pixels for the eye picture, for
the copy into the ring and for the copy into the swapchains, also where the
outer image has few pixels. The centre layer has the pixels of the centre
image only. Its eye picture keeps the size of the render, and the renderer
draws the two images into it at their own size, pixel for pixel, in place
of a draw of the whole eye picture from them. The compositor blends one more
layer over the part of the view of the centre.

**The foveation of the headset.** The log has the extensions of the
runtime (`XR_FB_foveation`, `XR_FB_foveation_configuration`,
`XR_FB_foveation_vulkan`, `XR_META_foveation_eye_tracked`,
`XR_FB_swapchain_update_state`, `XR_FB_composition_layer_settings`) and of
the GPU (`VK_KHR_fragment_shading_rate`, `VK_EXT_fragment_density_map` with
its features and the size of its texels, `VK_EXT_fragment_density_map2`,
`VK_QCOM_fragment_density_map_offset`). `XR_FB_foveation` acts on a render
pass that draws into an image of a swapchain. The renderer draws into
images of its own, and the app copies them into the swapchains, so it
cannot act on the eye pictures. `VK_EXT_fragment_density_map` acts on a
render pass of the renderer, but the GPU of the PC (Radeon RX 7900 XTX) and
the emulator have none, so no run can show it. The emulator has no shading
rate attachment either (`foveation`). The Quest 3 has a shading rate
attachment. The outer image of `fovea_res` already has fewer pixels at the
edge.

**The textures.** The renderer of the app has an anisotropic filter of up
to 16 (the log of the Quest: "filtering : anisotropic 16x, min=1 mag=1"),
and a mip chain for each texture. The mip level comes from the pixels of
the image of the draw, so the centre image at 200 percent takes the finer
level. No texture is blurred by the filter, so there is no key for the
filter.

**The log.** One line at the first picture of the projection layer:

```
xr           : the sharpness of the middle, in pixels for a unit of tangent across (and the pixels of the panel that one pixel covers in the middle of the lens): the panel 1096 in the middle (the mean 843, the view of the left eye -0.942 to 0.820 radians), the render and the eye picture 1380 by 1104 387 (2.83), the outer image 155 (7.09), the centre image 773 (1.42), each image a layer of its own in the swapchain, copied pixel for pixel (fovea_res_layer=1). ...
```

With `fovea_res_layer=1` one more line says where the two images are in
the swapchain and gives the angles of view of the centre layer.

**The test runtime** reports the projection layers of a frame in one line
(`projection` with the views of the first layer, then `projection flags N`
with the views of each other layer), and its script aims with the views of
the first projection layer. Each view of each layer gives a picture of the
word `eyes` (`-p0` to `-p3` with the centre layer).

**What the emulator shows** (`emu_game_test.ps1 -NewProfile -ProfileOk
480,670 -SceneEyes -SkipClick 547,575 -TestSave pen -PlaySeconds 90
-FrameLoop 1` with `fovea_res=1`, `fovea_res_outer_pct=40`,
`fovea_res_center_pct=200`, `fovea_res_center_deg=22`,
`fovea_res_center_down_deg=9`, render 1380 by 1104): 204 checks, 0 failed,
with `fovea_res_layer` 1 and 0. **A known limit of the emulator:** some runs
stop in game 1 at the SkipBox over the first frames of the scene (95 checks,
10 or 11 failed): the session thread of the app waits in `vkWaitForFences`
of the GPU of the emulator (a hang report of `androidemu` in logcat). It
came with `fovea_res_layer` 1 and 0, and the cause is not known. With 1 the
eye picture has the outer image
of 552 by 442 at 0 0 and the centre image of 624 by 624 at 554 0, and each
frame with a picture has two projection layers: the outer rect with the
angles of the view, and the centre rect with the flags 6 (the blend of the
source alpha, colors not multiplied) and the angles -0.3858 0.3814 0.2410
-0.5115. A crop of the middle at the same zoom (300 by 300 pixels of the
centre image, and the same part of the eye picture of 0 scaled by 2) has a
mean gradient of the luma of 12.07 with 1 and 8.34 with 0 in game 1, and
3.63 and 2.75 in game 2. The GPU of the emulator takes 0.24 to 0.44 ms for
a walk in the test save with 1 and 0.24 to 0.41 ms with 0: at this render
the difference is in the noise. With `-EyeSize 4096,4291` (render 1350 by
1080) the eye picture has the outer image of 540 by 432 at 0 0 and the
centre image of 610 by 610 at 542 0. In the emulator the swapchains of the
eyes always have the size of the picture, so the scale of the rects to a
swapchain of another size (`FitAtlas`) has only the math checks. Not
shown: the blend of the compositor of a Quest 3, the GPU time in a Quest
3, and the sharpness through the lens.

## The frame loop

A frame of the session that comes late makes the runtime show a picture
again, and the player sees judder. So the thread of the session must call
`xrWaitFrame`, `xrBeginFrame` and `xrEndFrame` at the display rate of the
headset, also when the game, the play of a picture or the GPU is slower.
The key `frame_loop` in `bwvr.ini` (`frame_loop.h`, `picture_ring.cpp`,
`PlayThread` and `TakePicture` of `main.cpp`):

- `frame_loop=1` (the default): the new loop. A play thread plays each
  picture of the game into a ring of pictures, and the thread of the
  session shows the newest picture that the GPU finished. No thread of the
  session waits on the CPU for GPU work of a picture of the game.
- `frame_loop=0`: the old loop. At each frame of the session the thread of
  the session plays the batch of the live stream into the renderer, copies
  the picture into the swapchains, and before each copy waits on the CPU
  for the fence of the command buffer before it (`GpuBegin`).

The row "The headset gets each frame (no wait for the GPU)" on the page
"Testing" sets the key (On is 1), with a help text. A save writes it. The
app reads it at the start of each Play, and the new loop runs from Play to
the end of that game. With no game the app has the old loop. The key is not
a row of `settings_table.h`, so the launcher of the PC does not show it. It
has no test key: the app has no hotkeys, and the headset is tested through
its `bwvr.ini` or the row.

**The threads.**

- **The play thread** (`bwvr play`). It waits up to 50 ms for frames of the
  reader (`StreamReader::WaitAll`), plays the batch into the renderer with
  the rules of the old loop (`PlayBatch`, "The live stream"), and puts the
  picture into a slot of the ring (`WriteSlot`). Then it takes the next
  batch, after the queue at the GPU lets it. While it runs, it is the only
  thread that uses the renderer. The end of the game ends it (a join), and
  the thread of the session has the renderer again.
- **The thread of the session** (`bwvr main`). `xrWaitFrame`,
  `xrBeginFrame`, the slot of the input channel, the newest finished picture
  of the ring (`TakePicture`), the menu, the copies of the frame into the
  swapchains in one command buffer, and `xrEndFrame` with the poses of the
  shown picture. It does not play and does not wait for the play thread.

**The ring of the pictures** (`PictureRing`, the rules in `frame_loop.h`,
`Ring`). Three slots. A slot has an image of the eye pictures (a layer for
each eye, at the size of the picture), and when a picture needs them an
image of the panel image and an image of the screen picture. The images
have the byte order of the swapchains: `R8G8B8A8_UNORM`, or
`B8G8R8A8_UNORM` when the swapchains have blue first. A slot also has the
values of its frame: the `kView` (the poses and the angles of the views
that the picture was drawn with), the quad layers, the pictures of the
cards, the screen of a frame with no scene (with the pixels of the CPU of
such a screen), and the frame number of the game.

- The play thread records the copies from the images of the renderer into
  the slot in one command buffer of the slot, and submits it with the fence
  of the slot. The renderer draws each walk into the next of two targets, so
  a picture stays in its target only until the walk after the next one. The
  copy comes right after the batch, before the next walk. With the same
  byte order it is one `vkCmdCopyImage` of both layers, with blue first a
  blit of the same size. A screen picture goes into a slot only when the
  slot does not hold that screen picture yet.
- The play thread takes a free slot. With no free slot it takes the oldest
  ready slot that no frame showed: that picture never shows, and the time
  log counts it as dropped. It waits for the fence of that slot only when
  the GPU did not finish its copies. The session never waits for it.
- At each frame the session takes the newest ready slot whose fence is
  signaled (`vkGetFenceStatus`, no wait) and that is newer than the shown
  slot. The shown slot before is free again, and so is each older ready
  slot: a newer picture replaced it, and the time log counts it. With no
  new finished picture the session shows the shown slot again. A picture
  that the GPU did not finish never holds a frame of the session.
- The play thread never writes the shown slot, and the session never shows
  an older picture than the shown one (the math checks).
- When the session leaves a slot (it takes a new slot, or the game ends),
  no layer copies again from an image of that slot (`FreezeSlotSources`),
  also with `xr_copy_on_change=0` or with a swapchain image that waits for
  a later frame. A layer keeps its image until the new picture gives it a
  new source.

**The queue at the GPU** (`frame_loop_gpu_depth` in `bwvr.ini`, read at
each Play, 1 by default, the rules in `frame_loop.h`, `PictureRing::WaitGpu`).
Before the play of a batch the play thread counts the slots whose copies
the GPU did not finish. The copies of a slot come after the walks of its
picture on the same queue, so that is the count of the pictures that are
not finished on the GPU. While the count is at the key or more, the play
thread waits on the CPU for one of the fences of those slots, in steps of
5 ms. The reader keeps the frames of the game meanwhile, so the next batch
has the newest frame, and the play of a batch plays the walks of its newest
frame ("The live stream"): the older frames do not show. So with 1 the GPU
has at most one picture of the game that is not finished, and a new picture
starts its play with the newest frame of the game when the GPU is free. The
session never waits for it. `frame_loop_gpu_depth=0`: no limit, the play
thread plays each batch at once, and the GPU can have each slot of the ring
in its queue. The log at Play: "game : frame_loop_gpu_depth=1. ...".

**The poses.** A picture goes out with the `kView` of its own frame: the
poses that the game drew it with. The runtime reprojects it to the pose of
the head at the display time, which is the normal work of the runtime. With
`motion_comp=0` a picture that shows in more than one frame of the session
has the same poses in each of them. With 1 and 2 each frame of the session
gives the poses the difference of the place of the player since the frame of
the picture ("The motion of the stick"), so a picture that shows again is at
its place in the world.

**One copy into each swapchain.** The copy of the session from a slot into
a swapchain image of the projection layer is one `vkCmdCopyImage`, with no
image between: the slot has the size and the byte order of the swapchain,
and a copy keeps the bytes also in a swapchain with an sRGB format. The
panel image and the screen picture go the same way. Only the stereo screen
(half the size) has a blit into an image between and then a copy. So with
`xr_eye_direct=0` (the default) each eye has two copies in all, one on the
play thread into the slot and one on the session into the swapchain, the
same count as the old loop (a blit into the image between and a copy). A
copy straight from the target of the renderer into a swapchain of the
projection layer does not fit the ring: the walk after the next writes the
target again while the session can still show its picture. With
`xr_eye_direct=1` each slot has two swapchains of its own, the play thread
copies a picture of the projection layer straight into them, and the
session copies nothing for the eyes ("The copies of the eye pictures").

**The copies of the session with no wait** (`gpu.cpp`, `GpuSetRing`, the
rules in `frame_loop.h`, `CommandRing`). In a frame the session records
each copy (the eyes, the panel, the screen quad, the cards, the walls, the
keyboard, the menu and the cursor) into one command buffer, submits it
before `xrEndFrame`, and then releases the swapchain images. The command
buffers go round, the least recently used first, up to 16. The fence of a
command buffer is checked only when it comes again. When the GPU did not
finish it, the ring gets a new command buffer in its place, so the CPU does
not wait. Only a ring of 16 unfinished command buffers waits, and a ring
that cannot make a new command buffer (the log says so) waits for its
oldest one. With no command buffer at all the frame copies nothing. With
no play thread (`pthread_create` failed) the game has the old loop.

The keys of the copies of the session are in `bwvr.ini` of the game folder,
read at the first Play with the frame loop. The log: "xr : xr_copy_once=1,
xr_pool_reset=1. ...".

- `xr_copy_once=1` (the default): a copy of the same size from a slot of
  the ring into a swapchain image (the eyes of the projection layer, the
  panel image) is a secondary command buffer that the session records one
  time, with its barriers, for each image of a slot, layer and swapchain
  image. Each frame then runs it with `vkCmdExecuteCommands`. The entries
  of a set go when its source has another size, when the ring of the
  pictures made or freed an image since their record (the ring makes the
  images of a slot again at a new size, also for a slot that the session
  never took, and a new image can have the handle of a freed one,
  `frame_loop::OnceSetValid`), when its swapchains go, and at each start
  and end of the frame loop. A free waits for the idle queue of the
  session. A set has up to 48 entries, and a copy with no free entry is
  recorded in its frame. At the end of the app the log has "xr : the copies
  recorded one time (xr_copy_once=1): N secondary command buffers recorded,
  M copies ran one of them, ...". `xr_copy_once=0`: each frame records the
  barriers and the copies.
- `xr_pool_reset=1` (the default): each new command buffer of the ring has a
  command pool of its own, with no reset of one command buffer, and its
  begin resets that pool (`vkResetCommandPool`), as the renderer does with
  the command buffers of its walks. `xr_pool_reset=0`: the command buffers
  of the ring are of one pool, and each begin resets its command buffer
  (`vkResetCommandBuffer`).

**Not shown yet:** the time of "the eye pictures" and "the panel image" in
the parts of the layers in a Quest 3 with these keys at 1 and at 0. In the
emulator both parts take less than 0.1 ms also with the keys at 0, so only
the Quest shows if one of the two removes the CPU time in the kernel.

**The pixels of the CPU** (the menu, the cursor, the cards, the keyboard,
the walls and a screen with the pixels of the CPU, `XrApp::PixelsFor`). Each
submit has a serial, and a staging buffer has the serial of the last submit
that read it (`GpuSerialDone`, a serial that no submit has yet is not
finished).

- `xr_staging_direct=1` (read at the start of the app, the default): new
  pixels go straight into the staging buffer when the GPU finished its last
  copy and no copy in memory waits. Then the copy of the layer in the next
  frame has no `memcpy`. Else they go into a copy in memory, as with 0.
- `xr_staging_direct=0`: new pixels go into a copy in memory. The copy of
  the layer puts them into the staging buffer only when the GPU finished the
  last copy of that buffer. Until then the layer keeps its image.
- `xr_layer_rect=1` (read at the start of the app): a card copies only its
  rect, into the staging buffer and from there into the swapchain image,
  because its quad shows only that rect (`frame_loop::CopyRect`). The rest
  of the image is undefined, so the quad of a card shows only when the
  image that its swapchain released has the rect of the quad
  (`frame_loop::RectShows`). A new rect whose copy did not happen yet (a
  busy staging buffer or swapchain image) hides the card for that frame.
  `xr_layer_rect=0` (the default): a card copies its whole image. No run
  of the emulator has a card, so no run has shown a copy of a rect yet. The
  other layers copy the whole image with each value.

The log at the start: "xr : xr_layer_rect=0, xr_staging_direct=1. ...".

**The two queues** (`frame_loop_queues` in `bwvr.ini`, read at the start
of the app, 2 by default). On one queue the GPU does the work in the order
of the submits, so a copy of the session waits on the GPU behind each walk
that the play thread submitted before it, and the runtime gets the picture
of the frame only after that walk. With `frame_loop_queues=2`, when the
queue family has two queues or more and the device has timeline
semaphores, the device gets two queues: the session and the runtime take
queue 0 with the priority 1.0, and the renderer of the mod and the copies
of the play thread take queue 1 with the priority 0.5. The renderer gets
its queue at its start (the first Play, or the start of the app with
`stream_live=1`): queue 1 when `frame_loop` is 1 then, and it keeps it to
the end of the process.

- A slot goes from queue 1 to the session when the session saw its fence
  signaled. The copy of the session has a barrier that makes the bytes of
  the slot visible to it.
- Each submit of the session signals a timeline semaphore with its serial.
  When the session takes a new slot, the slot shown before gets the serial
  of the last submit of the session, which has each copy from it. A write
  of the play thread into that slot waits on the GPU for that serial (no
  wait on the CPU).
- The old loop with the renderer on queue 1 (no game, a test aid, or a
  later Play with `frame_loop=0`): before the play of a batch the CPU waits
  for the fence of the last copy of the session, and after the play for
  the walks (`vkQueueWaitIdle` of queue 1), as the old loop waits for the
  GPU before each copy.

`frame_loop_queues=1`, or a family with one queue: one queue for all, as
above. The log at the start: "xr : frame_loop_queues=2. The family of the
queue has N queues. ..." and "xr : the renderer of the mod takes queue 1".

**A swapchain image that is not free** (`frame_loop_image_wait` in
`bwvr.ini`, read at each Play, 0 by default). With the frame loop the
session asks for the next image of a swapchain with a timeout of 0
(`xrWaitSwapchainImage`). When the runtime still uses that image, the
image stays acquired, the layer shows its image before, and the next frame
asks for the same image again. So the session does not wait for the
compositor. `frame_loop_image_wait=1`: the session waits for each image, as
the old loop does.

**The lock of the queue.** The renderer, the copies of the play thread and
the session use one lock for the queues (`LockQueue` of `xr_app.cpp`). The
session holds it for each submit and for `xrBeginFrame`, `xrEndFrame`,
`xrAcquireSwapchainImage` and `xrReleaseSwapchainImage`, the calls in which
the runtime can use the queue (XR_KHR_vulkan_enable2). It does not hold it
in `xrWaitFrame` or in `xrWaitSwapchainImage`. A wait for the device to be
idle (a new size of an image) holds it too, and with one lock that wait
has each queue.

**The log.** At each Play "game : frame_loop=1. A play thread plays each
picture of the game into a ring of 3 slots ...", at the first Play "ring :
a ring of 3 slots ...", one line for each image of a slot ("ring : slot 0:
an image of 1380 by 1104 with 2 layers for the eye pictures (11.6 MB)"),
"gpu : the ring of command buffers is on (up to 16)" at Play, and at the end
of the game "gpu : the ring of command buffers is off: N entries, M new
entries because the GPU was not done, K waits of a full ring". At the end
of the app "ring : the ring of the pictures ends" with the dropped and the
replaced pictures, and a line when copies of pixels of the CPU waited a
frame for a staging buffer. The time log has the lines of the play thread
("The time log of the app").

**The tests.**

- `.\dev.ps1 build`: the checks of `frame_loop_checks.h` (the choice of the
  newest finished picture, the drop rule, the end of a game, 300 frames
  with no write into the shown slot and no older picture, the ring of the
  command buffers, the serials, the age, the queue at the GPU with a loop
  of 30 steps, the direct writes, the rect of a copy, the row and its
  save).
- `emu_game_test.ps1 -FrameLoop 1` and `-FrameLoop 0`: the line of the key
  at each Play, and with `time_log=1` the lines of the play thread, the age
  of the shown picture, no wait of a copy of the session for the GPU, no
  wait of a full ring, and the frames of the session a second near the
  display rate (the median of the lines at 0.9 of the rate or more) with
  the new loop. With the old loop the lines say that there is no play
  thread, and there is no ring of command buffers. In each window the line
  of the parts of the layers adds up to the step of the layers, the new
  loop has the line of the queue at the GPU with the key of Play (with
  `-IniExtra frame_loop_gpu_depth=0` no wait), and the line of the GPU
  parts of the walk has timed walks with passes and draws.

The test runtime lists XR_KHR_convert_timespec_time (an XrTime of it is
`CLOCK_MONOTONIC` in ns), so the emulator has the age of the shown picture
in ms.

**What the emulator shows** (`emu_game_test.ps1 -NewProfile -ProfileOk
480,670 -SceneEyes -SkipClick 547,575 -TestSave pen -PlaySeconds 90`, Wine
of x86_64, the test runtime at 72 Hz, a render of 1380 by 1104). The device
of the emulator has a family with 2 queues, so the renderer has queue 1. The
test runtime gives a frame about each 14.5 ms (68.8 frames a second) at
72 Hz. In the test save (game 2), in the windows of 10 s with no read back
of a picture:

| | `frame_loop=1` (196 checks, 0 failed) | `frame_loop=0` (192 checks, 0 failed) |
| --- | --- | --- |
| frames of the game, pictures of the app, frames of the session a second | 68.7, 68.7, 68.7 | 68.8, 68.8, 68.8 |
| from the end of `xrWaitFrame` to the end of `xrEndFrame`, mean | 0.64 to 0.65 ms | 3.55 to 3.57 ms |
| of it the play of a batch, the layers, the submit | 0, 0.29 to 0.30, 0.12 ms | 1.95 to 1.96, 1.05, 0.36 ms |
| waits of a copy of the session for the GPU in 10 s | 0 | 2061 to 2067 |
| the play thread | 2.32 to 2.34 ms of CPU a picture, 16 percent of one core | none |
| from the start of the play to the display time of a new picture | 21.7 to 21.9 ms | 13.8 ms |
| from the arrival of its frame in the app to the display time | 21.9 to 22.0 ms | 21.3 to 21.6 ms |

So the latency of a picture from its frame of the game is the same in both
loops in the emulator (0.4 to 0.6 ms more with the frame loop). With the
frame loop the picture waits in the ring for the next frame of the session,
and in the old loop the frame waits in the queue of the reader. The
pictures of the eyes that the runtime read back have the game in both runs
(game 1 and game 2, both eyes). Over game 1 (the logos, the video, the
dialog of a new profile and the land) the median of the frames of the
session a second is 68.7 with each loop. In the frames of the session that
the test runtime holds for a read back the play thread waits for the lock
of the queue, so those windows have few pictures in both loops.

**Not shown yet:** only a headset can show the effect on the judder: the
frames of the session a second at the display rate of a Quest 3 with its
GPU, the waits of the runtime of Meta in `xrWaitFrame` and `xrEndFrame`,
the CPU time of the play thread next to the game under Wine with FEX or
Box64, and the memory of the ring at the render of the headset. Also only
a headset can show if the second queue with its lower priority keeps the
copies of the session from a wait behind a walk on its GPU, how the
runtime of Meta answers `xrWaitSwapchainImage` with a timeout of 0, which
step of the line of the steps takes the time of a frame of the session when
the GPU is busy, and the latency that the player feels.

**Known limits:**

- The ring needs memory of the GPU: three images of the eye pictures (and
  of the panel image when the mod gives panel quads) at the size of the
  render, at 2702 by 2162 about 67 MB for each slot with a panel. The old
  loop has one image between for each set of swapchains in place of them.
- The test runtime of the emulator holds a frame of the session for some
  seconds while it reads back a picture. The session then holds the lock of
  the queue in `xrEndFrame`, so the play thread waits too, and the frames of
  the game in that time do not show.
- A screen picture with the pixels of the CPU is a copy in memory in each
  slot that shows it (5.8 MB at 1380 by 1104).
- With `frame_loop_image_wait=0` each eye takes its new picture when its
  own swapchain image is free, so for a frame the two eyes can show two
  pictures, each with its own poses.
- The renderer keeps the queue of its start. After a start with
  `frame_loop=1` and two queues, a game with `frame_loop=0` has the old
  loop with the CPU waits of "The two queues", not the old loop on one
  queue. A new start of the app with `frame_loop=0` gives the old loop on
  one queue.

## The copies of the eye pictures

**What a copy costs.** Each copy of a picture in the app is a
`vkCmdCopyImage` of the same size, or with blue first a blit of the same
size with no filter (`VK_FILTER_NEAREST`). The layout of the target goes
from undefined to the layout of a transfer, so no step reads its old
pixels. A copy reads each byte of its rect one time and writes it one time,
so its GPU time is about its bytes over the bandwidth of the memory of the
GPU. At a render of 4096 by 3456 an eye picture with its two layers has
108 MB. A command buffer of 5.55 ms with such a copy writes about 19 GB a
second and reads as much, so the copies are bound by the memory and not by
a filter, a format or a layout. Only fewer bytes make a copy shorter. The
parts that the picture does not need:

- **The rest of an atlas** (`fovea_res_layer=1`, `backend.md`, "The middle
  as its own layer"). The eye picture keeps the size of the render, and the
  layers show only the outer rect and the centre rect. The part with both
  rects and their gap (`eye_copy_math::ShownPart`) is 2880 by 1854 of 4096
  by 3456 with the outer image at 25 percent and the centre at 200 percent
  for 22 degrees: 37.7 percent of the bytes (the math checks). The renderer
  cleared and wrote the whole eye picture, and each copy took the whole
  picture.
- **The second copy of each eye**: the copy of the session from the slot
  into the swapchain.
- A copy of the screen that did not change: there is none. A screen
  picture goes into a slot only when the slot does not hold it. The panel
  image goes into a slot after each walk that drew into the panel
  (`VkScenePanelFor`), and the session copies it for each new picture that
  has one. In the test save each walk draws into the panel, so each picture
  has two copies of the panel image of the size of the render (one layer,
  half the bytes of the eye picture). The renderer does not know if its
  pixels changed. This part stays (see the known limits).

**The keys** (`bwvr.ini` of the game folder, the rules in
`src/mod/eye_copy_math.h`, checks in
`test/controller_math/eye_copy_checks.h`):

| Key | Default | The new way | The old way |
| --- | --- | --- | --- |
| `xr_eye_rects` | 1 | A copy of an eye picture with an atlas, into a slot and into a swapchain, takes only the shown part. The rest of the image of the slot and of the swapchain image is undefined | 0: the whole picture |
| `vk_atlas_area` | 1 | The renderer of the app clears and writes only the shown part of the eye picture when it puts the two images into it (`ComposeAtlas` of `vk_scene.cpp`, a render area of that part). Its clear gives the gap its black with an alpha of 0 | 0: the whole eye picture. On the PC always 0 |
| `xr_eye_direct` | 0 | The play thread copies each picture of the projection layer straight into two swapchains of its slot, and the session copies nothing for the eyes | 0: the copy into the slot and the copy of the session |
| `frame_loop_hold` | 0 | N of 2 to 8: the session takes a new picture only after the shown picture showed in N frames of the session | 0 and 1: the newest finished picture at each frame |

The app reads `xr_eye_rects` with the keys of the copies of the session at
the first Play with the frame loop ("xr : xr_eye_rects=1. ..."), the
renderer reads `vk_atlas_area` with `fovea_res_layer` at the first walk of
each game ("vk scene : vk_atlas_area=1. ..." in `bwvr-quest.log` when
`fovea_res_layer` is on), and the app reads `xr_eye_direct` and
`frame_loop_hold` at each Play ("game : xr_eye_direct=1. ...", "game :
frame_loop_hold=2. ..."). There is no test key: the app has no hotkeys.
The keys are not rows of `settings_table.h`, so the launcher of the PC
does not show them.

**The eyes straight into the swapchains** (`xr_eye_direct=1`,
`XrApp::DirectBegin`, `DirectEnd`, `PictureRing::CopyDirect`).

- Each slot of the ring gets two swapchains (one for each eye) at its first
  picture of the projection layer, of the size of the shown part of that
  picture (`eye_copy_math::DirectSize`: the whole picture with no atlas).
  The play thread makes them (`xrCreateSwapchain` with the lock of the
  queue), and they stay to the end of the app. The log: "xr : slot 0 of
  the ring: two swapchains of W by H for the eye pictures of W by H ...".
- For a picture the play thread acquires and waits for an image of each
  swapchain of the slot (it may wait, the session never does), records the
  copy of the shown part of each layer into it after the walk, submits,
  releases the two images, and only then the slot is ready. The session
  shows a slot only when its fence is signaled, and the ring never lets the
  play thread write the shown slot (`frame_loop.h`, `Ring`). So no layer of
  a frame names a swapchain that has an acquired image, and the runtime
  reads a swapchain image only after the GPU finished it.
- The session takes the picture of a new slot with no copy: the views of
  the projection layer (and of the centre layer) name the swapchains of the
  slot, with the rects of the atlas at their place in the picture. The
  poses are the same as with the old way (`motion_comp`).
- A picture takes the old way when it does not go into the projection
  layer (the stereo screen, the cut scene window), when it does not fit in
  the swapchains of its slot (`eye_copy_math::DirectFits`: another size, or
  no atlas in swapchains of the shown part), and when the swapchains have
  blue first (a blit would change the bytes of an sRGB swapchain). An eye
  that shows a slot and gets a picture of the old way waits for its
  swapchain image in that frame, because the play thread can write that
  slot again. When the copy still does not happen, the frame has no
  projection layer, and the log counts it at the end of the app.
- A slot that has an image of the eye pictures from a picture of the old
  way frees it at its next picture straight into its swapchains, with one
  wait for the idle device ("ring : slot 0: its image of the eye pictures
  goes ...").
- **The memory.** Three slots with two swapchains of about three images
  each (the runtime chooses the count): 18 images of one eye, in place of
  the 3 images of the slots (6 images of one eye) and the 6 images of the
  two swapchains of the projection layer. At 4096 by 3456 with no atlas
  that is about 970 MB in place of about 650 MB. With the atlas of 2880 by
  1854 above it is about 370 MB, and no swapchain of the projection layer
  of the size of the render is made.
- At the end of the app: "xr : xr_eye_direct=1: N pictures went straight
  into the swapchains of the slots, M pictures of the projection layer took
  the old way because they did not fit in those swapchains, F frames of the
  session showed a picture of a slot, K frames had no projection layer
  ...".

**The even rate** (`frame_loop_hold`, `eye_copy_math::HoldTakes`). When the
GPU makes a picture in about each second frame of the display, the newest
finished picture shows in 1 or in 2 frames in turn, and a flight of the
camera of the game judders. With `frame_loop_hold=2` the session takes a
new picture only after the shown picture showed in 2 frames: 36 pictures a
second at 72 Hz with a picture in each frame of the GPU, each in 2 frames.
A picture that is not finished at that frame shows at the first later frame
where it is finished, and the count starts again. Each frame has the poses
of its picture with the rules of `motion_comp`, as before. The play thread
plays at its own rate, so a picture that a newer one replaced before the
take counts as replaced in the time log.

**The time log** (line 3 of "The time log of the app"): "time : the GPU of
the copies in ms, the mean and the most: ...", for each kind of copy the
mean and the most of its GPU time, the count, the MB of a copy, and the GB
that the GPU wrote in a second of the copy: the eye pictures into a slot,
the eye pictures straight into the swapchains, the panel image into a
slot, the screen picture into a slot, an eye picture of a slot into its
swapchain, an eye picture into the stereo screen, the panel image into its
swapchain, and the screen picture into the screen quad. A pair of
timestamps at the bottom of the pipe goes around each copy
(`copy_time.h`), so the time of a copy has no time of a walk. The parts of
a walk are in the line of the GPU parts of the walk (`gpu_parts=1`, "The
GPU parts of the walk").

**What the emulator shows** (`emu_game_test.ps1 -NewProfile -ProfileOk
480,670 -SceneEyes -SkipClick 547,575 -TestSave pen -PlaySeconds 90
-FrameLoop 1`, a render of 1380 by 1104, the test runtime at 72 Hz). The GPU
of the emulator is the GPU of the PC, so a copy takes 0.02 to 0.03 ms there.
The MB of the copies are the measure that a headset has too.

| | The new values | The old values |
| --- | --- | --- |
| No `fovea_res`, `xr_eye_direct=1`, `frame_loop_hold=2` and 0, 0 | 203 checks, 0 failed. 9299 pictures straight into the swapchains of the slots (1380 by 1104), 0 of the old way, 0 frames with no projection layer. The eyes: one copy of 11.6 MB a picture | the eyes: 11.6 MB into the slot and 2 copies of 5.8 MB into the swapchains, 23.2 MB a picture |
| `fovea_res=1`, `fovea_res_layer=1`, outer 25 and centre 200 percent, the four keys new and old | 203 checks, 0 failed. The swapchains of the slots have 1069 by 722 (the shown part), the outer rect 345 by 276 at 0 0, the centre rect 720 by 720 at 347 0. The eyes: one copy of 5.9 MB a picture | 204 checks, 0 failed. The eyes: 23.2 MB a picture |

In game 2 (the test save, the same frame of the scene in both runs) the
pictures of the two projection layers that the runtime read back are the
same in their rects: 0 of the 518400 pixels of the centre rect and 2 of the
95220 pixels of the outer rect differ between the new and the old values.
With `frame_loop_hold=2` and a picture in each frame (68.2 frames of the
session a second), each picture showed in 2 frames: 341 new pictures in a
window of 10 s, the age of the shown picture 0.50 frames as a mean and 1 at
most, and 341 pictures that a newer picture replaced.

**Not shown yet:** the time of the copies in a Quest 3 and in a Steam
Frame, the even rate in a flight of the camera, the memory of the
swapchains of the slots in a headset, and how a runtime of a headset
answers the calls of the swapchains of the slots on the play thread.

**Known limits:**

- In the emulator `fovea_res=1` with a render of 2760 by 2208
  (`-ResolutionPct 100`) or 4096 by 4096 (`-ResolutionPct 200`) stops after
  the first frames of the scene with the old and with the new values of the
  four keys: the session writes no more slots of the input channel, or the
  game process ends. The cause is not known.
- With `vk_atlas_area=1` the pixels of the eye picture outside the shown
  part are undefined. The screen quad of a frame with no scene right after
  a walk with an atlas shows the whole eye picture (`backend.md`, "The
  middle as its own layer"), so it shows those pixels too.
- The panel image still has two copies (into the slot and into its
  swapchain) of the size of the render for each walk that drew into the
  panel, in the test save for each picture. The panel quads show only some
  rects of it.
- The renderer still draws into its own two targets, and the play thread
  copies from them. A walk straight into a swapchain image would need the
  renderer to take a target for each walk.

## The time log of the app

`time_log=1` (the default, `time_log.cpp`, the parse of `/proc` in
`proc_stat.h`, the rules in `src/mod/time_log_math.h`). While the game runs,
each 10 s the app writes the lines `time` to `bwvr-quest.log`. The mod
writes a line of its own with the same key (`backend.md`, "The time log of
the mod").

1. **The frames.** The frames of the game a second (each frame that came
   through the live stream), the pictures of the app a second, the frames
   of the session a second, and why a frame of the game did not show. A
   frame that a newer frame replaced before the app took it did not show,
   because the app was still at the picture before. Of the time since that
   picture the largest part gives the cause (`CauseOf`). In the old loop
   (`frame_loop=0`, "The frame loop") the thread of the session plays: it
   waited for the GPU (the fences of its copies and of the walks of the
   renderer), it waited in `xrWaitFrame` or `xrEndFrame`, or its CPU
   worked. With `frame_loop=1` the play thread plays, and the time is of
   the picture before on the play thread: it waited for the GPU (the fences
   of the walks of the renderer and of a slot of the ring), it waited for
   the lock of the queue, or the play thread worked. A frame of the session
   with no new frame of the game shows the picture before again: no new
   frame came. At the end of the line: the pictures that the play thread
   dropped because the ring had no free slot, and the pictures that a newer
   finished picture replaced before a frame of the session showed them
   (both 0 in the old loop).
2. **The CPU of the app**, the mean and the most in ms: the play of a
   picture (the records of a batch into the renderer), the wait in
   `xrWaitFrame`, the time from the end of `xrWaitFrame` to the end of
   `xrEndFrame`, a wait of a copy for the GPU (the old loop: `GpuBegin`
   waits for the fence of the last command buffer of the app, and with
   `frame_loop=1` only a full ring of command buffers waits), and a wait of
   a walk for the GPU (the renderer waits for the fence of its slot). Then
   the play thread (`frame_loop=1`): its CPU time for each picture and in
   percent of one core (`CLOCK_THREAD_CPUTIME_ID`), and its waits for the
   fence of a slot of the ring. In the old loop the line says that there is
   no play thread. At the end: the age of the shown picture in frames of
   the session, the mean and the most (0 in a frame with a new picture, one
   more in each later frame that shows the same picture).
   **The steps of the thread of the session** (one more line): the sum in
   ms of each step in a frame from the end of `xrWaitFrame` to the end of
   `xrEndFrame`, the mean and the most: `xrBeginFrame`, the actions and the
   poses (`xrSyncActions`, `xrLocateSpace`), the slot of the input channel
   (`xrLocateViews`) and the pulses, the picture of the ring
   (`frame_loop=1`), the play of a batch (the old loop), the menu and the
   controllers (the menu button, the keyboard, the ray, the paint), the
   layers (the rest of `EndFrame`: the records of the copies and the
   layers), `xrAcquireSwapchainImage` (with the count of the calls),
   `xrWaitSwapchainImage` (with the count of the images that were not free
   at once, `frame_loop_image_wait=0`), the submit of the copies,
   `xrReleaseSwapchainImage` and `xrEndFrame`, and of them the waits for
   the lock of the queues. Then the age of the shown picture in ms, from
   the start of the play of its batch to the display time of the frame
   (`predictedDisplayTime` on `CLOCK_MONOTONIC` through
   XR_KHR_convert_timespec_time, when the runtime lists it): the mean and
   the most over each frame, and over the frames that show a new picture
   (the latency of a new picture). With no such extension the line says
   that there is no display time.
   **The parts of the age of a new picture** (one more line, with the
   display time): the mean and the most in ms over the new pictures
   (`frame_loop::AgeParts`): the game (from the write of the slot of the
   input channel that the frame of the mod read, `SlotData::frame` of its
   `kView`, to the arrival of the newest frame of the picture in the
   reader: the frame of the mod, its walks and the handover), the reader
   (to the start of the play), the play (to the submit of the copies into a
   slot of the ring, in the old loop to the end of the play), the GPU and
   the wait in the ring (to the take of the session), the session (to the
   end of `xrEndFrame`), and the runtime (to the display time). At the end
   the age from the input to the display time. A picture with no `kView` or
   a slot that the app does not know any more has no part of the game and
   no age. In the emulator (the test save, 72 Hz, a window of 10 s with no
   read back) a new picture is 49.5 to 50.5 ms old as a mean: the game 24.1
   to 25.0, the reader 0.1 to 0.2, the play 3.7 to 3.9, the GPU and the wait
   in the ring 7.7 to 8.0, the session 0.9 to 1.0, the runtime 12.7 to 12.8.
   **The motion of the stick** (one more line, "The motion of the stick").
3. **The GPU** in ms, from timestamps at the start and at the end of each
   command buffer: a walk of the renderer (`VkSceneTakeGpuTime`), a command
   buffer of the app (`Gpu::busy`, with `frame_loop=1` also the copies of
   the play thread into the slots), a picture (the walks and the command
   buffers of the 10 s over the pictures), and the percent of the time that
   the GPU worked for the app. The timestamp at the start of a command
   buffer of the app is at the bottom of the pipe, so it comes after the
   walk before it on the queue, and the time of the walk is not counted two
   times.
   **The GPU of the copies** (one more line): the GPU time of each kind of
   copy of a picture, with its count, its MB and the GB a second that it
   wrote ("The copies of the eye pictures").
   **The parts of the layers** (one more line): the step "the layers" of
   the line of the steps in its parts, each the mean and the most in ms of
   a frame of the session, with the CPU time of the thread of the session
   as a mean (`CLOCK_THREAD_CPUTIME_ID`): the begin of the command buffer
   of the frame (`GpuBegin`), the records of the copies of the eye pictures
   (also of the stereo screen), the panel image, the cards, the walls, the
   screen quad, the keyboard, the cursor, the menu, the copies of the
   pixels of the CPU into the staging buffers (`memcpy`), the lines of the
   log of a new picture, and the rest (the order and the quads of the
   layers). The calls of the swapchains and the submit are not in the
   parts: they are steps of their own. "In all" is the sum of the parts. At
   the end: the copies from a copy in memory into a staging buffer with
   their MB (and of them the copies of a rect), and the writes straight
   into a staging buffer ("The frame loop", "The pixels of the CPU").
   **The queue at the GPU** (one more line with `frame_loop=1`): the key
   `frame_loop_gpu_depth`, the waits of the play thread for the GPU before
   a play (the count, the mean and the most in ms), and the frames of the
   game that did not show after such a wait. Such a wait counts as a wait
   for the GPU in the cause of the frames that did not show (line 1).
   **The GPU parts of the walk** (one more line with `gpu_parts=1`, "The
   GPU parts of the walk").
4. **The threads** of the game process and of the app: the first 8 by
   their CPU time in the 10 s, each with its name, its id, the percent of
   one core (user and system), the core that it ran on last and its state.
   The app finds the game process in `/proc`: the process of the user of the
   app with the name `runblack.exe` (its parent is 1, "Play in the app").
   The line of a stop lists the threads of the game alone, the first 24.
   **The main thread of the game** is the thread whose id is the id of the
   process, with the name `runblack.exe`. The log marks it "(the main
   thread)". In each window of the runs in the emulator it has the most
   CPU time of the game: it runs the loop of the game and the frames of the
   mod. Wine gives the other
   threads the names of their descriptions (`wine_dsound_mix`,
   `wine_dinput_wor`). With `time_log=1` the app names its own threads:
   `bwvr main` (the loop of the session), `bwvr reader` (the live stream),
   `bwvr play` (the play thread of `frame_loop=1`), `bwvr painter`, `bwvr
   wine` and `bwvr game`.
5. **The processes and the cores**: the CPU time of the game process, of the
   app and of the other processes of the user of the app (Wine) in cores,
   and the busy time of each core from `/proc/stat`, with the count of the
   cores busy for half the time or more. A core with a count that went down
   (a core that went off) is not in the line. When the app cannot read
   `/proc/stat`, the log says so one time. The line ends with the time of
   the read of `/proc` on the thread of the session.

**The stop of the game.** When no new frame of the game came for 5 s, one
line names each thread of the game with its state (R runs, S sleeps, D
waits in the kernel), its percent of one core (user and system) since the
last window and its core: "time : no new frame of the game for 5.0 s (the
last is frame N). The threads of the game ...". So a frame that does not
end, with a thread at 100 percent in the kernel, shows in one line. A load
of the game can also give the line. At the same stop the mod writes the
32 bit modules of the game process to `bwvr.log` (`stall_modules=1`,
`backend.md`, "The module list at a stall"), so an address of a line
"RtlpWaitForCriticalSection" of `wine.log` can be named: put it into
`stall_addrs` of `bwvr.ini`, and the next stop gives its region, its
allocation base, its mapped file and its module.

**The cost.** Some reads of the clock in a frame of the session and one in
each frame that the reader takes, two timestamps in each command buffer of
the app, and one read of `/proc` each 10 s on the thread of the session.
With `frame_loop=1` also some reads of the clock and one of the CPU time of
the play thread for each picture, and two reads of the clock when the lock
of the queue is not free at once. The parts of the layers read the clock and
the CPU time of the thread at each change of the part, about 10 to 30 times
in a frame of the session.
`time_log=0`: no line, no timestamp, no read of the clock for the log, no
read of `/proc` and no name of a thread of the app. The key is read before
the threads of the app start, so each of them gets its name. There is no
test key.

**What the emulator shows** (`emu_game_test.ps1 -NewProfile -ProfileOk
480,670 -SceneEyes -SkipClick 547,575 -TestSave pen -PlaySeconds 90`, 170
checks, 0 failed, Wine of x86_64 with no Box64, the test runtime at 72 Hz,
4 cores). In each game the app logged its lines each 10 s over the whole
game (28 lines over 312.5 s of game 1), and the mod its line (29 lines over
290 s). The frames a second of the game fit: 42.4 in the app (12838 frames
in 302.5 s) and 42.9 in the mod (12440 frames in 290 s) in game 1, 58.4 and
59.6 in game 2. In the test save (game 2, a render of 1380 by 1104) a window
of 10 s has:

```
time         : 10.0 s to frame 887: the game 68.0 frames a second (681 frames came), the app took 681 frames and made 68.0 pictures a second (681), 68.0 frames of the session a second. 0 frames of the game did not show ...
time         : the CPU of the app in ms, the mean and the most: the play of a picture 2.15 and 4.19, the wait in xrWaitFrame 10.58 and 12.72, from the end of xrWaitFrame to the end of xrEndFrame 4.10 and 6.10, in xrEndFrame 0.02 and 0.51, a wait of a copy for the GPU 0.30 and 1.68 (2043 waits), a wait of a walk for the GPU 0.00 and 0.00 (0 waits)
time         : the GPU in ms: a picture 0.32 as a mean ..., a walk of the renderer 0.24 and 0.24 at most (681 walks), a command buffer of the app 0.03 and 0.03 at most (2043). The GPU worked 2.2 percent of the time for the app
time         : each 10 s, the threads in 10.0 s, ...: game runblack.exe 6171 (the main thread) 38.5 (1.4 + 37.1) core 0 R, app bwvr main 4524 27.6 (13.4 + 14.2) core 2 R, app bwvr reader 4752 7.6 (0.7 + 6.9) core 0 S, game AudioTrack 6548 1.2 ...
time         : the processes in cores: the game 0.42, the app 0.36, the other processes of the user of the app (Wine) 0.11. The cores: 4, 0.68 of them busy in all, ...
time log     : 10.0 s, 675 frames of the game (67.5 a second), 675 of them presented in a scene walk. A frame in ms, the mean and the most: the frame 14.82 and 20.21, the scene walk 5.49 and 11.96, the write of the stream 0.13 and 0.88, the wait of the pace 6.55 and 11.51, the rest 2.65 and 9.95
```

The main thread of the game has the most CPU time of the game in each
window. While the test runtime reads back a picture, it holds a frame of the
session for some seconds: then a window has, for example, "563 in
xrWaitFrame or xrEndFrame" and fewer pictures, while the reader still takes
each frame of the game. In the emulator the game, the app and Wine use less
than one core in all. With `-TestStallMs 8000` (`test_stall_ms` of the mod,
`backend.md`, "The module list at a stall") the frame thread of the mod
spins 8 s, and the line of a stop comes after 5 s of it: "no new frame of
the game came to the reader for 5.0 s ... runblack.exe <id> (the main
thread) R ...". After the spin the frames come again ("live
: frames come"). With `-TimeLog 0` neither log has a line of the times.

**Not shown yet:** the lines in a headset: the time of the GPU of a Quest 3,
the waits of the runtime of Meta, the threads under Box64, the load of the
cores, and the line of a stop of the game.

## The GPU parts of the walk

`gpu_parts=1` (read at the start of the app, the default, `walk_probe.cpp`,
the rules in `walk_parts.h`, only with `time_log=1`). The renderer of the
mod (`vk_scene.cpp`, the same code as on the PC) calls Vulkan through the
table `vk::f` of `vk_core.cpp`. After the first play of a batch that started
the renderer, the app puts its own functions into some entries of that table
on the thread that plays (`WalkProbeInstall`). Each of them counts and then
calls the function of the driver. The code of the mod does not change.

- **The timestamps.** Each command buffer of the renderer gets a timestamp
  at its start, before each `vkCmdBeginRendering`, after each
  `vkCmdEndRendering` and before its end (`VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT`,
  in a query pool of the app, up to 48 in a command buffer). So no
  timestamp is in a pass. A part is the time between two timestamps that
  follow.
- **The parts.** A pass has the key of its start: one layer, one eye or both
  eyes (the bits of the view mask), its size, cleared or loaded, and with
  or with no map of the rates (foveated rendering). For example the outer
  image and the centre image of `fovea_res`, their composition into the eye
  picture, and the panel image are passes of their own keys. A part between
  two passes has the kind of the most costly command in it: the uploads
  (`vkCmdCopyBufferToImage`), the copies and blits (for example the picture
  of the walk before), the read backs (`vkCmdCopyImageToBuffer`), or only
  barriers. The command buffer of the uploads of a walk is one part of
  uploads. After 48 timestamps the rest of a command buffer is one part.
- **The counts** of a command buffer: the passes, the draws, the bindings of
  pipelines and of descriptor sets, the descriptor writes
  (`vkUpdateDescriptorSets` while a command buffer of the renderer records),
  and the regions of the uploads with their texels (4 bytes each). The
  renderer gives its own counts of a walk too (`VkSceneDrawCounts`): the
  draws of the game, the draw commands after the merge, the draws that the
  sort moved, the draws in draws of the merge, and the binds and states that
  it left out because they were bound (`backend.md`, "Fewer state changes
  and fewer draws").
- **The results.** When the renderer begins a command buffer again, it
  waited for its fence, so the app reads the timestamps of the last submit
  of that command buffer then, with no wait. Only a submit of a walk gives
  parts: a command buffer with one pass or more, and the command buffer of
  its uploads in the same submit. A one-time command buffer of
  `vk_core.cpp` or a read back alone gives nothing. The app follows 16
  command buffers. With more, the one that began least recently goes.

The line each 10 s: "time : the GPU parts of the walk in ms (gpu_parts=1),
the mean in a walk and the most in one walk, N walks (M with no time): ...",
for each key of a pass the count in a walk, the mean in a walk and the most
in one walk in ms and the draws in a walk, then the parts between the
passes, the sum of the parts in a walk, the counts of a walk, and the counts
of the renderer in a walk ("The renderer in a walk: N draws of the game, M
draw commands, ..."). The sum is
a little more than the walk of the line of the GPU: the timestamps of the
renderer are inside those of the app, and the uploads have a command buffer
of their own.

**The cost.** 2 timestamps for each pass and 2 for each command buffer, and a
count in each call of the table that the app takes. Each timestamp waits for
the work before it on the GPU, but a pass ends there anyway.
`gpu_parts=0`: the table of the renderer stays as it is, and there is no
line. The log at the start: "gpu parts : gpu_parts=1. ..." at the first
install, or "gpu parts : gpu_parts=0, time_log=1. ..." with no parts.

**Not shown yet:** the parts in a Quest 3, and the cost of the timestamps on
its GPU (compare the walk of the line of the GPU with `gpu_parts` 1 and 0).

## The hot code under Box64

`profile_sample` and `profile_sample_outside` (`backend.md`, "Where the
time of the scene walk goes") stop the frame thread and read the address of
its next instruction with `GetThreadContext`. Under Box64 this address is
not the address of the code that runs:

- In the WoW64 of Wine the context of a 32 bit thread is the context in the
  CPU area of its TEB (`ThreadWow64Context`). `BTCpuGetContext` of
  `wowbox64.dll` reads it there (`wine/wow64/wowbox64.c` of Box64 0.4.4).
- Box64 writes that context only at a system call of the 32 bit code (the
  BOP of `EmitInterruptionImpl`): `Eip` is then the return address of the
  call in the 32 bit `ntdll.dll`. While the thread runs translated x86 code,
  its x86 registers are in the registers of the CPU and in the struct of the
  emulator, and the context keeps the values of the last system call.
- So a sample of a thread in a system call has the right place and a right
  stack (the callers of a wait), and a sample of a thread in the code of the
  game has the place of its last system call. Both are in `ntdll.dll` or
  `win32u.dll`, and the report cannot tell them apart.

The options of Box64 (`docs/USAGE.md` of Box64 0.4.4):

- `BOX64_DYNAREC_PERFMAP` writes the map of the translated blocks for
  `perf`. In the build of `wowbox64.dll` (`_WIN32`) `writePerfMap` is empty,
  and the option is not "Available in WowBox64". `BOX64_DYNAREC_GDBJIT` is
  not there either.
- `BOX64_DYNAREC_LOG`, `BOX64_DYNAREC_DUMP` and `BOX64_DYNAREC_DUMP_RANGE`
  are in WowBox64. They log the translation of each block, not where the
  time goes, and they cost time only when a block is translated. A dump of
  the native addresses of the blocks with samples of the native PC
  (`simpleperf`, if Meta Horizon OS has it) could map the hot ARM code to
  x86 code with no patch. Not known: if the dump gives the native
  addresses under WowBox64.
- `BOX64_PROFILE` is a set of options (safe, fast), not a profiler.

So no way gives the x86 address of the hot code under Box64 yet. What the
log gives now: the time log of the mod (the parts of a frame by clock reads,
which work under Box64), the time log of the app (the CPU time of each
thread of the game and of each core), and the samples of the system calls.
Ways to try in a later step:

- A patch of `BTCpuGetContext` of `wowbox64.dll` (our build of Box64,
  `build_box64.ps1 -Mingw`): for a thread that is stopped in translated
  code, it turns the native PC into the x86 `Eip` with
  `FindDynablockFromNativeAddress` (`custommem.c`) and `getX64Address`
  (`dynarec/dynablock.c`). Then `profile_sample` gives the x86 code that
  runs, and a block that runs costs nothing more.
- A patch that writes the x86 address of each block at its start into the
  context of the CPU area, at the cost of one store at each block.
- A patch that writes the perf map of the blocks under WoW64, with
  `simpleperf` of the headset for the samples of the ARM code. Not known: if
  `simpleperf` of Meta Horizon OS reads such a map.

**What the emulator shows** (Wine of x86_64 with `wow64cpu.dll`, no Box64,
`profile_sample_outside=1000`, a render of 2682 by 2146): the samples have
x86 addresses of the game and of the mod, with callers on the stack
(`runblack.exe+0x419F21`, `DDRAW.dll+0x12708B`). Of 10604 samples in 449
frames 72.9 percent were in `win32u.dll` and 26.1 percent in `ntdll.dll`. A
stop in Wine is slow: the samples held the frame thread 24.5 ms in a frame
of 45 ms, and the frames of the scene came at 1 to 22 a second in place of
about 50. Under Box64 the same sampler gives the address of the last system
call, as the source says. The emulator of the tests is x86_64, so it has no
Box64: no run under Box64 was made. A run of `profile_sample_outside` on a
Quest 3 would show it.

## The variables of Box64

The x86 code of the game runs in `wowbox64.dll` of Box64 on the ARM CPU of a
headset. Two keys of the app give variables of Box64 to the game
(`play_rules.h`, `Box64Vars`, `GameEnvironment`):

- `box64_strongmem` (0 by default): the row "Strict memory order of the
  game (Box64, slower)" of the page "Testing" of the menu. Off (0): no
  variable. 1 to 3: `BOX64_DYNAREC_STRONGMEM=<n>`. An x86 CPU keeps the
  order of the reads and the writes of memory between threads, and an ARM
  CPU does not. The dynarec of Box64 then adds memory barriers: 1 at the
  writes to memory, 2 also at the SIMD instructions, 3 also more often. The
  game runs slower with each step. Box64 also takes 4, a model of the whole
  x86 order in the way of QEMU ("for evaluation purposes" in the
  `docs/USAGE.md` of Box64). The row ends at 3:
  `box64_env=BOX64_DYNAREC_STRONGMEM=4` with `box64_strongmem=0` gives 4. Code of the game or of a DLL that gives
  data to another thread with no lock needs that order, and a fault there
  can stop the game. Not known: if the stop of the game on the Quest comes
  from that. The change counts at the next "Play".
- `box64_env` (empty by default, only in `bwvr.ini`): more variables,
  `NAME=VALUE` with spaces between them, for example
  `box64_env=BOX64_DYNAREC_BIGBLOCK=0 BOX64_DYNAREC_SAFEFLAGS=2`. Only a name
  that starts with `BOX64_` and has only capitals, digits and `_` goes to
  the game, each name one time. Each other word gives one line "box64_env:
  ... does not go to the game". With `box64_strongmem` 1 to 3 the key sets
  `BOX64_DYNAREC_STRONGMEM`, and `box64_env` does not.

Both keys go to the game only with `wow64_cpu=box64` (the default, see "FEX
as the CPU of the 32 bit code"), and on x86_64.

**How WowBox64 reads them** (the source of Box64 0.4.4): `BTCpuProcessInit`
of `wine/wow64/wowbox64.c` calls `LoadEnvVariables` (`src/tools/env.c`),
which reads each variable with `GetEnvironmentVariableA`: the Windows
environment of the process. It reads only the variables that are
"available in WowBox64" (the column `wine` of `src/include/env.h`):
`BOX64_DYNAREC_STRONGMEM` (0 to 4) and `BOX64_DYNAREC_BIGBLOCK` are. Under
Windows it reads no box64rc file (`InitializeEnvFiles` has no part for
`_WIN32`). It writes each variable that it takes to `wine.log`: a line
"BOX64ENV: Variables overridden:", then one line for each variable.

**The way to the game.** The app puts the variables into the environment of
the loader of Wine that starts the game, not of `reg import`, `wineserver
-w` or `wineboot`. Wine gives that environment to `start.exe`, and
`start.exe` to `runblack.exe`. The server of `reg import` ended at
`wineserver -w`, so the loader of the game also starts a new wine server
(see "Play in the app"), and that server gets the variables too. It is a
program of the ABI with no Box64, so they change nothing there. The log of the app has one line at the start
of the game, "game : box64_strongmem=N, box64_env="...". The game gets
...". At its start the mod writes the variables of Box64 of its own
environment: "box64 : the environment of the game has N variables of
Box64: ...". On a headset `wine.log` also has the lines of WowBox64.

**What the emulator shows** (`emu_game_test.ps1 ... -Box64StrongMem 2
-Box64Env 'BOX64_DYNAREC_BIGBLOCK=0 FOO=1'`, both games): the save of the
menu kept `box64_strongmem=2`. The app logged "The game gets the variables
of Box64 BOX64_DYNAREC_STRONGMEM=2 BOX64_DYNAREC_BIGBLOCK=0" and "box64_env:
FOO=1 (the name does not start with BOX64_) does not go to the game". The
mod in `runblack.exe` logged "the environment of the game has 2 variables
of Box64: BOX64_DYNAREC_STRONGMEM=2, BOX64_DYNAREC_BIGBLOCK=0", and
`/proc/<pid>/environ` of `runblack.exe` (read as root by the test) has the
two variables and no `FOO`. The emulator runs the x86_64 Wine with
`wow64cpu.dll` and no Box64, so only a headset can show what Box64 does
with the variables: the lines of WowBox64 in `wine.log`, the speed, and if
the game still stops.

## FEX as the CPU of the 32 bit code

The tree of arm64-v8a has two CPU DLLs for the 32 bit x86 code of the game:
`wowbox64.dll` of Box64 and `libwow64fex.dll` of FEX. FEX is the translator
of x86 code that Valve uses for the Steam Frame. Its WoW64 DLL is a CPU of
the WoW64 of Wine on ARM64, as `wowbox64.dll` is: a DLL for Windows on
ARM64 with the `BTCpu` functions of the CPU interface. The key `wow64_cpu`
chooses one of them (`wine_rules.h`, `Cpu`, `play_rules.h`, `FexVars`).

**The build** (`bwvr/tools/android/build_fex.ps1 -Source <FEX source>
-Mingw <bin folder of llvm-mingw>`). The source of FEX is not in the repo.
The script takes a clone of a release tag with its submodules (for example
`local/third-party-src/FEX-2610`), and stops when a part of the source that
goes into the DLL is not there. It configures FEX with the toolchain file
of FEX for mingw (`Data/CMake/toolchain_mingw.cmake`, the triple
`aarch64-w64-mingw32`) and the options of the Wine DLL build of FEX
(`.github/workflows/wine_build`: `Release`, no LTO, no assertions, no
tests, `TUNE_ARCH=generic`, `TUNE_CPU=none`), and it builds the target
`wow64fex` with the Ninja of the Android SDK in
`bwvr/build-android/fex-wow64`. The build runs `python3` (the generator of
the options) and `dd` (the mark of a builtin DLL of Wine in the PE header).
The script takes the `python3` of llvm-mingw and the `dd` of Git for
Windows. It downloads nothing. It writes a copy with no debug data to
`bwvr/out/android/fex/libwow64fex.dll`, and the license texts to
`bwvr/out/android/fex/licenses`: `fex.txt` (MIT) and one `fex_<part>.txt`
for each part of the source in the DLL (fmt, xxhash, rpmalloc, range-v3,
unordered_dense, tiny-json, SoftFloat-3e, cephes, cpp-optparse, and the
copyright lines of the files of musl in the CRT of FEX). With FEX-2610 and
llvm-mingw 20261006 the clean build takes 19 s, and the DLL is a PE file
for ARM64 (machine 0xAA64) of 4,976,640 bytes with 22 exports `BTCpu...`
and `__wine_get_unix_opcode`. It imports only `ntdll.dll` and `wow64.dll`.
The PE header has the time of the link, so each build gives other bytes.

`pack_wine.sh` puts the DLL into `aarch64-windows` of the tree beside
`wowbox64.dll`, and the license texts into `bwvr/out/android/wine/licenses`.
`build_apk.ps1` puts each `fex*.txt` into `assets/licenses/`. `check_apk.ps1`
checks that the tree has both DLLs, that each is a PE file for ARM64 with
the name `BTCpuProcessInit`, and that `fex.txt` is in the APK.

**One import of FEX that Wine 11.0 does not have.** `pack_wine.sh` lists
each function that a CPU DLL imports and that the tree does not export.
For `wowbox64.dll` there is none. For `libwow64fex.dll` there is one:
`ntdll.dll.RtlWow64SuspendThread`. Wine gives such an import a stub, and a
call of the stub stops the program with an exception. FEX calls it only in
`BTCpuThreadTerm` when a thread ends another thread (`TerminateThread`):
it stops that thread first, so that the thread holds no lock of the JIT
when FEX frees its state. A thread that ends itself does not call it.
`runblack.exe` and the DLLs of the game folder do not import
`TerminateThread` (their import tables), and no log of a Quest 3 shows a
call: each line "Terminating" of FEX in `wine.log` is of a thread that ends
itself.

**SuspendThread under FEX in Wine.** On Windows `RtlWow64SuspendThread`
stops a thread of x86 code at a safe point: the start of a block of
translated code (the suspend doorbell of the CPU area, which FEX reads in
`UnlockJITContext` and at the instruction `brk #0xCAFE`). Wine 11.0 has no
such suspend. `wow64_NtSuspendThread` (`dlls/wow64/process.c`) gives the
call of the 32 bit code to `NtSuspendThread` as it is. It does not call
`BTCpuSuspendLocalThread` of FEX, which also only calls `NtSuspendThread`.
So `SuspendThread` stops a thread at each place, also in the JIT of FEX.
Each translation of a block holds a shared lock of `CodeInvalidationMutex`
(`CompileBlock` of `FEXCore/Source/Interface/Core/Core.cpp`). A change of
the protection of a page with code (`VirtualProtect`, through
`InvalidationTracker::HandleMemoryProtectionNotification`) and the fault of
a write to a page with translated code (`HandleRWXAccessViolation`) wait
for the unique lock of the same mutex. Each change of protection, each
`FlushInstructionCache` and each such fault also take `ThreadCreationMutex`
of the WoW64 module of FEX (`Source/Windows/WOW64/Module.cpp`). When a
thread stops another thread that holds one of these locks, and then changes
the protection of a page with code, it waits for the thread that it
stopped, and the process hangs.

MinHook does that: `MH_EnableHook`, `MH_DisableHook` and `MH_RemoveHook`
stop each other thread of the process (`Freeze` of `hook.c`), then call
`VirtualProtect` on the target. So each hook that the mod enables or
disables while other threads run x86 code can hang the game under FEX, when
one of these threads translates a block at that time. The hooks of the mod
come in two groups at the start (see "The code that changes, on a Quest
3"), the timers of `walk_parts=1` come at the third scene walk, and some
hooks of the mod come and go while the game runs. Not shown: that the hangs
in the load of the game with `walk_parts=1` on a Quest 3 come from this. A
stop with the main thread of the game at 0 percent of a core in the line of
a stop (`time_log`) fits it. The samplers of the mod (`hang_watch`,
`profile_sample`) also stop the frame thread. They change no protection
while it is stopped, but they can also hang, less often: when the sampler
must translate a block while another thread waits for the unique lock, the
sampler can wait behind that thread, which waits for the stopped thread.

**The Unix library of FEX** (`libwow64fex.so` of `Source/Windows/UnixLib`)
is not in the tree. FEX loads it when it is there, and works with no such
library. With no library FEX has no hardware TSO (a mode of some ARM CPUs
that are not in a Quest), no name of its memory regions, and no file
mapping of the code cache. With the code cache on (`fex_disk_cache=1`, see
"The code cache of FEX") FEX reads the files of the cache with reads of the
file in place of a mapping.

**The switch** (`wow64_cpu`, the row "Translator of the game (Box64 or
FEX)" of the page "Testing", above the row of the memory order of Box64):

- `box64` (the default): `wowbox64.dll`.
- `fex`: `libwow64fex.dll`.

A step or the trigger gives the other value. The help says what each one
is. The save writes `wow64_cpu=box64` or `wow64_cpu=fex`. Each other value
of the key is `box64`. The change counts at the next "Play".

At the setup of Wine the app copies both CPU DLLs of the tree into
`system32` of the prefix (a new prefix, and a prefix of another tree).
`wine.inf` keeps `wowbox64.dll` as the default value of
`HKLM\Software\Microsoft\Wow64\x86`, so wineboot runs with Box64. At each
"Play" on arm64-v8a the app writes the value of the choice into `game.reg`
(`play_rules.h`, `GameRegistry`), before the known DLLs and the audio
driver of the same import:

```
[HKEY_LOCAL_MACHINE\Software\Microsoft\Wow64\x86]
@="libwow64fex.dll"
```

This is the registry of the Wine prefix in the app, not of a PC. With
`box64` the value is `wowbox64.dll`, the value of `wine.inf`. The WoW64 of
Wine reads the value at the start of each 32 bit process
(`get_cpu_dll_name` of `dlls/wow64/syscall.c`, with `KEY_WOW64_64KEY`), so
`runblack.exe` gets the DLL of the choice. When `system32` has no file of
the choice, the app copies the CPU DLLs again before the import. The log of
the app has one line at the start of the game: "game : wow64_cpu=fex. The
CPU of the 32 bit code of the game is libwow64fex.dll of FEX", and the line
of `reg import` names the value.

On x86_64 (the emulator) the 32 bit code runs natively through
`wow64cpu.dll` of Wine. The key does nothing there: `game.reg` has no key
`Wow64\x86`, the game gets the variables of Box64 as with `box64`, the value
of the row has "(no effect here)", and its help says that the row does
nothing on this device. The log says "game : wow64_cpu=fex. x86_64: the 32
bit code runs natively through wow64cpu.dll of Wine. The key does nothing
here".

**The variables of FEX** (`fex_env`, empty by default, only in `bwvr.ini`):
`NAME=VALUE` with spaces between them, for example
`fex_env=FEX_X87REDUCEDPRECISION=1 FEX_SILENTLOG=0`. Only a name that starts
with `FEX_` and has only capitals, digits and `_` goes to the game, each
name one time. Each other word gives one line "fex_env: ... does not go to
the game". The variables take the way of the variables of Box64 (see "The
variables of Box64"): the environment of the loader of Wine that starts the
game. With `wow64_cpu=fex` only `fex_env` goes to the game, and the log
says that `box64_strongmem` and `box64_env` do not go. With `box64` only
the variables of Box64 go, and a `fex_env` with a value gives the line
"fex_env=... does not go to the game: the CPU is not FEX".

**The code cache of FEX** (`fex_disk_cache`, 0 by default, only in
`bwvr.ini`, `play_rules.h`, `FexVars`). FEX-2610 can keep the code that it
translates in files (the disk cache of FEXCore,
`FEXCore/Source/Interface/Core/DiskCache.cpp`), and a later start of the
game takes each block from there in place of a new translation. The key of
a block of a module is its offset in the module and the module (its name,
its time stamp and its size). FEX compares a hash of the x86 code of the
block before it uses an entry, so a block that a hook of the mod changed
gets a new translation. The name of the folder of a cache has a hash of the
version of the cache and of the features of the CPU. The name of the file
has a hash of each setting of FEX that changes the translated code (for
example `FEX_TSOENABLED` and `FEX_X87REDUCEDPRECISION`), so each set of
`fex_env` has a cache of its own. At its start FEX deletes each folder in
the path of the cache that does not have the name of its own folder
(`PruneStaleEntries` of `DiskCache.cpp`): the folders of another version or
of another CPU, and each other folder in that path. So the path must be a
folder that only FEX uses.

- `fex_disk_cache=0`: no variable. FEX translates each block at each start.
- `fex_disk_cache=1`: with `wow64_cpu=fex` the game gets `FEX_DISKCACHE=1`
  and `FEX_DISKCACHEPATH=C:\bwvr\fex-cache\`, the folder
  `drive_c/bwvr/fex-cache` of the prefix in the internal folder of the app.
  The app makes the folder.

`fex_env` never sets `FEX_DISKCACHE` or `FEX_DISKCACHEPATH`, also with
`fex_disk_cache=0`: a path in `fex_env`, for example the game folder, would
lose each folder in it (`Profiles` too). A word with one of the two names
gives the line "fex_env: FEX_DISKCACHEPATH=... (only fex_disk_cache sets
it) does not go to the game".

The log of the app has one line before the game, "game : fex_disk_cache=1.
FEX keeps the code that it translates in C:\bwvr\fex-cache\ (<the folder>).
Before the game the folder has N files, M MB", and one line with the files
and the MB after the game. With Box64, and on x86_64, the key gives no
variable, and the log says "fex_disk_cache=1 does nothing: the CPU of the
game is not FEX". A thread of FEX with a low priority ("FEX:DiskCache")
writes the new entries. The file of a cache can get to 1 GB
(`FEX_DISKCACHEMAXFILESIZE`). The key has no test key: FEX reads it at the
start of the game. Not known: if FEX writes and reads the files under the
WoW64 of Wine (the two lines of the size show it), how much shorter the
load and the stutters of new code get with a full cache, and if an entry of
the cache can be wrong for this game.

**How FEX reads its settings** (the source of FEX-2610).
`BTCpuProcessInit` of `Source/Windows/WOW64/Module.cpp` calls
`FEX::Config::LoadConfig` (`Source/Common/Config.cpp`) with the Windows
environment of the process. The layers, the last one wins: the global and
the main config file (`Config.json`, the main one in `.fex-emu` of the home
folder that Wine gives in `WINEHOMEDIR`, or in `.config/fex-emu`), the
config file of the program (`AppConfig/runblack.exe.json`), the file of
`FEX_APP_CONFIG`, then the environment. In the environment a
variable `FEX_<OPTION>` sets the option `<OPTION>` (`EnvLoader`). So the
environment is enough, and the app writes no config file. The prefix of the
app has no config file of FEX.

**How FEX reports itself.** By default FEX writes no log
(`FEX_SILENTLOG=1`). With `FEX_SILENTLOG=0` in `fex_env` it writes its
messages through `__wine_dbg_output` of Wine to stderr, so they are in
`wine.log`: a letter of the level, the thread and the text, for example
"I 1F4 Terminating ...". FEX writes no line with its version at the start.
`wine_debug=+loaddll` makes Wine log the load of `libwow64fex.dll`. The
CPUID leaf 0x40000000 of the x86 code gives "FEXIFEXIEMU", and the leaf
0x40000001 with the sub-leaves 1 and 2 gives the version of FEX
("FEX-2610").

**The settings of FEX that can matter for this game** (the names of the
environment, the defaults of `FEXCore/Source/Interface/Config/Config.json.in`
of FEX-2610. A value 1 is on, 0 is off):

| Variable | Default | What it does, and what it costs |
| --- | --- | --- |
| `FEX_TSOENABLED` | 1 | The memory order of x86 (TSO): each load and store of the x86 code is an ordered access of ARM. It costs time at each access. 0 is faster, and the source says that 0 "is highly likely to break any multithreaded application". The Quest has no hardware TSO, so FEX always emulates it. See "The memory order under FEX" |
| `FEX_VECTORTSOENABLED` | 0 | With TSO, also the loads and stores of the vector registers (SSE) are ordered. 1 costs more time at each such access |
| `FEX_MEMCPYSETTSOENABLED` | 0 | With TSO, also `REP MOVS` and `REP STOS` are ordered. 1 costs more time at each copy and fill |
| `FEX_HALFBARRIERTSOENABLED` | 1 | With TSO, an unaligned load or store gets a half barrier after a fault, in place of a full atomic. The source says that an aligned access of the same code then is not atomic |
| `FEX_STRICTINPROCESSSPLITLOCKS` | 0 | A global lock for an unaligned atomic that crosses 16 bytes or a cache line. 1 is safer and slower |
| `FEX_VOLATILEMETADATA` | 1 | Uses the volatile metadata of a PE file (of newer compilers of Microsoft) to skip TSO where the code does not need it. A file with no such data gets TSO as set |
| `FEX_EXTENDEDVOLATILEMETADATA` | empty | A list of modules (and ranges) with no TSO, for WoW64, for example `runblack.exe`. Faster for that code, and a fault of the memory order there is possible. The modules have `:` between them. A name must have the case of the line "Load module" of FEX in `wine.log` (`runblack.exe`, `ScriptLibraryR.dll`, `LHaudiodllR.dll`). It does nothing with `FEX_TSOENABLED=0` |
| `FEX_SMCCHECKS` | `mtrack` | The checks of code that changes (self-modifying code): `mtrack` watches the pages of translated code and translates again after a write, `full` checks the code before each run (slow), `none` makes no check (fast, and code that changes runs old). The value is a word: each other value gives `none` |
| `FEX_MULTIBLOCK` | 1 | A block of translated code can hold more than one x86 block. 1 gives faster code, and the source says that it "can cause long JIT compilation times and stutter". 0 gives smaller blocks |
| `FEX_MAXINST` | 5000 | The most x86 instructions in one block |
| `FEX_DYNAMICL1CACHE` | 1 | The first cache of each thread that finds the translated block of an x86 address at an indirect jump, a call through a pointer and a return with no entry of the call. 1 starts it small and makes it larger or smaller by its misses each second. 0 gives the largest size from the start: fewer misses, more memory |
| `FEX_DISABLEL2CACHE` | 1 | 1: a miss of the first cache takes a lock and looks in the map of all blocks. 0: a second cache with no lock (a table for each page of x86 code) comes before the map. 0 can be faster in code with many indirect jumps, and it takes more memory |
| `FEX_DISKCACHE` | 0 | The code cache in files. The key `fex_disk_cache` sets it (see "The code cache of FEX") |
| `FEX_X87REDUCEDPRECISION` | 0 | 0: the x87 instructions run with 80 bit precision in software (SoftFloat-3e), which is slow. 1: 64 bit precision with the floating point of ARM, which is faster. The source says that 1 "may result in rendering bugs" |
| `FEX_SILENTLOG` | 1 | 0: the messages of FEX go to `wine.log` |

The game is a program of 2001 and uses x87 for its floating point, so
`FEX_X87REDUCEDPRECISION=1` can matter much for the speed. Not known: how
much, and if the picture of the game changes.

**The code that changes, on a Quest 3** (`fex_env=FEX_SILENTLOG=0`, the
lines "Add SMC interval" and "Handled self-modifying code" of FEX in
`wine.log`). With `FEX_SMCCHECKS=mtrack` FEX protects each page with
translated code against writes. A change of the protection of a page with
code to a protection with write and execute gives one line "Add SMC
interval", and FEX drops the translated code of the page. A write to a
protected page gives one line "Handled self-modifying code", and FEX drops
the translated code of the page. In one game of about 75 s:

- 282 lines "Add SMC interval", of them 138 at the load of the DLLs of the
  game and of the mod, and 121 at the start of the device of the game. These
  are the hooks of the mod (MinHook makes the target writable for its jump)
  and the patches of the mod: in `runblack.exe` (pages from 0x0042A000 to
  0x00879000), in `ddraw.dll` of the mod (pages from 0x791E0000) and in
  `QMIXER.dll` (0x18001000 to 0x18004000). After the start of the device:
  one line.
- 10 lines "Handled self-modifying code". 7 of them at the start: a write
  at the offset 8 of a page of code that is not in a module (0x022A0000,
  0x02980000). This fits a page of MinHook: its offset 8 is the count of
  the used slots of the page (`usedCount` of `buffer.c`), which MinHook
  writes for each new hook. 3 of them while the game runs, on pages of the
  same kind at 0x09560000 and 0x09570000.

So while the game runs, FEX drops translated code about 4 times in a
minute, and in nearly each frame 0 times. The checks of the code that
changes cost no time in a frame. `FEX_SMCCHECKS=none` gives no speed there,
and it is a risk: a hook of the mod on code that FEX translated before
would not work.

Two other lines of FEX come while the game runs, each for one exception of
the CPU. "Call-ret stack inbalance" (515 lines in the same game, about 7 a
second): the stack of the return addresses of FEX ran out at a return with
no call, and FEX sets it back. "Reconstructing context" with the code
C0000005 (102 lines): an access violation of the x86 code at 0x7BF05C69, in
`IsBadReadPtr` of `kernel32.dll`, which its handler catches.

**The memory order under FEX** (`FEX_TSOENABLED`). The Quest 3 has no
hardware TSO, so with `FEX_TSOENABLED=1` each load of the x86 code is a load
with acquire and each store a store with release (an unaligned access gets
a half barrier). With 0 each load and store is a plain access, and a thread
can see the writes of another thread in another order than the x86 code
wrote them. Each instruction with a lock prefix (`lock xadd`, `lock
cmpxchg`, `xchg` with memory) stays an atomic of ARM with acquire and
release (`ldaddal`, `casal` and `swpal` in `AtomicOps.cpp` of the JIT of
FEX), also with 0. So the functions `Interlocked...` of Windows,
the critical sections of Wine, and the ring of the live stream and the
input channel of the mod (`_InterlockedExchange` and
`_InterlockedCompareExchange` in `stream_ring.h` and `input_channel.h`) keep
their order with 0. What can break with 0: code that gives data to another
thread with a plain store of a flag after the data, and a plain load of the
flag on the other side. On x86 a compiler makes a store with release and a
load with acquire (`std::atomic` with these orders, `volatile` of MSVC,
`__atomic_store_n` of GCC and Clang) from a plain `mov`. Such code can be
in the 32 bit DLLs of Wine, in the code of the game and in the code of the
mod. A fault there is rare and random: a wrong value, a lost wake or a
hang.

`FEX_TSOENABLED=1 FEX_EXTENDEDVOLATILEMETADATA=runblack.exe` gives no order
to the code of `runblack.exe` (as with 0) and the order of x86 to each
other module. `ddraw.dll` of the mod has the volatile metadata of MSVC (the
line "Loaded volatile metadata for 791C0000: 357 entries" of FEX), so with
`FEX_VOLATILEMETADATA=1` (the default) only its 357 accesses to `volatile`
data are ordered. The cost is the order in the DLLs of Wine (for example
the heap of `ntdll.dll` and `memcpy` of `msvcrt.dll`) and in the other DLLs
of the game. Not known: the frames a second with each value.

**What the emulator shows** (`emu_game_test.ps1 -NewProfile -ProfileOk
480,670 -SceneEyes -SkipClick 547,575 -TestSave pen -PlaySeconds 90
-Wow64Cpu fex -FexEnv FEX_SILENTLOG=0`, 188 checks, 0 failed): in both
games the app logged "wow64_cpu=fex. x86_64: the 32 bit code runs natively
through wow64cpu.dll of Wine. The key does nothing here", the line of `reg
import` has "no CPU value", the save of the menu kept `wow64_cpu=fex` in
`bwvr.ini`, the app logged that `fex_env` does not go to the game, and
`/proc/<pid>/environ` of `runblack.exe` (29 variables) has no variable of
FEX. The game ran as with no key: the new game and the test save `pen`
(43.5 and 59.4 frames a second of the game in the log of the app). The
emulator runs the x86_64 tree with `wow64cpu.dll`, so FEX cannot run
there. With `-Wow64Cpu fex` and `fex_disk_cache=1` (the run of "The cores
of the headset") the log has "fex_disk_cache=1 does nothing: the CPU of the
game is not FEX" in each game.

**Only a headset can show** the effect of the settings above on the
frames a second and on the picture, if the code cache works under the WoW64
of Wine, and if a hang of the game is a thread that `SuspendThread` stopped
in the JIT.

## The cores of the headset

`thread_cores` and `thread_bind` (only in `bwvr.ini`, `thread_cores.cpp`,
the rules in `core_rules.h`). The line of the threads of the time log ("The
time log of the app") gives the CPU time of each thread over 10 s, and only
the core that it ran on last. These keys give the cores each second, and
they can bind threads to cores.

While a game runs, a thread of the app ("bwvr cores") reads each second the
line of each thread of the game process and of the app
(`/proc/<pid>/task/<tid>/stat`, as the time log). The thread runs only with
`thread_cores=1` or with a binding. At Play the app writes the cores of the
device to `bwvr-quest.log`: the online cores
(`/sys/devices/system/cpu/online`), the top speed of each core
(`cpufreq/cpuinfo_max_freq`, 0 when the file is not there) and its capacity
(`cpu_capacity`), the cores that the app can use (`sched_getaffinity`, and
`Cpus_allowed_list` of `/proc/<pid>/status`), its cpuset
(`/proc/<pid>/cpuset`), and the ids of the session thread and of the play
thread. When the thread finds the game process, it writes the same for the
game: "cores : the game is the process N (runblack.exe). It can use the
cores ...".

- `thread_cores=0` (the default): no line of the cores each second. Only
  the lines of the cores of the device and of the app at Play.
- `thread_cores=1`: each second two lines. The line of the game: the count
  of its threads, its CPU time in cores, the sum of the CPU time of its
  threads by the last core of each thread (an estimate, because a thread
  can move in the second), and each thread with CPU time, the most first:
  its name, its id, "(the main thread)", its last core and its percent of
  one core (user + system). Then the count of the threads with no CPU time.
  The line of the app: the first 8 threads with CPU time, with "(the
  session thread)" and "(the play thread)". With a binding each thread also
  has its place, from its real mask (`sched_getaffinity` in that second):
  `fast` (the fast cores), `other` (the other cores) or `cores <list>`. At
  the end of the line of the app: the new bindings in the second and the
  threads that got their cores again. With `time_log=1` the threads of
  the app have their names (`bwvr main`, `bwvr play`).

**The binding** (`thread_bind`). The fast cores are the cores that the app
can use with a higher top speed than the slowest of them (with no top
speed: the capacity). `thread_bind_cores` (empty by default) gives them as
a list of Linux, for example `thread_bind_cores=4-5`. The key threads are
the main thread of the game (it runs the loop of the game and the frames of
the mod), the session thread of the app (`bwvr main`) and the play thread
of the app (`bwvr play`, `frame_loop=1`).

- `thread_bind=0` (the default): no binding. Android chooses the cores.
- `thread_bind=1`: the key threads get the fast cores, and each other
  thread of the game and of the app gets the other cores that the app can
  use (`sched_setaffinity` of each thread).
- `thread_bind=2`: the key threads get the fast cores, and the other threads
  stay as they are.

There is no binding when no core of the app is faster than another and
`thread_bind_cores` is empty, when no core of `thread_bind_cores` is a core
of the app, or when the fast cores are each core of the app. The log says
why. Each second the app binds the new threads, and it reads the mask of
each thread that it bound (`sched_getaffinity`). A thread with another mask
(for example after a change of the cpuset of the app) gets its cores again,
with one line "thread_bind: the thread N (name) of the game had the cores
... It has the cores ... again". The line "thread_bind: the
main thread of the game (N, runblack.exe) is on the fast cores 4-5 now
(sched_getaffinity gives 4-5)" names each key thread with its cores after
the binding. A fault of `sched_setaffinity` gives one line with its errno,
one time for each errno, and that thread gets no second try in that place. At the end of the game each thread of the app
gets the cores of the app again. A new process and a new thread get the
cores of the thread that starts them. So with `thread_bind=1` the processes
that the app starts after Play (the loader of the game, the game process,
the new wine server and the services) start on the other cores, because the
thread of the app that starts them (`bwvr game`) is on the other cores. The
app then binds only the threads of the game process: the main thread of the
game to the fast cores, the other threads to the other cores. With
`thread_bind=2` they keep the cores of the app. The keys count at the next
Play. There is no test key: the app has no hotkeys.

**The cost.** Each second about 60 reads of small files of `/proc` and two
lines of the log on a thread of its own, one call of `sched_getaffinity` for
each thread that the app bound and for each thread in a line, and one call
of `sched_setaffinity` for each new thread.

**What the emulator shows** (`emu_game_test.ps1 -NewProfile -ProfileOk
480,670 -SceneEyes -SkipClick 547,575 -TestSave pen -PlaySeconds 90
-FrameLoop 1 -Wow64Cpu fex -IniExtra thread_cores=1,thread_bind=1,
thread_bind_cores=2-3,fex_disk_cache=1`, 208 checks, 0 failed, 4 cores).
The cores of the emulator have no top speed and no capacity (0 MHz, 0), so
only `thread_bind_cores` gives fast cores there. The app can use the cores
0-3, its cpuset is `/top-app`. In each game the log has "thread_bind=1. The
fast cores 2-3 get the main thread of the game, the session thread and the
play thread of the app. Each other thread of the game and of the app gets
the cores 0-1", then the session thread, the play thread and the main
thread of the game "on the fast cores 2-3 now (sched_getaffinity gives
2-3)". The game process could use the cores 0-1 when the app found it (the
cores of `bwvr game`). No line of a fault of `sched_setaffinity`. Of the
508 lines of the game in the two games, 506 have the main thread of the
game on the core 2 or 3 with the place `fast` (from its real mask). The
other 2 are the first line of each game: the second in which the binding
came, with the core from before it. Each other thread of the game with CPU
time (`wine_dsound_mix`, `AudioTrack`) was on the core 0 or 1 with the
place `other`. The line of the app counts 11, 3 and 1 new bindings in the
seconds with new threads, and no thread that got its cores again. At the
end of each game "24 threads of the app have the cores 0-3 again, 2 of them
did not (a thread that ended)". The frames a second of the game: 43.2
(game 1) and 58.7 (game 2, the test save) in the log of the app.

Some runs of this test do not end well, with these keys and with no key
(10 or 11 failed checks of 95 to 97): in game 1 the log has no "Quit game"
in 900 s, the menu over the game does not come, and the script stops. Then
the script does not pull the log of the app, so the cause is not known. It
comes also with the keys at 0, so it is not of the binding.

With the keys at 0 (the same test with no key, 204 checks, 0 failed) the log has only
the two lines "cores" of each Play (the cores of the device and of the
app), no line each second and no binding, and the game made 46.7 and 59.2 frames
a second.

**Only a headset can show** the cores of a Quest 3 and their speeds, the
cores of the cpuset of the app, if Android lets the app bind the threads of
the game process, and if the game makes more frames a second with its main
thread on a fast core.

## Wine in the app

The APK carries Wine 11.0 in the WoW64 mode, one tree for each ABI, and the
app sets it up at its start. It starts Windows programs in Wine as child
processes: the test program, and the game ("Play in the app").

**The two builds** (`bwvr/tools/android/build_wine.sh <work folder> <abi>`,
in a Linux of WSL2, see the head of the script for the work folder):

- `aarch64` for the headsets: `--host=aarch64-linux-android`,
  `--enable-archs=i386,aarch64`. All of Wine runs as ARM64 code, and the 32
  bit x86 code runs in `wowbox64.dll` of Box64 (`build_box64.ps1 -Mingw`).
- `x86_64` for the Android emulator of the tests: `--host=x86_64-linux-android`,
  `--enable-archs=i386,x86_64`. The 32 bit code runs natively through
  `wow64cpu.dll` of Wine. So the emulator tests the whole path of the app
  except the CPU emulator.

Each build has FreeType 2.13.3 as a shared library (`libfreetype.so`, with
the zlib of the NDK, no HarfBuzz, PNG, Brotli or bzip2), so Wine has the
TrueType fonts of `share/wine/fonts` (Tahoma, Marlett, Symbol, Webdings,
Wingdings) and makes its bitmap fonts (`.fon`) from them. The tools of Wine
for the build get a FreeType of the same source for the Linux of WSL2. Wine
gets no display driver (`--disable-wineandroid_drv`, no X), and no debug
data. For x86_64 the Unix side needs `-D_GNU_SOURCE -include strings.h`:
the code of the LDT calls `ffs`, which bionic declares in `strings.h`.
`build_wine.sh` applies the patches of `bwvr/tools/android/wine-patches/`
to the source one time. `0001-ntdll-no-casefold-ioctl-on-fuse.patch`: Wine
asks each directory that it looks into for the casefold flag
(`FS_IOC_GETFLAGS`), and the FUSE daemon of Android 14 (MediaProvider with
fuse-bpf) stops on that ioctl of a directory of `Android/data` ("Check
failed: active_nodes_.find(node)"). Then the app and each process with the
shared storage lose it. The app alone shows it in the emulator: an `open`
of the game folder, `fstatat` and `fstatfs` work, and the ioctl stops the
daemon. With the patch Wine makes no such ioctl on a FUSE file system,
which has no casefold flag. Not known: if the FUSE daemon of the Quest has
the same fault. `0002-ntdll-read-image-sections-on-noexec-fs.patch`: the
shared storage has `noexec`. The `mmap` of a PE file there works, but the
`mprotect` of a code section to `PROT_EXEC` fails, so with no patch the game
cannot run from the game folder (see "The noexec storage" in "Play in the
app"). With the patch `map_image_into_view` reads each section of the
image into memory, as Wine does for a file on a removable medium, when
`fstatvfs` gives `ST_NOEXEC` or when a mapping of the first page of the file
with `PROT_EXEC` fails with `EPERM` or `EACCES`. The second test is
necessary on a Quest 3: its FUSE passthrough gives `fstatvfs` the flags of
the file system under FUSE, with no `ST_NOEXEC`, although the mount has
`noexec`. `0003-wineaaudio.patch`: Wine has no audio driver for Android
(`winealsa`, `winepulse` and `wineoss` need what Android does not give an
app). The patch adds `dlls/wineaaudio.drv`, an audio driver of `mmdevapi`
with AAudio of the NDK, and its check in `configure` (the header
`aaudio/AAudio.h` and `libaaudio`). `build_wine.sh` stops when the configure
of Wine has no `dlls/wineaaudio.drv`. What the driver does: `audio.md`,
"In Wine on Android". `0004-server-ntdll-any-owner-in-a-container.patch`:
wineserver and each program of Wine stop when the prefix, its parent or the
folder of the server does not belong to `getuid` ("... is not owned by
you"). Lepton, the Android container of the Steam Frame, shows each file
with the owner 0, and the app gets its Android uid from `getuid`. With
`WINE_BWVR_ANY_OWNER=1` in the environment the five owner tests accept
each owner. The tests of the mode (no access for other users) stay.

**The owner of the folders** (`wine_owner_check`, `wine_rules::AnyOwner`).
Before Wine makes a folder, the app compares the owner of its internal
folder with its uid, and logs one line:

```
wine         : wine_owner_check=2. The internal folder belongs to 0, the app is 10014. Wine accepts each owner of its folders (WINE_BWVR_ANY_OWNER=1)
```

| Value | What Wine does |
| --- | --- |
| 2 (the default) | Each owner only when the internal folder has another owner than the app. On a Quest 3 and in the emulator the folder belongs to the app, so Wine tests the owner |
| 1 | The test of Wine, always |
| 0 | Each owner, always |

The environment of the app cannot give `WINE_BWVR_ANY_OWNER`: the app
drops each inherited name that starts with `WINE`.

**A system call that the container traps** (`wine_sigsys`,
`wine_rules::SigsysEnosys`, `0005-ntdll-enosys-for-a-trapped-system-call.patch`).
The seccomp filter of the apps in Lepton stops a process with SIGSYS at
`membarrier` (arm64 system call 282), and `NtFlushProcessWriteBuffers` of
Wine calls it, the first time from `__wine_main`. With
`WINE_BWVR_SIGSYS_ENOSYS=1` ntdll of arm64 installs a handler of SIGSYS at
the start of `__wine_main`, before its first system call: the trapped call
gives `-ENOSYS`, and `wine.log` has one line for each number
("bwvr: the container does not allow the system call 0282, it gives
-ENOSYS"). With no `membarrier`, `NtFlushProcessWriteBuffers` does no
barrier (a FIXME of Wine). wineserver does not load ntdll, so a trap there
still stops it.

| Value | What a trapped system call does |
| --- | --- |
| 2 (the default) | `-ENOSYS` only when the internal folder has another owner than the app (the test of `wine_owner_check`). On a Quest 3 and in the emulator: SIGSYS, as Android does |
| 1 | `-ENOSYS`, always |
| 0 | SIGSYS stops the process, always |
Wine for Android installs `bin` and `lib` into a folder of the ABI of
Android (`arm64-v8a`, `x86_64`) beside `share`, and its loader is
`lib/wine/<arch>-unix/wine`, in the folder of `ntdll.so`.

**The runtime tree** (`pack_wine.sh <work folder> <abi>`) takes from the
install only what a 32 bit program needs, strips each ELF file, checks the
machine of each ELF and PE file, and writes `bwvr/out/android/wine/<abi>.tar`
(ustar), `<abi>.id` (the build id: a hash of the names and the bytes of the
tree), the list of its files and the list of what it left out. It leaves
out:

| What | Why |
| --- | --- |
| `ddraw`, `ddrawex`, each `d3d*`, `wined3d`, `dxgi`, `opengl32`, `vulkan-1`, `winevulkan` (each architecture, also the Unix library) | "Only Vulkan": the mod is the DirectDraw of the game, the app draws, and Wine gets no graphics DLL |
| `include`, `share/man` | headers and man pages |
| the import libraries (`lib*.a`) | only a build uses them |
| the scripts of `bin` (`wineboot`, `winecfg`, `notepad` and the others), all but `wineserver` | they need `/bin/sh`, and the app starts the loader itself |
| `notepad`, `wordpad`, `write`, `winemine`, `winhlp32`, `iexplore`, `winefile` | programs with a window for a person at a desktop. A game does not start them |

`wineandroid.drv` is not built. The tree of arm64-v8a also gets
`wowbox64.dll` and `libwow64fex.dll` in `aarch64-windows` (see "FEX as the
CPU of the 32 bit code"). In both trees `pack_wine.sh` adds the
default value of `HKLM\Software\Microsoft\Wow64\x86` (the CPU of the 32 bit
code: `wowbox64.dll` and `wow64cpu.dll`) to the section `[Wow64]` of
`wine.inf`. wineboot reads that section in a 64 bit process before it runs
the 32 bit part of `wine.inf`, and the WoW64 of Wine takes `xtajit.dll` on
ARM64 when the value is not there (`dlls/wow64/syscall.c`). A seed of
`system.reg` would not work: the wine server makes the link
`CurrentControlSet` only for a new `system.reg`. The tree has no symbolic
links (`pack_wine.sh` checks it), and the app takes none from the tar file.

| Tree | Files | Bytes | In the APK |
| --- | --- | --- | --- |
| install of `x86_64` | | 514,308,971 | |
| tree of `x86_64` | 1592 | 359,945,233 | `assets/wine/x86_64.tar`, 360,826,880 bytes, compressed |
| install of `aarch64` | | 811,051,089 | |
| tree of `arm64-v8a` | 1591 | 637,808,737 | `assets/wine/arm64-v8a.tar`, 638,689,280 bytes, compressed |

Each tree has the three files of the audio driver: `wineaaudio.drv` of
i386 and of the ABI, and `wineaaudio.so` (it needs `libaaudio.so` of
Android). `pack_wine.sh` and `check_apk.ps1` check them. The PE DLLs of
ARM64 have the file alignment of 64 KB, so the ARM64 tree is larger. With
both trees and the files of the mod the APK is 206.8 MB (216,848,430 bytes).

**The APK.** `build_apk.ps1` puts the tree of each ABI that has one into
the APK as `assets/wine/<abi>.tar` (the method deflate) and
`assets/wine/<abi>.id`, with the license texts of Wine (LGPL-2.1),
FreeType (FTL), Box64 (MIT) and FEX (MIT, with the texts of the parts of
its source, `fex*.txt`) in `assets/licenses/`
(`third_party/README.md`). The asset manager of Android inflates a tar as a
stream, and the app reads only the tree of its own ABI. With no tree the APK
has no Wine and `targetSdkVersion` 32, and the log of the build says so for
each ABI.

**Executable files.** An app with `targetSdkVersion` 29 or later cannot
run (`execve`) a file of its own data folder (Android 10: the SELinux
domain `untrusted_app` has no `execute_no_trans` for `app_data_file`). Wine
must run its loader, its preloader and `wineserver`, and each new Windows
process is a new `execve` of the loader. So the APK with a Wine tree has
`targetSdkVersion` 28, as Winlator, and the app runs in the domain
`untrusted_app_27`, which can run the files of its folder. The tree is in
the internal folder of the app (`getFilesDir`), not on the shared storage.
What else changes with 28:

- Android 14 warns about an app whose `targetSdkVersion` is below
  `ro.build.version.min_supported_target_sdk`. The emulator of the tests
  (Android 14) has 28 there, so 28 gets no warning, and the app has the focus
  at its start. A later Android can raise the value.
- The shared storage: with 28 the app has the old storage of Android 9. The
  app uses only its own folder `Android/data/org.bwvr.quest`, which an app
  can read and write with each level. The game folder stays there.
- The package visibility of Android 11 (the `queries` of the manifest) is
  for 30 and later. With 28 the OpenXR loader sees each package, also the
  runtime broker.
- Not known: if Meta Horizon OS or the Steam Frame treat an app with 28
  differently (a warning, the OpenXR runtime). Only a headset can show it.

**The setup at each start** (`wine_runtime.cpp`, a thread, so the frame
loop and the menu do not wait):

1. With no tree of its ABI in the APK (`wine/<abi>.id`) the app sets up no
   Wine, and the log says so. `wine_setup=0` in `bwvr.ini`: the same, with
   a tree.
2. When the tree in `files/wine` has no build id or another one, the app
   unpacks the tar of the APK into `files/wine.new`, puts it in place of the
   old tree, and writes the build id last. A name that points out of the
   tree, a link or an entry of another kind stops the unpack.
3. The app checks the tree: the loader, `wineserver`, `ntdll` of i386 and of
   the ABI, the CPU DLL, and none of the DLLs that stay out.
4. With no marker in the prefix (`files/prefix/bwvr-prefix-id.txt`), the
   app makes a new prefix: it deletes the old one, writes a `user.reg` with
   `HKCU\Software\Wine\Drivers`, `Graphics` = `null` (no display driver)
   and `Audio` = `aaudio` (the audio driver of patch 0003. With
   `wine_audio=0` in `bwvr.ini` there is no `Audio` value, so a program has
   no sound), puts
   `wowbox64.dll` and `libwow64fex.dll` into `system32` on arm64-v8a, then runs
   `wine wineboot --init` and `wineserver -w`. The log has the time of each.
   The last step of a good prefix writes the marker: the build id of the
   tree. So a wineboot that stopped (the end of the app) or failed gives a
   new prefix at the next start. A prefix with the marker of another tree
   stays, and on arm64-v8a it gets the `wowbox64.dll` and the
   `libwow64fex.dll` of the new tree.
5. The drive `G:` of the prefix (`dosdevices/g:`) is a link to the game
   folder of the app.

The status line of the menu says what the app does: "Setting up Wine:
unpacking the runtime...", "Setting up Wine: making the prefix...", "Wine
ready.", or the fault. The log lines start with `wine`.

**A program in Wine.** The app starts the loader with the environment of
the app and `WINEPREFIX`, `WINESERVER`, `LD_LIBRARY_PATH` (the `lib` of the
tree, for `libfreetype.so`), `HOME` and `TMPDIR` (the internal folder),
`WINEDLLOVERRIDES=mscoree,mshtml=` (no Mono and no Gecko, so wineboot asks
for no download) and `WINEDEBUG` from `wine_debug` in `bwvr.ini` (with no
value, the default of Wine). Wine finds its DLLs and its loader from the
place of `ntdll.so`, so it needs no `WINELOADER` and no `WINEDLLPATH`. The
child gets each signal with its default action and none blocked, and no
file of the app: it closes each descriptor from 3 up before its `execve`.
Its stdout and stderr go to `wine.log` in the folder of the app, after a
line with the command, and the log of the app has its exit code. When
`execve` fails, the log has its errno. `wine.log` holds one start of the
app: the first program of a start makes it empty. At its end the app stops
the thread of the setup (a program that the thread waits for gets
`SIGKILL`), then each process of Wine (`wineserver -k`, also when no
program ran in this start). With `wine_setup=0` the app touches no part of
Wine: no thread, no `wine.log`, no `wineserver -k`.

**The test program** (`bwvr/test/wine_probe`, `bwvr_wine_probe.exe` of the
PC build, a 32 bit console program with no game code). `wine_probe=1` in
`bwvr.ini` (a test aid): after the setup the app runs
`G:\bwvr_wine_probe.exe` of the game folder one time. It writes
`bwvr-wine-probe.txt` beside itself, and the app copies each line into its
log: the Windows version, the version of Wine, the machines of
`IsWow64Process2`, the CPU DLL of the registry, the display driver, the time
of a fixed loop, a write, a read and a delete of a file of 64 KB on `G:`,
what `LoadLibraryEx` finds in the system folder for `ddraw`, `d3d9`,
`wined3d`, `dxgi`, `opengl32` and `vulkan-1`, the face, the extent and
the outline of a glyph of the font Tahoma, the audio driver of the
registry, and a tone of 1 s (440 Hz, at -26 dB of the full level) in the
format of QMixer (22050 Hz, 16 bit, stereo) through `waveOut` and through
DirectSound: what each call gave, and the time from the start of the play
to its end (`waveOut`: the buffer is done, DirectSound: the buffer does not
play now).

**The test in the emulator** (`emu_wine_test.ps1`, 69 checks, the
x86_64 tree, Android 14, SwiftShader, the emulator with `-audio none`: the
emulator gives Android an output device and plays its sound nowhere, so no
person hears the tone. `-Audio off` gives `-no-audio`, and `-LowLatency 0`
puts `wine_audio_low_latency=0` into `bwvr.ini` of the test program). The
test lists the prefix with
`adb root`, so it needs an emulator image of the kind userdebug
(`google_apis`, not `google_apis_playstore`). A clean install, then the first
start: the app ran in the domain `untrusted_app_27`, had the focus (no
warning of the system), and unpacked 1592 files (343.3 MB) in 2.9 s.
While wineboot ran, the test ended the app with the back key: the end of
the app stopped wineboot, `wineserver -k` ended with 0, and no process of
Wine stayed. The stopped prefix had a `system.reg` of wineboot and no
marker. The next start did no unpack and made a new prefix:
`wineboot --init` 17.5 s and `wineserver -w` 8.0 s, so the
prefix took 25.4 s, and the menu was on the screen before the setup
ended.
Then the test puts `bwvr_wine_probe.exe` and `bwvr.ini` with `wine_probe=1`
into the game folder, and the next start does no unpack and makes no
prefix. The test program ran from `G:` in 4.8 s and wrote: Windows
10.0.19045, Wine 11.0, a process for i386 on x86_64,
`cpu_dll=wow64cpu.dll` (the line of `pack_wine.sh` in `wine.inf`),
`graphics=null`, a loop of 80.5 ms, the write, the read and the delete
of the file of 64 KB on `G:`, no `ddraw`, `d3d9`, `wined3d`, `dxgi`,
`opengl32` or `vulkan-1` in the system folder (error 126), Tahoma as a
TrueType font with an extent of 200 by 39 and an outline of 112 bytes for
"A", and `audio=aaudio`. The sound: `waveOut` has one device ("AAudio
output"), and each call of the tone gave 0. The buffer of 1 s was done
after 879.2 ms at 22050 samples. DirectSound gave `DS_OK` for each call,
and the buffer stopped after 887.0 ms (the last play cursor 85096 of
88200 bytes). The end of a play is the time when the last frame went to
AAudio, so it is 1 s less the buffer of AAudio. Then the test program
waits 300 ms before the end of each stream. `wine.log` has: the output
of the emulator has 48000 Hz and bursts of 2180 frames (45.4 ms), so the
period of `mmdevapi` is 46 ms. The driver asked for the low latency mode,
and AAudio gave each stream the mode none (the emulator has no low latency
output). The stream of `waveOut` has 22050 Hz, 16 bit PCM, a burst of 441
frames and a buffer of 2012 frames (91.2 ms), the stream of DirectSound
has 48000 Hz, 32 bit float, a burst of 960 frames and a buffer of 4360
frames (90.8 ms). The latency from the timestamp of AAudio after 0.5 s:
161.9 ms and 162.2 ms. 0 underruns of AAudio. The frames of the program
that went to AAudio (all frames less the silence of the callback): 22050
of `waveOut` (23503 less 1453) and 57096 of DirectSound (60936 less
3840: the tone of 48000 frames and the silence of the mix of `dsound`
after it). When in the stream the frames of silence came is not in the
log. The log of the app has "wine_audio_low_latency=1", and each open
line of `wine.log` names the mode "low latency (WINEAAUDIO_LOW_LATENCY=1)".
With `-LowLatency 0` (69 checks, 0 failed) the open lines name the mode
"none (WINEAAUDIO_LOW_LATENCY=0)", the stream of `waveOut` has a burst of
1006 frames (in place of 441), and the frames of the program are 22050
and 56576.
`mmdevapi` has no line "No driver ... could be initialized". No process
of Wine was left after the setup or after the test program. At the end the back key ended the app, `wineserver -k` ended with
1 (no server ran: the server of the test program had ended, and -k gives 0
only when it stops one). The last start has `wine_setup=0`: the log says
"wine_setup=0. The app does not set up Wine", the app has no thread of the
setup, its end runs no `wineserver -k`, the folder of the app has no
`wine.log`, and no process of Wine runs. Logcat has no crash.

The game folder is on the shared storage, which has `noexec`. The `mmap`
of a PE file there works, so Wine 11.0 does not take the `read` path of
`map_pe_header` ("noexec file system, falling back to read"). The
`mprotect` of a code section to `PROT_EXEC` then fails, so the code of the
game and of its DLLs is not executable, and the game cannot start from the
shared storage with no patch. The test program of MSVC (a file alignment of
512 bytes) runs, because Wine reads a section that is not on a page of the
file in each case. With the patch 0002 each
image of the shared storage gets the line "noexec file system, reading the
sections of ..." (`runblack.exe`, the DLLs of the game folder,
`Plug Ins\ScriptLibraryR.dll`, `binkw32.dll`), and the game runs. The files
of the mod in `syswow64` are in the internal folder, which has no
`noexec`.

**On a Quest 3** (Meta Horizon OS v209, Android 14,
`ro.build.version.min_supported_target_sdk` 28, Adreno 740 with Vulkan 1.3):
the APK installs and the app with `targetSdkVersion` 28 starts the programs
of Wine from its folder. The first start unpacked the tree of arm64-v8a
(1587 files, 603.2 MB) in 2.7 s, `wineboot --init` took 8.3 s and
`wineserver -w` 7.4 s, so the prefix took 15.7 s. The test program ran with
`wowbox64.dll` as the CPU for x86 in 1.2 s: a process for i386 on arm64,
`cpu_dll=wowbox64.dll`, a loop of 143.6 ms (83 ms in the emulator with the
x86_64 tree), the drive, no graphics DLL of Wine, and Tahoma as a TrueType
font. A new tree over the prefix of an older tree unpacked in 2.3 s and put
the `wowbox64.dll` of the new tree into the prefix. The game ran with patch
0002 ("no PROT_EXEC for a mapping of the file") through the logos (6980
frames) and the video before the intro (skipped with the right trigger) to
the dialog of a new profile, as in the emulator.

On the Quest the end of the app by the system (after the session states 4,
3, 6 and 1) ended the process (`exit_at_end=1`), and "Quit game" at the
bottom of the menu over the game ended the game.

**Not shown yet:** "Exit" and the start after an end on a headset (the
emulator shows both, `emu_test.ps1`), the end of the process while the
activity is still in front after a quit of the OpenXR runtime (the
emulator logs "has died: fg TOP": not known what Meta Horizon OS does
then), the FUSE daemon of the Quest with the ioctl of patch 0001, and the
Steam Frame.

**Known limits:**

- The APK is large with both trees (see the table above): a tar in the
  method deflate. A better compression (xz) needs a decoder in the app.
- Wine has no display driver (`Graphics` = `null`). What the game does with
  it: see "Play in the app".
- The audio driver `wineaaudio` has one output device and no capture
  device and no MIDI device. A prefix of an older tree gets the `Audio`
  value at "Play" (the registry of the game), not at the start of the app,
  so the test program in such a prefix has no sound.
- The APK has no permission `INTERNET`, so Wine makes no socket
  ("WSASocketW Failed to create socket, this requires special
  permissions"). The network part of Wine cannot read the network cards
  of the headset ("nsi: bind failed, errno 13"), and a check of the
  network cards waits forever. The mod answers the whole check of the game
  for an internet connection with "no connection" (`net_check_skip=1`,
  `intro.md`, "The internet check").
- A tree of another build gets a new unpack, and the prefix with its
  marker stays. wineboot updates a prefix when `wine.inf` is newer than the
  prefix (`update_wineprefix`), but no test ran a new tree over an old
  prefix.

## Only Vulkan

The app has one renderer: the Vulkan renderer of the mod. There is no
fallback to Direct3D, DirectDraw or another graphics interface, now or with
the x86 runtime.

- The menu, the cursor, the stereo screen, the projection layer and the menu
  in the window go to the screen with Vulkan. `android.graphics.Canvas` only paints the picture
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

The Wine tree of the APK has no `ddraw`, `ddrawex`, `d3d*`, `wined3d`,
`dxgi`, `opengl32`, `vulkan-1` or `winevulkan` of Wine (the PE DLLs of each
architecture and their Unix libraries), and no display driver of Wine
(`wineandroid.drv` is not built, and the prefix has `Graphics` = `null`).
The list is `kLeftOutNames` and `kLeftOutPrefix` of
`android/src/wine_rules.h`: `pack_wine.sh` leaves the files out,
`check_apk.ps1` checks each tree, and the app checks its unpacked tree
before it starts Wine. So Wine can load
the `ddraw.dll` of the mod only as a native DLL, with no builtin one behind
it. The test program of Wine shows it in the emulator: `LoadLibraryEx` of
`ddraw.dll`, `d3d9.dll`, `wined3d.dll`, `dxgi.dll`, `opengl32.dll` and
`vulkan-1.dll` in the system folder finds none.

## What the app cannot do yet

**Start the game.** `runblack.exe` and the `ddraw.dll` of the mod are
Windows programs for x86 (32 bit), and the headsets have an ARM CPU with
Android. The plan, in steps:

1. **An x86 runtime in the app.** The APK carries Wine in the WoW64 mode
   for each ABI, and the app sets it up and runs a 32 bit Windows program
   in it (see "Wine in the app"). In the emulator the whole path runs with
   the x86_64 tree, and "Play" starts `runblack.exe` with the mod there
   ("Play in the app"). Open: a run of the arm64-v8a tree with
   `wowbox64.dll` on a headset, and the sound of the audio driver
   `wineaaudio` (patch 0003) in a headset.

   Box64 builds for the headsets with the NDK of the app:
   `bwvr/tools/android/build_box64.ps1 -Source <folder>` takes the folder
   of an unpacked source archive of Box64 (not in the repo, for example in
   `local/`), and writes `bwvr/out/android/box64/box64`. With the release
   v0.4.4 it is an AArch64 program for the linker of Android
   (`/system/bin/linker64`) of 21 MB that needs only `libc.so`, `libm.so`
   and `libdl.so`. The way of the app needs no Linux program box64, and the
   APK does not contain it. It did not run: the emulator of the tests is
   x86_64, so a run needs a headset or another ARM64 device.

   The way for the 32 bit code is an ARM64 Wine for Android in the WoW64
   mode that loads `wowbox64.dll` (the option `WOW64` of Box64) as its CPU
   for x86. Then all of Wine runs as native ARM64 code, and only the 32 bit
   code (the game, the mod, the 32 bit DLLs of Wine) goes through the
   emulator. Wine needs no x86_64 libraries and no Linux program box64.
   `wowbox64.dll` is a DLL for Windows on ARM64, so its build needs a
   compiler for Windows (llvm-mingw): the NDK has no headers of Windows.
   `build_box64.ps1 -Source <folder> -Mingw <bin folder of llvm-mingw>`
   builds it. With Box64 v0.4.4 and llvm-mingw 20261006 it is a DLL for
   ARM64 (machine 0xAA64) of 4.5 MB with the 13 exports `BTCpu...` of the
   CPU interface of the WoW64 of Wine, and it imports only `ntdll.dll` and
   `wow64.dll`. It goes into the Wine tree of arm64-v8a.

   The other CPU for the 32 bit code is `libwow64fex.dll` of FEX-2610
   (`build_fex.ps1`, the key `wow64_cpu`, see "FEX as the CPU of the 32 bit
   code"). It did not run yet.

   The fall back is an x86_64 Wine in the WoW64 mode under the Linux
   program box64, with the x86_64 libraries of glibc that it needs.
2. **The walks for the renderer in the app, live.** The renderer is in the
   app, and it draws a stream of the walks of the game to the pixel ("The
   stream of the walks"). The live stream gives the frames of the mod to the
   app through the ring file, frame by frame, and the eye pictures go to
   the stereo screen of the session ("The live stream"). So no draw goes
   through x86 code or through the Vulkan of Wine. The game already sends
   vertices in screen space, so the stream needs no matrices. With
   `xr_input=1` the eye pictures show in a projection layer with the values
   of the mod ("The projection layer"). With `vk_device=0` the mod has no
   Vulkan device of its own ("No Vulkan device of the mod"). "Play" starts
   the game in Wine with `vk_stream_live=1`, `vk_device=0` and
   `stream_live=1` ("Play in the app"). Open: a frame of the game in Wine
   through the ring, and the frames before the first frame of the scene
   (the logos, the videos, the dialog of a new profile).
3. **The head and the controllers.** Wine has no OpenXR for Android, so the
   app gives the mod the views, the hands, the display time, the focus and
   the recenter of its session through the input channel, and the mod gives
   the pulses back (see "The input channel"). Open: the channel under Wine
   and Box64, and the run with a real headset.

Each step needs a check that a desktop run can make. Step 2 has one: the
live stream to the test reader of the PC (`live_stream_run.ps1`), and the
frames of that run through the ring in the emulator (`emu_live_test.ps1`).
Step 3 has one: the input of a record through the channel of a desktop run
(`input_channel_run.ps1`), and the slot of the test runtime in the emulator
(`emu_xr_test.ps1`).

**The Steam Frame.** The Steamworks documentation of Valve ("Steam Frame")
says that the Steam Frame runs APKs in Lepton, an Android runtime of
SteamOS based on Waydroid, with OpenXR, and that an APK installs with `adb`
in the developer mode. So the APK of arm64-v8a is the way for the Steam
Frame too. Not tried: the APK in Lepton, `targetSdkVersion` 28 there, and
the child processes of Wine in Lepton. SteamOS also has Proton and FEX (an
x86 emulator), which could run the PC build of the mod with no APK, if the
OpenXR of SteamOS takes a 32 bit program. Not tried.

**Open questions:** the frame rate of the menu quads on a Quest 3, the
readability of the text at 1600 by 1200 on a quad of 1.4 m, the bindings of
the controllers of the Steam Frame, and if the Steam Frame gives an APK an
OpenXR runtime.
