// Dx12.cpp : Defines the entry point for the application.

#include "crtdbg.h"
#include "Application.h"

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    int crtDbgFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    crtDbgFlag |= _CRTDBG_LEAK_CHECK_DF;
    _CrtSetDbgFlag(crtDbgFlag);

    Vnm::Application application;
    application.Startup(hInstance, nCmdShow);

    MSG message = {};
    while (1)
    {
        if (PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                break;
            }

            TranslateMessage(&message);
            DispatchMessage(&message);
        }
        else
        {
            // Processing
            application.Mainloop();
        }
    }

    application.Shutdown();
    return (int)message.wParam;
}
