#include <iostream>
#include <fstream>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include <windows.h>
#include <filesystem>
#include <algorithm>
#include <nlohmann/json.hpp>
#include "detector.hpp"
#include "visualizer.hpp"
#include "results_json.hpp"

#include "fs_utf8.hpp"

using namespace cv;
using namespace std;



// Ленивая инициализация компонентов
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

int main() {
    // Принудительно переключаем вывод консоли в UTF-8
    SetConsoleOutputCP(CP_UTF8);

    std::cout << "[Main] Инициализация модулей программы..." << std::endl;

    // 1. Конфигурация путей
    std::string inputFolderPath = "C:/Users/sulim/CLionProjects/OpenCV_YOLO_C-/pcb_pic";   // Папка с исходными платами
    std::string outputFolderPath = "C:/Users/sulim/CLionProjects/OpenCV_YOLO_C-/pcb_pic_res";      // Папка для сохранения результатов

    // Проверяем существование входной директории
    std::vector<std::filesystem::path> files = listImageFilesSorted (inputFolderPath);
    if (files.empty()) {
        std::cerr << "[Main] ОШИБКА! входная папка не найдена или изоюражений нет" <<
            inputFolderPath << std::endl;
        return -1;
    }

    // создаем выходную папку, если её ещё нет на диске
   std::filesystem::create_directory(fsu8::pathFromUtf8(outputFolderPath));

    // Предварительный подсчет файлов для вывода прогресса в консоль
    int totalImages = static_cast<int>(files.size());
    std::cout << "[Main] Модули готовы. Найдено изображений для анализа: " << totalImages << std::endl;
    std::cout << "[Main] Начинаем пакетную обработку плат..." << std::endl;

    // 2. Основной цикл по отсортированному списку

    std::vector<BoardRecord> records;
    records.reserve(totalImages);

    int processedCount = 0;
    for (const auto& filePath : files) {
            processedCount++;

        // ID привязан к ПОЗИЦИИ в отсортированном списке, а не к успеху чтения:
        // если один файл битый, его номер пропустится, но остальные ID не сдвинутся
        std::string boardId = makeBoardId(processedCount -1);
        std::string filename = fsu8::pathToUtf8(filePath.filename());
        std::string fullPath = fsu8::pathToUtf8(filePath);

            std::cout << "[" << processedCount << "/" << totalImages << "] " <<
                boardId << " Анализ платы: " << filename << " -> ";

            // Считываем файл в матрицу OpenCV
            cv::Mat frame = fsu8::imreadUtf8(fullPath);
            if (frame.empty()) {
                std::cerr << "Ошибка чтения файла изображения!" << std::endl;
                continue;
            }

            // Отправляем кадр в модель через синглтон детектора
            std::vector<Detection> detections = getDetector().detect(frame);
            std::cout << "Найдено дефектов: " << detections.size() << std::endl;

            // Используем визуализатор для отрисовки рамок дефектов
            // Передаем 0.0 вместо FPS, так как для статичных файлов этот параметр не используется
            getVisualizer().draw(frame, detections, 0.0);

            std::string resultFilename = makeResultFilename(boardId,filename);

            // Сохраняем обработанную матрицу на жесткий диск
            std::string savePath = outputFolderPath + "/" + resultFilename;
            if (!fsu8::imwriteUtf8(savePath, frame)) {
                std::cerr << "Критическая ошибка при записи файла: " << savePath << std::endl;
            }

        BoardRecord rec;
        rec.id = boardId;
        rec.filename = resultFilename;
        rec.defects = detections;
        records.push_back(rec);

        }

// Записываем результаты в json файл
    std::string finalReportPath = outputFolderPath + "/global_results.json";
    nlohmann::ordered_json report = buildResultsJson(records);


    std::ofstream file(fsu8::pathFromUtf8(finalReportPath));
    if (file.is_open()) {
        file << report.dump(4);
        file.close();
        std::cout << "\nОбщий JSON-отчет сохранен: " << finalReportPath << std::endl;
    }else {
        std::cerr << "[Main] Не удались открыть файл для записи: " << finalReportPath << std::endl;
    }
    std::cout << "[Main] Программа успешно завершила работу." << std::endl;
    std::cout << "[Main] Все обработанные изображения сохранены в папку: " << outputFolderPath << std::endl;

    return 0;
}
