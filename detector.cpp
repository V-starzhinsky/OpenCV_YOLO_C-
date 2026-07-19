#include "detector.hpp"

#include <opencv2/opencv.hpp>
#include <string>

#include <iostream>     // Для cout, cerr



using namespace cv::dnn;


// Конструктор

Detector::Detector(
        const std::string& configPath,
        const std::string& weightsPath,
        const std::string& classesPath,
        float confThreshold,
        float nmsThreshold,
        int width,
        int height
)
        : confidenceThreshold(confThreshold),
          nmsThreshold(nmsThreshold),
          inputWidth(width),
          inputHeight(height),
          scaleFactor(1.0f / 255.0f),   // Нормализация: 0-255 → 0-1
          meanValue(0, 0, 0),           // Не вычитаем среднее
          swapRB(false)                  // BGR → RGB (YOLO ожидает RGB)
{

    // Загрука списка классов

    loadClasses(classesPath);
    std::cout << "[Detector] Loaded " << classes.size() << " classes from " << classesPath << std::endl;


    // ШАГ 2: ЗАГРУЗКА НЕЙРОСЕТИ

    try {
        // readNetFromDarknet - загружает YOLO модель
        //   configPath  - архитектура сети (yolov4-tiny.cfg)
        //   weightsPath - обученные веса (yolov4-tiny.weights)
        net = readNetFromDarknet(configPath, weightsPath);

        // Настройка бэкенда
        //   DNN_BACKEND_OPENCV - использует OpenCV (CPU)
        //   DNN_BACKEND_CUDA   - использует CUDA (GPU)
        net.setPreferableBackend(DNN_BACKEND_OPENCV);
        net.setPreferableTarget(DNN_TARGET_CPU);

        std::cout << "[Detector] Model loaded successfully" << std::endl;
        std::cout << "[Detector] Input size: " << inputWidth  << "x" << inputHeight << std::endl;
        std::cout << "[Detector] Confidence threshold: " << confidenceThreshold << std::endl;
        std::cout << "[Detector] NMS threshold: " << nmsThreshold << std::endl;

    } catch (const cv::Exception& e) {
        std::cerr << "[Detector] ERROR: Failed to load model!" << std::endl;
        std::cerr << "[Detector] " << e.what() << std::endl;
        throw;  // Пробрасываем исключение
    }
}


// ЗАГРУЗКА СПИСКА КЛАССОВ

void Detector::loadClasses(const std::string& classesPath) {
    std::ifstream file(classesPath);

    if (!file.is_open()) {
        std::cerr << "[Detector] ERROR: Cannot open classes file: " << classesPath << std::endl;
        return;
    }

    std::string className;
    while (getline(file, className)) {
        // Удаляем символ возврата каретки (для Windows)
        if (!className.empty() && className.back() == '\r') {
            className.pop_back();
        }
        classes.push_back(className);
    }

    file.close();

}


// ПРИМЕНЕНИЕ NMS

std::vector<Detection> Detector::applyNMS(
        const std::vector<cv::Rect_<int>>& boxes,
        const std::vector<float>& confidences,
        const std::vector<int>& classIds
) {
    std::vector<Detection> detections;


    // ЗАПУСК NMS

    // NMSBoxes - встроенная функция OpenCV
    // Параметры:
    //   boxes        - все прямоугольники
    //   confidences  - уверенность для каждого
    //   score_threshold - минимальная уверенность
    //   nms_threshold   - порог пересечения (IoU)
    //   indices      - выходные индексы оставленных рамок

    std::vector<int> indices;
    NMSBoxes(boxes, confidences, confidenceThreshold, nmsThreshold, indices);


    // ФОРМИРОВАНИЕ ВЕКТОРА DETECTION

    for (size_t i = 0; i < indices.size(); ++i) {
        int idx = indices[i];

        Detection det;
        det.bbox = boxes[idx];
        det.confidence = confidences[idx];
        det.classId = classIds[idx];
        det.className = classes[det.classId];

        detections.push_back(det);
    }

    return detections;
}

// ОСНОВНОЙ МЕТОД ДЕТЕКЦИИ

std::vector<Detection> Detector::detect(const cv::Mat& input) {

    // ПРОВЕРКА ВХОДНЫХ ДАННЫХ

    if (input.empty()) {
        std::cerr << "[Detector] ERROR: Empty input image!" << std::endl;
        return std::vector<Detection>();
    }


    // ДИАГНОСТИКА: РАЗМЕР И ТИП ВХОДА

  //  std::cout << "[Detector] Input: " << input.cols << "x" << input.rows
      //        << ", channels: " << input.channels()
      //        << ", type: " << input.type() << std::endl;


    // СОЗДАНИЕ BLOB

    cv::Mat blob = blobFromImage(
            input,
            scaleFactor,
            cv::Size(inputWidth, inputHeight),
            meanValue,
            false,
            false
    );

  //  std::cout << "[Detector] Blob created: " << blob.size[2] << "x" << blob.size[3]
     //         << ", channels: " << blob.size[1] << std::endl;


    // ЗАПУСК НЕЙРОСЕТИ

    net.setInput(blob);

    std::vector<std::string> outputNames = net.getUnconnectedOutLayersNames();
   // std::cout << "[Detector] Output layers: " << outputNames.size() << std::endl;
    for (size_t i = 0; i < outputNames.size(); ++i) {
        std::cout << "  - " << outputNames[i] << std::endl;
    }

    std::vector<cv::Mat> outputs;
    try {
        net.forward(outputs, outputNames);

       // std::cout << "[Detector] Forward pass completed, outputs: " << outputs.size() << std::endl;
    } catch (const cv::Exception &e) {
        std::cerr << "[Detector] ERROR in forward pass: " << e.what() << std::endl;
        return std::vector<Detection>();
    }


    //  ПРОВЕРКА ВЫХОДНЫХ ДАННЫХ

    int totalDetections = 0;
    for (size_t i = 0; i < outputs.size(); ++i) {
        totalDetections += outputs[i].rows;
      //  std::cout << "[Detector] Output " << i << ": rows=" << outputs[i].rows
      //            << ", cols=" << outputs[i].cols << std::endl;
    }
   // std::cout << "[Detector] Total raw detections: " << totalDetections << std::endl;

    if (totalDetections == 0) {
        std::cout << "[Detector] No detections from neural network!" << std::endl;
        return std::vector<Detection>();
    }


    // ИЗВЛЕЧЕНИЕ РЕЗУЛЬТАТОВ

    std::vector<cv::Rect> boxes;
    std::vector<float> confidences;
    std::vector<int> classIds;

    int originalWidth = input.cols;
    int originalHeight = input.rows;
    float xScale = (float) originalWidth / inputWidth;
    float yScale = (float) originalHeight / inputHeight;

   // std::cout << "[Detector] Scale: x=" << xScale << ", y=" << yScale << std::endl;

    int filteredByConfidence = 0;
    int filteredByClass = 0;

    for (size_t i = 0; i < outputs.size(); ++i) {
        const cv::Mat &output = outputs[i];

        for (int j = 0; j < output.rows; ++j) {
            float *data = (float *) output.ptr(j);

            // 1. Сначала извлекаем скоры классов (индексы 5...84)
            float *classScores = data + 5;
            cv::Mat scores(1, output.cols - 5, CV_32FC1, classScores);
            cv::Point classIdPoint;
            double maxClassScore;
            minMaxLoc(scores, 0, &maxClassScore, 0, &classIdPoint);

            // 2. Считаем НАСТОЯЩИЙ confidence по формуле YOLO: Objectness * ClassScore
            float objectness = data[4];
            float finalConfidence = objectness * (float) maxClassScore;

            /*/ ← ДИАГНОСТИКА
            if (j < 5) {
               std::cout << "[Detector] Sample " << j << ": final_conf=" << finalConfidence
                         << " (obj=" << objectness << ", class_score=" << maxClassScore << ")" << std::endl;
            }*/

            // Фильтруем один раз по правильному значению
            if (finalConfidence < confidenceThreshold) {
                filteredByConfidence++;
                continue;
            }

            int classId = classIdPoint.x;


         //   std::cout << "[DEBUG_COORDS] raw_cx: " << data[0] << ", raw_w: " << data[2] << " | orig_W: " << originalWidth << std::endl;

            // 1. Извлекаем нормализованные координаты (они от 0.0 до 1.0)
            float norm_cx = data[0];
            float norm_cy = data[1];
            float norm_w  = data[2];
            float norm_h  = data[3];

            // 2. Считаем левый верхний угол в относительных координатах
            float norm_left = norm_cx - (norm_w / 2.0f);
            float norm_top  = norm_cy - (norm_h / 2.0f);

            // 3. Умножаем на размер картинки, которую получил детектор
            int real_x = static_cast<int>(norm_left * originalWidth);
            int real_y = static_cast<int>(norm_top * originalHeight);
            int real_w = static_cast<int>(norm_w * originalWidth);
            int real_h = static_cast<int>(norm_h * originalHeight);

            // 4. Ограничиваем рамки в пределах кадра
            cv::Rect box;
            box.x = cv::max(0, cv::min(real_x, originalWidth - 1));
            box.y = cv::max(0, cv::min(real_y, originalHeight - 1));
            box.width = cv::max(1, cv::min(real_w, originalWidth - box.x));
            box.height = cv::max(1, cv::min(real_h, originalHeight - box.y));

            boxes.push_back(box);
            confidences.push_back(finalConfidence);
            classIds.push_back(classId);


        }

            // std::cout << "[Detector] Filtered by confidence: " << filteredByConfidence << std::endl;
            //   std::cout << "[Detector] Filtered by class score: " << filteredByClass << std::endl;
            //  std::cout << "[Detector] Boxes before NMS: " << boxes.size() << std::endl;;
        if (boxes.size() == 0) {
       //     std::cout << "[Detector] No objects passed confidence threshold ("
                  //    << confidenceThreshold << ")" << std::endl;
            return std::vector<Detection>();
        }


        // NMS

        std::vector<int> indices;
        NMSBoxes(boxes, confidences, confidenceThreshold, nmsThreshold, indices);

        std::cout << "[Detector] Boxes after NMS: " << indices.size() << std::endl;


        // ФОРМИРОВАНИЕ РЕЗУЛЬТАТА
        //
        std::vector<Detection> detections;

        for (size_t i = 0; i < indices.size(); ++i) {
            int idx = indices[i];

            Detection det;
            det.bbox = boxes[idx];
            det.confidence = confidences[idx];
            det.classId = classIds[idx];

            if (det.classId >= 0 && det.classId < (int) classes.size()) {
                det.className = classes[det.classId];
            } else {
                det.className = "unknown";
            }

            detections.push_back(det);

            //  ДИАГНОСТИКА: ПЕРВЫЕ НАЙДЕННЫЕ ОБЪЕКТЫ
            if (i < 5) {
                std::cout << "[Detector] Object " << i << ": " << det.className
                          << " (" << det.confidence << ") at ["
                          << det.bbox.x << "," << det.bbox.y << ","
                          << det.bbox.width << "," << det.bbox.height << "]" << std::endl;
            }
        }

        std::cout << "[Detector] FINAL detections: " << detections.size() << std::endl;

        return detections;
    }
}