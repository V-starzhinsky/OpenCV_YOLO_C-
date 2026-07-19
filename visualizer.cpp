// visualizer.cpp
#include <opencv2/opencv.hpp>
#include "visualizer.hpp"

#include "detector.hpp"

#include <vector>
#include <string>
#include <cstdio>


using namespace cv;

// ============================================================
// КОНСТРУКТОР
// ============================================================
Visualizer::Visualizer(
        Scalar color,
        int thickness,
        double fontSize,
        int fontThickness,
        bool showConf,
        bool showClass,
        bool showFPS,
        bool showCount
)
        : boxColor(color),
          boxThickness(thickness),
          fontSize(fontSize),
          fontThickness(fontThickness),
          showConfidence(showConf),
          showClassNames(showClass),
          showFPS(showFPS),
          showObjectCount(showCount)
{
}

// ============================================================
// ОСНОВНОЙ МЕТОД (БЕЗ RANGE-BASED FOR)
// ============================================================
Mat Visualizer::draw(
        Mat& frame,
        const std::vector<Detection>& detections,
        double fps
) {
    // ← ОБЫЧНЫЙ ЦИКЛ (НЕ RANGE-BASED)
    for (size_t i = 0; i < detections.size(); ++i) {
        const Detection& det = detections[i];
        drawDetection(frame, det);
    }

    if (showFPS || showObjectCount) {
        drawInfoPanel(frame, fps, detections.size());
    }

    return frame;
}

// ============================================================
// РИСОВАНИЕ ОДНОГО ОБЪЕКТА
// ============================================================
void Visualizer::drawDetection(
        Mat& frame,
        const Detection& det
) {
    rectangle(
            frame,
            det.bbox,
            boxColor,
            boxThickness
    );

    std::string label = "";

    if (showClassNames) {
        label += det.className;
    }

    if (showConfidence) {
        if (!label.empty()) label += " ";
        char buffer[10];
        sprintf(buffer, "%.2f", det.confidence);
        label += buffer;
    }

    if (!label.empty()) {
        Point textPos(
                det.bbox.x,
                det.bbox.y - 5
        );

        if (textPos.y < 0) {
            textPos.y = det.bbox.y + 15;
        }

        putText(
                frame,
                label,
                textPos,
                FONT_HERSHEY_SIMPLEX,
                fontSize,
                boxColor,
                fontThickness
        );
    }
}

// ============================================================
// ИНФОРМАЦИОННАЯ ПАНЕЛЬ
// ============================================================
void Visualizer::drawInfoPanel(
        Mat& frame,
        double fps,
        size_t objectCount
) {
    int x = 10;
    int y = 30;
    int lineHeight = 25;
    int startY = y;

    int panelWidth = 200;
    int panelHeight = 0;

    if (showFPS) panelHeight += lineHeight;
    if (showObjectCount) panelHeight += lineHeight;
    panelHeight += 10;

    rectangle(
            frame,
            Point(x - 5, startY - 20),
            Point(x + panelWidth, startY + panelHeight),
            Scalar(0, 0, 0),
            FILLED
    );

    rectangle(
            frame,
            Point(x - 5, startY - 20),
            Point(x + panelWidth, startY + panelHeight),
            Scalar(50, 50, 50),
            FILLED
    );

    if (showFPS) {
        std::string fpsText = "FPS: " + std::to_string((int)fps);
        putText(
                frame,
                fpsText,
                Point(x, y),
                FONT_HERSHEY_SIMPLEX,
                0.6,
                Scalar(0, 255, 0),
                2
        );
        y += lineHeight;
    }

    if (showObjectCount) {
        std::string countText = "Objects: " + std::to_string(objectCount);
        putText(
                frame,
                countText,
                Point(x, y),
                FONT_HERSHEY_SIMPLEX,
                0.6,
                Scalar(0, 255, 255),
                2
        );
        y += lineHeight;
    }
}