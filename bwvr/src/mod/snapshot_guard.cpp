// The save picture of the game in VR (docs/backend.md, "The save picture").
//
// WHAT THE GAME DOES. ScriptedScreenShot::Process makes the picture that the
// Save Game Room shows for a save. InstantSaveGame asks for it at each save,
// GoInsideCitadel when the player goes from the land into the Temple, and the
// Creature Room for its own pictures (ScriptedScreenShot::Request, which calls
// Process at once). Process locks the primary surface and then:
//
//     vstep = LHScreen::height * (1/128)
//     hstep = LHScreen::width  * (1/256)
//     px = ((uint32*)desc.lpSurface)[ ftol(r*vstep) * (desc.lPitch/4)
//                                     + ftol(c*hstep) ]
//
// It writes the 256 by 128 points into the top half of the texture
// save_game_picture_temporary.raw (LoadTextureCitadel, SaveTextureCitadel)
// and a thumbnail of 128 by 64 into its bottom half. The save then copies
// that texture into the pictures of its slot (fn_007923D0). The full picture
// (TakeFullScreen, 0x00821370) reads 400 by 300 points in the same way.
//
// THE PROBLEM. The base pointer, the pitch and the pixel format come from the
// locked descriptor. The EXTENT comes from LHScreen::width and height, and
// nothing is clamped. In VR the primary surface is the window, which is not
// the size of the render, so the read walks off the end of a row (an access
// violation with a square render). And the window does not hold the picture
// that the player sees: with dxwrapper the mod stops the copy to the monitor,
// and with backend=vulkan the primary surface has no pixels of the scene.
//
// WHAT THIS FILE DOES (save_picture=1). While VR runs, a hook on Process gives
// the game a primary surface of the mod for the call: kWidth by kHeight
// pixels of 32 bits with the middle of the eye picture of the last frame
// (save_picture_math.h), and LHScreen::width and height of that size. The
// game then makes the picture as it makes it from a screen, and the call
// puts the primary surface and the size back. The eye picture comes from the
// Vulkan renderer (backend=vulkan), from the wide target of one walk, or from
// the render target of the game. With no eye picture the call is skipped.
//
// save_picture=0: the call is skipped while VR runs, and the request flag is
// cleared as the original does, so the game does not think that a picture
// is still pending. The save then has no picture.
//
// 0x00821750 takes no arguments and ends C3, so it is safe either way, and it
// is named in the decompilation as ScriptedScreenShot::Process. The full
// picture at 0x00821370 is reached only from here, so one hook covers both.
//
// save_picture_test_dump=1, a test aid: after each picture, the picture of
// the mod goes to bwvr-save-picture-N.bmp beside runblack.exe, and a copy of
// the texture that the game wrote goes to bwvr-save-picture-N.raw.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "MinHook.h"
#include "one_walk.h"
#include "save_picture_math.h"
#include "vk_scene.h"

extern void ModLog(const char* format, ...);
const void* FrameSourceBegin(int* pitch);
void FrameSourceEnd();
bool FrameSourceSize(uint32_t* width, uint32_t* height);
bool StereoFrameOneWalk();

namespace {

// BW1W120 00821750 ScriptedScreenShot::Process(void)
const uintptr_t kScriptedScreenShotProcess = 0x00821750;
// ScriptedScreenShot::g_b_do_it: the flag of a request. The original clears
// it on entry.
const uintptr_t kSnapshotRequested = 0x00EB99B4;
// ScriptedScreenShot::g_b_fullscreen: the request is for the full picture of
// 400 by 300.
const uintptr_t kSnapshotFull = 0x00EB99B0;
// ScriptedScreenShot::g_info: the name of the thumbnail texture at +0, and of
// the picture texture at +0x100.
const uintptr_t kSnapshotInfo = 0x00EA9F70;
const int kInfoPicture = 0x100;
// LHSys::TheSystem.screen (0x00E85050): the width and the height (16 bits
// each) at +0x8 and +0xA, and PPrimarySurface at +0x30.
const uintptr_t kScreenWidth = 0x00E85058;
const uintptr_t kScreenHeight = 0x00E8505A;
const uintptr_t kScreenPrimary = 0x00E85080;
// The first pictures of a session have a line of the log each.
const int kLoggedPictures = 12;

typedef void(__cdecl* ProcessFn)();
ProcessFn o_Process = nullptr;

bool g_installed = false;
bool g_skip = false;      // VR runs
bool g_picture = true;    // save_picture
bool g_dump = false;      // save_picture_test_dump
uint64_t g_skipped = 0;
uint64_t g_made = 0;

// The picture that the game reads, BGRA.
uint8_t g_pixels[save_picture::kWidth * save_picture::kHeight * 4];

// The primary surface of the mod for one call of Process: Lock gives the
// picture, and Unlock does nothing. The game calls no other method there.
class PictureSurface : public IDirectDrawSurface7 {
 public:
#define BWVR_NO(name, args) \
  STDMETHOD(name) args override { return DDERR_UNSUPPORTED; }
  STDMETHOD(QueryInterface)(REFIID, LPVOID* out) override {
    if (out != nullptr) *out = nullptr;
    return E_NOINTERFACE;
  }
  STDMETHOD_(ULONG, AddRef)() override { return 1; }
  STDMETHOD_(ULONG, Release)() override { return 1; }
  BWVR_NO(AddAttachedSurface, (LPDIRECTDRAWSURFACE7))
  BWVR_NO(AddOverlayDirtyRect, (LPRECT))
  BWVR_NO(Blt, (LPRECT, LPDIRECTDRAWSURFACE7, LPRECT, DWORD, LPDDBLTFX))
  BWVR_NO(BltBatch, (LPDDBLTBATCH, DWORD, DWORD))
  BWVR_NO(BltFast, (DWORD, DWORD, LPDIRECTDRAWSURFACE7, LPRECT, DWORD))
  BWVR_NO(DeleteAttachedSurface, (DWORD, LPDIRECTDRAWSURFACE7))
  BWVR_NO(EnumAttachedSurfaces, (LPVOID, LPDDENUMSURFACESCALLBACK7))
  BWVR_NO(EnumOverlayZOrders, (DWORD, LPVOID, LPDDENUMSURFACESCALLBACK7))
  BWVR_NO(Flip, (LPDIRECTDRAWSURFACE7, DWORD))
  BWVR_NO(GetAttachedSurface, (LPDDSCAPS2, LPDIRECTDRAWSURFACE7*))
  BWVR_NO(GetBltStatus, (DWORD))
  BWVR_NO(GetCaps, (LPDDSCAPS2))
  BWVR_NO(GetClipper, (LPDIRECTDRAWCLIPPER*))
  BWVR_NO(GetColorKey, (DWORD, LPDDCOLORKEY))
  BWVR_NO(GetDC, (HDC*))
  BWVR_NO(GetFlipStatus, (DWORD))
  BWVR_NO(GetOverlayPosition, (LPLONG, LPLONG))
  BWVR_NO(GetPalette, (LPDIRECTDRAWPALETTE*))
  STDMETHOD(GetPixelFormat)(LPDDPIXELFORMAT out) override {
    if (out == nullptr) return DDERR_INVALIDPARAMS;
    Format(out);
    return DD_OK;
  }
  STDMETHOD(GetSurfaceDesc)(LPDDSURFACEDESC2 out) override {
    if (out == nullptr) return DDERR_INVALIDPARAMS;
    Describe(out);
    return DD_OK;
  }
  BWVR_NO(Initialize, (LPDIRECTDRAW, LPDDSURFACEDESC2))
  STDMETHOD(IsLost)() override { return DD_OK; }
  STDMETHOD(Lock)(LPRECT rect, LPDDSURFACEDESC2 out, DWORD, HANDLE) override {
    if (out == nullptr) return DDERR_INVALIDPARAMS;
    Describe(out);
    uint8_t* bits = g_pixels;
    if (rect != nullptr) {
      bits += rect->top * save_picture::kWidth * 4 + rect->left * 4;
    }
    out->dwFlags |= DDSD_LPSURFACE;
    out->lpSurface = bits;
    return DD_OK;
  }
  BWVR_NO(ReleaseDC, (HDC))
  STDMETHOD(Restore)() override { return DD_OK; }
  BWVR_NO(SetClipper, (LPDIRECTDRAWCLIPPER))
  BWVR_NO(SetColorKey, (DWORD, LPDDCOLORKEY))
  BWVR_NO(SetOverlayPosition, (LONG, LONG))
  BWVR_NO(SetPalette, (LPDIRECTDRAWPALETTE))
  STDMETHOD(Unlock)(LPRECT) override { return DD_OK; }
  BWVR_NO(UpdateOverlay,
          (LPRECT, LPDIRECTDRAWSURFACE7, LPRECT, DWORD, LPDDOVERLAYFX))
  BWVR_NO(UpdateOverlayDisplay, (DWORD))
  BWVR_NO(UpdateOverlayZOrder, (DWORD, LPDIRECTDRAWSURFACE7))
  BWVR_NO(GetDDInterface, (LPVOID*))
  BWVR_NO(PageLock, (DWORD))
  BWVR_NO(PageUnlock, (DWORD))
  BWVR_NO(SetSurfaceDesc, (LPDDSURFACEDESC2, DWORD))
  BWVR_NO(SetPrivateData, (REFGUID, LPVOID, DWORD, DWORD))
  BWVR_NO(GetPrivateData, (REFGUID, LPVOID, LPDWORD))
  BWVR_NO(FreePrivateData, (REFGUID))
  BWVR_NO(GetUniquenessValue, (LPDWORD))
  BWVR_NO(ChangeUniquenessValue, ())
  BWVR_NO(SetPriority, (DWORD))
  BWVR_NO(GetPriority, (LPDWORD))
  BWVR_NO(SetLOD, (DWORD))
  BWVR_NO(GetLOD, (LPDWORD))
#undef BWVR_NO

 private:
  static void Format(DDPIXELFORMAT* p) {
    ZeroMemory(p, sizeof(*p));
    p->dwSize = sizeof(*p);
    p->dwFlags = DDPF_RGB;
    p->dwRGBBitCount = 32;
    p->dwRBitMask = 0x00FF0000;
    p->dwGBitMask = 0x0000FF00;
    p->dwBBitMask = 0x000000FF;
  }
  static void Describe(DDSURFACEDESC2* d) {
    const DWORD size = d->dwSize;
    ZeroMemory(d, size >= sizeof(*d) ? sizeof(*d) : size);
    d->dwSize = size;
    d->dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH |
                 DDSD_PIXELFORMAT;
    d->dwWidth = save_picture::kWidth;
    d->dwHeight = save_picture::kHeight;
    d->lPitch = save_picture::kWidth * 4;
    d->ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_SYSTEMMEMORY;
    Format(&d->ddpfPixelFormat);
  }
};
PictureSurface g_surface;

// The part of the Vulkan picture, from VkSceneReadPicture.
struct VkRead {
  bool ok;
};

void TakeVulkan(const uint8_t* pixels, int pitch, uint32_t w, uint32_t h,
                bool red_first, void* context) {
  const save_picture::Rect all = {0, 0, static_cast<int>(w),
                                  static_cast<int>(h)};
  static_cast<VkRead*>(context)->ok = save_picture::Shrink(
      pixels, pitch, static_cast<int>(w), static_cast<int>(h), red_first, all,
      g_pixels, save_picture::kWidth, save_picture::kHeight);
}

// The middle of the eye picture of the last frame into g_pixels. `source`
// names where it came from, for the log.
bool MakePicture(const char** source, uint32_t* eye_w, uint32_t* eye_h) {
  using save_picture::Crop;
  using save_picture::Rect;
  *eye_w = 0;
  *eye_h = 0;
  if (VkSceneDirect()) {
    *source = "the left eye of the Vulkan renderer";
    if (!VkScenePictureSize(eye_w, eye_h)) return false;
    const Rect part = Crop(static_cast<int>(*eye_w), static_cast<int>(*eye_h),
                           save_picture::kShare);
    VkRead read = {false};
    return VkSceneReadPicture(part.x, part.y, static_cast<uint32_t>(part.w),
                              static_cast<uint32_t>(part.h), &TakeVulkan,
                              &read) &&
           read.ok;
  }
  if (StereoFrameOneWalk()) {
    *source = "the left eye of the wide target of one walk";
    int pitch = 0;
    const void* eye = OneWalkReadEye(0, &pitch, eye_w, eye_h);
    bool ok = false;
    if (eye != nullptr) {
      const Rect part = Crop(static_cast<int>(*eye_w),
                             static_cast<int>(*eye_h), save_picture::kShare);
      ok = save_picture::Shrink(static_cast<const uint8_t*>(eye), pitch,
                                static_cast<int>(*eye_w),
                                static_cast<int>(*eye_h), false, part,
                                g_pixels, save_picture::kWidth,
                                save_picture::kHeight);
      OneWalkReadDone();
    }
    if (ok) return true;
  }
  *source = "the render target of the game";
  if (!FrameSourceSize(eye_w, eye_h)) return false;
  int pitch = 0;
  const void* frame = FrameSourceBegin(&pitch);
  bool ok = false;
  if (frame != nullptr) {
    const Rect part = Crop(static_cast<int>(*eye_w), static_cast<int>(*eye_h),
                           save_picture::kShare);
    ok = save_picture::Shrink(static_cast<const uint8_t*>(frame), pitch,
                              static_cast<int>(*eye_w),
                              static_cast<int>(*eye_h), false, part, g_pixels,
                              save_picture::kWidth, save_picture::kHeight);
  }
  FrameSourceEnd();
  return ok;
}

// The folder of runblack.exe, with the last backslash.
void GameFolder(char* path, size_t size) {
  GetModuleFileNameA(GetModuleHandleA(nullptr), path,
                     static_cast<DWORD>(size));
  char* slash = strrchr(path, '\\');
  if (slash != nullptr) slash[1] = 0;
}

// save_picture_test_dump=1: the picture of the mod as a BMP, and a copy of
// the texture that the game wrote.
void Dump(uint64_t n, const char* texture) {
  char path[MAX_PATH];
  GameFolder(path, sizeof(path));
  char name[64];
  sprintf_s(name, "bwvr-save-picture-%llu.bmp", n);
  char bmp[MAX_PATH];
  strcpy_s(bmp, path);
  strcat_s(bmp, name);
  FILE* out = nullptr;
  bool bmp_ok = false;
  if (fopen_s(&out, bmp, "wb") == 0 && out != nullptr) {
    const int w = save_picture::kWidth, h = save_picture::kHeight;
    BITMAPFILEHEADER file = {};
    BITMAPINFOHEADER info = {};
    file.bfType = 0x4D42;
    file.bfOffBits = sizeof(file) + sizeof(info);
    file.bfSize = file.bfOffBits + w * h * 4;
    info.biSize = sizeof(info);
    info.biWidth = w;
    info.biHeight = -h;  // top row first
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;
    fwrite(&file, sizeof(file), 1, out);
    fwrite(&info, sizeof(info), 1, out);
    fwrite(g_pixels, 4, static_cast<size_t>(w) * h, out);
    fclose(out);
    bmp_ok = true;
  }
  sprintf_s(name, "bwvr-save-picture-%llu.raw", n);
  char raw[MAX_PATH];
  strcpy_s(raw, path);
  strcat_s(raw, name);
  const bool raw_ok = texture != nullptr && texture[0] != 0 &&
                      CopyFileA(texture, raw, FALSE) != 0;
  ModLog("snapshot     : test key save_picture_test_dump=1. The picture of the "
         "mod is %s %s, and the texture of the game is %s %s",
         bmp_ok ? "in" : "NOT in", bmp, raw_ok ? "in" : "NOT in", raw);
}

void __cdecl h_Process() {
  if (!g_skip) {
    o_Process();
    return;
  }
  uint32_t* requested = reinterpret_cast<uint32_t*>(kSnapshotRequested);
  if (IsBadWritePtr(requested, sizeof(uint32_t)) || *requested == 0) return;
  const bool full = *reinterpret_cast<const uint32_t*>(kSnapshotFull) != 0;
  char texture[0x100] = {};
  if (!full) {
    memcpy(texture, reinterpret_cast<const char*>(kSnapshotInfo + kInfoPicture),
           sizeof(texture) - 1);
  }
  const char* source = "no eye picture";
  uint32_t eye_w = 0, eye_h = 0;
  LARGE_INTEGER t0, t1, t2, hz;
  QueryPerformanceCounter(&t0);
  if (!g_picture || !MakePicture(&source, &eye_w, &eye_h)) {
    *requested = 0;
    ++g_skipped;
    if (g_picture && g_skipped <= kLoggedPictures) {
      ModLog("snapshot     : the game asked for a save picture, and the mod "
             "has none (%s, %u x %u). The game makes no picture",
             source, eye_w, eye_h);
    }
    return;
  }
  QueryPerformanceCounter(&t1);
  // The call of the game reads the surface of the mod with its own size.
  uint16_t* width = reinterpret_cast<uint16_t*>(kScreenWidth);
  uint16_t* height = reinterpret_cast<uint16_t*>(kScreenHeight);
  IDirectDrawSurface7** primary =
      reinterpret_cast<IDirectDrawSurface7**>(kScreenPrimary);
  const uint16_t old_w = *width, old_h = *height;
  IDirectDrawSurface7* const old_primary = *primary;
  *width = static_cast<uint16_t>(save_picture::kWidth);
  *height = static_cast<uint16_t>(save_picture::kHeight);
  *primary = &g_surface;
  o_Process();
  *primary = old_primary;
  *width = old_w;
  *height = old_h;
  QueryPerformanceCounter(&t2);
  QueryPerformanceFrequency(&hz);
  ++g_made;
  if (g_made <= kLoggedPictures) {
    float light = 0.0f, dark = 0.0f;
    save_picture::Measure(g_pixels, save_picture::kWidth * save_picture::kHeight,
                          &light, &dark);
    const double ms_eye =
        1000.0 * static_cast<double>(t1.QuadPart - t0.QuadPart) / hz.QuadPart;
    const double ms_game =
        1000.0 * static_cast<double>(t2.QuadPart - t1.QuadPart) / hz.QuadPart;
    ModLog("snapshot     : the game made a %s from %s (save_picture=1): the "
           "middle %.0f%% of the width of %u x %u, at %d x %d. Its mean light "
           "is %.2f, and %.1f%% of it is black. The eye picture took %.1f ms "
           "and the call of the game %.1f ms. %s%s",
           full ? "full picture" : "save picture", source,
           save_picture::kShare * 100.0f, eye_w, eye_h, save_picture::kWidth,
           save_picture::kHeight, light, dark * 100.0f, ms_eye, ms_game,
           full ? "" : "The texture is ", full ? "" : texture);
  }
  if (g_dump) Dump(g_made, full ? nullptr : texture);
}

}  // namespace

void SnapshotGuardSetSkip(bool on) {
  g_skip = on;
  if (!on) {
    ModLog("snapshot     : the game makes each save picture from its own "
           "screen");
  } else if (g_picture) {
    ModLog("snapshot     : save_picture=1. The game makes each save picture "
           "from a surface of the mod with the middle of the left eye "
           "picture, %d x %d, because its screen in VR is not the picture "
           "that the player sees",
           save_picture::kWidth, save_picture::kHeight);
  } else {
    ModLog("snapshot     : save_picture=0. The game makes no save picture in "
           "VR, because it reads past the end of a square window");
  }
}

void SnapshotGuardSetPicture(bool on, bool dump) {
  g_picture = on;
  g_dump = dump;
}

void SnapshotGuardInstall() {
  if (g_installed) return;
  void* target = reinterpret_cast<void*>(kScriptedScreenShotProcess);
  MH_STATUS status = MH_CreateHook(target, reinterpret_cast<void*>(&h_Process),
                                   reinterpret_cast<void**>(&o_Process));
  if (status == MH_OK) status = MH_EnableHook(target);
  g_installed = status == MH_OK;
  ModLog("hooks        : ScriptedScreenShot::Process at 0x%08X %s",
         kScriptedScreenShotProcess, g_installed ? "ok" : "FAILED");
}

void SnapshotGuardReport() {
  if (!g_installed || g_skipped == 0) return;
  ModLog("snapshot     : %llu save picture requests skipped", g_skipped);
  g_skipped = 0;
}
