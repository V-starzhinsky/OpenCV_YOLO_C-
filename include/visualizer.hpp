// visualizer.hpp
#pragma once

#include <vector>
#include <string>
#include <opencv2/opencv.hpp>
#include "detector.hpp"

class Visualizer final {
private:
    cv::Scalar boxColor;
    int boxThickness;
    double fontSize;
    int fontThickness;

    bool showConfidence;
    bool showClassNames;
    bool showFPS;
    bool showObjectCount;

public:
    Visualizer(
            cv::Scalar color = cv::Scalar(0, 255, 0),
            int thickness = 2,
            double fontSize = 0.5,
            int fontThickness = 2,
            bool showConf = true,
            bool showClass = true,
            bool showFPS = true,
            bool showCount = true
    );

    cv::Mat draw(
            cv::Mat& frame,
            const std::vector<Detection>& detections,
            double fps = 0.0
    );

    void drawInfoPanel(
            cv::Mat& frame,
            double fps,
            size_t objectCount
    );

    void drawDetection(
            cv::Mat& frame,
            const Detection& det
    );

    void setBoxColor(cv::Scalar color) { boxColor = color; }
    void setBoxThickness(int thickness) { boxThickness = thickness; }
    void setFontSize(double size) { fontSize = size; }
    void setShowConfidence(bool show) { showConfidence = show; }
    void setShowClassNames(bool show) { showClassNames = show; }
    void setShowFPS(bool show) { showFPS = show; }
    void setShowObjectCount(bool show) { showObjectCount = show; }
};