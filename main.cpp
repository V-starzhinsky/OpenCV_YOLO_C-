
#include <iostream>
#include <opencv2/opencv.hpp>
#include <windows.h>

#include "detector.hpp"
#include "visualizer.hpp"
#include "Preprocessor.hpp"
#include "FpsCounter.hpp"
#include "VideoStreamer.hpp"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

using namespace cv;
using namespace std;

//Пути к модели
std::string  modelPath = TOSTRING(MODEL_PATH);
std::string configPath = modelPath + "/yolov4-tiny.cfg";
std::string weightsPath = modelPath + "/yolov4-tiny.weights";
std::string classesPath = modelPath + "/coco.names";


// глобальная инициализация компонентов чтобы call-back функция имела к ним доступ
Detector& getDetector() {
    static Detector detector; // Инициализируется только при первом вызове функции
    return detector;
}

Visualizer& getVisualizer() {
    static Visualizer visualizer(
            Scalar(0, 255, 0),
            2,
            0.5,
            2,
            true,
            true,
            true,
            true
    );
    return visualizer;
}

FpsCounter& getFpsCounter() {
    static FpsCounter fpsCounter;
    return fpsCounter;
}
// streamer вызывает эту функцию на каждый кадр
void processFrame(const cv::Mat& frame){
    if (frame.empty()) return;

    // Делаем глубокую копию кадра в CPU память
    cv::Mat CPU_frame;
    frame.copyTo(CPU_frame);

    // Отправляем изолированный кадр в нейросеть
    std::vector<Detection> detections = getDetector().detect(CPU_frame);

    // Сигнализируем счетчику FPS, что обработан еще один кадр
    getFpsCounter().tick();

    // Создаем копию кадра для отрисовки интерфейса
    cv::Mat displayFrame = CPU_frame.clone();

    // Извлекаем посчитанный FPS из объекта счетчика
    double currentFpsValue = getFpsCounter().getValue(); // Или getFpsCounter().getFpsValue(), проверьте имя геттера в вашем hpp

    // Передаем значение FPS третьим аргументом в визуализатор
    getVisualizer().draw(displayFrame, detections, currentFpsValue);


    // Вывод на экран
    cv::imshow("YOLOv4 Vulkan Aplication", displayFrame);
}

// ================================================================================


int main(){

    // Принудительно переключаем вывод консоли в UTF-8
    SetConsoleOutputCP(CP_UTF8);

    std::cout << "[Main] Инициализация модулей программы..." << std::endl;


    VideoStreamer streamer;
    if(!streamer.open(0)){
        std::cerr << "[Main] WARNING! CRITICAL ERROR: WEB-cam not founf!" << std::endl;
        return -1;
    }

    std::cout << "[Main] Camera succesfull opened.Pass the processing function.." << std::endl;

    // Запускаем бесконечный цикл захвата
    streamer.startCaptureLoop(processFrame);

    std::cout << "[Main] Programm is succesfull finish work" << std::endl;
    return 0;
}
