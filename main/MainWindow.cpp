#include "App.h"
#include "MainWindow.h"

MainWindow::MainWindow( App & app ) : app(app)
{
    D3DXIMAGE_INFO imgInfo = { 0 };
    HRESULT hr = S_OK;// = D3DXCreateTextureFromFile( this->gi.pd3dDevice, LR"(J:\Pictures\image0001.jpg)", &this->pTexture );
    //hr = D3DXCreateTextureFromFileEx( this->gi.pd3dDevice, LR"(J:\Pictures\image0001-1.png)", D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, 0, &imgInfo, NULL, &pTexture);
    if (SUCCEEDED(hr))
    {
        this->imgSize.x = (float)imgInfo.Width;
        this->imgSize.y = (float)imgInfo.Height;
    }
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
        ImGui::Begin( u8"��ã�EienLogGui��", &eiengui_win );// Create a window called "Hello, world!" and append into it.
        ImGui::SetWindowDock( ImGui::GetCurrentWindow(), dockspace_id, ImGuiCond_Once );
        ImGui::Text( u8"����һЩ���õ��ı�������������ʾ��" );               // Display some text (you can use a format strings too)
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
        if ( ImGui::Checkbox( u8"��ֱͬ��", &vsync ) )
        {
            app.gi.d3dpp.PresentationInterval = vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
            app.gi.forceReset = true;
        }
        ImGui::End();
    }

    for ( auto && logCtx : this->logCtxs )
    {
        if ( logCtx.isShow )
        {
            ImGui::Begin( logCtx.name.c_str(), &logCtx.isShow );
            ImGui::SetWindowDock( ImGui::GetCurrentWindow(), dockspace_id, ImGuiCond_Once );
            ImGui::Text( u8"�����˿�%u", logCtx.port );
            if ( this->pTexture ) ImGui::Image( (ImTextureID)this->pTexture, this->imgSize );
            ImGui::End();
        }
    }

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
        if ( ImGui::BeginMenu(u8"�ļ�") )
        {
            if ( ImGui::MenuItem(u8"�½�...") )
            {
                //open_modal_window = true;
                toggle_popup = true;
            }
            ImGui::Separator();
            if ( ImGui::MenuItem(u8"�˳�") )
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
                ImGui::Text( u8"�½�һ���������ڣ�������־��Ϣ" );
                ImGui::Separator();
                static int port = 23456;
                // �����ǩ�ı�
                ImGui::AlignTextToFramePadding();
                ImGui::Text(u8"�����˿�"); // �⽫�����ı�ǩ
                ImGui::SameLine( 0.0f, 1.0f );
                // ���������Ŀ��
                //ImGui::PushItemWidth(-1);
                ImGui::InputInt( u8"##listen_port", &port, 1, 65535 );

                static char ch[] = {'A', 0 };
                static std::string name = std::string(u8"��־") + ch;
                ImGui::InputText( u8"��������", &name );
                //static bool dont_ask_me_next_time = false;
                ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 0 ) );
                //ImGui::Checkbox( "Don't ask me next time", &dont_ask_me_next_time );
                ImGui::PopStyleVar();
                //ImGui::Checkbox( "Don't ask me next time.", &dont_ask_me_next_time );

                if ( ImGui::Button( "OK", ImVec2( 120, 0 ) ) || ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)) ) {
                    ImGui::CloseCurrentPopup();

                    this->logCtxs.push_back( { (USHORT)port, name } );
                    ch[0]++;
                    name = std::string(u8"��־") + ch;
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
        if ( ImGui::BeginMenu(u8"����") )
        {
            static int colorsTheme = 2;
            if ( ImGui::MenuItem( u8"���ڣ�Dark��", nullptr, colorsTheme == 0 ) )
            {
                colorsTheme = 0;
                ImGui::StyleColorsDark();
            }
            if ( ImGui::MenuItem( u8"������Light��", nullptr, colorsTheme == 1 ) )
            {
                colorsTheme = 1;
                ImGui::StyleColorsLight();
            }
            if ( ImGui::MenuItem( u8"���䣨Classic��", nullptr, colorsTheme == 2 ) )
            {
                colorsTheme = 2;
                ImGui::StyleColorsClassic();
            }
            ImGui::EndMenu();
        }
        if ( ImGui::BeginMenu(u8"������") )
        {
            // Disabling fullscreen would allow the window to be moved to the front of other windows,
            // which we can't undo at the moment without finer window depth/z control.
            ImGui::MenuItem( u8"ȫ����ʾ", nullptr, &opt_fullscreen );
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
            u8"����ͣ�����ܺ���ʼ�տ��Խ����������ͣ������һ�������У�" "\n"
            u8"- �Ӵ��ڱ����������ǩ�϶���ͣ��/��ͣ����" "\n"
            u8"- �Ӵ��ڲ˵���ť�����Ͻǰ�ť���϶��ɽ�������ڵ㣨���д��ڣ���ͣ����" "\n"
            u8"- ��ס SHIFT ���ɽ���ͣ������� io.ConfigDockingWithShift == false��Ĭ��ֵ��" "\n"
            u8"- ��ס SHIFT ������ͣ������� io.ConfigDockingWithShift == true��"
        );

        ImGui::EndMenuBar();
    }
}
