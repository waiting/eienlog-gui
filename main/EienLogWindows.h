#pragma once
#include <vector>
#include <thread>
#include <mutex>

struct MainWindow;
struct EienLogWindow
{
    EienLogWindow( MainWindow * mainWindow, std::string const & name, std::string const & addr, USHORT port );
    ~EienLogWindow();
    void render();

    MainWindow * mainWindow;
    std::string addr;
    USHORT port;
    std::string name;
    std::vector<eienlog::LogRecord> logs;
    std::mutex mtx;
    winux::SimplePointer<std::thread> th;
    bool show = true;
};

struct EienLogWindows
{
    EienLogWindows( MainWindow * mainWindow );

    void addWindow( std::string const & name, std::string const & addr, USHORT port );
    void render();

    std::vector< winux::SimplePointer<EienLogWindow> > wins;
    MainWindow * mainWindow;
};

