#include "FileExplorer.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // 告诉 Windows：这个程序要用 UTF-8 编码
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    FileExplorer app;

    if (!app.Initialize(hInstance, 1280, 800)) {
        return -1;
    }

    return app.Run();
}