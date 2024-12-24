#include "App.h"
#include "LogViewerWindow.h"
#include "MainWindow.h"
#include "LogWindowsManager.h"

LogViewerWindow::LogViewerWindow( LogWindowsManager * manager, winux::Utf8String const & name, bool vScrollToBottom, winux::Utf8String const & logFile ) :
    manager(manager), name(name), vScrollToBottom(vScrollToBottom), logFile(logFile)
{
    if ( !this->logFile.empty() )
    {
        winux::String logFilePath = $L(this->logFile);
        winux::CsvReader csv{ winux::FileGetString( logFilePath, winux::feUnspec ) };
        for ( size_t i = 0; i < csv.getCount(); i++ )
        {
            auto && row = csv[(int)i];
            LogTextRecord tr;
            size_t columns = row.getCount();
            if ( columns > 0 )
            {
                tr.contentSize = row[0];
            }
            if ( columns > 1 )
            {
                tr.strContent = $u8(row[1].refUnicode());
                tr.strContentSlashes = winux::AddCSlashes(tr.strContent);
            }
            if ( columns > 2 )
            {
                tr.utcTime = $u8(row[2].refUnicode());
            }
            if ( columns > 3 )
            {
                tr.flag.value = row[3];
            }
            this->logs.push_back( std::move(tr) );
        }
    }
}

LogViewerWindow::~LogViewerWindow()
{
}

void LogViewerWindow::render()
{
    ImGui::Begin( this->name.c_str(), &this->show );
    ImGui::SetWindowDock( ImGui::GetCurrentWindow(), manager->mainWindow->dockSpaceId, ImGuiCond_Once );

    this->renderComponents();

    ImGui::End();
}

void LogViewerWindow::renderComponents()
{
    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 6.0f, 0 ) );
    if ( ImGui::Button(u8"取消选择") )
    {
        this->selected.clear();
        this->clickRowPrev = -1;
    }
    ImGui::SameLine();
    if ( ImGui::Button(u8"清空列表") )
    {
        std::lock_guard<std::mutex> lk(this->mtx);
        this->logs.clear();
        this->selected.clear();
        this->clickRowPrev = -1;
    }
    ImGui::PopStyleVar();

    ImGui::SameLine();

    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 0 ) );
    if ( ImGui::Checkbox( u8"自动滚动到底部", &this->vScrollToBottom ) )
    {
        bToggleVScrollToBottom = true;
    }
    ImGui::PopStyleVar();

    ImGui::SameLine();

    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 6.0f, 0 ) );
    if ( ImGui::Button(u8"到顶") )
    {
        bToggleToTop = true;
    }
    ImGui::PopStyleVar();

    int columns = 4; // 列数
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
                    sprintf( szNo, u8"%u", row + 1 );
                    bool isCtrlDown = ImGui::IsKeyDown( ImGui::GetKeyIndex(ImGuiKey_LeftCtrl) ) || ImGui::IsKeyDown( ImGui::GetKeyIndex(ImGuiKey_RightCtrl) );
                    bool isShiftDown = ImGui::IsKeyDown( ImGui::GetKeyIndex(ImGuiKey_LeftShift) ) || ImGui::IsKeyDown( ImGui::GetKeyIndex(ImGuiKey_RightShift) );
                    if ( ImGui::Selectable( szNo, this->selected.find(row) != this->selected.end() && this->selected[row], ImGuiSelectableFlags_SpanAllColumns ) )
                    {
                        if ( isCtrlDown )
                        {
                            this->selected[row] = !this->selected[row];
                        }
                        else
                        {
                            this->selected.clear();
                            this->selected[row] = true;
                        }

                        if ( isShiftDown )
                        {
                            if ( this->clickRowPrev != -1 )
                            {
                                int a = clickRowPrev, b = row;
                                for ( int i = a; ( ( clickRowPrev < row ) ? i <= b : i >= b ); ( ( clickRowPrev < row ) ? i++ : i-- ) )
                                {
                                    this->selected[i] = true;
                                }
                            }
                        }

                        this->clickRowPrev = row;
                    }
                    // 弹出详细显示框
                    ImGui::PushID(row * columns);
                    if ( ImGui::BeginPopupContextItem() )
                    {
                        if ( !isCtrlDown )
                        {
                            this->selected.clear();
                        }
                        this->selected[row] = true;
                        this->clickRowPrev = row;

                        bool copyToClipboard = ImGui::Button(u8"复制");
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
                        if (ImGui::Button(u8"关闭"))
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
            if ( this->vScrollToBottom )
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
                this->vScrollToBottom = true;
            }
            else
            {
                this->vScrollToBottom = false;
            }
        }

        if ( this->vScrollToBottom && !this->bToggleToTop )
        {
            ImGui::SetScrollHereY(1.0f);
        }
        else if ( this->bToggleToTop )
        {
            ImGui::SetScrollY(0.0f);
            bToggleToTop = false;
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}
