#pragma once
#include <opencv2/opencv.hpp>

/**
 * Класс Preprocessor отвечает за подготовку изображений к анализу.
 * Реализует цепочку обработки: масштабирование, цветокоррекция, улучшение.
 */

class Preprocessor final{
private:
    cv::Size targetSize; //целевой размер
    bool useCLAHE;  //адаптивное выравнивание
    bool useGaussianBlur; //применять ли размытие?
    int gaussianKernelSize; //размер ядра размытия
    double gaussianSigma; // сигма для размытия
    bool inputIsBGR; // true = камера в BGR, false = уже RGB

    cv::Ptr<cv::CLAHE> clahe;  //объект для хранения состояния инициализации CLAHE
public:
    Preprocessor(
            cv::Size target = cv::Size(416, 416),
            bool useAdaptive = false,
            bool blur = false,
            int kernelSize = 5,
            double sigma = 1.5,
            bool inputIsBGR = false // По умолчанию камера в BGR
                    );

    cv::Mat process(const cv::Mat& input); // Основной метод обработки


    // Сеттеры
    void setTargetSize (cv::Size newSize); // Изменение целевого размера изображения
    void setBlurEnabled(bool enabled); // Размытие  - ВКЛ\ВЫКЛ
    void setCLAHEEnabled(bool enabled);// Адаптивная обработка яркости - ВКЛ\ВЫКЛ

    //Геттеры
    cv::Size getTargetSize() const; // текущий размер
    bool isBlurEnabled() const; // информация о ВКЛ\ВЫКЛ размытии
    bool isCLAHEEnabled() const; // информация о ВКЛ\ВЫКЛ адаптивной обработке

    // Информационный метод
    void printParams() const; // Вывод текущих параметров обработки


};