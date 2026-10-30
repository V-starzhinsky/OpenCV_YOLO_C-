#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>

#include <opencv2/core.hpp>

#include "types.hpp"
#include <results_json.hpp>



enum class FileStatus {
    Pending,
    Processing,
    Done,
    Failed
};


struct BoardItem {

    std::string id;
    std::string sourceFilename;

    FileStatus status = FileStatus::Pending;
    std::vector<Detection> defects;

    cv::Mat annotated;
    cv::Mat original;
};

struct Progress {
    bool running = false;
    bool finished = false;
    bool canceled = false;
    bool success = false;

    int processed = 0;
    int total = 0;

    std::string currentFile;
    std::string message;
    std::string error;
};


struct RunSettings {
    char inputDir[512] = "C:/Users/sulim/CLionProjects/OpenCV_YOLO_C-/pcb_pic";
    char outputDir[512] = "C:/Users/sulim/CLionProjects/OpenCV_YOLO_C-/pcb_pic_res";

    float confThreshold = 0.25f;
    float nmsThreshold= 0.40f;

    bool saveImages = false;
    bool stampId = false;
};


struct AppState {
    RunSettings settings;
    int selectedIndex = -1;


    std::mutex mtx;
    std::vector<BoardItem> items;
    Progress progress;



    std::atomic<bool> cancelRequested{false};
};