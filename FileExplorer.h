#pragma once
#include <Windows.h>
#include <d3d11.h>      
#include <dxgi.h>  
#include <string>

class FileExplorer
{
public:
    FileExplorer();
    ~FileExplorer();

    bool Initialize(HINSTANCE hInstance, int width, int height);
    int Run();
    void Shutdown();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool InitD3D11();
    void CleanupD3D11();
    void Render();

private:
    HWND m_hwnd;
    HINSTANCE m_hInstance;
    int m_width;
    int m_height;
    bool m_running;

    // D3D11 设备
    ID3D11Device* m_pd3dDevice;
    ID3D11DeviceContext* m_pd3dDeviceContext;
    IDXGISwapChain* m_pSwapChain;
    ID3D11RenderTargetView* m_pMainRenderTargetView;

    // 文件浏览器组件
    class FileBrowser* m_fileBrowser;
};