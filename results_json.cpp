#define NOMINMAX

#include "results_json.h"
#include "results_json.hpp"

#include <algorithm>
#include <cstdio>
#include <cctype>
#include <cmath>
#include <nlohmann/json_fwd.hpp>

#include "fs_utf8.hpp"

// makeBoardId: собираем ID платы из номера.
//
// snprintf — безопасный брат printf: НЕ ВЫЙДЕТ за границы буфера, т.к. ему
// передан размер (sizeof(buf)). Формат "%04d" означает:
//   %d     — целое число;
//   04     — минимальная ширина 4 символа, недостающее слева добить нулями;
//            1 -> "0001", 15 -> "0015", 1234 -> "1234".
// Если плат больше 9999, число просто станет шире ("PCB-12345") —
// формат не теряет данные, только перестаёт выравнивать.

std::string makeBoardId(int zeroBasedIndex) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "PCB-%04d", zeroBasedIndex+1);
    return std::string(buf);
}

// makeDefectId: "PCB-0001" + номер -> "PCB-0001-D1".
// Дефекты внутри платы нумеруем с двух знаков (%02d) — при десятках дефектов
// ID при сортировке по алфавиту остаются читаемыми (D01..D12, а не D1, D10, D2).
std::string makeDefectId(const std::string &boardId, int zeroBasedIndex) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%s-D%02d",boardId.c_str(), zeroBasedIndex+1);
    return std::string(buf);
}

// isImageFile: проверка расширения. static = внутренняя линковка:
// функция видна только внутри этого .cpp (не конфликтует с isImage из main.cpp).
//
// Приведение к unsigned char перед tolower НЕ декоративное: std::tolower
// принимает int, и если char отрицательный (русские буквы в CP1251 —
// значения >127 становятся отрицательными!), в tolower уйдёт отрицательное
// число — это неопределённое поведение (UB). Каст "съедает" знак.
static bool isImageFile(const std::filesystem::path &path) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c){return static_cast<char>(std::tolower(c)); });
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bpm";
}


// listImageFilesSorted: прочитать папку -> отфильтровать картинки -> сортировать.
std::vector<std::filesystem::path> listImageFilesSorted(const std::string& utf8Dir) {
    std::vector<std::filesystem::path> result;

    std::filesystem::path dir = fsu8::pathFromUtf8(utf8Dir);
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        return result;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && isImageFile(entry.path())) {
            result.push_back(entry.path());
        }
    }

    // Сортируем по ИМЕНИ ФАЙЛА (не по полному пути — папка одна, имя достаточно).
    // Сравниваем wstring (UTF-16): имён с кириллицей std::string-вариант
    // не переживёт корректно. Порядок wchar_t-кодов соответствует порядку
    // символов Юникода — для наших целей этого достаточно.

    std::sort(result.begin(), result.end(),
        [](const std::filesystem::path& a, const std::filesystem::path& b)
        {return a.filename().string() < b.filename().string();});
    return result;
}

// boardRecordToJson: BoardRecord -> JSON-объект.
//
// nlohmann::ordered_json сохраняет ключи В ПОРЯДКЕ ДОБАВЛЕНИЯ (внутри — вектор
// пар), поэтому отчёт читается сверху вниз логично: id, filename, model...
// Обычный nlohmann::json отсортировал бы ключи по алфавиту (внутри — map).

nlohmann::ordered_json boardRecordToJson(const BoardRecord& rec) {
    nlohmann::ordered_json j;

    j["id"] = rec.id;
    j["filename"] = rec.filename;
    j["total_defects"] = rec.defects.size();
    j["defects"] = nlohmann::ordered_json::array();

    int index =0;
    for (const auto& det : rec.defects) {
        nlohmann::ordered_json d;

        d["id"] = makeDefectId(rec.id, index);
        d["type"] = det.className;

        // Округляем confidence до 3 знаков: 0.9131... -> 0.913.
        // float после умножения может дать 0.91299999, поэтому округляем явно.
        d["confidence"] = std::round(det.confidence *1000.0f)/1000.0f;

        // Список {"ключ", значение}, {"ключ", значение}, ... nlohmann понимает
        // как JSON-ОБЪЕКТ (а список значений без ключей был бы массивом).
        d["box"] = {
            {"x", det.bbox.x},
            {"y", det.bbox.y},
            {"width", det.bbox.width},
            {"height", det.bbox.height},
        };

        j["defects"].push_back(d);
        ++index;

    }
    return j;
}

// buildResultsJson: весь отчёт — JSON-массив из записей по всем платам.

nlohmann::ordered_json buildResultsJson(const std::vector<BoardRecor>& records) {
    nlohmann::ordered_json root = nlohmann::ordered_json::array();
    for (const auto& rec : records) {
        root.push_back(boardRecordToJson(rec));
    }
    return root;
}