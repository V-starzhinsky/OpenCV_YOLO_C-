#define NOMINMAX // Запрещает Windows ломать функции min и max

#include "include/detector.hpp"
#include <fstream>
#include <iostream>
#include <cmath>

Detector::Detector() : env(ORT_LOGGING_LEVEL_WARNING, "YOLOv11-DirectML") {
    // 1. Имена классов (теперь у нас 9 классов дефектов вместо 80 классов COCO)
    // Укажите путь к новому текстовому файлу, где построчно записаны ваши дефекты Short, Open и т.д.
    loadClassNames("C:/Users/ponka/CLionProjects/OpenCV_YOLO_CUDA/model/pcb_classes.txt", classesCopper);
    loadClassNames("C:/Users/ponka/CLionProjects/OpenCV_YOLO_CUDA/model/pcb_classes_green.txt", classesGreen);

    // 2. Настраиваем опции сессии и DirectML для работы на видеокарте AMD
    Ort::SessionOptions sessionOptions;
    sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    Ort::Status status(OrtSessionOptionsAppendExecutionProvider_DML(sessionOptions, 0));
    if (!status.IsOK()) {
        std::cerr << "[Detector] Ошибка подключения DirectML! Откат на CPU. Сообщение: "
                  << status.GetErrorMessage() << std::endl;
    } else {
        std::cout << "[Detector] Аппаратное ускорение DirectML (AMD GPU) подключено." << std::endl;
    }

    // 3. Загружаем вашу новую модель YOLOv11 ONNX (которую мы экспортировали с imgsz=320)
    std::wstring pathCopper = L"C:/Users/ponka/CLionProjects/OpenCV_YOLO_CUDA/model/best_copper.onnx";
    std::wstring pathGreen = L"C:/Users/ponka/CLionProjects/OpenCV_YOLO_CUDA/model/best_green.onnx";

    sessionCopper = std::make_unique<Ort::Session>(env, pathCopper.c_str(), sessionOptions);
    sessionGreen = std::make_unique<Ort::Session>(env, pathGreen.c_str(), sessionOptions);

    // ВАЖНО: В YOLOv11 имя входного слоя стандартное, а выходной слой ВСЕГО ОДИН.
    inputNames = { "images" };
    outputNames = { "output0" };

    // Убедитесь, что эти константы объявлены в вашем detector.hpp:
    inputWidth = 320;   // Меняем 416 на 320 под наше обучение
    inputHeight = 320;

    std::cout << "[Detector] Модель YOLOv11 ONNX успешно загружена!" << std::endl;
}

void Detector::loadClassNames(const std::string& path, std::vector<std::string>& targetVector) {
    targetVector.clear();
    std::ifstream ifs(path);
    std::string line;
    while (std::getline(ifs, line)) {
        if (!line.empty()) targetVector.push_back(line);
    }
}

std::vector<Detection> Detector::detect(const cv::Mat& input) {
    if (input.empty()) return {};

    int originalWidth = input.cols;
    int originalHeight = input.rows;

    //определение цвета платы
    cv::Mat hsv;
    cv::cvtColor(input, hsv, cv::COLOR_BGR2HSV);

    //создаем маску и проверяем пиксели , еси его HSV попадает в границы скаляра, то он становится белым
    //если нет - черным
    cv::Mat mask;
    cv::Scalar lowerGreen (35, 40, 40);
    cv::Scalar upperGreen(85, 255, 255);
    cv::inRange(hsv, lowerGreen, upperGreen, mask);

    //считаем только белыее пиксели в одноканальной маске
    double greenPixels = cv::countNonZero(mask);
    double totalPixels = originalWidth * originalHeight;
    double greenPercentage = (greenPixels/totalPixels) * 100.0;

    //Свитчер указателей на сессии
    Ort::Session* activeSession = nullptr;
    const std::vector<std::string>* activeClasses = nullptr;

    //если > 15% изображения зеленый цвет  выбираем "зеленую" сессию
    if(greenPercentage > 15.0){
        activeSession = sessionGreen.get();
        activeClasses = &classesGreen;
        std::cout << "[DML] Выбрана модель: ЗЕЛЕНАЯ ПЛАТА -> ";
    }
    else{
        activeSession = sessionCopper.get();
        activeClasses = &classesCopper;
        std::cout << "[DML] Выбрана модель: МЕДНАЯ ПЛАТА -> ";
    }

    // 1. Препроцессинг через OpenCV (YOLOv11 требует нормализацию 1/255 и формат RGB (swapRB=true))
    cv::Mat blob = cv::dnn::blobFromImage(input, 1.0 / 255.0, cv::Size(inputWidth, inputHeight), cv::Scalar(), true, false);

    std::vector<float> inputData;
    inputData.assign(blob.ptr<float>(), blob.ptr<float>() + blob.total());

    // 2. Создаем структуру тензора для ONNX Runtime
    std::vector<int64_t> inputDims = {1, 3, inputHeight, inputWidth};
    auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);

    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo,
            inputData.data(),
            inputData.size(),
            inputDims.data(),
            inputDims.size()
    );

    std::vector<const char*> inputNamesPtr = { inputNames[0].c_str() };
    std::vector<const char*> outputNamesPtr = { outputNames[0].c_str() };

    std::vector<Ort::Value> outputTensors;
    outputTensors.push_back(Ort::Value{nullptr}); // Выход всего один

    // 3. Запуск инференса
    try {
        activeSession->Run(
                Ort::RunOptions{nullptr},
                inputNamesPtr.data(), &inputTensor, 1,
                outputNamesPtr.data(), outputTensors.data(), 1
        );
    } catch (const std::exception& e) {
        std::cerr << "[Detector] Ошибка во время выполнения Run(): " << e.what() << std::endl;
        return {};
    }

    // 4. НОВЫЙ Постпроцессинг для YOLOv11
    std::vector<cv::Rect> boxes;
    std::vector<float> confidences;
    std::vector<int> classIds;

    // Извлекаем указатель на сырые float данные выхода
    float* rawOutput = outputTensors[0].GetTensorMutableData<float>();
    auto tensorInfo = outputTensors[0].GetTensorTypeAndShapeInfo();
    auto outputShape = tensorInfo.GetShape(); // Формат формы: [1, 13, 2100]

    // Выходная матрица YOLOv11 имеет структуру [1, (4_координаты + N_классов), Кол-во_анкеров]
    // Для нашего датасета: 4 + 9 классов дефектов = 13 строк. 2100 колонок (для размера 320x320)
    int rows = static_cast<int>(outputShape[1]); // 13
    int columns = static_cast<int>(outputShape[2]); // 2100
    int numClasses = rows - 4; // 9 классов дефектов

    // Для удобства работы в OpenCV оборачиваем указатель в cv::Mat. Размеры матрицы: 13 x 2100
    cv::Mat outputMatrix(rows, columns, CV_32F, rawOutput);

    // В YOLOv11 данные идут по столбцам, нам нужно транспонировать матрицу,
    // чтобы получить структуру: 2100 строк и 13 столбцов (каждая строка — один объект)
    cv::Mat transposedMatrix = outputMatrix.t();

    // Коэффициенты масштабирования из размера сети (320) в размер исходной картинки
    float scaleX = static_cast<float>(originalWidth) / inputWidth;
    float scaleY = static_cast<float>(originalHeight) / inputHeight;

    // Проходим циклом по всем 2100 потенциальным рамкам
    for (int i = 0; i < transposedMatrix.rows; ++i) {
        float* data = transposedMatrix.ptr<float>(i);

        // В YOLOv11 вероятности классов начинаются строго после первых 4 координат (индексы с 4 по 12)
        float* classesScores = data + 4;

        // Быстро находим индекс лучшего класса с помощью OpenCV аналога std::max_element
        cv::Mat scoresMat(1, numClasses, CV_32F, classesScores);
        double maxClassScore;
        cv::Point classIdPoint;
        cv::minMaxLoc(scoresMat, nullptr, &maxClassScore, nullptr, &classIdPoint);

        float finalConfidence = static_cast<float>(maxClassScore);

        // Отсекаем мусорные рамки по порогу уверенности (например, 0.25)
        if (finalConfidence > confidenceThreshold) {
            // YOLOv11 отдает координаты центра, ширины и высоты в абсолютных пикселях шкалы модели (320)
            float cx = data[0];
            float cy = data[1];
            float w = data[2];
            float h = data[3];

            // Переводим координаты в пиксели оригинального разрешения платы и смещаем центр к верхнему левому углу
            int real_w = static_cast<int>(w * scaleX);
            int real_h = static_cast<int>(h * scaleY);
            int real_x = static_cast<int>((cx - w / 2.0f) * scaleX);
            int real_y = static_cast<int>((cy - h / 2.0f) * scaleY);

            // Ограничиваем рамки рамками картинки, чтобы не выйти за границы std::vector / cv::Mat
            cv::Rect box;
            box.x = cv::max(0, cv::min(real_x, originalWidth - 1));
            box.y = cv::max(0, cv::min(real_y, originalHeight - 1));
            box.width = cv::max(1, cv::min(real_w, originalWidth - box.x));
            box.height = cv::max(1, cv::min(real_h, originalHeight - box.y));

            boxes.push_back(box);
            confidences.push_back(finalConfidence);
            classIds.push_back(classIdPoint.x);
        }
    }

    // 5. Применяем NMS (Non-Maximum Suppression) для удаления дублирующих перекрывающихся рамок
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, confidenceThreshold, nmsThreshold, indices);

    // 6. Формируем финальный вектор детекций дефектов для отрисовки
    std::vector<Detection> detections;
    for (size_t i = 0; i < indices.size(); ++i) {
        int idx = indices[i];
        Detection det;
        det.bbox = boxes[idx];
        det.confidence = confidences[idx];
        det.classId = classIds[idx];
        det.className = (det.classId >= 0 && det.classId < (int)activeClasses->size()) ? (*activeClasses)[det.classId] : "unknown";
        detections.push_back(det);
    }

    return detections;
}
