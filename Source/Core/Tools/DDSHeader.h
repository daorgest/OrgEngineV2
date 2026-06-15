//
// Created by Orgest on 5/30/2026.
//

#pragma once
#include <utility>

// DDS
using DWORD = uint32_t;
using BYTE = uint8_t;
constexpr uint32_t DDS_MAGIC = 0x20534444;


enum class DDS_HEADER_FLAGS : DWORD
{
    CAPS        = 0x1,
    HEIGHT      = 0x2,
    WIDTH       = 0x4,
    PITCH       = 0x8,
    PIXELFORMAT = 0x1000,
    MIPMAPCOUNT = 0x20000,
    LINEARSIZE  = 0x80000,
    DEPTH       = 0x800000,
};

enum class DDS_PIXEL_FLAGS : DWORD
{
    ALPHAPIXELS = 0x1,
    ALPHA       = 0x2,
    FOURCC      = 0x4,
    RGB         = 0x40,
    YUV         = 0x200,
    LUMINANCE   = 0x20000,
};

template <typename E>
requires std::is_enum_v<E>
constexpr bool operator&(E lhs, E rhs)
{
    return (std::to_underlying(lhs) & std::to_underlying(rhs)) != 0;
}

template <typename E>
requires std::is_enum_v<E>
constexpr E operator|(E lhs, E rhs)
{
    return static_cast<E>(std::to_underlying(lhs) | std::to_underlying(rhs));
}

#pragma pack(push, 1)
struct DDS_PIXELFORMAT
{
    DWORD dwSize;
    DDS_PIXEL_FLAGS dwFlags;
    DWORD dwFourCC;
    DWORD dwRGBBitCount;
    DWORD dwRBitMask;
    DWORD dwGBitMask;
    DWORD dwBBitMask;
    DWORD dwABitMask;
};
static_assert(sizeof(DDS_PIXELFORMAT) == 32, "DDS_PIXELFORMAT size mismatch");

struct DDS_HEADER
{
    DWORD dwSize;
    DDS_HEADER_FLAGS dwFlags;
    DWORD dwHeight;
    DWORD dwWidth;
    DWORD dwPitchOrLinearSize;
    DWORD dwDepth;
    DWORD dwMipMapCount;
    DWORD dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    DWORD dwCaps;
    DWORD dwCaps2;
    DWORD dwCaps3;
    DWORD dwCaps4;
    DWORD dwReserved2;
};
static_assert(sizeof(DDS_HEADER) == 124, "DDS_PIXELFORMAT size mismatch");

struct DDS_HEADER_DXT10
{
    DWORD dxgiFormat;
    DWORD resourceDimension;
    DWORD miscFlag;
    DWORD arraySize;
    DWORD miscFlags2;
};
#pragma pack(pop)