#include "underwater_renderer.h"

#include "fullscreen_triangle.h"
#include "log.h"
#include "water_renderer.h"

#include "ps_underwater.h"
#include "vs_fullscreen.h"

#include <cmath>
#include <cstring>

namespace
{
constexpr UINT kViewConstantRows = 9;
constexpr UINT kLookConstantRows = 6;
constexpr UINT kConstantRows = kViewConstantRows + kLookConstantRows;
constexpr DWORD kDepthStage = 0;
constexpr DWORD kSceneStage = 1;
constexpr DWORD kSceneBeforeSurfaceStage = 2;
constexpr DWORD kPassStages = 3;
constexpr DWORD kColourChannels = D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE;
constexpr DWORD kEverySample = 0xFFFFFFFF;

struct RenderStateValue
{
    D3DRENDERSTATETYPE state;
    DWORD value;
};

const RenderStateValue kPassRenderStates[] = {
    {D3DRS_ZENABLE, D3DZB_FALSE},
    {D3DRS_ZWRITEENABLE, FALSE},
    {D3DRS_STENCILENABLE, FALSE},
    {D3DRS_TWOSIDEDSTENCILMODE, FALSE},
    {D3DRS_ALPHATESTENABLE, FALSE},
    {D3DRS_ALPHABLENDENABLE, FALSE},
    {D3DRS_SEPARATEALPHABLENDENABLE, FALSE},
    {D3DRS_FOGENABLE, FALSE},
    {D3DRS_SRGBWRITEENABLE, FALSE},
    {D3DRS_CULLMODE, D3DCULL_NONE},
    {D3DRS_FILLMODE, D3DFILL_SOLID},
    {D3DRS_CLIPPLANEENABLE, 0},
    {D3DRS_SCISSORTESTENABLE, TRUE},
    {D3DRS_COLORWRITEENABLE, kColourChannels},
    {D3DRS_MULTISAMPLEMASK, kEverySample},
};

struct SamplerStateValue
{
    D3DSAMPLERSTATETYPE state;
    DWORD value;
};

const SamplerStateValue kPointClamp[] = {
    {D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP}, {D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP},
    {D3DSAMP_MAGFILTER, D3DTEXF_POINT},    {D3DSAMP_MINFILTER, D3DTEXF_POINT},
    {D3DSAMP_MIPFILTER, D3DTEXF_NONE},     {D3DSAMP_SRGBTEXTURE, FALSE},
    {D3DSAMP_MAXMIPLEVEL, 0},
};

const SamplerStateValue kLinearClamp[] = {
    {D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP}, {D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP},
    {D3DSAMP_MAGFILTER, D3DTEXF_LINEAR},   {D3DSAMP_MINFILTER, D3DTEXF_LINEAR},
    {D3DSAMP_MIPFILTER, D3DTEXF_NONE},     {D3DSAMP_SRGBTEXTURE, FALSE},
    {D3DSAMP_MAXMIPLEVEL, 0},
};

template <typename T>
void SafeRelease(T*& p)
{
    if (p)
    {
        p->Release();
        p = nullptr;
    }
}

RECT ViewportRect(const D3DVIEWPORT9& vp)
{
    return {static_cast<LONG>(vp.X), static_cast<LONG>(vp.Y), static_cast<LONG>(vp.X + vp.Width),
            static_cast<LONG>(vp.Y + vp.Height)};
}

bool InsideTarget(const D3DVIEWPORT9& world, const D3DSURFACE_DESC& desc)
{
    return world.Width > 0 && world.Height > 0 && world.X + world.Width <= desc.Width &&
           world.Y + world.Height <= desc.Height;
}

const char* UnusableTargets(const IDirect3DSurface9* boundDepth, IDirect3DSurface9* target, const SceneDepth& depth,
                            D3DSURFACE_DESC& depthDesc)
{
    D3DSURFACE_DESC targetDesc = {};
    if (!target)
        return "no render target";
    if (boundDepth != depth.bound)
        return "fog depth surface not bound";
    if (FAILED(target->GetDesc(&targetDesc)) || FAILED(depth.bound->GetDesc(&depthDesc)))
        return "surface description failed";
    if (targetDesc.Width != depthDesc.Width || targetDesc.Height != depthDesc.Height)
        return "render target and depth sizes differ";
    if (!SameSampleCount(targetDesc, depthDesc))
        return "render target and depth sample counts differ";
    return nullptr;
}

template <size_t N>
void Bind(IDirect3DDevice9* dev, DWORD stage, IDirect3DBaseTexture9* texture, const SamplerStateValue (&states)[N])
{
    dev->SetTexture(stage, texture);
    for (const SamplerStateValue& sampler : states)
        dev->SetSamplerState(stage, sampler.state, sampler.value);
}

float WrappedSeconds(double seconds)
{
    return static_cast<float>(std::fmod(seconds, kUnderwaterClockPeriodSeconds));
}

void FillLookConstants(const UnderwaterLook& look, bool useSceneBeforeSurface, double seconds,
                       float (&rows)[kLookConstantRows][4])
{
    const float values[kLookConstantRows][4] = {
        {look.extinction[0], look.extinction[1], look.extinction[2], look.surfaceHeight},
        {look.scatterColour[0], look.scatterColour[1], look.scatterColour[2], look.depthDarkening},
        {look.lightInWater[0], look.lightInWater[1], look.lightInWater[2], WrappedSeconds(seconds)},
        {look.sunLight[0], look.sunLight[1], look.sunLight[2], look.caustics},
        {look.shafts, look.surface, look.wobble, look.selfLit},
        {look.cameraDepth, useSceneBeforeSurface ? 1.0f : 0.0f, kWaterRefractiveIndex, 0.0f},
    };
    std::memcpy(rows, values, sizeof(values));
}
}

UnderwaterRenderer::~UnderwaterRenderer()
{
    ReleaseAll();
}

void UnderwaterRenderer::ReleaseDefaultPool()
{
    SafeRelease(m_scene.texture);
    SafeRelease(m_beforeSurface.texture);
    m_scene = {};
    m_beforeSurface = {};
    m_copyFailed = false;
    SafeRelease(m_state);
}

void UnderwaterRenderer::ReleaseAll()
{
    ReleaseDefaultPool();
    SafeRelease(m_vs);
    SafeRelease(m_ps);
    SafeRelease(m_decl);
    m_unsupportedDevice = nullptr;
}

bool UnderwaterRenderer::Skip(const char* reason)
{
    m_skip = reason;
    return false;
}

bool UnderwaterRenderer::EnsureShaders(IDirect3DDevice9* dev)
{
    if (m_unsupportedDevice == dev)
        return Skip("under-water shader unsupported");
    if (m_ps)
        return true;
    static const D3DVERTEXELEMENT9 kElements[] = {
        {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        D3DDECL_END(),
    };
    HRESULT result = dev->CreateVertexShader(reinterpret_cast<const DWORD*>(g_vs_fullscreen), &m_vs);
    if (SUCCEEDED(result))
        result = dev->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_underwater), &m_ps);
    if (SUCCEEDED(result))
        result = dev->CreateVertexDeclaration(kElements, &m_decl);
    if (SUCCEEDED(result))
        return true;
    SafeRelease(m_vs);
    SafeRelease(m_ps);
    SafeRelease(m_decl);
    m_unsupportedDevice = dev;
    VF_LOG_ERROR("under-water shaders could not be created (HRESULT 0x%08lX); the client's look stays under water",
                 static_cast<unsigned long>(result));
    return Skip("under-water shader unsupported");
}

bool UnderwaterRenderer::EnsureStateBlock(IDirect3DDevice9* dev)
{
    if (m_state)
        return true;
    if (FAILED(dev->BeginStateBlock()))
        return Skip("state block recording failed");
    for (const RenderStateValue& renderState : kPassRenderStates)
        dev->SetRenderState(renderState.state, 0);
    for (DWORD stage = 0; stage < kPassStages; ++stage)
    {
        dev->SetTexture(stage, nullptr);
        for (const SamplerStateValue& sampler : kLinearClamp)
            dev->SetSamplerState(stage, sampler.state, 0);
    }
    const float zeros[kConstantRows * 4] = {};
    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);
    dev->SetPixelShaderConstantF(0, zeros, kConstantRows);
    dev->SetVertexDeclaration(m_decl);
    dev->SetStreamSource(0, nullptr, 0, 0);
    D3DVIEWPORT9 viewport = {0, 0, 1, 1, 0.0f, 1.0f};
    dev->SetViewport(&viewport);
    RECT scissor = {0, 0, 1, 1};
    dev->SetScissorRect(&scissor);
    if (FAILED(dev->EndStateBlock(&m_state)) || !m_state)
    {
        m_state = nullptr;
        return Skip("state block recording failed");
    }
    return true;
}

bool UnderwaterRenderer::EnsureCopy(IDirect3DDevice9* dev, Copy& copy, D3DFORMAT format, UINT width, UINT height)
{
    if (copy.texture && copy.width == width && copy.height == height && copy.format == format)
        return true;
    SafeRelease(copy.texture);
    copy = {};
    if (m_copyFailed)
        return Skip("scene copy creation failed");
    const D3DFORMAT candidates[] = {format, D3DFMT_A8R8G8B8};
    for (D3DFORMAT candidate : candidates)
        if (SUCCEEDED(dev->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, candidate, D3DPOOL_DEFAULT,
                                         &copy.texture, nullptr)) &&
            copy.texture)
        {
            copy.width = width;
            copy.height = height;
            copy.format = format;
            return true;
        }
    copy.texture = nullptr;
    m_copyFailed = true;
    VF_LOG_ERROR("under-water scene copy creation failed (%ux%u)", width, height);
    return Skip("scene copy creation failed");
}

bool UnderwaterRenderer::CopyWorld(IDirect3DDevice9* dev, Copy& copy, IDirect3DSurface9* target,
                                   const D3DVIEWPORT9& world)
{
    D3DSURFACE_DESC desc = {};
    if (!target || FAILED(target->GetDesc(&desc)))
        return Skip("no render target");
    if (!InsideTarget(world, desc))
        return Skip("world viewport outside the render target");
    if (!EnsureCopy(dev, copy, desc.Format, world.Width, world.Height))
        return false;
    const RECT rect = ViewportRect(world);
    IDirect3DSurface9* surface = nullptr;
    const bool copied = SUCCEEDED(copy.texture->GetSurfaceLevel(0, &surface)) &&
                        SUCCEEDED(dev->StretchRect(target, &rect, surface, nullptr, D3DTEXF_POINT));
    SafeRelease(surface);
    return copied || Skip("scene copy failed");
}

bool UnderwaterRenderer::CaptureSceneBeforeSurface(IDirect3DDevice9* dev, const D3DVIEWPORT9& world)
{
    m_skip = "";
    if (dev->TestCooperativeLevel() != D3D_OK)
        return Skip("device not ready");
    IDirect3DSurface9* target = nullptr;
    dev->GetRenderTarget(0, &target);
    const bool copied = CopyWorld(dev, m_beforeSurface, target, world);
    SafeRelease(target);
    return copied;
}

void UnderwaterRenderer::DrawPass(IDirect3DDevice9* dev, const BoundTargets& bound, const SceneDepth& depth,
                                  const D3DVIEWPORT9& world, const float* constants, UINT constantRows,
                                  bool useSceneBeforeSurface)
{
    IDirect3DVertexBuffer9* stream = nullptr;
    UINT streamOffset = 0;
    UINT streamStride = 0;
    dev->GetStreamSource(0, &stream, &streamOffset, &streamStride);
    m_state->Capture();
    for (DWORD i = 1; i < kRenderTargets; ++i)
        if (bound.colour[i])
            dev->SetRenderTarget(i, nullptr);
    dev->SetDepthStencilSurface(nullptr);
    for (const RenderStateValue& renderState : kPassRenderStates)
        dev->SetRenderState(renderState.state, renderState.value);
    const RECT scissor = ViewportRect(world);
    dev->SetViewport(&world);
    dev->SetScissorRect(&scissor);
    dev->SetVertexShader(m_vs);
    dev->SetPixelShader(m_ps);
    dev->SetVertexDeclaration(m_decl);
    Bind(dev, kDepthStage, depth.texture, kPointClamp);
    Bind(dev, kSceneStage, m_scene.texture, kLinearClamp);
    Bind(dev, kSceneBeforeSurfaceStage, useSceneBeforeSurface ? m_beforeSurface.texture : m_scene.texture,
         kLinearClamp);
    dev->SetPixelShaderConstantF(0, constants, constantRows);
    DrawFullscreenTriangle(dev);
    for (DWORD i = 1; i < kRenderTargets; ++i)
        if (bound.colour[i])
            dev->SetRenderTarget(i, bound.colour[i]);
    m_state->Apply();
    dev->SetDepthStencilSurface(bound.depth);
    dev->SetStreamSource(0, stream, streamOffset, streamStride);
    SafeRelease(stream);
}

bool UnderwaterRenderer::RenderToTarget(IDirect3DDevice9* dev, const BoundTargets& bound, const SceneDepth& depth,
                                        const FrameInputs& in, const UnderwaterLook& look,
                                        bool useSceneBeforeSurface, double seconds)
{
    D3DSURFACE_DESC depthDesc = {};
    if (const char* unusable = UnusableTargets(bound.depth, bound.colour[0], depth, depthDesc))
        return Skip(unusable);
    const D3DVIEWPORT9& world = in.viewport;
    if (!CopyWorld(dev, m_scene, bound.colour[0], world))
        return false;
    if (!depth.Refresh(dev))
        return Skip("multisampled depth copy failed");
    float view[kViewConstantRows][4] = {};
    if (!BuildWaterViewConstants(in, depthDesc, view))
        return Skip("view matrix not invertible");
    const bool beforeSurface = useSceneBeforeSurface && m_beforeSurface.texture &&
                               m_beforeSurface.width == m_scene.width && m_beforeSurface.height == m_scene.height;
    float lookRows[kLookConstantRows][4] = {};
    FillLookConstants(look, beforeSurface, seconds, lookRows);
    float constants[kConstantRows][4] = {};
    std::memcpy(constants[0], view, sizeof(view));
    std::memcpy(constants[kViewConstantRows], lookRows, sizeof(lookRows));
    DrawPass(dev, bound, depth, world, &constants[0][0], kConstantRows, beforeSurface);
    ++m_draws;
    return true;
}

bool UnderwaterRenderer::Render(IDirect3DDevice9* dev, const SceneDepth& depth, const FrameInputs& in,
                                const UnderwaterLook& look, bool useSceneBeforeSurface, double seconds)
{
    m_skip = "";
    if (look.medium == UnderwaterMedium::None)
        return Skip("camera not in a liquid");
    if (!dev || !depth.texture || !depth.bound)
        return Skip("fog depth unavailable");
    if (dev->TestCooperativeLevel() != D3D_OK)
        return Skip("device not ready");
    if (!EnsureShaders(dev) || !EnsureStateBlock(dev))
        return false;
    BoundTargets bound;
    for (DWORD i = 0; i < kRenderTargets; ++i)
        dev->GetRenderTarget(i, &bound.colour[i]);
    dev->GetDepthStencilSurface(&bound.depth);
    const bool drawn = RenderToTarget(dev, bound, depth, in, look, useSceneBeforeSurface, seconds);
    for (IDirect3DSurface9*& colour : bound.colour)
        SafeRelease(colour);
    SafeRelease(bound.depth);
    return drawn;
}
