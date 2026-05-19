#include "FileExplorer.h"
#include "FileBrowser.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <dxgi.h>
#include <tchar.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void CreateRenderTarget(ID3D11Device* device, IDXGISwapChain* swapChain, ID3D11RenderTargetView** renderTargetView)
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    swapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    device->CreateRenderTargetView(pBackBuffer, nullptr, renderTargetView);
    pBackBuffer->Release();
}

FileExplorer::FileExplorer()
    : m_hwnd(nullptr), m_hInstance(nullptr), m_width(1280), m_height(800), m_running(false),
    m_pd3dDevice(nullptr), m_pd3dDeviceContext(nullptr), m_pSwapChain(nullptr), m_pMainRenderTargetView(nullptr),
    m_fileBrowser(nullptr) {
}

FileExplorer::~FileExplorer() { Shutdown(); }

bool FileExplorer::InitD3D11()
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = m_width;
    sd.BufferDesc.Height = m_height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd,
        &m_pSwapChain, &m_pd3dDevice, &featureLevel, &m_pd3dDeviceContext
    );

    if (res != S_OK) return false;

    CreateRenderTarget(m_pd3dDevice, m_pSwapChain, &m_pMainRenderTargetView);
    return true;
}

void FileExplorer::CleanupD3D11()
{
    if (m_pMainRenderTargetView) { m_pMainRenderTargetView->Release(); m_pMainRenderTargetView = nullptr; }
    if (m_pSwapChain) { m_pSwapChain->Release(); m_pSwapChain = nullptr; }
    if (m_pd3dDeviceContext) { m_pd3dDeviceContext->Release(); m_pd3dDeviceContext = nullptr; }
    if (m_pd3dDevice) { m_pd3dDevice->Release(); m_pd3dDevice = nullptr; }
}

bool FileExplorer::Initialize(HINSTANCE hInstance, int width, int height)
{
    m_hInstance = hInstance;
    m_width = width;
    m_height = height;

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, WndProc, 0L, 0L, hInstance,
                      LoadIcon(nullptr, IDI_APPLICATION), LoadCursor(nullptr, IDC_ARROW),
                      nullptr, nullptr, L"FileExplorerWindow", nullptr };
    RegisterClassExW(&wc);

    RECT rect = { 0, 0, width, height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    m_hwnd = CreateWindowW(wc.lpszClassName, L"File Explorer",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, wc.hInstance, this);

    if (!m_hwnd) return false;

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    if (!InitD3D11()) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();




    // 加载中文字体（以微软雅黑为例，请确认文件存在）
    ImGuiIO& io = ImGui::GetIO();
    ImFont* font = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 16.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());
    if (!font) {
        // 如果上面文件不存在，尝试黑体
        font = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/simhei.ttf", 16.0f);
    }
    // 注意：路径分隔符用 / 或者 \\，不要用 \ 单反斜杠



    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_pd3dDevice, m_pd3dDeviceContext);

    m_fileBrowser = new FileBrowser();
    m_running = true;
    return true;
}

int FileExplorer::Run()
{
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));

    while (m_running && msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        Render();
    }
    return (int)msg.wParam;
}

void FileExplorer::Render()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport();

    if (m_fileBrowser) {
        m_fileBrowser->Render();
    }

    ImGui::Render();

    const float clear_color[4] = { 0.06f, 0.06f, 0.08f, 1.00f };
    m_pd3dDeviceContext->OMSetRenderTargets(1, &m_pMainRenderTargetView, nullptr);
    m_pd3dDeviceContext->ClearRenderTargetView(m_pMainRenderTargetView, clear_color);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    m_pSwapChain->Present(1, 0);
}

void FileExplorer::Shutdown()
{
    if (m_fileBrowser) delete m_fileBrowser;
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupD3D11();
    if (m_hwnd) DestroyWindow(m_hwnd);
    UnregisterClassW(L"FileExplorerWindow", m_hInstance);
}

LRESULT CALLBACK FileExplorer::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return true;

    FileExplorer* app = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = reinterpret_cast<FileExplorer*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    else {
        app = reinterpret_cast<FileExplorer*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (app && msg == WM_DESTROY) {
        app->m_running = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT FileExplorer::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProc(hwnd, msg, wParam, lParam);
}