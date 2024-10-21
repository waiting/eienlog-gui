#pragma once
#include <vector>

struct MainWindow;
struct EienLogWindow
{
    EienLogWindow( MainWindow * mainWindow );
    void render();

    USHORT port;
    std::string name;
    MainWindow * mainWindow;
    bool show = true;
};

struct EienLogWindows
{
    EienLogWindows( MainWindow * mainWindow );

    void addWindow( std::string const & name, USHORT port );
    void render();

    std::vector< winux::SimplePointer<EienLogWindow> > wins;
    MainWindow * mainWindow;
};

