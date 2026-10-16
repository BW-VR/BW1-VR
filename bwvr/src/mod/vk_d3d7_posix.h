// THE PARTS OF DIRECT3D 7 AND DIRECTDRAW 7 THAT THE VULKAN RENDERER USES, for
// a build with no Windows (vk_platform_posix.h, docs/android.md, "The
// renderer in the app").
//
// The values are the values of d3dtypes.h and ddraw.h of the Windows SDK. The
// constants are constexpr and not macros, so the math test of the PC can
// include this file in a namespace beside the headers of the SDK and compare
// each value and each struct (test/controller_math/vk_platform_checks.h).
#pragma once

#include <stdint.h>

typedef uint32_t DWORD;
typedef int32_t LONG;
typedef int32_t HRESULT;

typedef float D3DVALUE;
typedef DWORD D3DCOLOR;

enum D3DRENDERSTATETYPE {
  D3DRENDERSTATE_ZENABLE = 7,
  D3DRENDERSTATE_ZWRITEENABLE = 14,
  D3DRENDERSTATE_ALPHATESTENABLE = 15,
  D3DRENDERSTATE_SRCBLEND = 19,
  D3DRENDERSTATE_DESTBLEND = 20,
  D3DRENDERSTATE_CULLMODE = 22,
  D3DRENDERSTATE_ZFUNC = 23,
  D3DRENDERSTATE_ALPHAREF = 24,
  D3DRENDERSTATE_ALPHAFUNC = 25,
  D3DRENDERSTATE_ALPHABLENDENABLE = 27,
  D3DRENDERSTATE_FOGENABLE = 28,
  D3DRENDERSTATE_SPECULARENABLE = 29,
  D3DRENDERSTATE_FOGCOLOR = 34,
  D3DRENDERSTATE_FOGTABLEMODE = 35,
  D3DRENDERSTATE_FORCE_DWORD = 0x7fffffff,
};

enum D3DTEXTURESTAGESTATETYPE {
  D3DTSS_COLOROP = 1,
  D3DTSS_COLORARG1 = 2,
  D3DTSS_COLORARG2 = 3,
  D3DTSS_ALPHAOP = 4,
  D3DTSS_ALPHAARG1 = 5,
  D3DTSS_ALPHAARG2 = 6,
  D3DTSS_TEXCOORDINDEX = 11,
  D3DTSS_ADDRESS = 12,
  D3DTSS_ADDRESSU = 13,
  D3DTSS_ADDRESSV = 14,
  D3DTSS_MAGFILTER = 16,
  D3DTSS_MINFILTER = 17,
  D3DTSS_MIPFILTER = 18,
  D3DTSS_MAXANISOTROPY = 21,
  D3DTSS_FORCE_DWORD = 0x7fffffff,
};

enum D3DPRIMITIVETYPE {
  D3DPT_POINTLIST = 1,
  D3DPT_LINELIST = 2,
  D3DPT_LINESTRIP = 3,
  D3DPT_TRIANGLELIST = 4,
  D3DPT_TRIANGLESTRIP = 5,
  D3DPT_TRIANGLEFAN = 6,
  D3DPT_FORCE_DWORD = 0x7fffffff,
};

enum D3DTEXTUREOP {
  D3DTOP_DISABLE = 1,
  D3DTOP_SELECTARG1 = 2,
  D3DTOP_SELECTARG2 = 3,
  D3DTOP_MODULATE = 4,
  D3DTOP_MODULATE2X = 5,
  D3DTOP_MODULATE4X = 6,
  D3DTOP_ADD = 7,
};

enum D3DCULL { D3DCULL_NONE = 1, D3DCULL_CW = 2, D3DCULL_CCW = 3 };
enum D3DCMPFUNC {
  D3DCMP_NEVER = 1,
  D3DCMP_LESS = 2,
  D3DCMP_EQUAL = 3,
  D3DCMP_LESSEQUAL = 4,
  D3DCMP_GREATER = 5,
  D3DCMP_NOTEQUAL = 6,
  D3DCMP_GREATEREQUAL = 7,
  D3DCMP_ALWAYS = 8,
};
enum D3DBLEND {
  D3DBLEND_ZERO = 1,
  D3DBLEND_ONE = 2,
  D3DBLEND_SRCCOLOR = 3,
  D3DBLEND_INVSRCCOLOR = 4,
  D3DBLEND_SRCALPHA = 5,
  D3DBLEND_INVSRCALPHA = 6,
};
enum D3DTEXTUREMIPFILTER { D3DTFP_NONE = 1, D3DTFP_POINT = 2, D3DTFP_LINEAR = 3 };

constexpr DWORD D3DTA_SELECTMASK = 0x0000000fu;
constexpr DWORD D3DTA_DIFFUSE = 0x00000000u;
constexpr DWORD D3DTA_CURRENT = 0x00000001u;
constexpr DWORD D3DTA_TEXTURE = 0x00000002u;
constexpr DWORD D3DTA_TFACTOR = 0x00000003u;
constexpr DWORD D3DTA_SPECULAR = 0x00000004u;
constexpr DWORD D3DCLEAR_TARGET = 0x00000001u;
constexpr DWORD D3DCLEAR_ZBUFFER = 0x00000002u;
constexpr DWORD D3DFVF_POSITION_MASK = 0x00Eu;
constexpr DWORD D3DFVF_XYZ = 0x002u;
constexpr DWORD D3DFVF_XYZRHW = 0x004u;
constexpr DWORD D3DFVF_NORMAL = 0x010u;
constexpr DWORD D3DFVF_DIFFUSE = 0x040u;
constexpr DWORD D3DFVF_SPECULAR = 0x080u;
constexpr DWORD D3DFVF_TEXCOUNT_MASK = 0xf00u;
constexpr DWORD D3DFVF_TEXCOUNT_SHIFT = 8;
constexpr DWORD DDSCAPS_TEXTURE = 0x00001000u;
constexpr DWORD DDSCAPS_3DDEVICE = 0x00002000u;
constexpr DWORD DDPF_ALPHAPIXELS = 0x00000001u;
constexpr DWORD DDPF_RGB = 0x00000040u;

struct D3DVIEWPORT7 {
  DWORD dwX;
  DWORD dwY;
  DWORD dwWidth;
  DWORD dwHeight;
  D3DVALUE dvMinZ;
  D3DVALUE dvMaxZ;
};

struct D3DRECT {
  LONG x1;
  LONG y1;
  LONG x2;
  LONG y2;
};

struct DDSCAPS2 {
  DWORD dwCaps;
  DWORD dwCaps2;
  DWORD dwCaps3;
  DWORD dwCaps4;
};

// A surface of the game. In the app it is a texture or a render target of
// the stream of the game.
struct IDirectDrawSurface7 {
  virtual HRESULT GetCaps(DDSCAPS2* caps) = 0;
  virtual unsigned long AddRef() { return 1; }
  virtual unsigned long Release() { return 1; }
  // The pixels as RGBA8, as ReadSurface of vk_scene.cpp gives them on the
  // PC. pixels must be freed with free(). False when the surface has none.
  virtual bool ReadRgba(uint32_t* width, uint32_t* height, uint8_t** pixels) = 0;

 protected:
  virtual ~IDirectDrawSurface7() = default;
};

// The device of the game, for the state at the start of a walk.
struct IDirect3DDevice7 {
  virtual HRESULT GetRenderState(D3DRENDERSTATETYPE state, DWORD* value) = 0;
  virtual HRESULT GetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type,
                                       DWORD* value) = 0;
  virtual HRESULT GetTexture(DWORD stage, IDirectDrawSurface7** texture) = 0;
  virtual HRESULT GetViewport(D3DVIEWPORT7* viewport) = 0;

 protected:
  virtual ~IDirect3DDevice7() = default;
};
