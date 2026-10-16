// The parts of Direct3D 7 and DirectDraw 7 of the headset app against the
// Windows SDK, a test (src/mod/vk_d3d7_posix.h). main.cpp includes this file
// after Check and Near, just before main.
//
// vk_scene.cpp gets these values from the SDK on the PC and from
// vk_d3d7_posix.h in the app. A value that differs would draw another
// state in the headset, so each value and each struct must be the same.
//
// The constants that the SDK has as macros are read first, and the macros
// go, so the header of the app can have its names in a namespace.
#pragma once

#include <d3d.h>
#include <ddraw.h>
#include <stddef.h>

namespace vk_platform_sdk {
constexpr unsigned long k_D3DTA_SELECTMASK = static_cast<unsigned long>(D3DTA_SELECTMASK);
constexpr unsigned long k_D3DTA_DIFFUSE = static_cast<unsigned long>(D3DTA_DIFFUSE);
constexpr unsigned long k_D3DTA_CURRENT = static_cast<unsigned long>(D3DTA_CURRENT);
constexpr unsigned long k_D3DTA_TEXTURE = static_cast<unsigned long>(D3DTA_TEXTURE);
constexpr unsigned long k_D3DTA_TFACTOR = static_cast<unsigned long>(D3DTA_TFACTOR);
constexpr unsigned long k_D3DTA_SPECULAR = static_cast<unsigned long>(D3DTA_SPECULAR);
constexpr unsigned long k_D3DCLEAR_TARGET = static_cast<unsigned long>(D3DCLEAR_TARGET);
constexpr unsigned long k_D3DCLEAR_ZBUFFER = static_cast<unsigned long>(D3DCLEAR_ZBUFFER);
constexpr unsigned long k_D3DFVF_POSITION_MASK = static_cast<unsigned long>(D3DFVF_POSITION_MASK);
constexpr unsigned long k_D3DFVF_XYZ = static_cast<unsigned long>(D3DFVF_XYZ);
constexpr unsigned long k_D3DFVF_XYZRHW = static_cast<unsigned long>(D3DFVF_XYZRHW);
constexpr unsigned long k_D3DFVF_NORMAL = static_cast<unsigned long>(D3DFVF_NORMAL);
constexpr unsigned long k_D3DFVF_DIFFUSE = static_cast<unsigned long>(D3DFVF_DIFFUSE);
constexpr unsigned long k_D3DFVF_SPECULAR = static_cast<unsigned long>(D3DFVF_SPECULAR);
constexpr unsigned long k_D3DFVF_TEXCOUNT_MASK = static_cast<unsigned long>(D3DFVF_TEXCOUNT_MASK);
constexpr unsigned long k_D3DFVF_TEXCOUNT_SHIFT = static_cast<unsigned long>(D3DFVF_TEXCOUNT_SHIFT);
constexpr unsigned long k_DDSCAPS_TEXTURE = static_cast<unsigned long>(DDSCAPS_TEXTURE);
constexpr unsigned long k_DDSCAPS_3DDEVICE = static_cast<unsigned long>(DDSCAPS_3DDEVICE);
constexpr unsigned long k_DDPF_ALPHAPIXELS = static_cast<unsigned long>(DDPF_ALPHAPIXELS);
constexpr unsigned long k_DDPF_RGB = static_cast<unsigned long>(DDPF_RGB);
}  // namespace vk_platform_sdk
#undef D3DTA_SELECTMASK
#undef D3DTA_DIFFUSE
#undef D3DTA_CURRENT
#undef D3DTA_TEXTURE
#undef D3DTA_TFACTOR
#undef D3DTA_SPECULAR
#undef D3DCLEAR_TARGET
#undef D3DCLEAR_ZBUFFER
#undef D3DFVF_POSITION_MASK
#undef D3DFVF_XYZ
#undef D3DFVF_XYZRHW
#undef D3DFVF_NORMAL
#undef D3DFVF_DIFFUSE
#undef D3DFVF_SPECULAR
#undef D3DFVF_TEXCOUNT_MASK
#undef D3DFVF_TEXCOUNT_SHIFT
#undef DDSCAPS_TEXTURE
#undef DDSCAPS_3DDEVICE
#undef DDPF_ALPHAPIXELS
#undef DDPF_RGB

namespace posix_d3d {
#include "../../src/mod/vk_d3d7_posix.h"
}  // namespace posix_d3d

namespace vk_platform_checks {

inline void VkPlatformChecks() {
  printf("\nThe values of Direct3D 7 of the headset app\n");
  int same = 0, all = 0;
  auto same_as = [&](unsigned long app, unsigned long sdk) {
    ++all;
    if (app == sdk) ++same;
  };
  static_assert(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_ZENABLE) == static_cast<unsigned long>(::D3DRENDERSTATE_ZENABLE), "D3DRENDERSTATE_ZENABLE");
  same_as(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_ZENABLE), static_cast<unsigned long>(::D3DRENDERSTATE_ZENABLE));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_ZWRITEENABLE) == static_cast<unsigned long>(::D3DRENDERSTATE_ZWRITEENABLE), "D3DRENDERSTATE_ZWRITEENABLE");
  same_as(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_ZWRITEENABLE), static_cast<unsigned long>(::D3DRENDERSTATE_ZWRITEENABLE));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_ALPHATESTENABLE) == static_cast<unsigned long>(::D3DRENDERSTATE_ALPHATESTENABLE), "D3DRENDERSTATE_ALPHATESTENABLE");
  same_as(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_ALPHATESTENABLE), static_cast<unsigned long>(::D3DRENDERSTATE_ALPHATESTENABLE));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_SRCBLEND) == static_cast<unsigned long>(::D3DRENDERSTATE_SRCBLEND), "D3DRENDERSTATE_SRCBLEND");
  same_as(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_SRCBLEND), static_cast<unsigned long>(::D3DRENDERSTATE_SRCBLEND));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_DESTBLEND) == static_cast<unsigned long>(::D3DRENDERSTATE_DESTBLEND), "D3DRENDERSTATE_DESTBLEND");
  same_as(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_DESTBLEND), static_cast<unsigned long>(::D3DRENDERSTATE_DESTBLEND));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_CULLMODE) == static_cast<unsigned long>(::D3DRENDERSTATE_CULLMODE), "D3DRENDERSTATE_CULLMODE");
  same_as(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_CULLMODE), static_cast<unsigned long>(::D3DRENDERSTATE_CULLMODE));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_ZFUNC) == static_cast<unsigned long>(::D3DRENDERSTATE_ZFUNC), "D3DRENDERSTATE_ZFUNC");
  same_as(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_ZFUNC), static_cast<unsigned long>(::D3DRENDERSTATE_ZFUNC));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_ALPHAREF) == static_cast<unsigned long>(::D3DRENDERSTATE_ALPHAREF), "D3DRENDERSTATE_ALPHAREF");
  same_as(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_ALPHAREF), static_cast<unsigned long>(::D3DRENDERSTATE_ALPHAREF));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_ALPHAFUNC) == static_cast<unsigned long>(::D3DRENDERSTATE_ALPHAFUNC), "D3DRENDERSTATE_ALPHAFUNC");
  same_as(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_ALPHAFUNC), static_cast<unsigned long>(::D3DRENDERSTATE_ALPHAFUNC));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_ALPHABLENDENABLE) == static_cast<unsigned long>(::D3DRENDERSTATE_ALPHABLENDENABLE), "D3DRENDERSTATE_ALPHABLENDENABLE");
  same_as(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_ALPHABLENDENABLE), static_cast<unsigned long>(::D3DRENDERSTATE_ALPHABLENDENABLE));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_FOGENABLE) == static_cast<unsigned long>(::D3DRENDERSTATE_FOGENABLE), "D3DRENDERSTATE_FOGENABLE");
  same_as(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_FOGENABLE), static_cast<unsigned long>(::D3DRENDERSTATE_FOGENABLE));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_SPECULARENABLE) == static_cast<unsigned long>(::D3DRENDERSTATE_SPECULARENABLE), "D3DRENDERSTATE_SPECULARENABLE");
  same_as(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_SPECULARENABLE), static_cast<unsigned long>(::D3DRENDERSTATE_SPECULARENABLE));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_FOGCOLOR) == static_cast<unsigned long>(::D3DRENDERSTATE_FOGCOLOR), "D3DRENDERSTATE_FOGCOLOR");
  same_as(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_FOGCOLOR), static_cast<unsigned long>(::D3DRENDERSTATE_FOGCOLOR));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_FOGTABLEMODE) == static_cast<unsigned long>(::D3DRENDERSTATE_FOGTABLEMODE), "D3DRENDERSTATE_FOGTABLEMODE");
  same_as(static_cast<unsigned long>(posix_d3d::D3DRENDERSTATE_FOGTABLEMODE), static_cast<unsigned long>(::D3DRENDERSTATE_FOGTABLEMODE));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTSS_COLOROP) == static_cast<unsigned long>(::D3DTSS_COLOROP), "D3DTSS_COLOROP");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTSS_COLOROP), static_cast<unsigned long>(::D3DTSS_COLOROP));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTSS_COLORARG1) == static_cast<unsigned long>(::D3DTSS_COLORARG1), "D3DTSS_COLORARG1");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTSS_COLORARG1), static_cast<unsigned long>(::D3DTSS_COLORARG1));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTSS_COLORARG2) == static_cast<unsigned long>(::D3DTSS_COLORARG2), "D3DTSS_COLORARG2");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTSS_COLORARG2), static_cast<unsigned long>(::D3DTSS_COLORARG2));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTSS_ALPHAOP) == static_cast<unsigned long>(::D3DTSS_ALPHAOP), "D3DTSS_ALPHAOP");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTSS_ALPHAOP), static_cast<unsigned long>(::D3DTSS_ALPHAOP));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTSS_ALPHAARG1) == static_cast<unsigned long>(::D3DTSS_ALPHAARG1), "D3DTSS_ALPHAARG1");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTSS_ALPHAARG1), static_cast<unsigned long>(::D3DTSS_ALPHAARG1));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTSS_ALPHAARG2) == static_cast<unsigned long>(::D3DTSS_ALPHAARG2), "D3DTSS_ALPHAARG2");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTSS_ALPHAARG2), static_cast<unsigned long>(::D3DTSS_ALPHAARG2));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTSS_TEXCOORDINDEX) == static_cast<unsigned long>(::D3DTSS_TEXCOORDINDEX), "D3DTSS_TEXCOORDINDEX");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTSS_TEXCOORDINDEX), static_cast<unsigned long>(::D3DTSS_TEXCOORDINDEX));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTSS_ADDRESS) == static_cast<unsigned long>(::D3DTSS_ADDRESS), "D3DTSS_ADDRESS");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTSS_ADDRESS), static_cast<unsigned long>(::D3DTSS_ADDRESS));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTSS_ADDRESSU) == static_cast<unsigned long>(::D3DTSS_ADDRESSU), "D3DTSS_ADDRESSU");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTSS_ADDRESSU), static_cast<unsigned long>(::D3DTSS_ADDRESSU));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTSS_ADDRESSV) == static_cast<unsigned long>(::D3DTSS_ADDRESSV), "D3DTSS_ADDRESSV");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTSS_ADDRESSV), static_cast<unsigned long>(::D3DTSS_ADDRESSV));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTSS_MAGFILTER) == static_cast<unsigned long>(::D3DTSS_MAGFILTER), "D3DTSS_MAGFILTER");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTSS_MAGFILTER), static_cast<unsigned long>(::D3DTSS_MAGFILTER));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTSS_MINFILTER) == static_cast<unsigned long>(::D3DTSS_MINFILTER), "D3DTSS_MINFILTER");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTSS_MINFILTER), static_cast<unsigned long>(::D3DTSS_MINFILTER));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTSS_MIPFILTER) == static_cast<unsigned long>(::D3DTSS_MIPFILTER), "D3DTSS_MIPFILTER");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTSS_MIPFILTER), static_cast<unsigned long>(::D3DTSS_MIPFILTER));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTSS_MAXANISOTROPY) == static_cast<unsigned long>(::D3DTSS_MAXANISOTROPY), "D3DTSS_MAXANISOTROPY");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTSS_MAXANISOTROPY), static_cast<unsigned long>(::D3DTSS_MAXANISOTROPY));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DPT_POINTLIST) == static_cast<unsigned long>(::D3DPT_POINTLIST), "D3DPT_POINTLIST");
  same_as(static_cast<unsigned long>(posix_d3d::D3DPT_POINTLIST), static_cast<unsigned long>(::D3DPT_POINTLIST));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DPT_LINELIST) == static_cast<unsigned long>(::D3DPT_LINELIST), "D3DPT_LINELIST");
  same_as(static_cast<unsigned long>(posix_d3d::D3DPT_LINELIST), static_cast<unsigned long>(::D3DPT_LINELIST));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DPT_LINESTRIP) == static_cast<unsigned long>(::D3DPT_LINESTRIP), "D3DPT_LINESTRIP");
  same_as(static_cast<unsigned long>(posix_d3d::D3DPT_LINESTRIP), static_cast<unsigned long>(::D3DPT_LINESTRIP));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DPT_TRIANGLELIST) == static_cast<unsigned long>(::D3DPT_TRIANGLELIST), "D3DPT_TRIANGLELIST");
  same_as(static_cast<unsigned long>(posix_d3d::D3DPT_TRIANGLELIST), static_cast<unsigned long>(::D3DPT_TRIANGLELIST));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DPT_TRIANGLESTRIP) == static_cast<unsigned long>(::D3DPT_TRIANGLESTRIP), "D3DPT_TRIANGLESTRIP");
  same_as(static_cast<unsigned long>(posix_d3d::D3DPT_TRIANGLESTRIP), static_cast<unsigned long>(::D3DPT_TRIANGLESTRIP));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DPT_TRIANGLEFAN) == static_cast<unsigned long>(::D3DPT_TRIANGLEFAN), "D3DPT_TRIANGLEFAN");
  same_as(static_cast<unsigned long>(posix_d3d::D3DPT_TRIANGLEFAN), static_cast<unsigned long>(::D3DPT_TRIANGLEFAN));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTOP_DISABLE) == static_cast<unsigned long>(::D3DTOP_DISABLE), "D3DTOP_DISABLE");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTOP_DISABLE), static_cast<unsigned long>(::D3DTOP_DISABLE));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTOP_SELECTARG1) == static_cast<unsigned long>(::D3DTOP_SELECTARG1), "D3DTOP_SELECTARG1");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTOP_SELECTARG1), static_cast<unsigned long>(::D3DTOP_SELECTARG1));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTOP_SELECTARG2) == static_cast<unsigned long>(::D3DTOP_SELECTARG2), "D3DTOP_SELECTARG2");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTOP_SELECTARG2), static_cast<unsigned long>(::D3DTOP_SELECTARG2));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTOP_MODULATE) == static_cast<unsigned long>(::D3DTOP_MODULATE), "D3DTOP_MODULATE");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTOP_MODULATE), static_cast<unsigned long>(::D3DTOP_MODULATE));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTOP_MODULATE2X) == static_cast<unsigned long>(::D3DTOP_MODULATE2X), "D3DTOP_MODULATE2X");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTOP_MODULATE2X), static_cast<unsigned long>(::D3DTOP_MODULATE2X));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTOP_MODULATE4X) == static_cast<unsigned long>(::D3DTOP_MODULATE4X), "D3DTOP_MODULATE4X");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTOP_MODULATE4X), static_cast<unsigned long>(::D3DTOP_MODULATE4X));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DTOP_ADD) == static_cast<unsigned long>(::D3DTOP_ADD), "D3DTOP_ADD");
  same_as(static_cast<unsigned long>(posix_d3d::D3DTOP_ADD), static_cast<unsigned long>(::D3DTOP_ADD));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DCMP_NEVER) == static_cast<unsigned long>(::D3DCMP_NEVER), "D3DCMP_NEVER");
  same_as(static_cast<unsigned long>(posix_d3d::D3DCMP_NEVER), static_cast<unsigned long>(::D3DCMP_NEVER));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DCMP_LESS) == static_cast<unsigned long>(::D3DCMP_LESS), "D3DCMP_LESS");
  same_as(static_cast<unsigned long>(posix_d3d::D3DCMP_LESS), static_cast<unsigned long>(::D3DCMP_LESS));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DCMP_EQUAL) == static_cast<unsigned long>(::D3DCMP_EQUAL), "D3DCMP_EQUAL");
  same_as(static_cast<unsigned long>(posix_d3d::D3DCMP_EQUAL), static_cast<unsigned long>(::D3DCMP_EQUAL));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DCMP_LESSEQUAL) == static_cast<unsigned long>(::D3DCMP_LESSEQUAL), "D3DCMP_LESSEQUAL");
  same_as(static_cast<unsigned long>(posix_d3d::D3DCMP_LESSEQUAL), static_cast<unsigned long>(::D3DCMP_LESSEQUAL));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DCMP_GREATER) == static_cast<unsigned long>(::D3DCMP_GREATER), "D3DCMP_GREATER");
  same_as(static_cast<unsigned long>(posix_d3d::D3DCMP_GREATER), static_cast<unsigned long>(::D3DCMP_GREATER));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DCMP_NOTEQUAL) == static_cast<unsigned long>(::D3DCMP_NOTEQUAL), "D3DCMP_NOTEQUAL");
  same_as(static_cast<unsigned long>(posix_d3d::D3DCMP_NOTEQUAL), static_cast<unsigned long>(::D3DCMP_NOTEQUAL));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DCMP_GREATEREQUAL) == static_cast<unsigned long>(::D3DCMP_GREATEREQUAL), "D3DCMP_GREATEREQUAL");
  same_as(static_cast<unsigned long>(posix_d3d::D3DCMP_GREATEREQUAL), static_cast<unsigned long>(::D3DCMP_GREATEREQUAL));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DCMP_ALWAYS) == static_cast<unsigned long>(::D3DCMP_ALWAYS), "D3DCMP_ALWAYS");
  same_as(static_cast<unsigned long>(posix_d3d::D3DCMP_ALWAYS), static_cast<unsigned long>(::D3DCMP_ALWAYS));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DBLEND_ZERO) == static_cast<unsigned long>(::D3DBLEND_ZERO), "D3DBLEND_ZERO");
  same_as(static_cast<unsigned long>(posix_d3d::D3DBLEND_ZERO), static_cast<unsigned long>(::D3DBLEND_ZERO));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DBLEND_ONE) == static_cast<unsigned long>(::D3DBLEND_ONE), "D3DBLEND_ONE");
  same_as(static_cast<unsigned long>(posix_d3d::D3DBLEND_ONE), static_cast<unsigned long>(::D3DBLEND_ONE));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DBLEND_SRCCOLOR) == static_cast<unsigned long>(::D3DBLEND_SRCCOLOR), "D3DBLEND_SRCCOLOR");
  same_as(static_cast<unsigned long>(posix_d3d::D3DBLEND_SRCCOLOR), static_cast<unsigned long>(::D3DBLEND_SRCCOLOR));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DBLEND_INVSRCCOLOR) == static_cast<unsigned long>(::D3DBLEND_INVSRCCOLOR), "D3DBLEND_INVSRCCOLOR");
  same_as(static_cast<unsigned long>(posix_d3d::D3DBLEND_INVSRCCOLOR), static_cast<unsigned long>(::D3DBLEND_INVSRCCOLOR));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DBLEND_SRCALPHA) == static_cast<unsigned long>(::D3DBLEND_SRCALPHA), "D3DBLEND_SRCALPHA");
  same_as(static_cast<unsigned long>(posix_d3d::D3DBLEND_SRCALPHA), static_cast<unsigned long>(::D3DBLEND_SRCALPHA));
  static_assert(static_cast<unsigned long>(posix_d3d::D3DBLEND_INVSRCALPHA) == static_cast<unsigned long>(::D3DBLEND_INVSRCALPHA), "D3DBLEND_INVSRCALPHA");
  same_as(static_cast<unsigned long>(posix_d3d::D3DBLEND_INVSRCALPHA), static_cast<unsigned long>(::D3DBLEND_INVSRCALPHA));
  static_assert(posix_d3d::D3DTA_SELECTMASK == vk_platform_sdk::k_D3DTA_SELECTMASK, "D3DTA_SELECTMASK");
  same_as(posix_d3d::D3DTA_SELECTMASK, vk_platform_sdk::k_D3DTA_SELECTMASK);
  static_assert(posix_d3d::D3DTA_DIFFUSE == vk_platform_sdk::k_D3DTA_DIFFUSE, "D3DTA_DIFFUSE");
  same_as(posix_d3d::D3DTA_DIFFUSE, vk_platform_sdk::k_D3DTA_DIFFUSE);
  static_assert(posix_d3d::D3DTA_CURRENT == vk_platform_sdk::k_D3DTA_CURRENT, "D3DTA_CURRENT");
  same_as(posix_d3d::D3DTA_CURRENT, vk_platform_sdk::k_D3DTA_CURRENT);
  static_assert(posix_d3d::D3DTA_TEXTURE == vk_platform_sdk::k_D3DTA_TEXTURE, "D3DTA_TEXTURE");
  same_as(posix_d3d::D3DTA_TEXTURE, vk_platform_sdk::k_D3DTA_TEXTURE);
  static_assert(posix_d3d::D3DTA_TFACTOR == vk_platform_sdk::k_D3DTA_TFACTOR, "D3DTA_TFACTOR");
  same_as(posix_d3d::D3DTA_TFACTOR, vk_platform_sdk::k_D3DTA_TFACTOR);
  static_assert(posix_d3d::D3DTA_SPECULAR == vk_platform_sdk::k_D3DTA_SPECULAR, "D3DTA_SPECULAR");
  same_as(posix_d3d::D3DTA_SPECULAR, vk_platform_sdk::k_D3DTA_SPECULAR);
  static_assert(posix_d3d::D3DCLEAR_TARGET == vk_platform_sdk::k_D3DCLEAR_TARGET, "D3DCLEAR_TARGET");
  same_as(posix_d3d::D3DCLEAR_TARGET, vk_platform_sdk::k_D3DCLEAR_TARGET);
  static_assert(posix_d3d::D3DCLEAR_ZBUFFER == vk_platform_sdk::k_D3DCLEAR_ZBUFFER, "D3DCLEAR_ZBUFFER");
  same_as(posix_d3d::D3DCLEAR_ZBUFFER, vk_platform_sdk::k_D3DCLEAR_ZBUFFER);
  static_assert(posix_d3d::D3DFVF_POSITION_MASK == vk_platform_sdk::k_D3DFVF_POSITION_MASK, "D3DFVF_POSITION_MASK");
  same_as(posix_d3d::D3DFVF_POSITION_MASK, vk_platform_sdk::k_D3DFVF_POSITION_MASK);
  static_assert(posix_d3d::D3DFVF_XYZ == vk_platform_sdk::k_D3DFVF_XYZ, "D3DFVF_XYZ");
  same_as(posix_d3d::D3DFVF_XYZ, vk_platform_sdk::k_D3DFVF_XYZ);
  static_assert(posix_d3d::D3DFVF_XYZRHW == vk_platform_sdk::k_D3DFVF_XYZRHW, "D3DFVF_XYZRHW");
  same_as(posix_d3d::D3DFVF_XYZRHW, vk_platform_sdk::k_D3DFVF_XYZRHW);
  static_assert(posix_d3d::D3DFVF_NORMAL == vk_platform_sdk::k_D3DFVF_NORMAL, "D3DFVF_NORMAL");
  same_as(posix_d3d::D3DFVF_NORMAL, vk_platform_sdk::k_D3DFVF_NORMAL);
  static_assert(posix_d3d::D3DFVF_DIFFUSE == vk_platform_sdk::k_D3DFVF_DIFFUSE, "D3DFVF_DIFFUSE");
  same_as(posix_d3d::D3DFVF_DIFFUSE, vk_platform_sdk::k_D3DFVF_DIFFUSE);
  static_assert(posix_d3d::D3DFVF_SPECULAR == vk_platform_sdk::k_D3DFVF_SPECULAR, "D3DFVF_SPECULAR");
  same_as(posix_d3d::D3DFVF_SPECULAR, vk_platform_sdk::k_D3DFVF_SPECULAR);
  static_assert(posix_d3d::D3DFVF_TEXCOUNT_MASK == vk_platform_sdk::k_D3DFVF_TEXCOUNT_MASK, "D3DFVF_TEXCOUNT_MASK");
  same_as(posix_d3d::D3DFVF_TEXCOUNT_MASK, vk_platform_sdk::k_D3DFVF_TEXCOUNT_MASK);
  static_assert(posix_d3d::D3DFVF_TEXCOUNT_SHIFT == vk_platform_sdk::k_D3DFVF_TEXCOUNT_SHIFT, "D3DFVF_TEXCOUNT_SHIFT");
  same_as(posix_d3d::D3DFVF_TEXCOUNT_SHIFT, vk_platform_sdk::k_D3DFVF_TEXCOUNT_SHIFT);
  static_assert(posix_d3d::DDSCAPS_TEXTURE == vk_platform_sdk::k_DDSCAPS_TEXTURE, "DDSCAPS_TEXTURE");
  same_as(posix_d3d::DDSCAPS_TEXTURE, vk_platform_sdk::k_DDSCAPS_TEXTURE);
  static_assert(posix_d3d::DDSCAPS_3DDEVICE == vk_platform_sdk::k_DDSCAPS_3DDEVICE, "DDSCAPS_3DDEVICE");
  same_as(posix_d3d::DDSCAPS_3DDEVICE, vk_platform_sdk::k_DDSCAPS_3DDEVICE);
  static_assert(posix_d3d::DDPF_ALPHAPIXELS == vk_platform_sdk::k_DDPF_ALPHAPIXELS, "DDPF_ALPHAPIXELS");
  same_as(posix_d3d::DDPF_ALPHAPIXELS, vk_platform_sdk::k_DDPF_ALPHAPIXELS);
  static_assert(posix_d3d::DDPF_RGB == vk_platform_sdk::k_DDPF_RGB, "DDPF_RGB");
  same_as(posix_d3d::DDPF_RGB, vk_platform_sdk::k_DDPF_RGB);
  Check(same == all, "each value is the value of the Windows SDK", same, all);

  static_assert(offsetof(posix_d3d::D3DVIEWPORT7, dwX) == offsetof(::D3DVIEWPORT7, dwX), "D3DVIEWPORT7.dwX");
  static_assert(offsetof(posix_d3d::D3DVIEWPORT7, dwY) == offsetof(::D3DVIEWPORT7, dwY), "D3DVIEWPORT7.dwY");
  static_assert(offsetof(posix_d3d::D3DVIEWPORT7, dwWidth) == offsetof(::D3DVIEWPORT7, dwWidth), "D3DVIEWPORT7.dwWidth");
  static_assert(offsetof(posix_d3d::D3DVIEWPORT7, dwHeight) == offsetof(::D3DVIEWPORT7, dwHeight), "D3DVIEWPORT7.dwHeight");
  static_assert(offsetof(posix_d3d::D3DVIEWPORT7, dvMinZ) == offsetof(::D3DVIEWPORT7, dvMinZ), "D3DVIEWPORT7.dvMinZ");
  static_assert(offsetof(posix_d3d::D3DVIEWPORT7, dvMaxZ) == offsetof(::D3DVIEWPORT7, dvMaxZ), "D3DVIEWPORT7.dvMaxZ");
  static_assert(offsetof(posix_d3d::D3DRECT, x1) == offsetof(::D3DRECT, x1), "D3DRECT.x1");
  static_assert(offsetof(posix_d3d::D3DRECT, y1) == offsetof(::D3DRECT, y1), "D3DRECT.y1");
  static_assert(offsetof(posix_d3d::D3DRECT, x2) == offsetof(::D3DRECT, x2), "D3DRECT.x2");
  static_assert(offsetof(posix_d3d::D3DRECT, y2) == offsetof(::D3DRECT, y2), "D3DRECT.y2");
  static_assert(offsetof(posix_d3d::DDSCAPS2, dwCaps) == offsetof(::DDSCAPS2, dwCaps), "DDSCAPS2.dwCaps");
  static_assert(offsetof(posix_d3d::DDSCAPS2, dwCaps2) == offsetof(::DDSCAPS2, dwCaps2), "DDSCAPS2.dwCaps2");
  static_assert(sizeof(posix_d3d::D3DVIEWPORT7) == sizeof(::D3DVIEWPORT7), "D3DVIEWPORT7");
  static_assert(sizeof(posix_d3d::D3DRECT) == sizeof(::D3DRECT), "D3DRECT");
  static_assert(sizeof(posix_d3d::DDSCAPS2) == sizeof(::DDSCAPS2), "DDSCAPS2");
  Check(sizeof(posix_d3d::D3DVIEWPORT7) == sizeof(::D3DVIEWPORT7) &&
            sizeof(posix_d3d::D3DRECT) == sizeof(::D3DRECT) &&
            sizeof(posix_d3d::DDSCAPS2) == sizeof(::DDSCAPS2),
        "D3DVIEWPORT7, D3DRECT and DDSCAPS2 have the layout of the SDK", 0, 0);
}

}  // namespace vk_platform_checks
