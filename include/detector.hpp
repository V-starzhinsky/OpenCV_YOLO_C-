// Detector.hpp
#pragma once
#include <opencv2/core.hpp>

#include <vector>
#include <string>
#include <fstream>
#
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>

#include "types.hpp"

class Detector final {
private:
    cv::dnn::Net net;
    std::vector<std::string> classes;

    float confidenceThreshold;
    float nmsThreshold;
    int inputWidth;
    int inputHeight;
    float scaleFactor;
    cv::Scalar meanValue;
    bool swapRB;

    void loadClasses(const std::string& classesPath);
    std::vector<Detection> applyNMS(
            const std::vector<cv::Rect>& boxes,
            const std::vector<float>& confidences,
            const std::vector<int>& classIds
    );

public:
    Detector(
            const std::string& configPath,
            const std::string& weightsPath,
            const std::string& classesPath,
            float confThreshold = 0.5,
            float nmsThreshold = 0.4,
            int width = 416,
            int height = 416
    );

    std::vector<Detection> detect(const cv::Mat& input);

    void setConfidenceThreshold(float threshold) { confidenceThreshold = threshold; }
    void setNMSThreshold(float threshold) { nmsThreshold = threshold; }
    float getConfidenceThreshold() const { return confidenceThreshold; }
    float getNMSThreshold() const { return nmsThreshold; }
    cv::Size getInputSize() const { return cv::Size(inputWidth, inputHeight); }


};