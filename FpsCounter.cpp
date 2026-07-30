//
// Created by ponka on 22.07.2026.
//

#include "include/FpsCounter.hpp"

FpsCounter :: FpsCounter() : lastTime(std::chrono::high_resolution_clock::now()),
                            frameCount(0),
                            currentFps(0.0),
                            fpsText("FPS: 0"){}

void FpsCounter::tick() {
    frameCount++;

    // Получаем текущую временную точку
    auto currentTime = std::chrono::high_resolution_clock::now();

    // Считаем, сколько времени (в секундах) прошло с момента последнего обновления
    std::chrono::duration<float> elapsedTime = currentTime - lastTime;

    // Обновляем счетчик строго один раз в секунду, чтобы избежать мерцания цифр
    if (elapsedTime.count() >= 1.0f) {
        currentFps = static_cast<float>(frameCount) / elapsedTime.count();
        fpsText = "FPS: " + std::to_string(static_cast<int>(currentFps));

        // Сбрасываем счетчик кадров и обновляем базовую временную точку
        frameCount = 0;
        lastTime = currentTime;
    }
}
