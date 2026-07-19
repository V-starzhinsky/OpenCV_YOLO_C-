cmake_minimum_required(VERSION 3.10)
project(OpenCV_YOLO_CUDA)

# Устанавливаем стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MODEL_PATH "${CMAKE_SOURCE_DIR}/model")
add_definitions(-DMODEL_PATH=${MODEL_PATH})

# Находим OpenCV
find_package(OpenCV REQUIRED)

# Создаём исполняемый файл
add_executable(${PROJECT_NAME}
        main.cpp
        Preprocessor.cpp
        visualizer.cpp
        detector.cpp
)

# Подключаем библиотеки OpenCV
target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})

# Сообщаем, где находятся заголовочные файлы
target_include_directories(${PROJECT_NAME} PRIVATE
        ${OpenCV_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include
)
