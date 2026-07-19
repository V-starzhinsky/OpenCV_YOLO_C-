#include <iostream>
#include <opencv4/opencv2/opencv.hpp>
#include "Preprocessor.hpp"
#include "detector.hpp"
#include "visualizer.hpp"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

using namespace cv;
using namespace std;

int main() {

    //Пути к модели
    string  modelPath = TOSTRING(MODEL_PATH);
    string configPath = modelPath + "/yolov4-tiny.cfg";
    string weightsPath = modelPath + "/yolov4-tiny.weights";
    string classesPath = modelPath + "/coco.names";

    //===================================================

    //Создание модулей
    //Загрузка детектора
    Detector detector(configPath, weightsPath, classesPath,
                      0.2,0.4,416,416);

    /*/Загрузка Препроцессора (размер = размеру входа детектора)
    Preprocessor preprocessor(detector.getInputSize(),
                              true,             //CLAHE
                              false,        //Blur
                              5,               //Размер зерна
                              1.5,        // Сигма
                              true);       // Input is BGR флаг */

    //Загрузка Визуализатора
    Visualizer visualizer(
            Scalar(0, 255, 0),
            2,
            0.5,
            2,
            true,
            true,
            true,
            true
    );

   //preprocessor.printParams();

    //=====================================================

    const char windowName[] = "Detection";

    // Открытие камеры с проверкой
    VideoCapture inVid(0);
    if(!inVid.isOpened()){
        cerr << "Error! Camera is not ready! \n " << endl;
        return -1;
    }

    // Получение параметров камеры
    int width = (int)inVid.get(CAP_PROP_FRAME_WIDTH);
    int height = (int)inVid.get(CAP_PROP_FRAME_HEIGHT);
    double fps = inVid.get(CAP_PROP_FPS);
    if(fps <= 0 ) fps = 30;

    cout << "Camera: " << width << "x" << height << " @ " << fps << " FPS" << endl;

    //=========================================================

    // Настройка окна
    namedWindow(windowName, WINDOW_NORMAL); // WINDOW_NORMAL позволяет менять размер окна мышкой
    moveWindow(windowName, 500, 200);


    //==========================================================

    // Переменные
    Mat videoFrame;
    Mat processedFrame;
    Mat displayFrame;
    vector<Detection> detections;


    int frameCount = 0;
    bool paused = false;
    double currentFPS = 0;
    auto lastFpsUpdate = chrono::steady_clock::now();
    int fpsFrameCount = 0;

    //===========================================================

    // Основной цикл
    while(true){

        // Захват кадра
        inVid >> videoFrame;
        if(videoFrame.empty()){
            cerr << "Empty frame captured, skipping.." << endl;
            waitKey(10);
            continue;
        }


        //Препроцессинг ( Обработка кадра)
        //Измеряем время обработки
        auto start = chrono::steady_clock::now();
        processedFrame = videoFrame;
        // processedFrame = preprocessor.process(videoFrame);
        auto end = chrono::steady_clock::now();
        auto processingTime = chrono::duration_cast<chrono::milliseconds>
                (end - start).count();

        if(processedFrame.empty()){
            cerr << "Error: Processing failed!" << endl;
            continue;
        }



        //Детекция
        auto detectStart = chrono::steady_clock::now();
        detections = detector.detect(processedFrame);
        auto detectEnd = chrono::steady_clock::now();
        auto detectTime = chrono::duration_cast<chrono::milliseconds>
                (detectEnd - detectStart).count();

        //Рассчет FPS
        fpsFrameCount++;
        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>
                (now - lastFpsUpdate).count();
        if(elapsed >= 1000){
            currentFPS = fpsFrameCount * 1000.0 / elapsed;
            fpsFrameCount = 0;
            lastFpsUpdate = now;
        }

        //Визуализация
        //Копируем оригинальный кадр
        displayFrame = videoFrame.clone();
        //Рисуем объекты
        visualizer.draw(displayFrame,detections, currentFPS);


        // Статистика
        frameCount++;
        if (frameCount % 30 == 0) {
            cout << "Frame " << frameCount
                 << " | FPS: " << (int)currentFPS
                 //<< " | Preproc: " << processingTime << " ms"
                 << " | Detect: " << detectTime << " ms"
                 << " | Objects: " << detections.size() << endl;
        }
        //Отображение
        imshow(windowName,displayFrame);


        if (waitKey(1000/fps) >= 0) break;
    }
    inVid.release();
    destroyAllWindows();

    cout << "\nTotal frames processed: " << frameCount << endl;
    cout << "Program finished." << endl;

    return 0;
}
