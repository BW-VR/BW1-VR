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
  `wowbox64.dll` of Box64 as the CPU of the 32 bit x86 code on ARM64. At its
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
| `android/src/wine_rules.h` | The rules of Wine in the app: the DLLs that stay out of the tree, the paths, the environment, the build id, the seed of `user.reg`, the reader of the tar file. No Android code |
| `test/wine_probe/main.cpp` | The test program of Wine (`bwvr_wine_probe.exe`), a 32 bit console program with no game code |
| `tools/android/build_wine.sh` | The build of FreeType and Wine for each ABI, in a Linux of WSL2 |
| `tools/android/wine-patches/` | The patches of the source of Wine |
| `tools/android/pack_wine.sh` | The Wine tree of the APK for each ABI, with its checks |
| `tools/android/build_box64.ps1` | The build of Box64 and of `wowbox64.dll` |
| `tools/android/emu_wine_test.ps1` | The test of Wine in the app in the Android emulator |
| `android/src/play_rules.h` | The rules of "Play": the keys of a run, the files of the mod, the registry of the game, the environment and the command line of the game, the input of the mod while the menu shows over the game. No Android code |
| `test/controller_math/play_rules_checks.h` | The math checks of `play_rules.h` and of "Quit game" in the menu |
| `tools/android/emu_game_test.ps1` | The game from the menu of the app in the Android emulator, with the test runtime |

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

**Steam Frame.** Turn on the developer mode, start Lepton Development, and
connect `adb` as the Steamworks documentation of Valve says ("Steam Frame").
Then `adb install -r` as above. The app is in the Android part of the
headset. Lepton has OpenXR, so the app starts its session there. If the
session fails, the app shows the menu in a window. Not tried yet.

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
the dialog of a new profile. `-WithProfile` copies them too, but a profile
of the PC also has values in the registry of the PC, which the folder does
not carry. Start the app again after the copy, so it checks
`runblack.exe`.

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
  display rate").
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
   tree gets it too. No value of a profile. The registry of the PC is not
   a source of the app: these are the values of the launcher of the PC.
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
quad layers. A hold of the menu button of the left controller for
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
Quit game, and the exit code. The log of the mod is `bwvr.log` in the game
folder, as on the PC, and `wine.log` in the folder of the app has the
output of Wine.

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

1. **With no device** (`check_apk.ps1`, 72 checks with both Wine trees):
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
   and Box64. With a tree `targetSdkVersion` must be 28, with none 32.
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
   the Touch controller, and the pulse of 80 ms at 0.50 at the runtime. The pictures
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
   and the rules of Wine (`wine_rules_checks.h`: the DLLs
   that stay out, the paths of each ABI, the build id, the environment of a
   program, the seed of `user.reg`, the marker of the prefix and the copy
   of the CPU DLL, the headers of the tar file and the names that stay in
   the tree).
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
game. A record (`vk_stream_record`) and the live stream get the same bytes
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
(`KeepWalks`, `stream_split.h`). At each frame of the session the loop of
the app takes the whole queue (a batch) and gives its frames to the renderer
(the renderer is not thread safe), through the same player as the file
(`PlayStreamRecords`, `scene_player.cpp`). It plays the walks of the newest
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
input, at 1024 a ring of 64 events, and at 2048 a ring of 32 keys. Each part has one layout on 32 bit
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
  that the headset asks for, and the gaze of an eye tracker (the app has
  none yet).

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

- The app has no eye tracker action, so the gaze of the slot is never
  valid.
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
released. The app adds no motion: no smoothing and no prediction of the
pose. A picture with no view shows on the stereo screen, and the log says so
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
in Wine under Box64 at each rate. A runtime with no extension on a real
headset (the Steam Frame).

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
"In Wine on Android".
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
`wowbox64.dll` in `aarch64-windows`. In both trees `pack_wine.sh` adds the
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
| tree of `arm64-v8a` | 1590 | 632,832,097 | `assets/wine/arm64-v8a.tar`, 633,712,640 bytes, compressed |

Each tree has the three files of the audio driver: `wineaaudio.drv` of
i386 and of the ABI, and `wineaaudio.so` (it needs `libaaudio.so` of
Android). `pack_wine.sh` and `check_apk.ps1` check them. The PE DLLs of
ARM64 have the file alignment of 64 KB, so the ARM64 tree is larger. With
both trees and the files of the mod the APK is 205.4 MB (215,368,922
bytes).

**The APK.** `build_apk.ps1` puts the tree of each ABI that has one into
the APK as `assets/wine/<abi>.tar` (the method deflate) and
`assets/wine/<abi>.id`, with the license texts of Wine (LGPL-2.1),
FreeType (FTL) and Box64 (MIT) in `assets/licenses/`
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
   `wowbox64.dll` into `system32` on arm64-v8a, then runs
   `wine wineboot --init` and `wineserver -w`. The log has the time of each.
   The last step of a good prefix writes the marker: the build id of the
   tree. So a wineboot that stopped (the end of the app) or failed gives a
   new prefix at the next start. A prefix with the marker of another tree
   stays, and on arm64-v8a it gets the `wowbox64.dll` of the new tree.
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
  permissions"). The mod answers the check of the game for an internet
  connection itself (`net_check=0`).
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
   `wow64.dll`. It goes into the Wine tree of arm64-v8a. It did not run
   yet.

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
