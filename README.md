# 🚀 OpenCV YOLOv4-Tiny CUDA Object Detection

[![C++ Standard](https://shields.io)](https://cppreference.com)
[![OpenCV Version](https://shields.io)](https://opencv.org)
[![CUDA Speedup](https://shields.io)](https://nvidia.com)

Приложение на C++ для распознавания 80 классов объектов в реальном времени с использованием архитектуры **YOLOv4-tiny** и модуля **OpenCV DNN**. Проект разработан в среде **CLion** .

---

## ✨ Особенности / Features
* 📦 **YOLOv4-Tiny:** .
* 🛠️ **OpenCV DNN:** реализация детекции на C++ .



---

## 🛠️ Требования и Зависимости
Для сборки проекта вам понадобятся:
1. **Компилятор:** С поддержкой стандарта C++17 (MSVC, GCC или Clang).
2. **Сборщик:** CMake 3.10 или выше.
3. **Библиотека:** OpenCV 4.13.0 (для поддержки CUDA требуется ручная сборка из исходников).

---

## 🚀 Быстрый запуск

### 1. Клонирование репозитория
```bash
git clone https://github.com
cd OpenCV_YOLO_CUDA
```

### 2. Загрузка весов нейросети
Файлы весов (`.weights`) имеют большой размер и исключены из Git. Скачайте их вручную и положите в папку `model/`:
* 📥 [yolov4-tiny.weights](https://github.com)
* Файлы `yolov4-tiny.cfg` и `coco.names` уже находятся в папке `model/`.

### 3. Настройка CLion
1. Откройте проект в CLion.
2. Перейдите в `Run -> Edit Configurations...`.
3. В поле **Working directory** обязательно укажите корневую папку проекта (где лежит `CMakeLists.txt`), иначе модель не сможет найти файлы конфигурации.

---

### Режим CPU (По умолчанию)
```cpp
net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
```

### Режим OpenCL (Базовое GPU ускорение)
```cpp
net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
net.setPreferableTarget(cv::dnn::DNN_TARGET_OPENCL_FP16);
```

### Режим NVIDIA CUDA (Максимальный FPS)
> ⚠️ *Требует OpenCV, скомпилированную с флагами `WITH_CUDA` и `WITH_CUDNN`.*
```cpp
net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
```

---

## 📁 Структура проекта
```text
├── include/            # Заголовочные файлы (.h)
├── src/                # Исходный код (.cpp)
│   ├── main.cpp        # Точка входа, захват видеопотока
│   └── detector.cpp    # Логика инференса YOLO и фильтрация NMS
├── model/              # Конфигурация модели
│   ├── coco.names      # Список из 80 классов
│   └── yolov4-tiny.cfg # Архитектура сети
├── CMakeLists.txt      # Сценарий сборки CMake
└── .gitignore          # Исключение временных файлов и весов из Git
```
