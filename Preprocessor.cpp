#include "Preprocessor.hpp"
#include <opencv2/opencv.hpp>
#include <iostream>

using namespace cv;
using namespace std;



Preprocessor::Preprocessor(
            cv::Size target,
            bool useAdaptive,
            bool blur,
            int kernelSize,
            double sigma,
            bool inputIsBGR)
            :
            targetSize(target),
            useGaussianBlur(blur),
            gaussianKernelSize(kernelSize),
            gaussianSigma(sigma),
            useCLAHE(useAdaptive),
            inputIsBGR(inputIsBGR)

            {
            //Инициализация CLAHE (адаптивное выравнивание гистограмм)
            clahe = createCLAHE(2.0, Size(8,8));
            cout << "[Preprocessor] Initialized with target size: " <<
            targetSize.width << "x" << targetSize.height << endl;
    }


    //Основной метод обработки
    Mat Preprocessor::process(const Mat& input){
        Mat output;

        //Проверяем на пустой кадр
        if(input.empty()){
            cerr << "[Preprocessor] Error: Empty input frame!" << endl;
            return Mat();
        }

        //Копируем , чтобы не изменять исходник.
        input.copyTo(output);

        /*/Камера отдает BGR , для распознования нужен RGB. Конвертируем
        if(inputIsBGR) {
            cvtColor(output, output, COLOR_BGR2RGB);
        }*/

        // Адаптивное выравнивание гисторграммы(распределение яркости пикселей)
        // Улучшает контраст при плохом освещении
        if(useCLAHE) {
            Mat labImage;

            //CLAHE работает в LAB
            cvtColor(output, labImage, COLOR_RGB2Lab);
            //Создаем вектор для разделения на каналы . Для CLAHE нужен канал L - яркость.
            //В LAB он [0]
            vector<Mat> labChannels(3);

            //Разбиваем на каналы
            split(labImage, labChannels);
            //Применяем CLAHE только к каналу яркости
            clahe->apply(labChannels[0], labChannels[0]);

            //Собираем обратно
            merge(labChannels, labImage);
            cvtColor(labImage, output, COLOR_Lab2BGR);
        }

        // Удаляем шум с изображения. Для этого применяем размытие, а именно Гауссово размытие,
        // Оно убирает мелкие артефакты и шум сенсора. Ядро должно быть нечетным, для того чтобы
        // центральный пиксель

        if(useGaussianBlur){
            GaussianBlur(output, output, Size(gaussianKernelSize, gaussianKernelSize),
                         gaussianSigma);
        }

        // Масштабирование . Меняем размер до целевого ( чтобы использовать нейросеть)
        if(!targetSize.empty()){
            resize(output, output, targetSize);
        }

        // --- ШАГ 6: НОРМАЛИЗАЦИЯ (для нейросетей) ---
        // Приводим значения пикселей к диапазону [0, 1] или [-1, 1]
        // Это зависит от требований конкретной модели
        // (Пока оставляем как [0, 255], т.к. blobFromImage сделает нормализацию)
        cvtColor(output, output, COLOR_BGR2RGB);
        return output;
    }

    //Методы для изменения параметров обработки (Сеттеры)
    void Preprocessor::setTargetSize (Size newSize){
        targetSize = newSize;
    }

    void Preprocessor::setBlurEnabled(bool enabled){
        useGaussianBlur = enabled;
    }

    void Preprocessor::setCLAHEEnabled(bool enabled){
        useCLAHE = enabled;
    }

    // Отображение текущих параметров
    void Preprocessor::printParams() const{
        cout << "**** Preprocessor Parameters ****" << endl;
        cout << "Target size: " << targetSize.width << "x" << targetSize.height << endl;
        cout << "Gaussian blur: " << (useGaussianBlur ? "ON": "OFF") << endl;
        cout << "CLAHE: " << (useCLAHE ? "ON" : "OFF") << endl;
        cout << "=================================" << endl;
    }

    //Получение текущих параметров обработки (Геттеры)
    Size Preprocessor::getTargetSize() const{
        return targetSize;
    }

    bool Preprocessor::isBlurEnabled() const{
        return useGaussianBlur;
    }

    bool Preprocessor::isCLAHEEnabled() const{
        return useCLAHE;
    }
