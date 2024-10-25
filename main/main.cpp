#include "App.h"

App g_app;

// Main code
int APIENTRY wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow )
{
    eiennet::SocketLib sockLib;

    if ( !g_app.initInstance( hInstance, nCmdShow ) ) return 1;
    int rc = g_app.run();
    g_app.exitInstance();
    return rc;
}

int wmain()
{
    eiennet::SocketLib sockLib;

    if ( !g_app.initInstance( GetModuleHandle(nullptr), SW_NORMAL ) ) return 1;
    int rc = g_app.run();
    g_app.exitInstance();
    return rc;
}
