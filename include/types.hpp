// types.hpp
#pragma once

#include <string>
#include <opencv2/core.hpp>

struct Detection {
    cv::Rect bbox;
    float confidence;
    int classId;
    std::string className;
};

