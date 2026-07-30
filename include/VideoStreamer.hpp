#pragma once

#include <opencv2/opencv.hpp>
#include <functional>
#include <string>

class VideoStreamer {
public:
    using FrameCallBack = std::function<void(const cv::Mat&)>;

private:
    cv::VideoCapture cap; // Объект захвата видео OpenCV
    bool isRunning;       //Флаг управления циклом

public:
    VideoStreamer();
    ~VideoStreamer();

    // Открытие из разных источников данных
    bool open(int cameraIndex);
    bool open(const std::string& videoPath);

    // Метод остановки стрима
    void stop();

    void startCaptureLoop(FrameCallBack onFrameReceived);

};