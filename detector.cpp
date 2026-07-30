#define __builtin_verbose_trap(category, reason) ((void)0)
#define NOMINMAX // Запрещает Windows ломать функции min и max

#include "include/detector.hpp"
#include <fstream>
#include <iostream>
#include <cmath>

Detector::Detector() : env(ORT_LOGGING_LEVEL_WARNING, "YOLOv4-Tiny-DML") {
    // 1. Загружаем имена классов COCO
    loadClassNames("C:/Users/ponka/CLionProjects/OpenCV_YOLO_CUDA/model/coco.names");

    // 2. Настраиваем опции сессии и включаем аппаратное ускорение DirectML (AMD Radeon)
    Ort::SessionOptions sessionOptions;
    sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    // 0 — индекс видеокарты AMD в системе. Подключаем DirectML.
    Ort::Status status(OrtSessionOptionsAppendExecutionProvider_DML(sessionOptions, 0));

    // Проверяем успешность подключения DirectML
    if (!status.IsOK()) {
        std::cerr << "[Detector] Ошибка подключения DirectML! Откат на CPU. Сообщение: "
                  << status.GetErrorMessage() << std::endl;
    } else {
        std::cout << "[Detector] Аппаратное ускорение DirectML успешно подключено." << std::endl;
    }

    // 3. Загружаем саму модель ONNX
    std::wstring modelPath = L"C:/Users/ponka/CLionProjects/OpenCV_YOLO_CUDA/model/yolov4-tiny.onnx";
    session = std::make_unique<Ort::Session>(env, modelPath.c_str(), sessionOptions);

    // Точные имена входных и выходных слоев вашей модели (сохраняются как std::string)
    inputNames = { "input_1" };
    outputNames = { "conv2d_17", "conv2d_20" };

    std::cout << "[Detector] Модель ONNX успешно загружена через DirectML на GPU!" << std::endl;
}

void Detector::loadClassNames(const std::string& path) {
    std::ifstream ifs(path);
    std::string line;
    while (std::getline(ifs, line)) {
        if (!line.empty()) classes.push_back(line);
    }
}

std::vector<Detection> Detector::detect(const cv::Mat& input) {
    if (input.empty()) return {};

    int originalWidth = input.cols;
    int originalHeight = input.rows;

    // 1. Препроцессинг через OpenCV (YOLO требует нормализацию 1/255 и RGB)
    cv::Mat blob = cv::dnn::blobFromImage(input, 1.0 / 255.0, cv::Size(inputWidth, inputHeight), cv::Scalar(), true, false);

    std::vector<float> inputData;
    inputData.assign(blob.ptr<float>(), blob.ptr<float>() + blob.total());

    // 2. Создаем структуру памяти тензора для ONNX Runtime
    std::vector<int64_t> inputDims = {1, 3, inputHeight, inputWidth};
    auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);

    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo,
            inputData.data(),
            inputData.size(),
            inputDims.data(),
            inputDims.size()
    );

    size_t numOutputs = session->GetOutputCount();

    // Безопасно преобразуем векторы строк во временные массивы указателей const char* для API ONNX Runtime
    std::vector<const char*> inputNamesPtr;
    for (const auto& name : inputNames) {
        inputNamesPtr.push_back(name.c_str());
    }

    std::vector<const char*> outputNamesPtr;
    for (const auto& name : outputNames) {
        outputNamesPtr.push_back(name.c_str());
    }

    // Создаем вектор пустых Ort::Value (аллокацию памяти под выходы берет на себя ONNX Runtime)
    std::vector<Ort::Value> outputTensors;
    for (size_t i = 0; i < numOutputs; ++i) {
        outputTensors.push_back(Ort::Value{nullptr});
    }

    // 3. Запускаем инференс
    try {
        session->Run(
                Ort::RunOptions{nullptr},
                inputNamesPtr.data(), &inputTensor, 1,
                outputNamesPtr.data(), outputTensors.data(), numOutputs
        );
    } catch (const std::exception& e) {
        std::cerr << "[Detector] Ошибка во время выполнения Run(): " << e.what() << std::endl;
        return {};
    }

    // 4. Постпроцессинг (разбор геометрии под формат NCHW [1, 255, H, W])
    std::vector<cv::Rect> boxes;
    std::vector<float> confidences;
    std::vector<int> classIds;

    const int numAnchorsPerLayer = 3;
    const int numClasses = 80;
    const int elementsPerAnchor = 5 + numClasses; // 85

    // Стандартные анкоры для YOLOv4-Tiny (базовые эталонные размеры коробок)
    // Разделены на два выходных слоя по правилам архитектуры
    const float anchors[2][3][2] = {
            { {81, 82},  {135, 169}, {344, 319} },  // Для сетки 13x13 (conv2d_17)
            { {10, 14},  {23, 27},   {37, 58} }     // Для сетки 26x26 (conv2d_20)
    };

    for (size_t t = 0; t < numOutputs; ++t) {
        float* rawOutput = outputTensors[t].GetTensorMutableData<float>();
        auto tensorInfo = outputTensors[t].GetTensorTypeAndShapeInfo();
        auto outputShape = tensorInfo.GetShape();

        int gridH = static_cast<int>(outputShape[2]); // 13 или 26
        int gridW = static_cast<int>(outputShape[3]); // 13 или 26
        int gridSize = gridH * gridW;

        // Вычисляем шаг сетки относительно входного размера сети (416x416)
        float strideX = static_cast<float>(inputWidth) / gridW;
        float strideY = static_cast<float>(inputHeight) / gridH;

        for (int a = 0; a < numAnchorsPerLayer; ++a) {
            int anchorOffset = a * elementsPerAnchor;

            for (int q = 0; q < gridH; ++q) {
                for (int p = 0; p < gridW; ++p) {
                    int cellIndex = q * gridW + p;

                    int objChannel = anchorOffset + 4;
                    float objectness = rawOutput[objChannel * gridSize + cellIndex];
                    objectness = 1.0f / (1.0f + std::exp(-objectness)); // Сигмоида

                    if (objectness < confidenceThreshold) continue;

                    // Поиск лучшего класса
                    float maxClassScore = 0.0f;
                    int classId = -1;
                    for (int c = 0; c < numClasses; ++c) {
                        int classChannel = anchorOffset + 5 + c;
                        float score = rawOutput[classChannel * gridSize + cellIndex];
                        score = 1.0f / (1.0f + std::exp(-score)); // Сигмоида

                        if (score > maxClassScore) {
                            maxClassScore = score;
                            classId = c;
                        }
                    }

                    float finalConfidence = objectness * maxClassScore;

                    if (finalConfidence > confidenceThreshold) {
                        float raw_x = rawOutput[(anchorOffset + 0) * gridSize + cellIndex];
                        float raw_y = rawOutput[(anchorOffset + 1) * gridSize + cellIndex];
                        float raw_w = rawOutput[(anchorOffset + 2) * gridSize + cellIndex];
                        float raw_h = rawOutput[(anchorOffset + 3) * gridSize + cellIndex];

                        // Математически корректное восстановление центра бокса на входном разрешении 416x416
                        float cx = (1.0f / (1.0f + std::exp(-raw_x)) + p) * strideX;
                        float cy = (1.0f / (1.0f + std::exp(-raw_y)) + q) * strideY;

                        // Восстановление размеров w и h с использованием логарифмических анкоров
                        float anchorW = anchors[t][a][0];
                        float anchorH = anchors[t][a][1];
                        float nw = std::exp(raw_w) * anchorW;
                        float nh = std::exp(raw_h) * anchorH;

                        // Перевод координат из масштаба нейросети (416) в масштаб экрана веб-камеры
                        float scaleX = static_cast<float>(originalWidth) / inputWidth;
                        float scaleY = static_cast<float>(originalHeight) / inputHeight;

                        int real_w = static_cast<int>(nw * scaleX);
                        int real_h = static_cast<int>(nh * scaleY);
                        int real_x = static_cast<int>((cx - nw / 2.0f) * scaleX);
                        int real_y = static_cast<int>((cy - nh / 2.0f) * scaleY);

                        cv::Rect box;
                        box.x = cv::max(0, cv::min(real_x, originalWidth - 1));
                        box.y = cv::max(0, cv::min(real_y, originalHeight - 1));
                        box.width = cv::max(1, cv::min(real_w, originalWidth - box.x));
                        box.height = cv::max(1, cv::min(real_h, originalHeight - box.y));

                        boxes.push_back(box);
                        confidences.push_back(finalConfidence);
                        classIds.push_back(classId);
                    }
                }
            }
        }
    }

    // 5. Применяем NMS (Non-Maximum Suppression) для удаления дубликатов рамок
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, confidenceThreshold, nmsThreshold, indices);

    // 6. Формируем финальный вектор детекций с исправленными индексами NMS
    std::vector<Detection> detections;
    for (size_t i = 0; i < indices.size(); ++i) {
        int idx = indices[i];
        Detection det;
        det.bbox = boxes[idx];
        det.confidence = confidences[idx];
        det.classId = classIds[idx];
        det.className = (det.classId >= 0 && det.classId < (int)classes.size()) ? classes[det.classId] : "unknown";
        detections.push_back(det);
    }

    return detections;
}
