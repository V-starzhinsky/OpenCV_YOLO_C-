#include "VideoStreamer.hpp"
#include <iostream>
#include <opencv2/opencv.hpp>

VideoStreamer::VideoStreamer() : isRunning(false){}

VideoStreamer::~VideoStreamer() {
    stop();
}

bool VideoStreamer::open(int cameraIndex) {
    return cap.open(cameraIndex);
}

bool VideoStreamer::open(const std::string &videoPath) {
    return cap.open(videoPath);
}

void VideoStreamer::stop() {
    isRunning = false;
    if(cap.isOpened()){
        cap.release(); // ОСвобождаем камеру\файл
        std::cout << "[Streamer] Видеопоток успешно останвлен и освобожден" << std::endl;
    }
}

void VideoStreamer::startCaptureLoop(VideoStreamer::FrameCallBack onFrameReceived) {
    if(!cap.isOpened()){
        std::cerr << "[Streamer] КРИТИЧЕСКАЯ ОШИБКА! Попытка запустить цикл без открытого источника!" << std::endl;
    }

    isRunning = true;
    cv::Mat frame;

    while(isRunning){
        // Захватываем сырой кадр из источника
        cap >> frame;

        //Проверяем на конец видеофайла или отключение камеры
        if(frame.empty()){
            std::cout << "[Streamer] Получен пустой кадр, завершение работы" << std::endl;
            break;
        }

        onFrameReceived(frame);

        if (cv::waitKey(1) == 27){
            isRunning = false;
        }
    }

    stop();
}