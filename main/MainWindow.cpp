﻿#include "App.h"
#include "MainWindow.h"

MainWindow::MainWindow( App & app ) : app(app)
{
    this->logWinManager.attachNew( new LogWindowsManager(this) );
    this->newListenWindow.attachNew( new NewLogListenWindowModal( this->logWinManager.get(), u8"新建监听..." ) );
}

void MainWindow::render()
{
    this->renderDockSpace();

    ImGuiWindowClass window_class;
    //window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
    ImGui::SetNextWindowClass(&window_class);

    if ( showSettingsWindow )
    {
        ImGui::Begin( u8"EienLogViewer", &showSettingsWindow );
        ImGui::SetWindowDock( ImGui::GetCurrentWindow(), dockSpaceId, ImGuiCond_Once );
        ImGui::PushFont(app.bigFont);
        ImGui::Text(app.welcomeText.c_str());
        ImGui::PopFont();
        ImGui::Text(u8"这里是一些相关设置，您可以修改它们。");
        //ImGui::SameLine();
        //if ( ImGui::Button( "OK" ) ) MessageBoxW( app.wi.hWnd, L"msgbox", L"", 0 );
        ImGui::Separator();
        ImGui::Checkbox( u8"演示窗口", &showDemoWindow );      // Edit bools storing our window open/close state
        ImGui::Checkbox( u8"关于窗口", &showAboutWindow );

        //static float f = 0.0f;
        //ImGui::SliderFloat( "float", &f, 0.0f, 1.0f );            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3( u8"背景擦除色", (float*)&app.bgClearColor ); // Edit 3 floats representing a color

        static bool vsync = true;
        if ( ImGui::Checkbox( u8"垂直同步", &vsync ) )
        {
            app.gi.d3dpp.PresentationInterval = vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
            app.gi.forceReset = true;
        }
        ImGui::Text( u8"应用程序平均 %.3f ms/frame (%.1f FPS)", 1000.0f / app.ctx->IO.Framerate, app.ctx->IO.Framerate );

        //ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 10, 5 ) );
        if ( ImGui::Button(u8"创建新日志窗口") )
        {
            this->newListenWindow->toggle();
        }
        //ImGui::PopStyleVar();

        ImGui::End();
    }

    this->logWinManager->render();
    this->newListenWindow->render();

    if ( showDemoWindow )
        ImGui::ShowDemoWindow(&showDemoWindow);

    if ( showAboutWindow )
    {
        ImGui::Begin(u8"关于", &showAboutWindow, 0/*ImGuiWindowFlags_AlwaysAutoResize*/);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        ImGui::Text(u8"EienLog Viewer 1.0.0");
        ImGui::TextLinkOpenURL("FastDo", "https://fastdo.net");
        ImGui::SameLine();
        HelpMarker(u8"官方网站");
        ImGui::Separator();
        ImGui::Text(app.welcomeText.c_str());
        if (ImGui::Button(u8"关闭", ImVec2(100, 0)))
            showAboutWindow = false;
        ImGui::End();
    }
}

void MainWindow::renderDockSpace()
{
    // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
    // because it would be confusing to have two docking targets within each others.
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground;
    if ( optFullScreen )
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
        dockSpaceFlags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
    }

    // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
    // and handle the pass-thru hole, so we ask Begin() to not render a background.
    if ( dockSpaceFlags & ImGuiDockNodeFlags_PassthruCentralNode ) window_flags |= ImGuiWindowFlags_NoBackground;

    // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
    // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
    // all active windows docked into it will lose their parent and become undocked.
    // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
    // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
    if ( !optPadding ) ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("EienLog DockSpace", nullptr, window_flags);

    if (!optPadding) ImGui::PopStyleVar();

    if (optFullScreen) ImGui::PopStyleVar(2);

    // Submit the DockSpace
    ImGuiIO& io = this->app.ctx->IO;

    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        dockSpaceId = ImGui::GetID("EienLogGuiDockSpace");
        ImGui::DockSpace(dockSpaceId, ImVec2(0.0f, 0.0f), dockSpaceFlags);
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

void MainWindow::renderDockSpaceMenuBar()
{
    bool isCtrlDown = ImGui::IsKeyDown( ImGui::GetKeyIndex(ImGuiKey_LeftCtrl) ) || ImGui::IsKeyDown( ImGui::GetKeyIndex(ImGuiKey_RightCtrl) );
    // 快捷 新建监听
    if ( isCtrlDown && ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_N) ) )
    {
        this->newListenWindow->toggle();
    }
    // 快捷 打开日志
    if ( isCtrlDown && ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_O) ) )
    {
    }

    // 菜单栏
    if (ImGui::BeginMenuBar())
    {
        if ( ImGui::BeginMenu(u8"文件") )
        {
            static int idLogWindow = 1;

            if ( ImGui::MenuItem(u8"新建监听...", u8"Ctrl+N") )
            {
                this->newListenWindow->toggle();
            }
            if ( ImGui::MenuItem(u8"打开日志...", u8"Ctrl+O") )
            {
                winplus::FileDialog dlg{app.wi.hWnd};
                if ( dlg.doModal( L".", L"日志文件(*.csvlog)\0*.csvlog\0全部文件(*.*)\0*.*\0\0" ) )
                {
                    winux::String filepath = dlg.getFilePath();
                    winux::String filename;
                    winux::FilePath( filepath, &filename );

                    this->logWinManager->addWindow( u8"日志[" + $u8( winux::FileTitle(filename) ) + winux::FormatA( u8"] %d", idLogWindow++ ), true, $u8(filepath) );
                }
            }
            ImGui::Separator();
            if ( ImGui::BeginMenu(u8"最近开启监听") )
            {
                auto & listenHistory = this->app.appConfig.listenHistory;
                for ( size_t i = 0; i < listenHistory.size(); i++ )
                {
                    auto lparams = listenHistory[i]; // 这里不能使用引用，因为addWindow()内调用setRecent*()可能会移除本元素导致引用悬垂
                    if ( ImGui::MenuItem( winux::FormatA( u8"%s-%s-%hu-%u-%u-%u-%u", lparams.name.c_str(), lparams.addr.c_str(), lparams.port, lparams.waitTimeout, lparams.updateTimeout, lparams.vScrollToBottom, lparams.soundEffect ).c_str() ) )
                    {
                        this->logWinManager->addWindow(lparams);
                    }
                }
                ImGui::EndMenu();
            }
            if ( ImGui::BeginMenu(u8"最近打开日志") )
            {
                auto & logFileHistory = this->app.appConfig.logFileHistory;
                for ( size_t i = 0; i < logFileHistory.size(); i++ )
                {
                    auto logFile = logFileHistory[i]; // 这里不能使用引用，因为addWindow()内调用setRecent*()可能会移除本元素导致引用悬垂
                    if ( ImGui::MenuItem( logFile.c_str() ) )
                    {
                        winux::String filename;
                        winux::FilePath( $L(logFile), &filename );

                        this->logWinManager->addWindow( u8"日志[" + $u8( winux::FileTitle(filename) ) + winux::FormatA( u8"] %d", idLogWindow++ ), true, logFile );
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if ( ImGui::MenuItem(u8"退出") )
            {
                PostQuitMessage(0);
            }
            ImGui::EndMenu();
        }
        if ( ImGui::BeginMenu(u8"主题") )
        {
            if ( ImGui::MenuItem( u8"暗黑（Dark）", nullptr, this->app.appConfig.colorTheme == 0 ) )
            {
                this->app.appConfig.colorTheme = 0;
                ImGui::StyleColorsDark();
            }
            if ( ImGui::MenuItem( u8"明亮（Light）", nullptr, this->app.appConfig.colorTheme == 1 ) )
            {
                this->app.appConfig.colorTheme = 1;
                ImGui::StyleColorsLight();
            }
            if ( ImGui::MenuItem( u8"经典（Classic）", nullptr, this->app.appConfig.colorTheme == 2 ) )
            {
                this->app.appConfig.colorTheme = 2;
                ImGui::StyleColorsClassic();
            }
            ImGui::EndMenu();
        }
        if ( ImGui::BeginMenu(u8"工作区") )
        {
            // Disabling fullscreen would allow the window to be moved to the front of other windows,
            // which we can't undo at the moment without finer window depth/z control.
            ImGui::MenuItem( u8"全窗显示", nullptr, &optFullScreen );
            ImGui::MenuItem( u8"Padding", nullptr, &optPadding );
            ImGui::Separator();

            if (ImGui::MenuItem("Flag: NoDockingOverCentralNode", "", (dockSpaceFlags & ImGuiDockNodeFlags_NoDockingOverCentralNode) != 0)) { dockSpaceFlags ^= ImGuiDockNodeFlags_NoDockingOverCentralNode; }
            if (ImGui::MenuItem("Flag: NoDockingSplit",         "", (dockSpaceFlags & ImGuiDockNodeFlags_NoDockingSplit) != 0))             { dockSpaceFlags ^= ImGuiDockNodeFlags_NoDockingSplit; }
            if (ImGui::MenuItem("Flag: NoUndocking",            "", (dockSpaceFlags & ImGuiDockNodeFlags_NoUndocking) != 0))                { dockSpaceFlags ^= ImGuiDockNodeFlags_NoUndocking; }
            if (ImGui::MenuItem("Flag: NoResize",               "", (dockSpaceFlags & ImGuiDockNodeFlags_NoResize) != 0))                   { dockSpaceFlags ^= ImGuiDockNodeFlags_NoResize; }
            if (ImGui::MenuItem("Flag: AutoHideTabBar",         "", (dockSpaceFlags & ImGuiDockNodeFlags_AutoHideTabBar) != 0))             { dockSpaceFlags ^= ImGuiDockNodeFlags_AutoHideTabBar; }
            if (ImGui::MenuItem("Flag: PassthruCentralNode",    "", (dockSpaceFlags & ImGuiDockNodeFlags_PassthruCentralNode) != 0, optFullScreen)) { dockSpaceFlags ^= ImGuiDockNodeFlags_PassthruCentralNode; }
            //ImGui::Separator();

            /*if (ImGui::MenuItem("Close", NULL, false, p_open != NULL))
            *p_open = false;*/
            ImGui::EndMenu();
        }
        if ( ImGui::BeginMenu(u8"窗口") )
        {
            ImGui::MenuItem( u8"设置...", nullptr, &showSettingsWindow );
            ImGui::MenuItem( u8"演示...", nullptr, &showDemoWindow );
            ImGui::MenuItem( u8"关于", nullptr, &showAboutWindow );

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
