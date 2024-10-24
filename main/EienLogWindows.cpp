#include "App.h"
#include "EienLogWindows.h"
//#include <lock>

// struct EienLogWindow -----------------------------------------------------------------------
EienLogWindow::EienLogWindow( MainWindow * mainWindow, std::string const & name, std::string const & addr, USHORT port, bool vScrollToBottom ) :
    mainWindow(mainWindow), name(name), addr(addr), port(port), vScrollToBottom(vScrollToBottom)
{
    // 创建线程读取LOGs
    this->th.attachNew( new std::thread( [this] () {
        while ( this->show )
        {
            {
                std::lock_guard<std::mutex> lk(this->mtx);
            }
            //Beep( 355, 666 );
            Sleep(1000);
        }
    } ) );

    LogTextRecord tr;
    tr.text = u8"第一条日志";
    tr.utcTime = winux::DateTimeL().fromCurrent().toString<char>();
    tr.flag.value = 0;
    this->logs.push_back(tr);
    tr.text = u8"第二条日志...";
    tr.utcTime = winux::DateTimeL().fromCurrent().toString<char>();
    tr.flag.value = 0;
    this->logs.push_back(tr);
}

EienLogWindow::~EienLogWindow()
{
    this->show = false;
    this->th->join();
}

void EienLogWindow::render()
{
    ImGui::Begin( this->name.c_str(), &this->show );
    ImGui::SetWindowDock( ImGui::GetCurrentWindow(), mainWindow->dockspace_id, ImGuiCond_Once );

    ImGui::Text( u8"正在读取<%s:%u>的日志...", this->addr.c_str(), this->port );
    ImGui::SameLine();
    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 0 ) );
    ImGui::Checkbox( u8"自动滚动到底部", &this->vScrollToBottom );
    ImGui::PopStyleVar();

    // [Method 2] Using TableNextColumn() called multiple times, instead of using a for loop + TableSetColumnIndex().
    // This is generally more convenient when you have code manually submitting the contents of each column.
    static ImGuiTableFlags flags = // ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY;
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

    ImGui::PushStyleVar( ImGuiStyleVar_CellPadding, ImVec2(6.0f, 2.0f) );
    if (ImGui::BeginTable("table_logs", 3, flags))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(u8"编号", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(u8"时间", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(u8"日志内容", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        //static auto dts = winux::DateTimeL().fromCurrent().toString<char>();

        {
            std::lock_guard<std::mutex> lk(this->mtx);

            ImGuiListClipper clipper;
            clipper.Begin( this->logs.size() );
            while ( clipper.Step() )
            {
                for ( int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++ )
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    char sz[32] = { 0 };
                    sprintf( sz, "No.%d", row );
                    if ( ImGui::Selectable( sz, this->selected == row, ImGuiSelectableFlags_SpanAllColumns ) ) this->selected = row;
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text( this->logs[row].utcTime.c_str() );
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text( this->logs[row].text.c_str() );
                    //ImGui::Text( "%d, %d, %d", clipper.DisplayStart, clipper.DisplayEnd, selected );
                }
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

void EienLogWindows::addWindow( std::string const & name, std::string const & addr, USHORT port, bool vScrollToBottom )
{
    auto p = winux::MakeSimple( new EienLogWindow( mainWindow, name, addr, port, vScrollToBottom ) );
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
            //it++;
        }
    }
}
