#include <iostream>
#include <opencv2/opencv.hpp>
#include <windows.h>
#include <filesystem>
#include <algorithm>

#include "detector.hpp"
#include "visualizer.hpp"

namespace fs = std::filesystem;
using namespace cv;
using namespace std;

// Ленивая инициализация компонентов (Ваша исходная архитектура)
Detector& getDetector() {
    static Detector detector;
    return detector;
}

Visualizer& getVisualizer() {
    static Visualizer visualizer(
            Scalar(0, 0, 255), // Красный цвет рамок для подсветки дефектов плат
            2,
            0.5,
            1,
            true,
            true,
            false,
            false
    );
    return visualizer;
}

// Вспомогательный метод для фильтрации только графических файлов
bool isImage(const fs::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp");
}

int main() {
    // Принудительно переключаем вывод консоли в UTF-8
    SetConsoleOutputCP(CP_UTF8);

    std::cout << "[Main] Инициализация модулей программы..." << std::endl;

    // 1. Конфигурация путей (Укажите ваши реальные папки)
    std::string inputFolderPath = "C:/Users/ponka/CLionProjects/OpenCV_YOLO_CUDA/pcb_train_pic";   // Папка с исходными платами
    std::string outputFolderPath = "C:/Users/ponka/CLionProjects/OpenCV_YOLO_CUDA/pcb_train_res";      // Папка для сохранения результатов

    // Проверяем существование входной директории
    if (!fs::exists(inputFolderPath) || !fs::is_directory(inputFolderPath)) {
        std::cerr << "[Main] КРИТИЧЕСКАЯ ОШИБКА: Входная папка с изображениями не найдена по пути: "
                  << inputFolderPath << std::endl;
        return -1;
    }

    // Автоматически создаем выходную папку, если её ещё нет на диске
    if (!fs::exists(outputFolderPath)) {
        fs::create_directories(outputFolderPath);
    }

    // Предварительный подсчет файлов для вывода красивого прогресса в консоль
    int totalImages = 0;
    for (const auto& entry : fs::directory_iterator(inputFolderPath)) {
        if (fs::is_regular_file(entry.path()) && isImage(entry.path())) {
            totalImages++;
        }
    }

    std::cout << "[Main] Модули готовы. Найдено изображений для анализа: " << totalImages << std::endl;
    std::cout << "[Main] Начинаем пакетную обработку плат..." << std::endl;

    // 2. Основной цикл последовательного чтения папки (Итератор C++17)
    int processedCount = 0;
    for (const auto& entry : fs::directory_iterator(inputFolderPath)) {
        if (fs::is_regular_file(entry.path()) && isImage(entry.path())) {

            processedCount++;
            std::string currentImgPath = entry.path().string();
            std::string filename = entry.path().filename().string();

            std::cout << "[" << processedCount << "/" << totalImages << "] Анализ платы: " << filename << " -> ";

            // Считываем файл в матрицу OpenCV
            cv::Mat frame = cv::imread(currentImgPath);
            if (frame.empty()) {
                std::cerr << "Ошибка чтения файла изображения!" << std::endl;
                continue;
            }

            // Отправляем кадр в вашу модель YOLOv11 через синглтон детектора
            std::vector<Detection> detections = getDetector().detect(frame);
            std::cout << "Найдено дефектов: " << detections.size() << std::endl;

            // Используем ваш визуализатор для отрисовки рамок дефектов
            // Передаем 0.0 вместо FPS, так как для статичных файлов этот параметр не используется
            getVisualizer().draw(frame, detections, 0.0);

            // Сохраняем обработанную матрицу на жесткий диск
            std::string savePath = outputFolderPath + "/" + filename;
            if (!cv::imwrite(savePath, frame)) {
                std::cerr << "Критическая ошибка при записи файла: " << savePath << std::endl;
            }
        }
    }

    std::cout << "[Main] Программа успешно завершила работу." << std::endl;
    std::cout << "[Main] Все обработанные изображения сохранены в папку: " << outputFolderPath << std::endl;

    return 0;
}
