//
// Created by ponka on 22.07.2026.
//
#pragma once
#include <chrono>
#include <string>

class FpsCounter {
public:
    FpsCounter();
    void tick();

    std::string getText() const {return fpsText;}
    float getValue() const {return currentFps;}


private:
    std::chrono::time_point<std::chrono::high_resolution_clock> lastTime; // время фиксации последнего обновления счетчика
    int frameCount;            //счетчик кадров внутри текущей секунды
    float currentFps;          // счетчик FPS
    std::string  fpsText;      // строка для вывода на экранр
};

