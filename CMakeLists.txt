cmake_minimum_required(VERSION 3.10)
project(OpenCV_YOLO_CUDA)

# Принудительно заменяет макрос на пустую инструкцию только для компилятора MSVC
add_compile_options("/D__builtin_verbose_trap(a,b)=__noop")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



# QT
set(Qt6_DIR "C:/libs/Qt_MSVC/6.7.3/msvc2019_64/lib/cmake/Qt6")
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOMOC ON)


set(MODEL_PATH "${CMAKE_SOURCE_DIR}/model")
add_definitions(-DMODEL_PATH=${MODEL_PATH})

# 🚀 УКАЗЫВАЕМ ПУТЬ К НОВОЙ СБОРКЕ OPENCV ДЛЯ VISUAL STUDIO
set(OpenCV_DIR "C:/libs/opencv/build/x64/vc16/lib")

# Находим OpenCV
find_package(OpenCV REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
message(STATUS "Qt6 найден: ${Qt6_DIR}")


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
add_executable(${PROJECT_NAME}
        main.cpp
        Preprocessor.cpp
        visualizer.cpp
        detector.cpp
        FpsCounter.cpp
        VideoStreamer.cpp
        mainwindow.h
        mainwindow.cpp
        batchworker.cpp
        batchworker.h
)

target_link_libraries(${PROJECT_NAME}
        ${OpenCV_LIBS}
        onnxruntime.lib
        nlohmann_json::nlohmann_json
        Qt6::Widgets
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${OpenCV_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include
)
