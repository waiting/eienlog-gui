#include "App.h"
#include "MainWindow.h"

MainWindow::MainWindow( App & app ) : app(app)
{
    this->logWinManager.attachNew( new EienLogWindows(this) );
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
            toggleNewPopup = true;
        }
        //ImGui::PopStyleVar();

        ImGui::End();
    }

    this->logWinManager->render();

    if (showDemoWindow)
        ImGui::ShowDemoWindow(&showDemoWindow);

    if (showAboutWindow)
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
    if (ImGui::BeginMenuBar())
    {
        if ( ImGui::BeginMenu(u8"文件") )
        {
            if ( ImGui::MenuItem(u8"新建...") )
            {
                toggleNewPopup = true;
            }
            ImGui::Separator();
            if ( ImGui::MenuItem(u8"退出") )
            {
                PostQuitMessage(0);
            }
            ImGui::EndMenu();
        }

        {
            std::string popupWinName = u8"新建...";
            if ( toggleNewPopup ) { ImGui::OpenPopup(popupWinName.c_str()); toggleNewPopup = false; }
            // Always center this window when appearing
            //ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            //ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2( 0.5f, 0.5f ) );

            if ( ImGui::BeginPopupModal( popupWinName.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings ) )
            {
                ImGui::Text( u8"新建一个日志窗口，监听日志信息" );
                ImGui::Separator();
                static std::string addr = u8"";
                static int port = 22345;
                // 对齐标签文本
                ImGui::AlignTextToFramePadding();
                ImGui::Text(u8"地址"); // 这将是左侧的标签
                ImGui::SameLine( 0.0f, 1.0f );
                // 设置输入框的宽度
                ImGui::PushItemWidth(160);
                ImGui::InputText( u8"##addr", &addr );
                ImGui::PopItemWidth();
                ImGui::SameLine();
                ImGui::AlignTextToFramePadding();
                ImGui::Text(u8"端口"); // 这将是左侧的标签
                ImGui::SameLine( 0.0f, 1.0f );
                ImGui::PushItemWidth(120);
                ImGui::InputInt( u8"##port", &port, 1 );
                ImGui::PopItemWidth();

                static char ch[] = { 'A', '\0' };
                static std::string name = std::string(u8"日志") + ch;
                ImGui::AlignTextToFramePadding();
                ImGui::Text(u8"名称");
                ImGui::SameLine( 0.0f, 1.0f );
                ImGui::InputText( u8"##name", &name );
                static bool vScrollToBottom = true;
                ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 0 ) );
                ImGui::Checkbox( u8"自动滚动到底部", &vScrollToBottom );
                ImGui::PopStyleVar();

                if ( ImGui::Button( u8"确定", ImVec2( 120, 0 ) ) || ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)) )
                {
                    ImGui::CloseCurrentPopup();

                    this->logWinManager->addWindow( name, addr, (USHORT)port, vScrollToBottom );

                    ch[0]++;
                    name = std::string(u8"日志") + ch;
                    port++;
                }
                ImGui::SetItemDefaultFocus();
                ImGui::SameLine();
                if ( ImGui::Button( u8"取消", ImVec2( 120, 0 ) ) )
                {
                    ImGui::CloseCurrentPopup();
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
