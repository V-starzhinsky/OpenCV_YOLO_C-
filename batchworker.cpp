#include "batchworker.h"

#include <QDir>
#include <QFileInfo>
#include <QStringList>

#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>

#include <fstream>
#include <string>
#include <vector>
#include <exception>

#include "detector.hpp"
#include "visualizer.hpp"


static nlohmann::ordered_json processImageResults(
    const std::string& fileName,
    const std::vector<Detection>& detections)
{
    nlohmann::ordered_json imageJson;
    imageJson["filename"] = fileName;
    imageJson["total_defects"] = detections.size();
    imageJson["defects"] = nlohmann::ordered_json::array();

    for (const auto& det : detections) {
        nlohmann::ordered_json defectJson;

        // Используем актуальные поля из структуры Detection: className и bbox
        defectJson["type"] = det.className;

        defectJson["box"] = {
            {"x", det.bbox.x},
            {"y", det.bbox.y},
            {"width", det.bbox.width},
            {"height", det.bbox.height}
        };
        imageJson["defects"].push_back(defectJson);
    }
    return imageJson;
}


BatchWorker::BatchWorker(QObject *parent) : QObject(parent) {}
BatchWorker::~BatchWorker() {}


void BatchWorker::stop() {
    m_stop.store(true);
    finished();
}

void BatchWorker::start(const QString &inputDir, const QString &outputDir) {
        //сбрасываем флаг
    m_stop.store(false);

    emit logMessage(QString("Вход: %1").arg(inputDir));
    emit logMessage(QString("Выход: %1").arg(outputDir));

    //Входная папка
    QDir inDir(inputDir);
    if (!inDir.exists()) {
        emit logMessage("ОШИБКА: входная папка не существует.");
        emit finished();
        return;
    }

    //список изображений
    const QStringList filters = {
        "*.jpg", "*.jpeg", "*.png", "*.bmp"
    };

    const QFileInfoList files = inDir.entryInfoList(filters, QDir::Files,
        QDir::Name);

    const int total = files.size();
    if (total == 0) {
        emit logMessage("В папке нет изображений!");
        emit finished();
        return;
    }

    //Выходная папка (создаем при необходимости)
    if (!QDir(outputDir).exists()) {
        if (!QDir().mkpath(outputDir)) {
            emit logMessage("Не удалось создать папку результатов");
            emit finished();
            return;
        }
        emit logMessage(QString("Папка результатов создана: %1").arg(outputDir));
    }

    emit logMessage(QString("Найдено изображений: %1").arg(total));
    emit progressChanged(0, total);

    //создание тяжелый объектов пайплайна
    emit logMessage("Загрузка модели YOLO...");

    struct Defect {
        std::string type;
        cv::Rect box;
    };

    Detector detector;

    Visualizer visualizer(
                cv::Scalar(0, 0, 255), // Красный цвет рамок для подсветки дефектов плат
                2,
                0.5,
                1,
                true,
                true,
                false,
                false
        );

    //Корень сводного JSON-отчета
    nlohmann::ordered_json rootResultArray = nlohmann::ordered_json::array();


    //Главный цикл:
    int processed = 0;
    int failed = 0;

    for (int i = 0; i < total; i++) {
        //Точка остановки - проверяем флаг перед КАЖДЫМ файлом
        if (m_stop.load()) {
            emit logMessage("Остановлено пользователем");
            break;
        }

        const QFileInfo fileInfo = files.at(i);

        emit logMessage(QString("[%1%2] %3")
            .arg(i+1)
            .arg(total)
            .arg(fileInfo.fileName()));

        try {
            const std::string imgPath = fileInfo.absoluteFilePath().toStdString();
            const std::string fileName = fileInfo.fileName().toStdString();

            //Чтение изображения
                    // Считываем файл в матрицу OpenCV
                    cv::Mat frame = cv::imread(imgPath);
                    if (frame.empty()) {
                        emit logMessage(QString("ОШИБКА (%1): не удалось прочитать файлы.")
                            .arg(fileInfo.fileName()));
                        ++failed;
                    }
                    else{
                    // детекция
                    std::vector<Detection> detections = detector.detect(frame);
                    emit logMessage(QString("Найдено дефектов: %1").arg(detections.size()));

                    // Передаем 0.0 вместо FPS, так как для статичных файлов этот параметр не используется
                    visualizer.draw(frame, detections, 0.0);

                    // Сохраняем обработанную матрицу на жесткий диск
                   const std::string savePath = outputDir.toStdString() + "/" + fileName;
                    if (!cv::imwrite(savePath, frame)) {
                        emit logMessage(QString("ОШИБКА записи файла: %1").arg(QString::fromStdString(savePath)));
                    }

                    //JSON-отчет по изображению
                        rootResultArray.push_back(processImageResults(fileName, detections));
                        processed++;
                }

        }
        catch (const std::exception &e) {
            emit logMessage(QString("ОШИБКА (%1): %2").arg(fileInfo.fileName())
                .arg(e.what()));
            ++failed;
        }

        emit progressChanged(i + 1, total);
    }

    //Сводный отчет result.json
    if (processed >0) {
        const std::string jsonPath =
            outputDir.toStdString() + "/result.json";
        std::ofstream jsonFile(jsonPath);
        if (jsonFile.is_open()) {
            jsonFile << rootResultArray.dump(4);
            emit logMessage(QString("JSON-отчет сохранен: %1").arg(QString::fromStdString(jsonPath)));
        }
        else {
            emit logMessage(QString("ОШИБКА: не удалось записать result.json"));
        }
    }

    //итоги
    emit logMessage(QString("Итог: успешно %1б с ошибками %2, всего %3.")
        .arg(processed)
        .arg(failed)
        .arg(total));

    emit finished();
    }
