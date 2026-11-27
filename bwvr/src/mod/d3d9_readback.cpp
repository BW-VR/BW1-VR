// D3D9 system-memory readback for Wine/Proton backends whose DirectDraw
// surface view cannot be locked by the CPU.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <stdint.h>
#include <string.h>

extern void ModLog(const char* format, ...);

namespace {

IDirect3DSurface9* g_readback = nullptr;
uint8_t* g_pixels = nullptr;
uint32_t g_pixels_width = 0;
uint32_t g_pixels_height = 0;
bool g_logged_no_device = false;

uint8_t Expand(uint32_t value, int bits) {
  if (bits >= 8) return static_cast<uint8_t>(value >> (bits - 8));
  const uint8_t scaled = static_cast<uint8_t>(value << (8 - bits));
  return static_cast<uint8_t>(scaled | (scaled >> bits));
}

void ReleaseSurface() {
  if (g_readback != nullptr) {
    g_readback->Release();
    g_readback = nullptr;
  }
}

}  // namespace

const void* FrameD3D9Readback(void* object, int* pitch,
                              uint32_t expected_width,
                              uint32_t expected_height, uint32_t* width,
                              uint32_t* height) {
  IDirect3DDevice9* device = static_cast<IDirect3DDevice9*>(object);
  if (device == nullptr) {
    if (!g_logged_no_device) {
      ModLog("  frame source : D3D9 readback has no device");
      g_logged_no_device = true;
    }
    return nullptr;
  }

  IDirect3DSurface9* source = nullptr;
  if (FAILED(device->GetRenderTarget(0, &source)) || source == nullptr) {
    ModLog("  frame source : D3D9 GetRenderTarget(0) failed");
    return nullptr;
  }
  D3DSURFACE_DESC desc;
  if (FAILED(source->GetDesc(&desc))) {
    ModLog("  frame source : D3D9 GetDesc failed");
    source->Release();
    return nullptr;
  }
  if (desc.Width != expected_width || desc.Height != expected_height) {
    ModLog("  frame source : D3D9 render target is %ux%u, but DirectDraw is "
           "%ux%u", desc.Width, desc.Height, expected_width, expected_height);
    source->Release();
    return nullptr;
  }

  D3DSURFACE_DESC old = {};
  if (g_readback != nullptr &&
      (FAILED(g_readback->GetDesc(&old)) || old.Width != desc.Width ||
       old.Height != desc.Height || old.Format != desc.Format)) {
    ReleaseSurface();
  }
  if (g_readback == nullptr &&
      FAILED(device->CreateOffscreenPlainSurface(
          desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM,
          &g_readback, nullptr))) {
    ModLog("  frame source : D3D9 CreateOffscreenPlainSurface failed for "
           "%ux%u format 0x%08lX", desc.Width, desc.Height, desc.Format);
    source->Release();
    return nullptr;
  }
  const HRESULT copied = device->GetRenderTargetData(source, g_readback);
  source->Release();
  if (FAILED(copied)) {
    ModLog("  frame source : D3D9 GetRenderTargetData failed (0x%08lX)",
           copied);
    return nullptr;
  }

  D3DLOCKED_RECT locked;
  if (FAILED(g_readback->LockRect(&locked, nullptr, D3DLOCK_READONLY))) {
    ModLog("  frame source : D3D9 readback LockRect failed");
    return nullptr;
  }
  const size_t row_bytes = static_cast<size_t>(desc.Width) * 4;
  const size_t bytes = row_bytes * desc.Height;
  if (g_pixels == nullptr || g_pixels_width != desc.Width ||
      g_pixels_height != desc.Height) {
    delete[] g_pixels;
    g_pixels = new uint8_t[bytes];
    g_pixels_width = desc.Width;
    g_pixels_height = desc.Height;
  }

  if (desc.Format == D3DFMT_X8R8G8B8 || desc.Format == D3DFMT_A8R8G8B8) {
    for (uint32_t y = 0; y < desc.Height; ++y) {
      memcpy(g_pixels + y * row_bytes,
             static_cast<const uint8_t*>(locked.pBits) + y * locked.Pitch,
             row_bytes);
    }
  } else if (desc.Format == D3DFMT_R5G6B5) {
    for (uint32_t y = 0; y < desc.Height; ++y) {
      const uint16_t* src = reinterpret_cast<const uint16_t*>(
          static_cast<const uint8_t*>(locked.pBits) + y * locked.Pitch);
      uint8_t* dst = g_pixels + y * row_bytes;
      for (uint32_t x = 0; x < desc.Width; ++x) {
        const uint16_t p = src[x];
        dst[x * 4 + 0] = Expand(p & 0x1F, 5);
        dst[x * 4 + 1] = Expand((p >> 5) & 0x3F, 6);
        dst[x * 4 + 2] = Expand((p >> 11) & 0x1F, 5);
        dst[x * 4 + 3] = 255;
      }
    }
  } else {
    ModLog("  frame source : D3D9 readback format 0x%08lX is unsupported",
           desc.Format);
    g_readback->UnlockRect();
    return nullptr;
  }
  g_readback->UnlockRect();
  *pitch = static_cast<int>(row_bytes);
  *width = desc.Width;
  *height = desc.Height;
  return g_pixels;
}

void FrameD3D9ReadbackShutdown() {
  ReleaseSurface();
  delete[] g_pixels;
  g_pixels = nullptr;
  g_pixels_width = 0;
  g_pixels_height = 0;
  g_logged_no_device = false;
}
