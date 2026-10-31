// GX2 resources: surfaces, render targets, textures, samplers, shaders,
// uniforms and vertex attribute buffers.
#include "Cafe/HW/Latte/Core/FetchShader.h"
#include "gx2.h"
#include "fetch_attributes.h"
#include "gx2_cmd.h"
#include "gx2_regs.h"
#include "gx2_surface_calc.h"
#include "gx2_texture_regs.h"
#include "runtime.h"
#include "../aspect.h"

#include <algorithm>
#include <cstring>
#include <set>

using namespace Latte;
using gx2::set_reg;
using gx2::set_regs;

template <typename T>
static T* gp(uint32 addr) { return (T*)mem::ptr(addr); }
static float fa(Cpu* c, int i) { return (float)c->f[1 + i].ps0; }

// ---------------------------------------------------------------- surfaces
HLE(gx2, GX2CalcSurfaceSizeAndAlignment) { gx2calc::CalcSurfaceSizeAndAlignment(gp<GX2Surface>(arg(c, 0))); }

HLE(gx2, GX2SetSurfaceSwizzle) {
    GX2Surface* s = gp<GX2Surface>(arg(c, 0));
    s->swizzle = (s->swizzle & 0xFFFF00FF) | ((arg(c, 1) & 0xFF) << 8);
}

HLE(gx2, GX2GetSurfaceSwizzle) { ret(c, (gp<GX2Surface>(arg(c, 0))->swizzle >> 8) & 0xFF); }
HLE(gx2, GX2SurfaceIsCompressed) { ret(c, IsCompressedFormat((E_GX2SURFFMT)arg(c, 0))); }
// bits per pixel (a compressed format's block bits over its 4x4 pixels)
HLE(gx2, GX2GetSurfaceFormatBits) {
    E_GX2SURFFMT f = (E_GX2SURFFMT)arg(c, 0);
    ret(c, GetFormatBits(f) / (IsCompressedFormat(f) ? 16 : 1));
}

// A CPU-linear view of a tiled surface level, element (x, y) at address + (pitch * y + x) * element size, as the
// GPU's aperture maps it (surface, level, slice, endian, u32* handle, u32* address). TP HD's field map tests a
// region mask's alpha through one (02A37BA0, from dMenu_Fmap: opening the map in Lake Hylia or Hyrule Field; the
// address 0 it got before crashed the game). The view is the level's texels in guest memory, untiled into a
// host-only buffer, which is kept for the next call while the surface's sampled words stay the same; writes
// through it are not tiled back. Render targets and compressed formats get none (address 0): a render target's
// texels are on the GPU. The frame begin takes one of the converted depth copy each frame (02BDE660, for dPeekZ;
// peekData is hooked instead, peekz.cpp).
HLE(gx2, GX2AllocateTilingApertureEx) {
    GX2Surface* s = gp<GX2Surface>(arg(c, 0));
    const uint32_t level = arg(c, 1), slice = arg(c, 2);
    LatteAddrLib::AddrSurfaceInfo_OUT info{};
    gx2calc::CalculateSurfaceInfo(s, level, &info);
    const uint32_t base = level == 0 ? (uint32_t)s->imagePtr : level == 1 ? (uint32_t)s->mipPtr : s->mipPtr + s->mipOffset[level - 1];
    const auto tm = (E_HWTILEMODE)info.hwTileMode;
    const uint32_t bytes = info.bpp / 8, w = std::max(1u, s->width >> level), h = std::max(1u, s->height >> level);
    if ((s->resFlag & 6) /* a color or depth buffer */ || IsCompressedFormat(s->format) || !bytes || !base || slice >= info.depth) {
        static std::set<uint32_t> seen;
        if (seen.insert(c->lr).second)
            LOG("[gx2] no tiling aperture for surface %08X (%ux%u format %#x), called from %08X", base, (uint32_t)s->width,
                (uint32_t)s->height, (uint32_t)s->format.value(), c->lr);
        st32(arg(c, 4), 0);
        st32(arg(c, 5), 0);
        return;
    }
    static uint32_t buf = 0, buf_size = 0, key[6] = {}, sum = 0;
    uint32_t k[6] = {base, (uint32_t)s->format.value(), w, h, info.pitch, slice}, h32 = 2166136261u;
    for (uint32_t i = 0; i < 256; i++) h32 = (h32 ^ ld32(base + (uint32_t)(info.surfSize / 256 * i) / 4 * 4)) * 16777619u;
    if (memcmp(k, key, sizeof k) || h32 != sum) {
        const uint32_t size = info.pitch * h * bytes;
        if (size > buf_size) buf = mem::host_alloc(size, 0x100), buf_size = size;
        LatteAddrLib::CachedSurfaceAddrInfo ci;
        if (TM_IsMacroTiled(tm))
            LatteAddrLib::SetupCachedSurfaceAddrInfo(&ci, slice, 0, info.bpp, info.pitch, info.height, info.depth, 1, tm, false,
                                                     (s->swizzle >> 8) & 1, (s->swizzle >> 9) & 3);
        for (uint32_t y = 0; y < h; y++)
            for (uint32_t x = 0; x < w; x++) {
                const uint32_t off = tm == E_HWTILEMODE::TM_LINEAR_GENERAL || tm == E_HWTILEMODE::TM_LINEAR_ALIGNED
                    ? LatteAddrLib::ComputeSurfaceAddrFromCoordLinear(x, y, slice, 0, info.bpp, info.pitch, info.height, info.depth)
                    : !TM_IsMacroTiled(tm) ? LatteAddrLib::ComputeSurfaceAddrFromCoordMicroTiled(x, y, slice, info.bpp, info.pitch, info.height, tm, false)
                    : LatteAddrLib::ComputeSurfaceAddrFromCoordMacroTiledCached(x, y, &ci);
                memcpy(mem::ptr(buf + (info.pitch * y + x) * bytes), mem::ptr(base + off), bytes);
            }
        memcpy(key, k, sizeof k);
        sum = h32;
        LOG("[gx2] tiling aperture for surface %08X (%ux%u format %#x, level %u), called from %08X", base, w, h,
            (uint32_t)s->format.value(), level, c->lr);
    }
    st32(arg(c, 4), buf);
    st32(arg(c, 5), buf);
}
HLE(gx2, GX2FreeTilingAperture) {}

HLE(gx2, GX2CalcColorBufferAuxInfo) { st32(arg(c, 1), 0x1000); st32(arg(c, 2), 0x100); }
HLE(gx2, GX2CalcDepthBufferHiZInfo) { st32(arg(c, 1), 0x1000); st32(arg(c, 2), 0x100); }
HLE(gx2, GX2InitDepthBufferHiZEnable) {}

// ---------------------------------------------------------------- render targets
HLE(gx2, GX2InitColorBufferRegs) { GX2::GX2InitColorBufferRegs(gp<GX2::GX2ColorBuffer>(arg(c, 0))); }
HLE(gx2, GX2InitDepthBufferRegs) { GX2::GX2InitDepthBufferRegs(gp<GX2::GX2DepthBuffer>(arg(c, 0))); }

namespace gx2 {
uint32 color_buffer_address(const GX2::GX2ColorBuffer* cb) {
    uint32 mip = cb->viewMip;
    uint32 base = cb->surface.imagePtr;
    if (mip == 1) base = cb->surface.mipPtr;
    else if (mip > 1) base = cb->surface.mipPtr + cb->surface.mipOffset[mip - 1];
    uint32 swizzle = cb->surface.swizzle;
    if (TM_IsMacroTiled(cb->surface.tileMode) && mip < ((swizzle >> 16) & 0xFF)) base ^= (swizzle & 0xFFFF);
    return base;
}
uint32 color_buffer_slices(const GX2::GX2ColorBuffer* cb) {
    const auto dim = cb->surface.dim.value();
    if (dim == Latte::E_DIM::DIM_2D_ARRAY) return std::max<uint32>(cb->surface.depth, 1);
    if (dim == Latte::E_DIM::DIM_3D) return std::max<uint32>(uint32(cb->surface.depth) >> cb->viewMip, 1);
    return 1;
}
}  // namespace gx2

HLE(gx2, GX2SetColorBuffer) {
    auto* cb = gp<GX2::GX2ColorBuffer>(arg(c, 0));
    uint32 target = arg(c, 1) & 7;
    set_reg(mmCB_COLOR0_BASE + target, gx2::color_buffer_address(cb));  // full address; our renderer's convention
    set_reg(mmCB_COLOR0_SIZE + target, cb->reg_size);
    set_reg(mmCB_COLOR0_VIEW + target, cb->reg_view);
    set_reg(mmCB_COLOR0_INFO + target, cb->reg_info);
    // our convention (gx2.h kColorTarget3D): the unused TILE/FRAG registers carry the view's real width
    // (| slices << 16 | volume flag) and height. A volume's slices are its depth at the view's mip
    // (gx2::color_buffer_slices); the Picto Box renders its 8x8x8 colour table slice by slice, caller 027B9E14
    bool volume = cb->surface.dim.value() == Latte::E_DIM::DIM_3D;
    uint32 slices = gx2::color_buffer_slices(cb);
    set_reg(mmCB_COLOR0_TILE + target, std::max<uint32>(cb->surface.width >> cb->viewMip, 1) | (std::min<uint32>(slices, 0x7FFF) << 16) |
                                           (volume ? gx2::kColorTarget3D : 0));
    set_reg(mmCB_COLOR0_FRAG + target, std::max<uint32>(cb->surface.height >> cb->viewMip, 1));
}

HLE(gx2, GX2SetDepthBuffer) {
    auto* db = gp<GX2::GX2DepthBuffer>(arg(c, 0));
    uint32 info = ((uint32)db->surface.tileMode.value() & 0xF) << 15;
    switch (db->surface.format.value()) {
    case E_GX2SURFFMT::D16_UNORM: info |= 1; break;
    case E_GX2SURFFMT::D24_S8_UNORM: info |= 3; break;
    case E_GX2SURFFMT::D24_S8_FLOAT: info |= 5; break;
    case E_GX2SURFFMT::D32_FLOAT: info |= 6; break;
    case E_GX2SURFFMT::D32_S8_FLOAT: info |= 7; break;
    default: break;
    }
    uint32 view = (db->viewFirstSlice & 0x7FF) | (((db->viewNumSlices + db->viewFirstSlice - 1) & 0x7FF) << 13);
    set_reg(mmDB_DEPTH_SIZE, db->reg_size);
    set_reg(mmDB_DEPTH_BASE, db->surface.imagePtr);  // full address; our renderer's convention
    set_reg(mmDB_DEPTH_INFO, info);
    set_reg(mmDB_DEPTH_VIEW, view);
    set_reg(mmDB_HTILE_DATA_BASE, (uint32)db->surface.width << 16 | (db->surface.height & 0xFFFF));  // our convention
    // our convention: unused register 0xA002 (between DB_DEPTH_VIEW and DB_DEPTH_BASE) = array slices
    set_reg(gx2::kDepthSlicesReg, db->surface.dim.value() == Latte::E_DIM::DIM_2D_ARRAY ? std::max<uint32>(db->surface.depth, 1) : 1);
}

// ---------------------------------------------------------------- textures and samplers
HLE(gx2, GX2InitTextureRegs) { GX2::GX2InitTextureRegs(gp<GX2::GX2Texture>(arg(c, 0))); }

static void set_texture(uint32 texAddr, uint32 baseReg, uint32 unit) {
    auto* tex = gp<GX2::GX2Texture>(texAddr);
    uint32 image = tex->surface.imagePtr, mip = tex->surface.mipPtr;
    if (!mip) mip = image;
    uint32 swizzle = tex->surface.swizzle;
    if (TM_IsMacroTiled(tex->surface.tileMode)) {
        uint32 stop = (swizzle >> 16) & 0xFF;
        if (stop > 0) image ^= (swizzle & 0xFFFF);
        if (stop > 1) mip ^= (swizzle & 0xFFFF);
    }
    uint32 w[7] = {tex->regTexWord0.value().getRawValue(), tex->regTexWord1.value().getRawValue(), image >> 8, mip >> 8,
                   tex->regTexWord4.value().getRawValue(), tex->regTexWord5.value().getRawValue(),
                   tex->regTexWord6.value().getRawValue()};
    set_regs(baseReg + unit * 7, w, 7);
}
HLE(gx2, GX2SetPixelTexture) { set_texture(arg(c, 0), REGADDR::SQ_TEX_RESOURCE_WORD0_N_PS, arg(c, 1)); }
HLE(gx2, GX2SetVertexTexture) { set_texture(arg(c, 0), REGADDR::SQ_TEX_RESOURCE_WORD0_N_VS, arg(c, 1)); }
HLE(gx2, GX2SetGeometryTexture) { set_texture(arg(c, 0), REGADDR::SQ_TEX_RESOURCE_WORD0_N_GS, arg(c, 1)); }

using SW0 = LATTE_SQ_TEX_SAMPLER_WORD0_0;
HLE(gx2, GX2InitSampler) { GX2::GX2InitSampler(gp<GX2::GX2Sampler>(arg(c, 0)), (SW0::E_CLAMP)arg(c, 1), (SW0::E_XY_FILTER)arg(c, 2)); }
HLE(gx2, GX2InitSamplerXYFilter) {
    GX2::GX2InitSamplerXYFilter(gp<GX2::GX2Sampler>(arg(c, 0)), (SW0::E_XY_FILTER)arg(c, 1), (SW0::E_XY_FILTER)arg(c, 2), arg(c, 3));
}
HLE(gx2, GX2InitSamplerZMFilter) {
    GX2::GX2InitSamplerZMFilter(gp<GX2::GX2Sampler>(arg(c, 0)), (SW0::E_Z_FILTER)arg(c, 1), (SW0::E_Z_FILTER)arg(c, 2));
}
HLE(gx2, GX2InitSamplerLOD) { GX2::GX2InitSamplerLOD(gp<GX2::GX2Sampler>(arg(c, 0)), fa(c, 0), fa(c, 1), fa(c, 2)); }
HLE(gx2, GX2InitSamplerClamping) {
    GX2::GX2InitSamplerClamping(gp<GX2::GX2Sampler>(arg(c, 0)), (SW0::E_CLAMP)arg(c, 1), (SW0::E_CLAMP)arg(c, 2), (SW0::E_CLAMP)arg(c, 3));
}
HLE(gx2, GX2InitSamplerBorderType) { GX2::GX2InitSamplerBorderType(gp<GX2::GX2Sampler>(arg(c, 0)), (SW0::E_BORDER_COLOR_TYPE)arg(c, 1)); }
HLE(gx2, GX2InitSamplerDepthCompare) { GX2::GX2InitSamplerDepthCompare(gp<GX2::GX2Sampler>(arg(c, 0)), (SW0::E_DEPTH_COMPARE)arg(c, 1)); }

static void set_sampler(uint32 sampler, uint32 index) {
    uint32 w[3] = {ld32(sampler), ld32(sampler + 4), ld32(sampler + 8)};
    set_regs(REGADDR::SQ_TEX_SAMPLER_WORD0_0 + index * 3, w, 3);
}
HLE(gx2, GX2SetPixelSampler) { set_sampler(arg(c, 0), arg(c, 1) + SAMPLER_BASE_INDEX_PIXEL); }
HLE(gx2, GX2SetVertexSampler) { set_sampler(arg(c, 0), arg(c, 1) + SAMPLER_BASE_INDEX_VERTEX); }
HLE(gx2, GX2SetGeometrySampler) { set_sampler(arg(c, 0), arg(c, 1) + SAMPLER_BASE_INDEX_GEOMETRY); }

static void set_border(uint32 base, Cpu* c) {
    uint32 v[4] = {gx2::fbits(fa(c, 0)), gx2::fbits(fa(c, 1)), gx2::fbits(fa(c, 2)), gx2::fbits(fa(c, 3))};
    set_regs(base + arg(c, 0) * 4, v, 4);
}
HLE(gx2, GX2SetPixelSamplerBorderColor) { set_border(REGADDR::TD_PS_SAMPLER0_BORDER_RED, c); }
HLE(gx2, GX2SetVertexSamplerBorderColor) { set_border(REGADDR::TD_VS_SAMPLER0_BORDER_RED, c); }
HLE(gx2, GX2SetGeometrySamplerBorderColor) { set_border(REGADDR::TD_GS_SAMPLER0_BORDER_RED, c); }

// ---------------------------------------------------------------- shaders
HLE(gx2, GX2SetVertexShader) {
    gx2::ShaderBindRegs tmp;  // only the registers the bind writes are forwarded
    uint32 vs = arg(c, 0);
    uint32 n = std::min<uint32>(ld32(vs + 0x0C), 10), nsem = std::min<uint32>(ld32(vs + 0x40), 32);
    gx2::bind_vertex_shader_regs(tmp, vs);
    bool gs_mode = ld32(vs + 0xD8) == 2;
    if (gs_mode) {
        set_regs(mmSQ_PGM_START_ES, tmp.at(mmSQ_PGM_START_ES), 2);
        set_reg(mmSQ_PGM_RESOURCES_ES, tmp[mmSQ_PGM_RESOURCES_ES]);
    } else {
        set_regs(mmSQ_PGM_START_VS, tmp.at(mmSQ_PGM_START_VS), 2);
        set_reg(mmSQ_PGM_RESOURCES_VS, tmp[mmSQ_PGM_RESOURCES_VS]);
        set_reg(mmVGT_PRIMITIVEID_EN, tmp[mmVGT_PRIMITIVEID_EN]);
        set_reg(mmSPI_VS_OUT_CONFIG, tmp[mmSPI_VS_OUT_CONFIG]);
        set_reg(mmPA_CL_VS_OUT_CNTL, tmp[mmPA_CL_VS_OUT_CNTL]);
        if (n) set_regs(mmSPI_VS_OUT_ID_0, tmp.at(mmSPI_VS_OUT_ID_0), n);
    }
    if (nsem) {
        set_reg(mmSQ_VTX_SEMANTIC_CLEAR, tmp[mmSQ_VTX_SEMANTIC_CLEAR]);  // (the slots past nsem: gx2_shader_regs.cpp)
        set_regs(mmSQ_VTX_SEMANTIC_0, tmp.at(mmSQ_VTX_SEMANTIC_0), nsem);
    }
}

HLE(gx2, GX2SetPixelShader) {
    gx2::ShaderBindRegs tmp;
    uint32 ps = arg(c, 0);
    uint32 n = std::min<uint32>(ld32(ps + 4 * 4), 0x20);
    gx2::bind_pixel_shader_regs(tmp, ps);
    set_regs(mmSQ_PGM_START_PS, tmp.at(mmSQ_PGM_START_PS), 2);
    set_reg(mmSQ_PGM_RESOURCES_PS, tmp[mmSQ_PGM_RESOURCES_PS]);
    set_regs(mmSPI_PS_IN_CONTROL_0, tmp.at(mmSPI_PS_IN_CONTROL_0), 2);
    if (n) set_regs(mmSPI_PS_INPUT_CNTL_0, tmp.at(mmSPI_PS_INPUT_CNTL_0), n);
    set_reg(mmCB_SHADER_MASK, tmp[mmCB_SHADER_MASK]);
    set_reg(mmCB_SHADER_CONTROL, tmp[mmCB_SHADER_CONTROL]);
    set_reg(mmDB_SHADER_CONTROL, tmp[mmDB_SHADER_CONTROL]);
    set_reg(mmSPI_INPUT_Z, tmp[mmSPI_INPUT_Z]);
}

HLE(gx2, GX2SetGeometryShader) {
    // TODO: geometry shaders (Metal object/mesh shader path)
    static bool warned = false;
    if (!warned) { warned = true; LOG("[gx2] geometry shaders not implemented yet"); }
}
HLE(gx2, GX2SetGeometryShaderInputRingBuffer) {}
HLE(gx2, GX2SetGeometryShaderOutputRingBuffer) {}

// ---------------------------------------------------------------- fetch shader
// Our fetch "program": header (magic, count) followed by 16 bytes per attribute:
//   +0 location | buffer << 8 | indexType << 16 | endianSwap << 24
//   +4 offset, +8 format | aluDivisor << 16, +C destSel
namespace gx2 {
constexpr uint32 kFetchMagic = 0x57574653;  // "WWFS"

LatteFetchShader* decode_fetch_attributes(uint32 program, uint32 size, uint64_t* sourceHash) {
    if (size < 32 || size > 32 + 64 * 52 || uint64_t(program) + size > 0x100000000ull) return nullptr;
    return decode_fetch_attributes_bytes({mem::ptr(program),size},sourceHash);
}
LatteFetchShader* build_fetch_shader(uint32 program) {
    if(uint64_t(program)+16>0x100000000ull || ld32(program)!=kFetchMagic) return nullptr;
    const uint32 count=ld32(program+4);
    if(count>64 || uint64_t(program)+16+uint64_t(count)*16>0x100000000ull) return nullptr;
    return build_fetch_shader_bytes({mem::ptr(program),16+size_t(count)*16},program);
}

}  // namespace gx2

HLE(gx2, GX2InitFetchShaderEx) {
    // (GX2FetchShader*, void* program, count, GX2AttribStream*, type, tessMode)
    uint32 fs = arg(c, 0), prog = arg(c, 1), n = arg(c, 2), attrs = arg(c, 3);
    st32(prog, gx2::kFetchMagic);
    st32(prog + 4, n);
    st32(prog + 8, 0);
    st32(prog + 12, 0);
    for (uint32 i = 0; i < n; i++) {
        uint32 s = attrs + i * 0x20, d = prog + 16 + i * 16;
        // GX2AttribStream: +0 location, +4 buffer, +8 offset, +C format, +10 indexType, +14 aluDivisor, +18 destSel, +1C endianSwap
        st32(d, (ld32(s) & 0xFF) | (ld32(s + 4) & 0xFF) << 8 | (ld32(s + 0x10) & 0xFF) << 16 | (ld32(s + 0x1C) & 0xFF) << 24);
        st32(d + 4, ld32(s + 8));
        st32(d + 8, (ld32(s + 0xC) & 0xFFFF) | (ld32(s + 0x14) & 0xFFFF) << 16);
        st32(d + 12, ld32(s + 0x18));
    }
    memset(mem::ptr(fs), 0, 0x20);
    st32(fs + 0x08, 16 + n * 16);  // shaderSize
    st32(fs + 0x0C, prog);         // shaderPtr
    st32(fs + 0x10, n);            // attribCount
}

HLE(gx2, GX2SetFetchShader) {
    uint32 fs = arg(c, 0);
    set_reg(mmSQ_PGM_START_FS, ld32(fs + 0x0C) >> 8);
    set_reg(mmSQ_PGM_START_FS + 1, ld32(fs + 0x08) >> 3);
}

// ---------------------------------------------------------------- uniforms and attribute buffers
static void uniform_block(uint32 blockStart, uint32 index, uint32 addr, uint32 size) {
    uint32 w[7] = {addr, size - 1, 0, 1, 0, 0, 0xC0000000};
    set_regs(blockStart + index * 7, w, 7);
}
HLE(gx2, GX2SetVertexUniformBlock) { uniform_block(mmSQ_VTX_UNIFORM_BLOCK_START, arg(c, 0), arg(c, 2), arg(c, 1)); }
HLE(gx2, GX2SetPixelUniformBlock) { uniform_block(mmSQ_PS_UNIFORM_BLOCK_START, arg(c, 0), arg(c, 2), arg(c, 1)); }
HLE(gx2, GX2SetGeometryUniformBlock) { uniform_block(mmSQ_GS_UNIFORM_BLOCK_START, arg(c, 0), arg(c, 2), arg(c, 1)); }

static void uniform_regs(uint32 stageBase, uint32 offset, uint32 count, uint32 values) {
    if (offset & 0x8000) return;
    count &= ~3u;
    if (offset + count > 0x400) count = 0x400 - std::min<uint32>(offset, 0x400);
    if (!count) return;
    // (this fork) the command itself: the first register, then the values (set_regs would copy them again)
    uint32 cmd[1 + 0x400];  // on the stack: several cores record display lists at once
    cmd[0] = mmSQ_ALU_CONSTANT0_0 + stageBase + offset;
    for (uint32 i = 0; i < count; i++) cmd[1 + i] = ld32(values + 4 * i);
    if (stageBase == 0x400 && count == 16 && aspect::tagged_projection()) {  // a layout projection (aspect.cpp)
        gx2::emit(gx2::OP_SET_PROJ_REGS, cmd, 17);
        return;
    }
    gx2::emit(gx2::OP_SET_REGS, cmd, count + 1);
}
HLE(gx2, GX2SetVertexUniformReg) { uniform_regs(0x400, arg(c, 0), arg(c, 1), arg(c, 2)); }
HLE(gx2, GX2SetPixelUniformReg) { uniform_regs(0, arg(c, 0), arg(c, 1), arg(c, 2)); }

HLE(gx2, GX2SetAttribBuffer) {
    // (index, size, stride, data)
    uint32 w[7] = {arg(c, 3), arg(c, 1) - 1, (arg(c, 2) & 0xFFFF) << 11, 0, 0, 0, 0xC0000000};
    set_regs(mmSQ_VTX_ATTRIBUTE_BLOCK_START + arg(c, 0) * 7, w, 7);
}

// ---------------------------------------------------------------- shader queries
HLE(gx2, GX2GetVertexShaderGPRs) { ret(c, ld32(arg(c, 0)) & 0xFF); }        // SQ_PGM_RESOURCES_VS.NUM_GPRS
HLE(gx2, GX2GetVertexShaderStackEntries) { ret(c, (ld32(arg(c, 0)) >> 8) & 0xFF); }
HLE(gx2, GX2GetPixelShaderGPRs) { ret(c, ld32(arg(c, 0)) & 0xFF); }
HLE(gx2, GX2GetPixelShaderStackEntries) { ret(c, (ld32(arg(c, 0)) >> 8) & 0xFF); }
HLE(gx2, GX2GetGeometryShaderGPRs) { ret(c, ld32(arg(c, 0)) & 0xFF); }
HLE(gx2, GX2GetGeometryShaderStackEntries) { ret(c, (ld32(arg(c, 0)) >> 8) & 0xFF); }
