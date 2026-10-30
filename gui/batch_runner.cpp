#include "batch_runner.hpp"

#include <vector>
#include <filesystem>

#include "detector.hpp"      // Detector
#include "visualizer.hpp"    // Visualizer
#include "results_json.hpp"  // listImageFilesSorted, makeBoardId, makeResultFilename, buildResultsJson
#include "fs_utf8.hpp"       // fsu8::imreadUtf8 / imwriteUtf8 / pathToUtf8 / pathFromUtf8

#include <opencv2/opencv.hpp>
#include <fstream>


static Visualizer makeVisualizer() {
    return Visualizer(
        cv::Scalar(0,0,255),
        2,
        0.5,
        1,
        true,
        true,
        false,
        false);
}


void BatchRunner::start(AppState &state) {
    if (thread_.joinable()) return;
    state.cancelRequested = false;

    RunSettings snapshot = state.settings;
    thread_ = std::thread([this, &state, &snapshot]() {
        run(state,snapshot);
    });
}

void BatchRunner::stop() {
    if (!thread_.joinable()) return;
    thread_.join();
}

BatchRunner::~BatchRunner() {
    stop();
}


void BatchRunner::run(AppState &state, RunSettings s) {

    const std::string inputDir(s.inputDir);
    const std::string outputDir(s.outputDir);

    std::vector<std::filesystem::path> files = listImageFilesSorted(inputDir);

    {
        std::lock_guard<std::mutex> lock(state.mtx);
        state.progress = Progress{};
        state.progress.running = true;
        state.progress.total = static_cast<int>(files.size());
        state.items.clear();
        state.items.reserve(files.size());

        for (size_t i =0; i < files.size(); ++i) {
            BoardItem item;
            item.id = makeBoardId(static_cast<int>(i));
            item.sourceFilename = fsu8::pathToUtf8(files[i].filename());
            state.items.push_back(item);
        }

        if (files.empty()) {
            state.progress.running = false;
            state.progress.finished = true;
            state.progress.success = false;
            state.progress.error = "Входная папка пуста или не существует" ;
            return;
        }

    }

    {
        std::lock_guard<std::mutex> lock(state.mtx);
        state.progress.message = "Загрузка моделей ONNX...";
    }

    try {

        Detector detector;
        Visualizer visualizer = makeVisualizer();

        std::vector<BoardRecord> records;
        records.reserve(files.size());

        bool canceled = false;
        int doneCount = 0;
        int failCount = 0;
        int defectTotal = 0;

        for (size_t i =0; i < files.size(); ++i) {
            if (state.cancelRequested.load()) {
                std::lock_guard<std::mutex> lock(state.mtx);
                state.progress.running = false;
                state.progress.finished = true;
                state.progress.canceled = true;
                state.progress.message = "Отменено пользователем";
                break;
            }
            {
                std::lock_guard<std::mutex> lock(state.mtx);
                state.items[i].status = FileStatus::Processing;
                state.progress.currentFile = state.items[i].sourceFilename;
                state.progress.message = "Анализ " + state.items[i].id;
            }

            const std::string fullPath = fsu8::pathToUtf8(files[i]);
            cv::Mat frame = fsu8::imreadUtf8(fullPath);
            if (frame.empty()) {
                std::lock_guard<std::mutex> lock(state.mtx);
                state.items[i].status = FileStatus::Failed;
                ++failCount;
                state.progress.processed = static_cast<int>(i+1);
                continue;
            }

            std::vector<Detection> detections = detector.detect(frame,
                s.confThreshold, s.nmsThreshold);

            cv::Mat original = frame.clone();

            visualizer.draw(frame, detections, 0.0);

            if (s.stampId) {
                std::string boardId = state.items[i].id;

                std::lock_guard<std::mutex> lock(state.mtx);
                boardId = state.items[i].id;
                cv::putText(frame,boardId, cv::Point(10,30), cv::FONT_HERSHEY_SIMPLEX, 0.9,
                    cv::Scalar(0, 255,255), 2);
            }


            std::string resultFilename = makeResultFilename(state.items[i].id,
                state.items[i].sourceFilename);


            if (s.saveImages) {
                std::string savePath = outputDir     + "/" + resultFilename;
                if (!fsu8::imwriteUtf8(savePath, frame)) {
                    std::lock_guard<std::mutex> lock(state.mtx);
                    state.progress.message = "Ошибка записи: " + savePath;
                }
            }

            {
                std::lock_guard<std::mutex> lock(state.mtx);
                state.items[i].status = FileStatus::Done;
                state.items[i].defects = detections;

                state.items[i].annotated = frame;
                state.items[i].original = original;
                ++doneCount;
                defectTotal += static_cast<int>(detections.size());
                state.progress.processed = static_cast<int>(i+1);
            }

            BoardRecord rec;
            rec.id = state.items[i].id;
            rec.filename = resultFilename;
            rec.defects = detections;
            records.push_back(std::move(rec));
        }

        {
            std::lock_guard<std::mutex> lock(state.mtx);
            state.progress.running = false;
            state.progress.finished = true;
            state.progress.canceled = canceled;
            state.progress.success = !canceled;

            if (canceled) {
                state.progress.message = "Отменено пользователем.";
            }else {
                state.progress.message = "Готово плат " + std::to_string(doneCount) +
                    ", ошибок "+ std::to_string(failCount) + ", дефектов " +
                        std::to_string(defectTotal);
            }

        }
        if (!canceled) {
            nlohmann::ordered_json report = buildResultsJson(records);
            std::ofstream file(fsu8::pathFromUtf8(outputDir + "/global_results.json"));
            if (file.is_open()) {
                file << report.dump(4);;
            }
        }

    }catch (std::exception& e) {
        std::lock_guard<std::mutex> lock(state.mtx);
        state.progress.running = false;
        state.progress.finished = true;
        state.progress.error = e.what();
        state.progress.message = "Ошибка: " + std::string(e.what());
    }
}