#pragma once
// 1. Сначала подключаем базовый C API
#include <onnxruntime_c_api.h>

#include <onnxruntime_cxx_api.h> // Заголовок ONNX Runtime C++
#include <dml_provider_factory.h> // Заголовок DirectML для AMD Radeon

#include "types.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

class Detector {
public:
    Detector();
    std::vector<Detection> detect(const cv::Mat& input);

private:
    // Объекты окружения и сессии ONNX Runtime
    Ort::Env env;
    std::unique_ptr<Ort::Session> session;

    // Параметры модели
    int inputWidth = 416;
    int inputHeight = 416;
    float confidenceThreshold = 0.25f;
    float nmsThreshold = 0.4f;

    std::vector<std::string> classes;

    // ИСПРАВЛЕНО: Используем std::string для безопасного хранения строк
    std::vector<std::string> inputNames;
    std::vector<std::string> outputNames;

    void loadClassNames(const std::string& path);
};
