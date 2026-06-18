#include <Windows.h>
#include "core/Application.h"
#include "util/ComInit.h"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    LightRec::Util::ComInit comInit;

    lightrec::core::Application app;
    if (!app.init()) {
        MessageBoxW(nullptr, L"Failed to initialize LightRec Application.", L"LightRec Error", MB_ICONERROR | MB_OK);
        return -1;
    }

    return app.run();
}
