﻿#include "App.h"
#include "MainWindow.h"
#include "LogListenWindow.h"

// struct LogListenWindow ---------------------------------------------------------------------
LogListenWindow::LogListenWindow( LogWindowsManager * manager, App::ListenParams const & lparams ) :
    LogViewerWindow( manager, lparams.name, lparams.vScrollToBottom ), lparams(lparams)
{
    // 创建线程读取LOGs
    this->th.attachNew( new std::thread( [this] () {
        eienlog::LogReader reader( winux::UnicodeConverter(this->lparams.addr).toUnicode(), this->lparams.port );
        if ( reader.errNo() ) this->show = false;
        while ( this->show )
        {
            eienlog::LogRecord record;
            if ( reader.readRecord( &record, this->lparams.waitTimeout, this->lparams.updateTimeout ) )
            {
                std::lock_guard<std::mutex> lk(this->mtx);

                LogTextRecord tr;
                tr.flag.value = record.flag;
                tr.utcTime = winux::DateTimeL( winux::DateTimeL::MilliSec(record.utcTime) ).toString<char>();
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

void LogListenWindow::renderComponents()
{
    ImGui::Text( u8"正在监听<%s#%u>的日志...", this->lparams.addr.c_str(), this->lparams.port );
    ImGui::SameLine();

    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 6.0f, 0 ) );
    if ( ImGui::Button(u8"保存文件") )
    {
        std::lock_guard<std::mutex> lk(this->mtx);

        winplus::FileDialog dlg{ this->manager->mainWindow->app.wi.hWnd, FALSE, L"保存日志文件", L"csvlog" };
        if ( dlg.doModal( L".", L"日志文件(*.csvlog)\0*.csvlog\0全部文件(*.*)\0*.*\0\0" ) )
        {
            winux::String path = dlg.getFilePath();
            winux::MemoryFile memFile;
            winux::CsvWriter csv(&memFile);
            switch ( this->saveTargetType )
            {
            case 0:
            case 2:
                for ( int i = 0; i < (int)this->logs.size(); i++ )
                {
                    if ( this->saveTargetType == 2 && winux::isset( this->selected, i ) && this->selected[i] ) continue;
                    auto && log = this->logs[i];
                    winux::Mixed record;
                    record.createArray();
                    record.add(log.contentSize);
                    record.add( $L(log.strContent) );
                    record.add( $L(log.utcTime) );
                    record.add(log.flag.value);
                    csv.writeRecord(record);
                }
                break;
            case 1:
                for ( auto && pr : this->selected )
                {
                    if ( pr.second )
                    {
                        auto && log = this->logs[pr.first];
                        winux::Mixed record;
                        record.createArray();
                        record.add(log.contentSize);
                        record.add( $L(log.strContent) );
                        record.add( $L(log.utcTime) );
                        record.add(log.flag.value);
                        csv.writeRecord(record);
                    }
                }
                break;
            }

            winux::TextArchive ta;
            ta.saveEx( memFile.buffer(), winux::IsLittleEndian() ? "UTF-16LE" : "UTF-16BE", path, winux::feUtf8Bom );
        }
    }
    ImGui::SameLine();

    static char const * saveTargetTypes[] = { u8"全部", u8"已选择", u8"非选择" };
    ImGui::PushItemWidth( ImGui::GetFontSize() * 4 + 2 * 6.0f );
    if ( ImGui::Combo(u8"##save_target", &this->saveTargetType, saveTargetTypes, countof(saveTargetTypes) ) )
    {
        //printf("%d\n", this->saveTarget);
    }
    ImGui::PopItemWidth();

    ImGui::PopStyleVar();
    ImGui::SameLine();

    LogViewerWindow::renderComponents();
}
