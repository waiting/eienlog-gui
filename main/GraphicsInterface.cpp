#include "imgui_impl_dx9.h"
#include <d3d9.h>
#include "WindowInterface.h"
#include "GraphicsInterface.h"

bool GraphicsInterface::create( WindowInterface & wi )
{
    if ((this->pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
        return false;

    // Create the D3DDevice
    ZeroMemory(&this->d3dpp, sizeof(this->d3dpp));
    this->d3dpp.Windowed = TRUE;
    this->d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    this->d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
    this->d3dpp.EnableAutoDepthStencil = TRUE;
    this->d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    this->d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;           // Present with vsync
                                                                          //this->d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // Present without vsync, maximum unthrottled framerate
    if (this->pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wi.hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &this->d3dpp, &this->pd3dDevice) < 0)
        return false;

    return true;
}

void GraphicsInterface::cleanup()
{
    if (this->pd3dDevice)
    {
        this->pd3dDevice->Release();
        this->pd3dDevice = nullptr;
    }
    if (this->pD3D)
    {
        this->pD3D->Release();
        this->pD3D = nullptr;
    }
}

void GraphicsInterface::reset()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = this->pd3dDevice->Reset(&this->d3dpp);
    if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}
