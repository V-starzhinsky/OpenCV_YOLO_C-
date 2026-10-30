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
#include "results_json.hpp"

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
static bool createDeviceD3D(HWND hWnd) {
    // Описание swap chain. "= {}" обнуляет структуру: без этого в полях
    // остался бы мусор со стека . Массив из двух буферов — база двойной буферизации.
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = g_width;
    sd.BufferDesc.Height = g_height;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // тот же формат, что у текстур
    sd.BufferDesc.RefreshRate = {60, 1}; // 60 Гц (важно только для полного экрана)
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd; // окно, куда показываем картинку
    sd.SampleDesc.Count = 1; // без MSAA-сглаживания
    sd.Windowed = TRUE; // оконный режим
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; // показать буфер, старый выбросить

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
    ID3D11Texture2D *back = nullptr;
    hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&back));
    if (FAILED(hr) || !back) return false;
    hr = g_pd3dDevice->CreateRenderTargetView(back, nullptr, &g_mainRTV);
    back->Release();
    if (FAILED(hr)) return false;

    g_pd3dContext->OMSetRenderTargets(1, &g_mainRTV, nullptr);
    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<FLOAT>(g_width);
    vp.Height = static_cast<FLOAT>(g_height);
    g_pd3dContext->RSSetViewports(1, &vp);
    return true;
}

// cleanupDeviceD3D: освобождение в порядке, обратном созданию.
// Порядок важен: контекст пользуется RTV и буферами, поэтому умирает раньше.

static void cleanupDeviceD3D() {
    if (g_mainRTV) {
        g_mainRTV->Release();
        g_mainRTV = nullptr;
    }
    if (g_pSwapChain) {
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }
    if (g_pd3dContext) {
        g_pd3dContext->Release();
        g_pd3dContext = nullptr;
    }
    if (g_pd3dDevice) {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
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

static bool pickFolderDialog(HWND owner, std::string &outPath) {
    IFileOpenDialog *dlg = nullptr;
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

// отрисовка UI
struct TextureTracker {
    std::string boardId;
    bool isOriginal = false;
    const unsigned char *data = nullptr;
};


static ImVec4 statusColor(FileStatus s) {
    switch (s) {
        case FileStatus::Pending: return ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
        case FileStatus::Processing: return ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
        case FileStatus::Done: return ImVec4(0.4f, 0.9f, 0.4f, 1.0f);
        case FileStatus::Failed: return ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
    }
    return ImVec4(1, 1, 1, 1);
}

static void drawUi(AppState &state, BatchRunner &runner,
                   D3D11Texture &texImage, TextureTracker &trk) {
    Progress prog;
    std::vector<BoardItem> items;
    int selectedIndex;
    {
        std::lock_guard<std::mutex> lock(state.mtx);
        prog = state.progress;
        items = state.items;
        selectedIndex = state.selectedIndex;
    }

    if (selectedIndex >= static_cast<int>(items.size())) selectedIndex = -1;

    const BoardItem *sel = (selectedIndex >= 0 ? &items[selectedIndex] : nullptr);


    bool wantOriginal = state.settings.showOriginalView;
    const cv::Mat desiredMat = sel ? (wantOriginal ? sel->original : sel->annotated) : cv::Mat();
    const bool wantValid = sel && sel->status == FileStatus::Done && !desiredMat.empty();

    if (wantValid) {
        const unsigned char *wantData = desiredMat.data;
        if (trk.boardId != sel->id || trk.isOriginal != wantOriginal || trk.data != wantData) {
            texImage.upload(desiredMat);
            trk.boardId = sel->id;
            trk.isOriginal = wantOriginal;
            trk.data = wantData;
        }
    } else if (texImage.valid()) {
        texImage.release();
        trk = TextureTracker{};
    }


    const ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("##PCBDetector", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);


    ImGui::BeginChild("левая", ImVec2(300, 0), ImGuiChildFlags_Borders);

    ImGui::TextDisabled("Папка с исходными платами");
    ImGui::InputText("##in", state.settings.inputDir, IM_ARRAYSIZE(state.settings.inputDir),
                     ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("Обзор..##id0")) {
        std::string picked;
        if (pickFolderDialog(GetActiveWindow(), picked)) {
            strcpy_s(state.settings.inputDir, picked.c_str());
        }
    }

    ImGui::TextDisabled("Папка результатов");
    ImGui::InputText("##out", state.settings.outputDir, IM_ARRAYSIZE(state.settings.outputDir),
                     ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("Обзор..##id1")) {
        std::string picked;
        if (pickFolderDialog(GetActiveWindow(), picked)) {
            strcpy_s(state.settings.outputDir, picked.c_str());
        }
    }

    ImGui::Spacing();


    // Кнопки: Start неактивна во время прогона, Stop — вне его.
    // BeginDisabled/EndDisabled визуально "серят" содержимое и блокируют ввод.
    ImGui::BeginDisabled(prog.running);
    if (ImGui::Button(" Старт анализа", ImVec2(-1, 0))) {
        runner.start(state);   // start() сам защищён от повторного запуска
    }
    ImGui::EndDisabled();

    ImGui::BeginDisabled(prog.running);
    if (ImGui::Button("Стоп", ImVec2(-1, 0))) {
        state.cancelRequested.store(true);
    }
    ImGui::EndDisabled();

    if (prog.total > 0) {
        float frac = static_cast<float>(prog.processed) / static_cast<float>(prog.total);
        char overlay[32];
        snprintf(overlay, sizeof(overlay), "%d / %d", prog.processed, prog.total);
        ImGui::ProgressBar(frac, ImVec2(-1, 0), overlay);
    }
    ImGui::TextWrapped("%s", prog.message.c_str());
    if (!prog.error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
        ImGui::TextWrapped("Ошибка: %s", prog.error.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Separator();

    static char filter[128] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##фильтр", "поиск по ID или имени...", filter, sizeof(filter));
    static bool onlyWithDefects = false;
    ImGui::Checkbox("Только с дефектами", &onlyWithDefects);

    //список плат
    ImGui::BeginChild("список", ImVec2(0, 0), ImGuiChildFlags_None);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(items.size()));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            const BoardItem &it = items[i];

            if (filter[0] != '\0' || onlyWithDefects) {
                bool passFilter = true;
                if (onlyWithDefects && it.defects.empty()) passFilter = false;
                if (passFilter && filter[0] != '\0') {
                    std::string hay = (it.id + " " + it.sourceFilename);
                    std::string needle = filter;
                    for (auto &c: hay) c = (char) tolower((unsigned char) c);
                    for (auto &c: needle) c = (char) tolower((unsigned char) c);
                    if (hay.find(needle) != std::string::npos) passFilter = false;
                }
                if (!passFilter) continue;
            }


            ImGui::PushStyleColor(ImGuiCol_Text, statusColor(it.status));
            bool clicked = ImGui::Selectable(it.id.c_str(), i == selectedIndex);
            ImGui::PopStyleColor();
            ImGui::SameLine(120);
            ImGui::TextDisabled("%s", it.sourceFilename.c_str());
            if (!it.defects.empty()) {
                ImGui::SameLine(ImGui::GetWindowWidth() - 40);
                ImGui::Text("%d", static_cast<int>(it.defects.size()));
            }
            if (clicked) {
                state.selectedIndex = i;
            }
        }
    }
    clipper.End();
    ImGui::EndChild(); // list

    ImGui::EndChild(); // left panel
    ImGui::SameLine();


    ImGui::BeginChild("правая", ImVec2(0, 0), ImGuiChildFlags_None);

    ImGui::BeginChild("канва", ImVec2(0, -140), ImGuiChildFlags_Borders);

    if (sel) {
        ImGui::Text("%s %s", sel->id.c_str(), sel->sourceFilename.c_str());
    }
    ImGui::Checkbox("Показывать исходное(без рамок)", &state.settings.showOriginalView);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (texImage.valid() && sel) {
        float scale = (avail.x / texImage.width() < avail.y / texImage.height())
                          ? avail.x / texImage.width()
                          : avail.y / texImage.height();
        ImVec2 size(texImage.width() * scale, texImage.height() * scale);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail.x - size.x) * 0.5f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.0f,
            (avail.y - size.y) * 0.5f));
        ImGui::Image(texImage.view(), size);
    } else {
        ImVec2 pos(avail.x * 0.5f - 120, avail.y * 0.5f);
        if (pos.x > 0)
            ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + pos.x,
                                       ImGui::GetCursorPosY() + pos.y));
        ImGui::TextDisabled(sel ? "Картинка появится после обработки платы" : "Выберите плату в списке слева");
    }
    ImGui::EndChild();

    //Lower panel: defects table for choosing pcb
    ImGui::BeginChild("дефекты", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if (sel && sel->status == FileStatus::Done) {
        if (ImGui::BeginTable("дефектыТаблица", 4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("ID дефекта");
            ImGui::TableSetupColumn("Тип");
            ImGui::TableSetupColumn("Уверенность");
            ImGui::TableSetupColumn("Бокс(x, y, w, h)");
            ImGui::TableHeadersRow();

            for (int d = 0; d < static_cast<int>(sel->defects.size()); d++) {
                const Detection &det = sel->defects[d];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", makeDefectId(sel->id, d).c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", det.className.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.3f", det.confidence);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%d, %d, %d x %d", det.bbox.x, det.bbox.y, det.bbox.width, det.bbox.height);
            }
            ImGui::EndTable();
        }
        if (sel->defects.empty())
            ImGui::TextDisabled("Дефектов не найдено");
        } else if (sel) {
            ImGui::TextDisabled("Плата еще не обработана");
        } else {
            ImGui::TextDisabled("Выберите плату - здесь появятся ее дефекты");
        }
        ImGui::EndChild();

        ImGui::EndChild();
        ImGui::End();

}

static void loadCyrillicFont() {
    ImGuiIO &io = ImGui::GetIO();
    const char *fontPath = "C:\\Windows\\Fonts\\segoeui.ttf";
    if (GetFileAttributesA(fontPath) != INVALID_FILE_ATTRIBUTES) {
        io.Fonts->AddFontFromFileTTF(fontPath, 17.0f, nullptr,
                                     io.Fonts->GetGlyphRangesCyrillic());
    }
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"PCBDetectorClass";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"PCB DETECTOR - YOLOv11s",
                                WS_OVERLAPPEDWINDOW, 100, 100,
                                static_cast<int>(g_width), static_cast<int>(g_height),
                                nullptr, nullptr, hInstance, nullptr);
    if (!hwnd || !createDeviceD3D(hwnd)) {
        MessageBoxA(nullptr, "Не удалось инициализировать DirectX 11",
                    "PCB DETECTOR", MB_ICONERROR);
        cleanupDeviceD3D();
        CoUninitialize();
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);


    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    loadCyrillicFont();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);


    AppState state;
    BatchRunner runner;
    D3D11Texture texImage;
    TextureTracker tracker;
    texImage.init(g_pd3dDevice);

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0U,
                            0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;


        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        drawUi(state, runner, texImage, tracker);

        ImGui::Render();


        const float clear_color[4] = {0.08f, 0.09f, 0.10f, 1.0f};
        g_pd3dContext->OMSetRenderTargets(1, &g_mainRTV, nullptr);
        g_pd3dContext->ClearRenderTargetView(g_mainRTV, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());


        g_pSwapChain->Present(1, 0);
    }

    runner.stop();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    texImage.release();
    cleanupDeviceD3D();

    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    CoUninitialize();


    return 0;
}
