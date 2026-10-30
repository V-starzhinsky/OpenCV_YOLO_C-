#include <windows.h>     // окно, сообщения
#include <d3d11.h>       // устройство, контекст, текстуры
#include <dxgi.h>        // фабрика и цепочка буферов (swap chain)
#include <shobjidl.h>    // IFileOpenDialog — системный диалог выбора папки
#include <objbase.h>     // CoInitializeEx, CoTaskMemFree (COM)

#include <imgui.h>
#include <imgui_impl_win32.h>   // бэкенд: окно и ввод
#include <imgui_impl_dx11.h>    // бэкенд: рендер через DX11

#include <string>
#include "fs_utf8.hpp"       // fsu8::wideToUtf8 — диалог отдаёт wchar_t*
#include "app_state.hpp"     // AppState — общее состояние
#include "batch_runner.hpp"  // BatchRunner — рабочий поток
#include "d3d11_texture.hpp" // D3D11Texture — выбранная картинка на GPU

// static = видны только внутри этого файла. Глобальные указатели здесь —
// осознанный выбор: приложение однооконное, каждый ресурс создаётся один
// раз и живёт до конца программы (как в официальных примерах ImGui).
//   Device    — "фабрика" GPU-объектов (текстуры, шейдеры);
//   Context   — очередь команд рисования ("что рисовать сейчас");
//   SwapChain — пара экранных буферов (см. объяснение после кода);
//   RTV       — render target view: "куда рисовать этот кадр".
static ID3D11Device *g_pd3dDevice = nullptr;
static ID3D11DeviceContext *g_pd3dContext = nullptr;
static IDXGISwapChain *g_pSwapChain = nullptr;
static ID3D11RenderTargetView *g_mainRTV = nullptr;
static UINT g_width = 1280;
static UINT g_height = 800;


// Обработчик из бэкенда ImGui: он должен видеть КАЖДОЕ сообщение окна
// ДО нашей логики, иначе ввод не дойдёт до интерфейса. extern-объявление
// нужно, потому что бэкенд — отдельная единица трансляции.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


// resizeBackbuffer: реакция на изменение размера окна.
// Задний буфер создать "с запасом" нельзя — его размер фиксирован при
// создании, поэтому при ресайзе: отпустить RTV → ResizeBuffers (пересоздать
// пару буферов) → собрать RTV заново. Порядок обязателен: RTV держит
// ссылку на буфер, и ResizeBuffers откажет, пока она жива
static void resizeBackbuffer(UINT w, UINT h) {
    if (!g_pd3dDevice || w == 0 || h == 0) return;
    if (g_mainRTV) {
        g_mainRTV->Release();
        g_mainRTV = nullptr;
    }

    g_pSwapChain->ResizeBuffers(0,
                                w, h,
                                DXGI_FORMAT_UNKNOWN,
                                0);
    g_width = w;
    g_height = h;

    ID3D11Texture2D *back = nullptr;
    if (FAILED(g_pSwapChain->GetBuffer(0,IID_PPV_ARGS(&back))) || !back) {
        return;
    }
    g_pd3dDevice->CreateRenderTargetView(back, nullptr, &g_mainRTV);
    back->Release();

    g_pd3dContext->OMSetRenderTargets(1, &g_mainRTV, nullptr);
    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<FLOAT>(w);
    vp.Height = static_cast<FLOAT>(h);
    g_pd3dContext->RSSetViewports(1, &vp);
}



// createDeviceD3D: создаёт Device + Context + SwapChain + первый RTV.
// Вызывается один раз из wWinMain
static bool createDeviceD3D(HWND hWnd)
{
    // Описание swap chain. "= {}" обнуляет структуру: без этого в полях
    // остался бы мусор со стека . Массив из двух буферов — база двойной буферизации.
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width  = g_width;
    sd.BufferDesc.Height = g_height;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // тот же формат, что у текстур
    sd.BufferDesc.RefreshRate = { 60, 1 };   // 60 Гц (важно только для полного экрана)
    sd.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;                  // окно, куда показываем картинку
    sd.SampleDesc.Count = 1;                 // без MSAA-сглаживания
    sd.Windowed      = TRUE;                 // оконный режим
    sd.SwapEffect    = DXGI_SWAP_EFFECT_DISCARD; // показать буфер, старый выбросить

    // D3D11_CREATE_DEVICE_BGRA_SUPPORT — совместимость форматов с 2D-выводом
    UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0; // базовый уровень DX11

    // Одна функция создаёт ВСЁ сразу: устройство, контекст и цепочку.
    // D3D_DRIVER_TYPE_HARDWARE — реальный GPU (SOFTWARE — медленный fallback).
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags,
            &fl, 1, D3D11_SDK_VERSION,
            &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pd3dContext);
    if (FAILED(hr)) return false;

    // Собрать render target из заднего буфера (общая логика с ресайзом)
    ID3D11Texture2D* back = nullptr;
    hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&back));
    if (FAILED(hr) || !back) return false;
    hr = g_pd3dDevice->CreateRenderTargetView(back, nullptr, &g_mainRTV);
    back->Release();
    if (FAILED(hr)) return false;

    g_pd3dContext->OMSetRenderTargets(1, &g_mainRTV, nullptr);
    D3D11_VIEWPORT vp = {};
    vp.Width  = static_cast<FLOAT>(g_width);
    vp.Height = static_cast<FLOAT>(g_height);
    g_pd3dContext->RSSetViewports(1, &vp);
    return true;
}

// cleanupDeviceD3D: освобождение в порядке, обратном созданию.
// Порядок важен: контекст пользуется RTV и буферами, поэтому умирает раньше.

static void cleanupDeviceD3D() {
    if (g_mainRTV) {g_mainRTV->Release(); g_mainRTV = nullptr;}
    if (g_pSwapChain) {g_pSwapChain->Release(); g_pSwapChain = nullptr;}
    if (g_pd3dContext) {g_pd3dContext->Release(); g_pd3dContext = nullptr;}
    if (g_pd3dDevice) {g_pd3dDevice->Release(); g_pd3dDevice = nullptr;}
}

// WndProc — оконная процедура: функция, которую Windows вызывает с каждым
// сообщением этого окна. Главный switch нашей реакции на события.
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return 1;

    switch (msg) {
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED)
                resizeBackbuffer(LOWORD(lParam), HIWORD(lParam));
            return 0;

            case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

            default: return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

static bool pickFolderDialog ( HWND owner, std::string& outPath) {
    IFileOpenDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) return false;


    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

    bool ok = false;
    if (SUCCEEDED(dlg->Show(owner))) {
        IShellItem *item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item) {
            PWSTR wPath = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &wPath)) && wPath) {
                outPath = fsu8::wideToUtf8(wPath);
                CoTaskMemFree(wPath);

                ok = true;
            }
            item->Release();
        }
    }
    dlg->Release();
    return ok;
}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    return 0;
}