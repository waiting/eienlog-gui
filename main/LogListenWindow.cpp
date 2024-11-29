#include "App.h"
#include "MainWindow.h"
#include "LogListenWindow.h"

// struct LogListenWindow ---------------------------------------------------------------------
LogListenWindow::LogListenWindow( LogWindowsManager * manager, App::ListenParams const & lparams ) :
    LogViewerWindow( manager, lparams.name, lparams.vScrollToBottom, {} ), lparams(lparams)
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

        winplus::FileDialog dlg{ this->manager->mainWindow->app.wi.hWnd, FALSE, $T("保存日志文件"), $T("csvlog") };
        if ( dlg.doModal( $T("."), $T("日志文件(*.csvlog)\0*.csvlog\0全部文件(*.*)\0*.*\0\0") ) )
        {
            winux::String path = dlg.getFilePath();
            winplus::MsgBox(path);

            //winux::CsvWriter();
        }
    }
    ImGui::PopStyleVar();
    ImGui::SameLine();

    LogViewerWindow::renderComponents();
}

