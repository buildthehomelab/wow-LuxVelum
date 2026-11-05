#pragma once

#include "engine.h"
#include "msaa_depth.h"
#include "underwater_look.h"

#include <d3d9.h>

constexpr double kUnderwaterClockPeriodSeconds = 4096.0;

class UnderwaterRenderer
{
public:
    ~UnderwaterRenderer();

    void ReleaseDefaultPool();
    void ReleaseAll();

    bool CaptureSceneBeforeSurface(IDirect3DDevice9* dev, const D3DVIEWPORT9& world);
    bool Render(IDirect3DDevice9* dev, const SceneDepth& depth, const FrameInputs& in, const UnderwaterLook& look,
                bool useSceneBeforeSurface, double seconds);

    const char* LastSkipReason() const { return m_skip; }
    unsigned Draws() const { return m_draws; }
    bool HoldsCopies() const { return m_scene.texture || m_beforeSurface.texture; }

private:
    static constexpr DWORD kRenderTargets = 4;

    struct BoundTargets
    {
        IDirect3DSurface9* colour[kRenderTargets] = {};
        IDirect3DSurface9* depth = nullptr;
    };

    struct Copy
    {
        IDirect3DTexture9* texture = nullptr;
        UINT width = 0;
        UINT height = 0;
        D3DFORMAT format = D3DFMT_UNKNOWN;
    };

    bool Skip(const char* reason);
    bool EnsureShaders(IDirect3DDevice9* dev);
    bool EnsureStateBlock(IDirect3DDevice9* dev);
    bool EnsureCopy(IDirect3DDevice9* dev, Copy& copy, D3DFORMAT format, UINT width, UINT height);
    bool CopyWorld(IDirect3DDevice9* dev, Copy& copy, IDirect3DSurface9* target, const D3DVIEWPORT9& world);
    bool RenderToTarget(IDirect3DDevice9* dev, const BoundTargets& bound, const SceneDepth& depth,
                        const FrameInputs& in, const UnderwaterLook& look, bool useSceneBeforeSurface,
                        double seconds);
    void DrawPass(IDirect3DDevice9* dev, const BoundTargets& bound, const SceneDepth& depth, const D3DVIEWPORT9& world,
                  const float* constants, UINT constantRows, bool useSceneBeforeSurface);

    IDirect3DVertexShader9* m_vs = nullptr;
    IDirect3DPixelShader9* m_ps = nullptr;
    IDirect3DVertexDeclaration9* m_decl = nullptr;
    IDirect3DStateBlock9* m_state = nullptr;
    Copy m_scene;
    Copy m_beforeSurface;
    bool m_copyFailed = false;
    IDirect3DDevice9* m_unsupportedDevice = nullptr;
    unsigned m_draws = 0;
    const char* m_skip = "";
};
