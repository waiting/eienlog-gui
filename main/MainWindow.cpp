#include "App.h"
#include "MainWindow.h"

MainWindow::MainWindow( App & app ) : app(app)
{
    this->logWindows.attachNew( new EienLogWindows(this) );
}

void MainWindow::render()
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
        if ( ImGui::Button( "OK" ) ) MessageBoxW( app.wi.hWnd, L"msgbox", L"", 0 );
        ImGui::Separator();
        ImGui::Checkbox( "Demo Window", &show_demo_window );      // Edit bools storing our window open/close state
        ImGui::Checkbox( "Another Window", &show_another_window );

        static float f = 0.0f;
        ImGui::SliderFloat( "float", &f, 0.0f, 1.0f );            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3( "clear color", (float*)&app.clear_color ); // Edit 3 floats representing a color

        static int counter = 0;
        if ( ImGui::Button( "Button" ) )                            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text( "counter = %d", counter );

        ImGui::Text( "Application average %.3f ms/frame (%.1f FPS)", 1000.0f / app.ctx->IO.Framerate, app.ctx->IO.Framerate );
        static bool vsync = true;
        if ( ImGui::Checkbox( u8"垂直同步", &vsync ) )
        {
            app.gi.d3dpp.PresentationInterval = vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
            app.gi.forceReset = true;
        }
        ImGui::End();
    }

    this->logWindows->render();

    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);

    if (show_another_window)
    {
        ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        ImGui::Text("Hello from another window!");
        if (ImGui::Button("Close Me"))
            show_another_window = false;
        ImGui::End();
    }
}

void MainWindow::renderDockSpace()
{
    // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
    // because it would be confusing to have two docking targets within each others.
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground;
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
    ImGuiIO& io = this->app.ctx->IO;

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

void MainWindow::renderDockSpaceMenuBar()
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
                ImGui::AlignTextToFramePadding();
                ImGui::Text(u8"区分名称");
                ImGui::SameLine( 0.0f, 1.0f );
                ImGui::InputText( u8"##name", &name );
                //static bool dont_ask_me_next_time = false;
                ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 0 ) );
                //ImGui::Checkbox( "Don't ask me next time", &dont_ask_me_next_time );
                ImGui::PopStyleVar();
                //ImGui::Checkbox( "Don't ask me next time.", &dont_ask_me_next_time );

                if ( ImGui::Button( "OK", ImVec2( 120, 0 ) ) || ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)) ) {
                    ImGui::CloseCurrentPopup();

                    this->logWindows->addWindow( name, (USHORT)port );
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
