﻿#include "App.h"
#include "MainWindow.h"

#include <dwmapi.h>

inline static winux::String __CalcAppConfigPath()
{
    winux::String exeFileName;
    winux::String exeDirPath = winux::FilePath( winux::GetExecutablePath(), &exeFileName );
    winux::String jsonConfigFilePath = winux::CombinePath( exeDirPath, winux::FileTitle(exeFileName) + L".json" );
    return jsonConfigFilePath;
}

void App::loadConfig()
{
    auto jsonConfig = winux::Json( winux::FileGetString( __CalcAppConfigPath(), winux::feUnspec ) );
    if ( !jsonConfig.isCollection() ) jsonConfig.createCollection();

    this->appConfig.colorTheme = jsonConfig.get( L"color_theme", 2 ).toInt();

    if ( jsonConfig.has(L"font") )
    {
        auto && fontConfig = jsonConfig[L"font"];
        this->appConfig.fontPath = winux::UnicodeConverter( fontConfig.get( L"path", L"C:\\Windows\\Fonts\\simhei.ttf" ).toUnicode() ).toUtf8();
        this->appConfig.fontSize = fontConfig.get( L"size", 18.0f ).toFloat();
    }
    else
    {
        this->appConfig.fontPath = u8"C:\\Windows\\Fonts\\simhei.ttf";
        this->appConfig.fontSize = 18.0f;
    }

    if ( jsonConfig.has(L"big_font") )
    {
        auto && bigFontConfig = jsonConfig[L"big_font"];
        this->appConfig.bigFontPath = winux::UnicodeConverter( bigFontConfig.get( L"path", L"C:\\Windows\\Fonts\\simhei.ttf" ).toUnicode() ).toUtf8();
        this->appConfig.bigFontSize = bigFontConfig.get( L"size", 32.0f ).toFloat();
    }
    else
    {
        this->appConfig.bigFontPath = u8"C:\\Windows\\Fonts\\simhei.ttf";
        this->appConfig.bigFontSize = 32.0f;
    }

    this->appConfig.logTableColumnResize = jsonConfig.get( L"log_table_column_resize", true );

    auto const & listenHistory = jsonConfig[L"listen_history"];
    for ( size_t i = 0; i < listenHistory.getCount(); i++ )
    {
        auto && lparams = listenHistory[i];
        ListenParams listenParams;
        listenParams.name = winux::UnicodeConverter( lparams.get( L"name", L"" ).toUnicode() ).toUtf8();
        listenParams.addr = winux::UnicodeConverter( lparams.get( L"addr", L"" ).toUnicode() ).toUtf8();
        listenParams.port = lparams.get( L"port", 22345 ).toUShort();
        listenParams.waitTimeout = lparams.get( L"wait_timeout", 50 ).toUInt64();
        listenParams.updateTimeout = lparams.get( L"update_timeout", 300 ).toUInt64();
        listenParams.vScrollToBottom = lparams.get( L"vscroll_to_bottom", true ).toBool();
        listenParams.soundEffect = lparams.get( L"sound_effect", true ).toBool();

        this->appConfig.listenHistory.push_back( std::move(listenParams) );
    }

    auto const & logFileHistory = jsonConfig[L"logfile_history"];
    for ( size_t i = 0; i < logFileHistory.getCount(); i++ )
    {
        this->appConfig.logFileHistory.push_back( winux::UnicodeConverter( logFileHistory[i].toUnicode() ).toUtf8() );
    }
}

void App::saveConfig()
{
    winux::Mixed jsonConfig;
    jsonConfig.createCollection();
    jsonConfig[L"color_theme"] = this->appConfig.colorTheme;
    jsonConfig[L"font"] = winux::$c{
        { L"path", winux::UnicodeConverter(this->appConfig.fontPath).toUnicode() },
        { L"size", this->appConfig.fontSize }
    };
    jsonConfig[L"big_font"] = winux::$c{
        { L"path", winux::UnicodeConverter(this->appConfig.bigFontPath).toUnicode() },
        { L"size", this->appConfig.bigFontSize }
    };
    jsonConfig[L"log_table_column_resize"] = this->appConfig.logTableColumnResize;
    auto & listenHistory = jsonConfig[L"listen_history"].createArray();
    for ( auto && listenParams : this->appConfig.listenHistory )
    {
        winux::Mixed lparams;
        lparams.createCollection();
        lparams[L"name"] = winux::UnicodeConverter(listenParams.name).toUnicode();
        lparams[L"addr"] = winux::UnicodeConverter(listenParams.addr).toUnicode();
        lparams[L"port"] = listenParams.port;
        lparams[L"wait_timeout"] = listenParams.waitTimeout;
        lparams[L"update_timeout"] = listenParams.updateTimeout;
        lparams[L"vscroll_to_bottom"] = listenParams.vScrollToBottom;
        lparams[L"sound_effect"] = listenParams.soundEffect;
        listenHistory.add( std::move(lparams) );
    }
    auto & logFileHistory = jsonConfig[L"logfile_history"].createArray();
    for ( auto && logFilePath : this->appConfig.logFileHistory )
    {
        logFileHistory.add( winux::UnicodeConverter(logFilePath).toUnicode() );
    }
    winux::FilePutString( __CalcAppConfigPath(), jsonConfig.myJson( false, L"    ", L"\n" ), winux::feUtf8Bom );
}

void App::setRecentListen( ListenParams const & lparams )
{
    auto & listenHistory = this->appConfig.listenHistory;
    auto it = listenHistory.begin();
    if ( ( it = std::find( it, listenHistory.end(), lparams ) ) != listenHistory.end() )
    {
        listenHistory.erase(it);
    }
    listenHistory.insert( listenHistory.begin(), lparams );
}

void App::setRecentOpen( winux::Utf8String const & logFile )
{
    auto & logFileHistory = this->appConfig.logFileHistory;
    auto it = logFileHistory.begin();
    if ( ( it = std::find( it, logFileHistory.end(), logFile ) ) != logFileHistory.end() )
    {
        logFileHistory.erase(it);
    }
    logFileHistory.insert( logFileHistory.begin(), logFile );
}

bool App::initInstance( HINSTANCE hInstance, int nCmdShow )
{
    // 加载JSON配置文件
    this->loadConfig();

    ImGui_ImplWin32_EnableDpiAwareness();

    // Create application window
    if ( !wi.registerWndClass( L"EienLog Viewer Class", hInstance ) ) return false;
    if ( !wi.createWindow(L"EienLog日志查看器") ) return false;

    //SetLayeredWindowAttributes(wi.hWnd, 0, 255, LWA_ALPHA);
    //const MARGINS margin = { -1, 0, 0, 0 };
    //DwmExtendFrameIntoClientArea(wi.hWnd, &margin);

    // Initialize Direct3D
    if ( !gi.create(wi) ) return false;

    // Show the window
    wi.showUpdate(nCmdShow);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    this->ctx = ImGui::CreateContext();
    //ImGuiIO& io = ImGui::GetIO();
    this->ctx->IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    this->ctx->IO.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    this->ctx->IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    //this->ctx->IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    switch ( this->appConfig.colorTheme )
    {
    case 0:
        ImGui::StyleColorsDark();
        break;
    case 1:
        ImGui::StyleColorsLight();
        break;
    case 2:
        ImGui::StyleColorsClassic();
        break;
    }

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (this->ctx->IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(wi.hWnd);
    ImGui_ImplDX9_Init(gi.pd3dDevice);

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

    #include "..\char-ranges\char-ranges.inl"
    const ImWchar * CharsetRanges = app_full_ranges;

    this->normalFont = this->ctx->IO.Fonts->AddFontFromFileTTF(
        this->appConfig.fontPath.c_str(),
        this->appConfig.fontSize,
        &config,
        CharsetRanges
    );
    IM_ASSERT(this->normalFont != nullptr);

    this->welcomeText = u8"您好，欢迎使用EienLog日志查看器！";
    static ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddText(this->welcomeText.c_str());
    builder.BuildRanges(&ranges);

    this->bigFont = this->ctx->IO.Fonts->AddFontFromFileTTF(
        this->appConfig.bigFontPath.c_str(),
        this->appConfig.bigFontSize,
        &config,
        ranges.Data
    );
    IM_ASSERT(this->bigFont != nullptr);

    this->mainWindow = new MainWindow(*this);

    //HRESULT hr = D3DXCreateTextureFromFileEx(
    //    this->gi.pd3dDevice,
    //    LR"(J:\Pictures\ComicImages\23_2.jpg)",
    //    D3DX_DEFAULT,
    //    D3DX_DEFAULT,
    //    D3DX_DEFAULT,
    //    0,
    //    D3DFMT_UNKNOWN,
    //    D3DPOOL_MANAGED,
    //    D3DX_DEFAULT,
    //    D3DX_DEFAULT,
    //    0,
    //    &this->bgImgInfo,
    //    NULL,
    //    &this->pBgTexture
    //);


    return true;
}

void App::exitInstance()
{
    // Cleanup
    if ( this->mainWindow ) delete this->mainWindow;

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(this->ctx);

    gi.cleanup();
    wi.destroyWindow();
    wi.unregisterWndClass();

    // 保存JSON配置文件
    this->saveConfig();
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
        if (gi.deviceLost)
        {
            HRESULT hr = gi.pd3dDevice->TestCooperativeLevel();
            if (hr == D3DERR_DEVICELOST)
            {
                ::Sleep(10);
                continue;
            }
            if (hr == D3DERR_DEVICENOTRESET)
                gi.reset();
            gi.deviceLost = false;
        }

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (gi.resizeWidth != 0 && gi.resizeHeight != 0)
        {
            gi.d3dpp.BackBufferWidth = gi.resizeWidth;
            gi.d3dpp.BackBufferHeight = gi.resizeHeight;
            gi.resizeWidth = gi.resizeHeight = 0;
            gi.reset();
        }

        if (gi.forceReset)
        {
            gi.forceReset = false;
            gi.reset();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // 渲染UI
        this->renderUI();

        // Rendering
        ImGui::EndFrame();

        gi.pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        gi.pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        gi.pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(bgClearColor.x*bgClearColor.w*255.0f), (int)(bgClearColor.y*bgClearColor.w*255.0f), (int)(bgClearColor.z*bgClearColor.w*255.0f), (int)(bgClearColor.w*255.0f));
        gi.pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);

        if (gi.pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            gi.pd3dDevice->EndScene();
        }

        // Update and Render additional Platform Windows
        if (this->ctx->IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        HRESULT result = gi.pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
        if (result == D3DERR_DEVICELOST)
            gi.deviceLost = true;
    }
    return 0;
}

void App::renderUI()
{
    if ( mainWindow )
    {
        mainWindow->render();
    }

    if ( this->pBgTexture )
    {
        ImGuiIO& io = ImGui::GetIO();
        // 设置窗口位置和大小
        ImGui::SetNextWindowPos( ImVec2( 0, 0 ), ImGuiCond_Always, ImVec2( 0, 0 ) );
        ImGui::SetNextWindowSize( ImVec2( io.DisplaySize.x, io.DisplaySize.y ) );
        // 设置窗口为透明
        ImGui::SetNextWindowBgAlpha(0);
        // 纹理ID
        static ImTextureID bg_tex_id = nullptr;
        if ( !bg_tex_id )
        {
            // 这里使用opencv加载图片，你也可以使用其他方式加载图片
            bg_tex_id = (ImTextureID)pBgTexture;
        }
        // 设置窗口的padding为0，使图片控件充满窗口
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );
        // 设置窗口为无边框
        ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0 );
        // 创建窗口使其固定在一个位置
        ImGui::Begin( u8"背景", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar );
        ImGui::Image( bg_tex_id, ImGui::GetContentRegionAvail() );
        ImGui::End();
        ImGui::PopStyleVar(2);
    }
}

