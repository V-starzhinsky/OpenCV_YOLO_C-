cmake_minimum_required(VERSION 3.20)
project(OpenCV_YOLO_CUDA)

# Принудительно заменяет макрос на пустую инструкцию только для компилятора MSVC
add_compile_options("/D__builtin_verbose_trap(a,b)=__noop")
add_compile_options("/utf-8")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MODEL_PATH "${CMAKE_SOURCE_DIR}/model")
add_definitions(-DMODEL_PATH=${MODEL_PATH})

# 🚀 УКАЗЫВАЕМ ПУТЬ К НОВОЙ СБОРКЕ OPENCV ДЛЯ VISUAL STUDIO
set(OpenCV_DIR "C:/libs/opencv/build/x64/vc16/lib")

# Находим OpenCV
find_package(OpenCV REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
include(FetchContent)

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.9b
)
# FetchContent_MakeAvailable — ИСПОЛНЕНИЕ: при первой конфигурации клонирует
# репозиторий в <каталог сборки>/_deps/imgui-src (нужен интернет ОДИН раз),
# и задаёт переменную imgui_SOURCE_DIR с путём к исходникам.
FetchContent_MakeAvailable(imgui)

# У ImGui НЕТ своего CMakeLists.txt — это просто набор .cpp/.h.
# Поэтому собираем его как СВОЮ статическую библиотеку из нужных файлов:
add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp           # ядро (текстовый буфер, компоновка)
        ${imgui_SOURCE_DIR}/imgui_draw.cpp      # отрисовка (вершинники, шрифты)
        ${imgui_SOURCE_DIR}/imgui_tables.cpp    # таблицы
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp   # виджеты (кнопки, слайдеры, списки)
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp   # бэкенд: рендер через DX11
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp  # бэкенд: окна/ввод Win32
)

# PUBLIC = пути видят и все, кто слинкуется с imgui (наш pcb_gui):
# include <imgui.h> и <imgui_impl_dx11.h> заработают автоматически.
target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

# Бэкенд DX11 обращается к системным библиотекам — линкуем их к imgui
target_link_libraries(imgui PUBLIC d3d11 dxgi dwmapi)

# ============================================================================
# РУЧНОЕ ПОДКЛЮЧЕНИЕ ONNX RUNTIME DIRECTML (Исправленные пути)
# ============================================================================
set(ONNX_ROOT "C:/libs/onnxruntime-dml")

# Указываем линковщику искать файл onnxruntime.lib там, где он у вас лежит [1]
link_directories("${ONNX_ROOT}/runtimes/win-x64/native")

# Подключаем папки с заголовочными .h файлами для компилятора [1]
include_directories("${ONNX_ROOT}/build/native/include")
include_directories("${ONNX_ROOT}")

# ============================================================================
# СБОРКА И ЛИНКОВКА ИСПОЛНЯЕМОГО ФАЙЛА
# ============================================================================
add_executable(${PROJECT_NAME} WIN32
        gui/d3d11_texture.cpp
        gui/batch_runner.cpp
        gui/main_gui.cpp
        results_json.cpp
        detector.cpp
        visualizer.cpp
)

target_link_libraries(${PROJECT_NAME}
        ${OpenCV_LIBS}
        onnxruntime.lib
        nlohmann_json::nlohmann_json
        imgui
        ole32
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${OpenCV_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include

)

target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)