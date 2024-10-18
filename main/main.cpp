// Dear ImGui: standalone example application for DirectX 9

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "misc/cpp/imgui_stdlib.h"
#include <d3d9.h>
#include <d3dx9tex.h>
#include <tchar.h>

#include <thread>
#include <vector>

#include "resource.h"

#ifndef WM_DPICHANGED
#define WM_DPICHANGED                   0x02E0
#endif

// DirectX相关
struct D3D
{
    // Helper functions
    bool createDeviceD3D(HWND hWnd)
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
        if (this->pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &this->d3dpp, &this->pd3dDevice) < 0)
            return false;

        return true;
    }

    void cleanupDeviceD3D()
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

    void resetDevice()
    {
        ImGui_ImplDX9_InvalidateDeviceObjects();
        HRESULT hr = this->pd3dDevice->Reset(&this->d3dpp);
        if (hr == D3DERR_INVALIDCALL)
            IM_ASSERT(0);
        ImGui_ImplDX9_CreateDeviceObjects();
    }

    LPDIRECT3D9 pD3D = nullptr;
    LPDIRECT3DDEVICE9 pd3dDevice = nullptr;
    bool deviceLost = false;
    bool forceReset = false;
    UINT resizeWidth = 0, resizeHeight = 0;
    D3DPRESENT_PARAMETERS d3dpp = {};
};

// Win32窗口相关
struct Win32Window
{
    static LRESULT WINAPI WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

    bool registerWndClass( LPCWSTR wndClassName, HINSTANCE hInstance )
    {
        this->hIcon = LoadIconW( hInstance, MAKEINTRESOURCEW(IDI_EIENLOGGUI) );
        this->wc.cbSize = sizeof(WNDCLASSEXW);
        this->wc.style = CS_CLASSDC;
        this->wc.lpfnWndProc = WndProc;
        this->wc.cbClsExtra = 0;
        this->wc.cbWndExtra = 0;
        this->wc.hInstance = hInstance;
        this->wc.hIcon = this->hIcon;
        this->wc.hCursor = nullptr;
        this->wc.hbrBackground = nullptr;
        this->wc.lpszMenuName = nullptr;
        this->wc.lpszClassName = wndClassName;
        this->wc.hIconSm = this->hIcon;

        if ( !RegisterClassExW(&this->wc) ) return false;
        return true;
    }

    void unregisterWndClass()
    {
        UnregisterClassW( wc.lpszClassName, wc.hInstance );
    }

    bool createWindow( LPCWSTR wndTitleName )
    {
        this->hWnd = ::CreateWindowExW( 0, wc.lpszClassName, wndTitleName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, wc.hInstance, nullptr );
        bool ok = this->hWnd != nullptr;
        return ok;
    }

    void showUpdate( int cmdShow = SW_SHOWDEFAULT, bool update = true )
    {
        ::ShowWindow( this->hWnd, cmdShow );
        if ( update ) ::UpdateWindow(this->hWnd);
    }

    void destroyWindow()
    {
        if ( this->hWnd != nullptr ) ::DestroyWindow(this->hWnd);
    }

    WNDCLASSEXW wc;
    HWND hWnd;
    HICON hIcon;
};

struct EienLogContext
{
    USHORT port;
    std::string name;
};

// 本应用程序
struct App
{
    static App * app;

    App( HINSTANCE hInstance, int nCmdShow ) : hInstance(hInstance), nCmdShow(nCmdShow)
    {
    }

    ~App()
    {
    }

    bool initInstance();

    void exitInstance();

    // 消息循环
    int run();

    // 渲染界面
    void renderUI();

    void renderDockSpace();
    void renderDockSpaceMenuBar();

    // 渲染Tooltip
    static void HelpMarker(const char* desc)
    {
        ImGui::TextDisabled("(?)");
        if ( ImGui::BeginItemTooltip() )
        {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    HINSTANCE hInstance;
    int nCmdShow;
    D3D dx; // DirectX 3D
    Win32Window w32wnd; // Win32 Window
    ImGuiContext * ctx; // ImGui Context

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Dock space state
    ImGuiID dockspace_id;
    bool opt_fullscreen = true;
    bool opt_padding = false;
    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

    // MyData
    LPDIRECT3DTEXTURE9 pTexture = nullptr;
    ImVec2 imgSize;
    std::vector<EienLogContext> logCtxs;
};

App * App::app;

bool App::initInstance()
{
    // Create application window
    if ( !w32wnd.registerWndClass( L"EienLog Viewer Class", this->hInstance ) ) return false;
    if ( !w32wnd.createWindow(L"EienLog日志查看器") ) return false;

    // Initialize Direct3D
    if ( !dx.createDeviceD3D(w32wnd.hWnd) ) return false;

    // Show the window
    w32wnd.showUpdate(this->nCmdShow);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    this->ctx = ImGui::CreateContext();
    //ImGuiIO& io = ImGui::GetIO();
    this->ctx->IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    this->ctx->IO.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    this->ctx->IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    //ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();
    ImGui::StyleColorsClassic();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (this->ctx->IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(w32wnd.hWnd);
    ImGui_ImplDX9_Init(dx.pd3dDevice);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //ImFont * font = this->ctx->IO.Fonts->AddFontDefault();
    ImFontConfig config;
    //config.MergeMode = true;
    //ImFont * font = this->ctx->IO.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont * font = this->ctx->IO.Fonts->AddFontFromFileTTF(
    //    "C:\\Windows\\Fonts\\ArialUni.ttf",
    //    18.0f,
    //    nullptr,
    //    this->ctx->IO.Fonts->GetGlyphRangesChineseSimplifiedCommon()
    //);
    //IM_ASSERT(font != nullptr);

    //ImFont* font1 = this->ctx->IO.Fonts->AddFontFromFileTTF(
    //    "C:\\Windows\\Fonts\\msyh.ttc",
    //    20,
    //    nullptr,
    //    this->ctx->IO.Fonts->GetGlyphRangesChineseFull()
    //);
    //IM_ASSERT(font1 != nullptr);

    static ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges( this->ctx->IO.Fonts->GetGlyphRangesChineseSimplifiedCommon() );
    ImWchar c = u'钮';
    builder.AddChar(c);
    builder.BuildRanges(&ranges);
    const ImWchar * CharsetData = ranges.Data;

    /*setlocale( LC_CTYPE, "" );
    auto i = 0, n = 0;
    for ( ; CharsetData[i]; i+=2 )
    {
        n += CharsetData[i+1] - CharsetData[i] + 1;
        for ( auto c = CharsetData[i]; c <= CharsetData[i + 1]; c++ )
        {
            wprintf( L"%c[%x] ", c, c );
        }
        wprintf(L"\n");
        //wprintf( L"%c[%x] ~ %c[%x] : %d\n", CharsetData[i], CharsetData[i], CharsetData[i+1], CharsetData[i+1], CharsetData[i+1] - CharsetData[i] + 1 );
    }
    wprintf( L"%d, %d\n", i, n );*/

    //std::thread th( [this] {
    ImFont* font2 = this->ctx->IO.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\simhei.ttf",
        16,
        &config,
        CharsetData
        //this->ctx->IO.Fonts->GetGlyphRangesChineseSimplifiedCommon()
    );
    IM_ASSERT(font2 != nullptr);
    //} ); th.detach();

    D3DXIMAGE_INFO imgInfo = { 0 };
    HRESULT hr = S_OK;// = D3DXCreateTextureFromFile( this->dx.pd3dDevice, LR"(J:\Pictures\image0001.jpg)", &this->pTexture );
    //hr = D3DXCreateTextureFromFileEx( this->dx.pd3dDevice, LR"(J:\Pictures\image0001-1.png)", D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, 0, &imgInfo, NULL, &pTexture);
    if (SUCCEEDED(hr))
    {
        this->imgSize.x = (float)imgInfo.Width;
        this->imgSize.y = (float)imgInfo.Height;
    }
    return true;
}

void App::exitInstance()
{
    // Cleanup
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(this->ctx);

    dx.cleanupDeviceD3D();
    w32wnd.destroyWindow();
    w32wnd.unregisterWndClass();
}

int App::run()
{
    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle lost D3D9 device
        if (dx.deviceLost)
        {
            HRESULT hr = dx.pd3dDevice->TestCooperativeLevel();
            if (hr == D3DERR_DEVICELOST)
            {
                ::Sleep(10);
                continue;
            }
            if (hr == D3DERR_DEVICENOTRESET)
                dx.resetDevice();
            dx.deviceLost = false;
        }

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (dx.resizeWidth != 0 && dx.resizeHeight != 0)
        {
            dx.d3dpp.BackBufferWidth = dx.resizeWidth;
            dx.d3dpp.BackBufferHeight = dx.resizeHeight;
            dx.resizeWidth = dx.resizeHeight = 0;
            dx.resetDevice();
        }

        if (dx.forceReset)
        {
            dx.forceReset = false;
            dx.resetDevice();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // 1. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        this->renderUI();

        // 2. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // Rendering
        ImGui::EndFrame();
        dx.pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        dx.pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        dx.pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_color.x*clear_color.w*255.0f), (int)(clear_color.y*clear_color.w*255.0f), (int)(clear_color.z*clear_color.w*255.0f), (int)(clear_color.w*255.0f));
        dx.pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
        if (dx.pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            dx.pd3dDevice->EndScene();
        }

        // Update and Render additional Platform Windows
        if (this->ctx->IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        HRESULT result = dx.pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
        if (result == D3DERR_DEVICELOST)
            dx.deviceLost = true;
    }
    return 0;
}

void App::renderUI()
{
    this->renderDockSpace();

    ImGuiWindowClass window_class;
    //window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
    ImGui::SetNextWindowClass(&window_class);

    static bool eiengui_win = true;
    if ( eiengui_win )
    {
        ImGui::Begin( u8"你好，EienLogGui！", &eiengui_win );// Create a window called "Hello, world!" and append into it.
        ImGui::SetWindowDock( ImGui::GetCurrentWindow(), dockspace_id, ImGuiCond_Once );
        ImGui::Text( u8"这是一些无用的文本，测试中文显示。" );               // Display some text (you can use a format strings too)
        ImGui::SameLine();
        if ( ImGui::Button( "OK" ) ) MessageBoxW( w32wnd.hWnd, L"msgbox", L"", 0 );
        ImGui::Separator();
        ImGui::Checkbox( "Demo Window", &show_demo_window );      // Edit bools storing our window open/close state
        ImGui::Checkbox( "Another Window", &show_another_window );

        static float f = 0.0f;
        ImGui::SliderFloat( "float", &f, 0.0f, 1.0f );            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3( "clear color", (float*)&clear_color ); // Edit 3 floats representing a color

        static int counter = 0;
        if ( ImGui::Button( "Button" ) )                            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text( "counter = %d", counter );

        ImGui::Text( "Application average %.3f ms/frame (%.1f FPS)", 1000.0f / this->ctx->IO.Framerate, this->ctx->IO.Framerate );
        static bool vsync = true;
        if ( ImGui::Checkbox( u8"垂直同步", &vsync ) )
        {
            dx.d3dpp.PresentationInterval = vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
            dx.forceReset = true;
        }
        ImGui::End();
    }

    for ( auto && logCtx : this->logCtxs )
    {
        ImGui::Begin( logCtx.name.c_str() );
        ImGui::SetWindowDock( ImGui::GetCurrentWindow(), dockspace_id, ImGuiCond_Once );
        ImGui::Text( u8"监听端口%u", logCtx.port );
        if ( this->pTexture ) ImGui::Image( (ImTextureID)this->pTexture, this->imgSize );
        ImGui::End();
    }
}

void App::renderDockSpace()
{
    // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
    // because it would be confusing to have two docking targets within each others.
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if ( opt_fullscreen )
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    }
    else
    {
        dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
    }

    // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
    // and handle the pass-thru hole, so we ask Begin() to not render a background.
    if ( dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode ) window_flags |= ImGuiWindowFlags_NoBackground;

    // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
    // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
    // all active windows docked into it will lose their parent and become undocked.
    // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
    // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
    if ( !opt_padding ) ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("EienLog DockSpace", nullptr, window_flags);

    if (!opt_padding) ImGui::PopStyleVar();

    if (opt_fullscreen) ImGui::PopStyleVar(2);

    // Submit the DockSpace
    ImGuiIO& io = this->ctx->IO;

    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        dockspace_id = ImGui::GetID("EienLogGuiDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }
    else
    {
        ImGui::Text("ERROR: Docking is not enabled! See Demo > Configuration.");
        ImGui::Text("Set io.ConfigFlags |= ImGuiConfigFlags_DockingEnable in your code, or ");
        ImGui::SameLine(0.0f, 0.0f);
        if (ImGui::SmallButton("click here"))
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    }

    this->renderDockSpaceMenuBar();

    ImGui::End();
}

void App::renderDockSpaceMenuBar()
{
    if (ImGui::BeginMenuBar())
    {
        //static bool open_modal_window = false;
        static bool toggle_popup = false;
        if ( ImGui::BeginMenu(u8"文件") )
        {
            if ( ImGui::MenuItem(u8"新建...") )
            {
                //open_modal_window = true;
                toggle_popup = true;
            }
            ImGui::Separator();
            if ( ImGui::MenuItem(u8"退出") )
            {
                PostQuitMessage(0);
            }
            ImGui::EndMenu();
        }
        //if ( open_modal_window )
        {
            if ( toggle_popup ) { ImGui::OpenPopup("New..."); toggle_popup = false; }
            // Always center this window when appearing
            //ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            //ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2( 0.5f, 0.5f ) );

            if ( ImGui::BeginPopupModal( "New...", NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
            {
                ImGui::Text( u8"新建一个监听窗口，监听日志信息" );
                ImGui::Separator();
                static int port = 23456;
                // 对齐标签文本
                ImGui::AlignTextToFramePadding();
                ImGui::Text(u8"监听端口"); // 这将是左侧的标签
                ImGui::SameLine( 0.0f, 1.0f );
                // 设置输入框的宽度
                //ImGui::PushItemWidth(-1);
                ImGui::InputInt( u8"##listen_port", &port, 1, 65535 );

                static char ch[] = {'A', 0 };
                static std::string name = std::string(u8"日志") + ch;
                ImGui::InputText( u8"区分名称", &name );
                //static bool dont_ask_me_next_time = false;
                ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 0 ) );
                //ImGui::Checkbox( "Don't ask me next time", &dont_ask_me_next_time );
                ImGui::PopStyleVar();
                //ImGui::Checkbox( "Don't ask me next time.", &dont_ask_me_next_time );

                if ( ImGui::Button( "OK", ImVec2( 120, 0 ) ) || ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)) ) {
                    ImGui::CloseCurrentPopup();
                    
                    this->logCtxs.push_back( { (USHORT)port, name } );
                    ch[0]++;
                    name = std::string(u8"日志") + ch;
                    port++;
                }
                ImGui::SetItemDefaultFocus();
                ImGui::SameLine();
                if ( ImGui::Button( "Cancel", ImVec2( 120, 0 ) ) ) {
                    ImGui::CloseCurrentPopup();
                    //open_modal_window = false;
                }
                ImGui::EndPopup();
            }
        }
        if ( ImGui::BeginMenu(u8"主题") )
        {
            static int colorsTheme = 2;
            if ( ImGui::MenuItem( u8"暗黑（Dark）", nullptr, colorsTheme == 0 ) )
            {
                colorsTheme = 0;
                ImGui::StyleColorsDark();
            }
            if ( ImGui::MenuItem( u8"明亮（Light）", nullptr, colorsTheme == 1 ) )
            {
                colorsTheme = 1;
                ImGui::StyleColorsLight();
            }
            if ( ImGui::MenuItem( u8"经典（Classic）", nullptr, colorsTheme == 2 ) )
            {
                colorsTheme = 2;
                ImGui::StyleColorsClassic();
            }
            ImGui::EndMenu();
        }
        if ( ImGui::BeginMenu(u8"工作区") )
        {
            // Disabling fullscreen would allow the window to be moved to the front of other windows,
            // which we can't undo at the moment without finer window depth/z control.
            ImGui::MenuItem( u8"全窗显示", nullptr, &opt_fullscreen );
            ImGui::MenuItem( u8"Padding", nullptr, &opt_padding );
            ImGui::Separator();

            if (ImGui::MenuItem("Flag: NoDockingOverCentralNode", "", (dockspace_flags & ImGuiDockNodeFlags_NoDockingOverCentralNode) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoDockingOverCentralNode; }
            if (ImGui::MenuItem("Flag: NoDockingSplit",         "", (dockspace_flags & ImGuiDockNodeFlags_NoDockingSplit) != 0))             { dockspace_flags ^= ImGuiDockNodeFlags_NoDockingSplit; }
            if (ImGui::MenuItem("Flag: NoUndocking",            "", (dockspace_flags & ImGuiDockNodeFlags_NoUndocking) != 0))                { dockspace_flags ^= ImGuiDockNodeFlags_NoUndocking; }
            if (ImGui::MenuItem("Flag: NoResize",               "", (dockspace_flags & ImGuiDockNodeFlags_NoResize) != 0))                   { dockspace_flags ^= ImGuiDockNodeFlags_NoResize; }
            if (ImGui::MenuItem("Flag: AutoHideTabBar",         "", (dockspace_flags & ImGuiDockNodeFlags_AutoHideTabBar) != 0))             { dockspace_flags ^= ImGuiDockNodeFlags_AutoHideTabBar; }
            if (ImGui::MenuItem("Flag: PassthruCentralNode",    "", (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) != 0, opt_fullscreen)) { dockspace_flags ^= ImGuiDockNodeFlags_PassthruCentralNode; }
            //ImGui::Separator();

            /*if (ImGui::MenuItem("Close", NULL, false, p_open != NULL))
            *p_open = false;*/
            ImGui::EndMenu();
        }

        HelpMarker(
            u8"启用停靠功能后，您始终可以将大多数窗口停靠到另一个窗口中！" "\n"
            u8"- 从窗口标题栏或其标签拖动到停靠/解停靠。" "\n"
            u8"- 从窗口菜单按钮（左上角按钮）拖动可解除整个节点（所有窗口）的停靠。" "\n"
            u8"- 按住 SHIFT 键可禁用停靠（如果 io.ConfigDockingWithShift == false，默认值）" "\n"
            u8"- 按住 SHIFT 键启用停靠（如果 io.ConfigDockingWithShift == true）"
        );

        ImGui::EndMenuBar();
    }
}


// Main code
int APIENTRY wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow )
{
    App::app = new App( hInstance, nCmdShow );
    if ( !App::app->initInstance() ) return 1;
    int rc = App::app->run();
    App::app->exitInstance();
    delete App::app;
    App::app = nullptr;
    return rc;
}

int wmain()
{
    App::app = new App( GetModuleHandle(nullptr), SW_NORMAL );
    if ( !App::app->initInstance() ) return 1;
    int rc = App::app->run();
    App::app->exitInstance();
    delete App::app;
    App::app = nullptr;
    return rc;
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI Win32Window::WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        App::app->dx.resizeWidth = (UINT)LOWORD(lParam); // Queue resize
        App::app->dx.resizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED:
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
        {
            //const int dpi = HIWORD(wParam);
            //printf("WM_DPICHANGED to %d (%.0f%%)\n", dpi, (float)dpi / 96.0f * 100.0f);
            const RECT* suggested_rect = (RECT*)lParam;
            ::SetWindowPos(hWnd, nullptr, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        break;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
