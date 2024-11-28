#include "App.h"
#include "MainWindow.h"
#include "LogWindowsManager.h"

using namespace winux;
using namespace eienlog;

// struct LogListenWindow ---------------------------------------------------------------------
LogListenWindow::LogListenWindow( LogWindowsManager * manager, App::ListenParams const & lparams ) : manager(manager), lparams(lparams)
{
    // 创建线程读取LOGs
    this->th.attachNew( new std::thread( [this] () {
        LogReader reader( UnicodeConverter(this->lparams.addr).toUnicode(), this->lparams.port );
        if ( reader.errNo() ) this->show = false;
        while ( this->show )
        {
            LogRecord record;
            if ( reader.readRecord( &record, this->lparams.waitTimeout, this->lparams.updateTimeout ) )
            {
                std::lock_guard<std::mutex> lk(this->mtx);

                LogTextRecord tr;
                tr.flag.value = record.flag;
                tr.utcTime = DateTimeL( DateTimeL::MilliSec(record.utcTime) ).toString<char>();
                tr.contentSize = record.data.getSize();

                // 如果非二进制，才进行编码转换
                if ( !tr.flag.binary )
                {
                    // 根据编码进行转换
                    switch ( tr.flag.logEncoding )
                    {
                    case eienlog::leUtf8:
                        {
                            tr.strContent.assign( record.data.toString<char>() );
                        }
                        break;
                    case eienlog::leUtf16Le:
                        {
                            winux::Utf16String ustr = record.data.toString<winux::char16>();
                            if ( winux::IsBigEndian() )
                            {
                                if ( ustr.length() > 0 ) winux::InvertByteOrderArray( &ustr[0], ustr.length() );
                            }
                            tr.strContent.assign( winux::UnicodeConverter(ustr).toUtf8() );
                        }
                        break;
                    case eienlog::leUtf16Be:
                        {
                            winux::Utf16String ustr = record.data.toString<winux::char16>();
                            if ( winux::IsLittleEndian() )
                            {
                                if ( ustr.length() > 0 ) winux::InvertByteOrderArray( &ustr[0], ustr.length() );
                            }
                            tr.strContent.assign( winux::UnicodeConverter(ustr).toUtf8() );
                        }
                        break;
                    default:
                        {
                            tr.strContent.assign( winux::LocalToUtf8( record.data.toString<char>() ) );
                        }
                        break;
                    }
                }
                else // 二进制数据
                {
                    int i = 1;
                    for ( auto && byt : record.data )
                    {
                        tr.strContent += winux::BufferToHex<char>( winux::Buffer( &byt, 1, true ) );
                        if ( i % 16 )
                        {
                            tr.strContent += " ";
                        }
                        else
                        {
                            tr.strContent += "\n";
                        }
                        i++;
                    }
                    winux::StrMakeUpper(&tr.strContent);
                }

                tr.strContentSlashes = winux::AddCSlashes(tr.strContent);
                this->logs.push_back(tr);
            }
        }
    } ) );
}

LogListenWindow::~LogListenWindow()
{
    this->show = false;
    this->th->join();
}

void LogListenWindow::render()
{
    ImGui::Begin( this->lparams.name.c_str(), &this->show );
    ImGui::SetWindowDock( ImGui::GetCurrentWindow(), manager->mainWindow->dockSpaceId, ImGuiCond_Once );

    ImGui::Text( u8"正在读取<%s#%u>的日志...", this->lparams.addr.c_str(), this->lparams.port );
    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 0 ) );
    ImGui::SameLine();
    if ( ImGui::Checkbox( u8"自动滚动到底部", &this->lparams.vScrollToBottom ) )
    {
        bToggleVScrollToBottom = true;
    }
    ImGui::SameLine();
    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 6.0f, 0 ) );
    if ( ImGui::Button(u8"清空列表") )
    {
        std::lock_guard<std::mutex> lk(this->mtx);
        this->logs.clear();
        this->selected = -1;
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();

    int columns = 4;
    bool logTableColumnResize = this->manager->mainWindow->app.appConfig.logTableColumnResize;
    static ImGuiTableFlags flags = ImGuiTableFlags_Hideable | ImGuiTableFlags_Reorderable | ( logTableColumnResize ? ImGuiTableFlags_Resizable : 0 ) |
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ContextMenuInBody;

    ImGui::PushStyleVar( ImGuiStyleVar_CellPadding, ImVec2(6.0f, 2.0f) );
    if (ImGui::BeginTable("table_logs", columns, flags))
    {
        ImGui::TableSetupScrollFreeze(0, 1); // 固定第一行（表格头）
        ImGui::TableSetupColumn(u8"编号", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(u8"时间", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(u8"长度", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(u8"日志内容", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableHeadersRow();

        {
            std::lock_guard<std::mutex> lk(this->mtx);

            ImGuiListClipper clipper;
            clipper.Begin( (int)this->logs.size() );
            while ( clipper.Step() )
            {
                for ( int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++ )
                {
                    auto & log = this->logs[row];

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    char szNo[20] = { 0 };
                    sprintf( szNo, "%u", row + 1 );
                    if ( ImGui::Selectable( szNo, this->selected == row, ImGuiSelectableFlags_SpanAllColumns ) ) this->selected = row;
                    // 弹出详细显示框
                    ImGui::PushID(row * columns);
                    if (ImGui::BeginPopupContextItem())
                    {
                        this->selected = row;

                        bool copyToClipboard = ImGui::Button(u8"复制日志");
                        ImGui::SameLine();
                        ImGui::Text(u8"编号：%s，大小：%u", szNo, log.contentSize );
                        ImGui::Separator();
                        if (copyToClipboard)
                        {
                            ImGui::LogToClipboard();
                            //ImGui::LogText("```\n"); // Back quotes will make text appears without formatting when pasting on GitHub
                        }

                        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50.0f);
                        if (log.flag.fgColorUse)
                        {
                            ImVec4 color;
                            _GetImVec4ColorFromColorR5G5B5(log.flag.fgColor, &color);
                            ImGui::PushStyleColor(ImGuiCol_Text, color);
                        }
                        ImGui::TextUnformatted(log.strContent.c_str(), log.strContent.c_str() + log.strContent.length());
                        if (log.flag.fgColorUse)
                        {
                            ImGui::PopStyleColor();
                        }
                        ImGui::PopTextWrapPos();

                        if (copyToClipboard)
                        {
                            //ImGui::LogText("\n```\n");
                            ImGui::LogFinish();
                        }

                        ImGui::Separator();
                        if (ImGui::Button(u8"关闭浮窗"))
                            ImGui::CloseCurrentPopup();
                        ImGui::SameLine();
                        ImGui::Text(u8"时间：%s", log.utcTime.c_str());
                        ImGui::EndPopup();
                    }
                    ImGui::PopID();


                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextEx(log.utcTime.c_str(), log.utcTime.c_str() + log.utcTime.length());

                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text( "%u", (winux::uint)log.contentSize );

                    ImGui::TableSetColumnIndex(3);
                    if ( log.flag.bgColorUse )
                    {
                        ImVec4 color;
                        _GetImVec4ColorFromColorR4G4B4( log.flag.bgColor, &color );
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(color) );
                    }

                    if ( log.flag.fgColorUse )
                    {
                        ImVec4 color;
                        _GetImVec4ColorFromColorR5G5B5( log.flag.fgColor, &color );

                        ImGui::PushStyleColor(ImGuiCol_Text, color);
                        ImGui::TextUnformatted( log.strContentSlashes.c_str(), log.strContentSlashes.c_str() + log.strContentSlashes.length() );
                        ImGui::PopStyleColor();
                    }
                    else
                    {
                        ImGui::TextUnformatted( log.strContentSlashes.c_str(), log.strContentSlashes.c_str() + log.strContentSlashes.length() );
                    }
                }
            }
        }

        // 自动滚动到底部的操作代码
        float a = ImGui::GetScrollY();
        float b = ImGui::GetScrollMaxY();
        if ( this->bToggleVScrollToBottom )
        {
            if ( this->lparams.vScrollToBottom )
            {
                a = b;
            }
            else
            {
                a = ( a > 1.0f ? a - 1.0f : 0.0f );
                ImGui::SetScrollY(a);
            }
            this->bToggleVScrollToBottom = false;
        }

        if ( b > 0.0f )
        {
            float f = a / b;
            if ( 1.0f - f < 1.0e-6f ) // f == 1.0f
            {
                this->lparams.vScrollToBottom = true;
            }
            else
            {
                this->lparams.vScrollToBottom = false;
            }
        }

        if ( this->lparams.vScrollToBottom ) ImGui::SetScrollHereY(1.0f);

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    ImGui::End();
}

// struct LogWindowsManager -------------------------------------------------------------------
LogWindowsManager::LogWindowsManager( MainWindow * mainWindow ) : mainWindow(mainWindow)
{

}

void LogWindowsManager::addWindow( App::ListenParams const & lparams )
{
    auto p = winux::MakeSimple( new LogListenWindow( this, lparams ) );
    this->wins.emplace_back(p);

    this->mainWindow->app.setRecentListen(lparams);
}

void LogWindowsManager::render()
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
