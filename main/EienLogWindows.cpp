#include "App.h"
#include "EienLogWindows.h"

using namespace winux;
using namespace eienlog;

// struct EienLogWindow -----------------------------------------------------------------------
EienLogWindow::EienLogWindow( EienLogWindows * manager, std::string const & name, std::string const & addr, USHORT port, time_t waitTimeout, time_t updateTimeout, bool vScrollToBottom ) :
    manager(manager), name(name), addr(addr), port(port), waitTimeout(waitTimeout), updateTimeout(updateTimeout), vScrollToBottom(vScrollToBottom)
{
    // 创建线程读取LOGs
    this->th.attachNew( new std::thread( [this] () {
        LogReader reader( UnicodeConverter(this->addr).toUnicode(), this->port );
        if ( reader.errNo() ) this->show = false;
        while ( this->show )
        {
            LogRecord record;
            if ( reader.readRecord( &record, this->waitTimeout, this->updateTimeout ) )
            {
                std::lock_guard<std::mutex> lk(this->mtx);

                LogTextRecord tr;
                tr.flag.value = record.flag;
                tr.utcTime = DateTimeL( DateTimeL::MilliSec(record.utcTime) ).toString<char>();
                tr.content = std::move(record.data);

                // 根据编码进行转换
                switch ( tr.flag.logEncoding )
                {
                case eienlog::leUtf8:
                    {
                        tr.strContent.assign( tr.content.toString<char>() );
                    }
                    break;
                case eienlog::leUtf16Le:
                    {
                        winux::Utf16String ustr = tr.content.toString<winux::char16>();
                        if ( winux::IsBigEndian() )
                        {
                            if ( ustr.length() > 0 ) winux::InvertByteOrderArray( &ustr[0], ustr.length() );
                        }
                        tr.strContent.assign( winux::UnicodeConverter(ustr).toUtf8() );
                    }
                    break;
                case eienlog::leUtf16Be:
                    {
                        winux::Utf16String ustr = tr.content.toString<winux::char16>();
                        if ( winux::IsLittleEndian() )
                        {
                            if ( ustr.length() > 0 ) winux::InvertByteOrderArray( &ustr[0], ustr.length() );
                        }
                        tr.strContent.assign( winux::UnicodeConverter(ustr).toUtf8() );
                    }
                    break;
                default:
                    {
                        if ( tr.flag.binary )
                            tr.strContent.assign( "<" + winux::BufferToHex<char>(tr.content) + ">" );
                        else
                            tr.strContent.assign( winux::LocalToUtf8( tr.content.toString<char>() ) );
                    }
                    break;
                }

                this->logs.push_back(tr);
            }
        }
    } ) );
}

EienLogWindow::~EienLogWindow()
{
    this->show = false;
    this->th->join();
}

// Foreground color: R5G5B5
inline static void _GetImVec4ColorFromLogFgColor( winux::uint16 fgColor, ImVec4 * color )
{
    float r = ( fgColor & 31 ) / 31.0f, g = ( ( fgColor >> 5 ) & 31 ) / 31.0f, b = ( ( fgColor >> 10 ) & 31 ) / 31.0f;
    color->x = r;
    color->y = g;
    color->z = b;
    color->w = 1.0f;
}

// Background color: R4G4B4
inline static void _GetImVec4ColorFromLogBgColor( winux::uint16 bgColor, ImVec4 * color )
{
    float r = ( bgColor & 15 ) / 15.0f, g = ( ( bgColor >> 4 ) & 15 ) / 15.0f, b = ( ( bgColor >> 8 ) & 15 ) / 15.0f;
    color->x = r;
    color->y = g;
    color->z = b;
    color->w = 1.0f;
}

void EienLogWindow::render()
{
    ImGui::Begin( this->name.c_str(), &this->show );
    ImGui::SetWindowDock( ImGui::GetCurrentWindow(), manager->mainWindow->dockSpaceId, ImGuiCond_Once );

    ImGui::Text( u8"正在读取<%s#%u>的日志...", this->addr.c_str(), this->port );
    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 0 ) );
    ImGui::SameLine();
    if ( ImGui::Checkbox( u8"自动滚动到底部", &this->vScrollToBottom ) )
    {
        bToggleVScrollToBottom = true;
    }
    ImGui::SameLine();
    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 6.0f, 0 ) );
    if ( ImGui::Button( u8"清空列表" ) )
    {
        std::lock_guard<std::mutex> lk(this->mtx);
        this->logs.clear();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();

    // [Method 2] Using TableNextColumn() called multiple times, instead of using a for loop + TableSetColumnIndex().
    // This is generally more convenient when you have code manually submitting the contents of each column.
    static ImGuiTableFlags flags = // ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY;
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

    ImGui::PushStyleVar( ImGuiStyleVar_CellPadding, ImVec2(6.0f, 2.0f) );
    if (ImGui::BeginTable("table_logs", 4, flags))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(u8"编号", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(u8"时间", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(u8"长度", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(u8"日志内容", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        //static auto dts = winux::DateTimeL().fromCurrent().toString<char>();

        {
            std::lock_guard<std::mutex> lk(this->mtx);

            ImGuiListClipper clipper;
            clipper.Begin( (int)this->logs.size() );
            while ( clipper.Step() )
            {
                for ( int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++ )
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    char sz[20] = { 0 };
                    sprintf( sz, "%u", row + 1 );
                    if ( ImGui::Selectable( sz, this->selected == row, ImGuiSelectableFlags_SpanAllColumns ) ) this->selected = row;

                    auto & log = this->logs[row];
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text( log.utcTime.c_str() );

                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text( "%u", (winux::uint)log.content.getSize() );

                    ImGui::TableSetColumnIndex(3);
                    if ( log.flag.bgColorUse )
                    {
                        ImVec4 color;
                        _GetImVec4ColorFromLogBgColor( log.flag.bgColor, &color );
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(color) );
                    }

                    if ( log.flag.fgColorUse )
                    {
                        ImVec4 color;
                        _GetImVec4ColorFromLogFgColor( log.flag.fgColor, &color );
                        ImGui::TextColored( color, log.strContent.c_str() );
                    }
                    else
                    {
                        ImGui::Text( log.strContent.c_str() );
                    }
                }
            }
        }

        // 自动滚动到底部的操作代码
        float a = ImGui::GetScrollY();
        float b = ImGui::GetScrollMaxY();
        //printf("%f/%f\n", a, b);
        if ( bToggleVScrollToBottom )
        {
            if ( vScrollToBottom )
            {
                a = b;
            }
            else
            {
                a = ( a > 1.0f ? a - 1.0f : 0.0f );
                ImGui::SetScrollY(a);
            }
            bToggleVScrollToBottom = false;
        }

        if ( b > 0.0f )
        {
            float f = a / b;
            if ( 1.0f - f < 1.0e-6f ) // f == 1.0f
            {
                vScrollToBottom = true;
            }
            else
            {
                vScrollToBottom = false;
            }
        }

        if ( vScrollToBottom ) ImGui::SetScrollHereY(1.0f);

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    ImGui::End();
}

// struct EienLogWindows ----------------------------------------------------------------------
EienLogWindows::EienLogWindows( MainWindow * mainWindow ) : mainWindow(mainWindow)
{

}

void EienLogWindows::addWindow( std::string const & name, std::string const & addr, USHORT port, time_t waitTimeout, time_t updateTimeout, bool vScrollToBottom )
{
    auto p = winux::MakeSimple( new EienLogWindow( this, name, addr, port, waitTimeout, updateTimeout, vScrollToBottom ) );
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
