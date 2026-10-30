#pragma once

#include <thread>
#include <atomic>
#include <string>

#include "app_state.hpp"

class BatchRunner {
public:
    void start(AppState &state);


    void stop();

    ~BatchRunner();

private:
    void run(AppState &state, RunSettings settings);

    std::thread thread_;
};
