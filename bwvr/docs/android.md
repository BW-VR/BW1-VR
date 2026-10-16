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
| `android/src/game_folder.cpp` | The game folder, the check of `runblack.exe`, the read and the write of a file |
| `src/common/settings_table.h` | The table of the settings, for the launcher and for the menu |
| `src/common/ini_text.h` | An ini file as text, with the rules of `GetPrivateProfileString` |
| `src/common/exe_sha1.h` | The SHA-1 of `runblack.exe` and the known builds, for the launcher, the mod and the app |
| `tools/android/build_apk.ps1` | The build of the APK |
| `tools/android/check_apk.ps1` | The checks of the APK with no device |
| `tools/android/emu_test.ps1` | The test in the Android emulator |
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
the app cannot read or write in it (the emulator test showed it). Then
uninstall the app and install it again. `bwvr.ini` is in the
game folder beside `runblack.exe`, as on the PC. At each start the app
reads `runblack.exe` and compares its SHA-1 with the value of
`bw_addresses.h` (`exe_sha1.h`). The status line of the menu says what it
found.

## The menu

The menu has the form of the menu of the game: a dark panel with a frame of
bronze, the title in gold and the entries in the middle of the main page.

- **The main page.** "Play in VR", "Play, new game", one entry for each
  group of the table ("Picture", "Performance", "Foveated rendering",
  "View", "Moving around", "Controllers", "Testing") and "Save Config".
- **A group page.** The rows of the group: the label at the left, the value
  at the right. A check box for a setting that is on or off, a slider with
  its value for a slider, the name of the item for a list. "Back" at the
  end.
- **The help.** Below the rows, the help text of the row with the highlight,
  the same text as the tooltip of the launcher.
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
  and the device (Vulkan 1.1 is enough for the menu).
- The reference space is `LOCAL`. The menu quad is 1.4 m wide, 1.4 m in
  front of the head and 0.15 m below it (`menu_layout.h`, `MenuQuad`). The
  cursor quad is 3 cm wide and 4 mm in front of the menu.
- The app copies the picture of the menu into an image of the swapchain of
  each quad at each frame, and gives the images in the layout of a color
  attachment. The swapchain format is the first of R8G8B8A8_SRGB,
  B8G8R8A8_SRGB, R8G8B8A8_UNORM and B8G8R8A8_UNORM that the runtime has.
  The pixels of the canvas are sRGB with the color multiplied by the alpha,
  which is what a quad with `XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT`
  takes.
- The actions: an aim pose, select, back, the stick and a pulse for each
  hand, with bindings for `khr/simple_controller`,
  `oculus/touch_controller` and `valve/index_controller`.
- The app paints the menu again only when it changes (`Menu::Revision`).

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

1. **With no device** (`check_apk.ps1`, 30 checks): the signature, the
   manifest (the package, the SDK levels, the categories of an immersive app
   of Meta and of OpenXR, the permissions and the queries of the loader, the
   headsets of Meta, no Java code), the libraries of both ABIs (stored,
   on pages of 16 KB, LOAD segments of 16 KB, no `libc++_shared.so`), and
   that the APK has no other file. So no file of the game can be in it.
2. **In the Android emulator** (`emu_test.ps1`): the same APK on an x86_64
   image of Android 14 with no window. The script puts a `bwvr.ini` and a
   stand in for `runblack.exe` (or with `-Exe` a real one) into the app
   folder, starts the app, drives the menu with key events, and checks the
   saved `bwvr.ini`, the log, logcat (no JNI error, no crash) and three
   screenshots. The emulator has no OpenXR runtime, so this step tests the
   menu, the canvas, the save, Vulkan in a window and the lifecycle, not
   the session.
3. **On the PC** (`.\dev.ps1 build`): the math test checks the ini text,
   the table, the menu (the pages, the steps, the save rules) and the hit of
   a ray on the quad (`test/controller_math/menu_checks.h`).
4. **In a headset** (only a headset can show it): install the APK with
   `adb install -r`, start it from "Unknown sources", and read the log. Look
   for the lines `xr           : runtime`, `the headset asks for` and `the
   session is made`. Check that the menu is in front of you at a good
   height, that the ray and the cursor meet the rows, that the trigger, the
   stick, B and Y do what "The menu" says, and that the text is sharp.
   `menu_capture=1` gives the picture of the menu.

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
2. **The renderer of the scene walks in the app.** With `backend=vulkan` the
   device of the mod gives each call of a walk to the Vulkan renderer
   (`VkSceneRenderState`, `VkSceneDraw`, `VkSceneTexture` and the others in
   `vk_scene.h`). The plan: a backend `remote` that sends these calls and
   the pixels of the textures through shared memory to the app, which draws
   them natively on ARM with Vulkan into the swapchains of its session. So
   no draw goes through x86 code or through the Vulkan of Wine. The game
   already sends vertices in screen space, so the stream needs no matrices.
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
