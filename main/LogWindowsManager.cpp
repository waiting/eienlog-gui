#include "App.h"
#include "MainWindow.h"
#include "LogListenWindow.h"
#include "LogWindowsManager.h"

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

void LogWindowsManager::addWindow( winux::Utf8String const & name, bool vScrollToBottom, winux::Utf8String const & logFile )
{
    auto p = winux::MakeSimple( new LogViewerWindow( this, name, vScrollToBottom, logFile ) );
    this->wins.emplace_back(p);

    this->mainWindow->app.setRecentOpen(logFile);
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
        }
    }
}
