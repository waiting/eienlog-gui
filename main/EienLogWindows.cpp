#include "App.h"
#include "EienLogWindows.h"

// struct EienLogWindow -----------------------------------------------------------------------
EienLogWindow::EienLogWindow( MainWindow * mainWindow ) : mainWindow(mainWindow)
{

}

void EienLogWindow::render()
{
    ImGui::Begin( this->name.c_str(), &this->show );
    ImGui::SetWindowDock( ImGui::GetCurrentWindow(), mainWindow->dockspace_id, ImGuiCond_Once );
    ImGui::Text( u8"监听端口%u", this->port );

    // [Method 2] Using TableNextColumn() called multiple times, instead of using a for loop + TableSetColumnIndex().
    // This is generally more convenient when you have code manually submitting the contents of each column.
    static ImGuiTableFlags flags = // ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY;
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

    ImGui::PushStyleVar( ImGuiStyleVar_CellPadding, ImVec2(6.0f, 3.0f) );
    if (ImGui::BeginTable("table_logs", 3, flags))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(u8"编号", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(u8"时间", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(u8"日志内容", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        static auto dts = winux::DateTimeL().fromCurrent().toString<char>();

        ImGuiListClipper clipper;
        clipper.Begin(20000);
        static int selected = -1;
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                char sz[32] = { 0 };
                sprintf( sz, "No. %d", row );
                if ( ImGui::Selectable( sz, selected == row, ImGuiSelectableFlags_SpanAllColumns ) )
                    selected = row;
                //ImGui::Text("No. %d", row);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text( dts.c_str() );
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%d, %d, %d", clipper.DisplayStart, clipper.DisplayEnd, selected);
            }
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    ImGui::End();
}

// struct EienLogWindows ----------------------------------------------------------------------
EienLogWindows::EienLogWindows( MainWindow * mainWindow ) : mainWindow(mainWindow)
{

}

void EienLogWindows::addWindow( std::string const & name, USHORT port )
{
    auto p = winux::MakeSimple( new EienLogWindow(mainWindow) );
    p->name = name;
    p->port = port;
    this->wins.emplace_back(p);
}

void EienLogWindows::render()
{
    for ( auto it = this->wins.begin(); it != this->wins.end(); )
    {
        if ( (*it)->show )
        {
            (*it)->render();
            it++;
        }
        else
        {
            it = this->wins.erase(it);
        }
    }
}
