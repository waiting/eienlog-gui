#pragma once

struct MainWindow;
struct LogViewerWindow;

struct LogWindowsManager
{
    LogWindowsManager( MainWindow * mainWindow );

    void addWindow( App::ListenParams const & lparams );
    void addWindow( winux::Utf8String const & name, bool vScrollToBottom, winux::Utf8String const & logFile );
    void render();

    std::vector< winux::SimplePointer<LogViewerWindow> > wins;
    MainWindow * mainWindow;
};
