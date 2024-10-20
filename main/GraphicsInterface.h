#pragma once

struct WindowInterface;
// 图形底层 DirectX
struct GraphicsInterface
{
    // Helper functions
    bool create( WindowInterface & wi );

    void cleanup();

    void reset();

    LPDIRECT3D9 pD3D = nullptr;
    LPDIRECT3DDEVICE9 pd3dDevice = nullptr;
    bool deviceLost = false;
    bool forceReset = false;
    UINT resizeWidth = 0, resizeHeight = 0;
    D3DPRESENT_PARAMETERS d3dpp = {};
};
