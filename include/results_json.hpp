// results_json.hpp — общий модуль отчёта по результатам анализа (JSON v2).
//
// Используется ДВУМЯ потребителями:
//   1) консольная пакетная обработка (main.cpp);
//   2) GUI-приложение (gui/batch_runner.cpp).

#pragma once

#include <string>
#include <vector>
#include <filesystem>

#include <nlohmann/json.hpp>
#include <types.hpp>

struct BoardRecord {
    std::string id;
    std::string filename;
    std::vector<Detection> defects;
};

// ID платы по порядковому номеру в ОТСОРТИРОВАННОМ списке файлов.
// zeroBasedIndex=0 -> "PCB-0001", 15 -> "PCB-0016".
std::string makeBoardId(int zeroBasedIndex);

// ID отдельного дефекта внутри платы: ("PCB-0001", 0)
std::string makeDeffectId(const std::string &boardId,  int zeroBasedIndex);

// Возвращает список картинок в папке, ОТСОРТИРОВАННЫЙ по имени файла.
//
// Почему сортировка обязательна: стандарт C++ не гарантирует порядок обхода
// directory_iterator (NTFS обычно отдаёт ~по алфавиту, но это совпадение
// реализации). ID плат назначаются по порядковому номеру, поэтому порядок
// должен быть воспроизводимым от запуска к запуску — иначе "PCB-0007"
// сегодня и завтра будут разными файлами.
//
// Фильтрует по расширению (.jpg/.jpeg/.png/.bmp, регистр не важен).
// utf8Dir — путь к папке в UTF-8. Возвращает пути в "родном" виде
// (std::filesystem::path внутри хранит UTF-16 — корректно для любых имён).
std::vector<std::filesystem::path> listImageFilesSorted(const std::string& utf8Dir);

// Одна плата -> JSON-объект (порядок ключей сохраняется как при добавлении).
nlohmann::ordered_json boardRecordToJson(const BoardRecord& rec );

// Полный отчёт: массив записей по всем платам.
nlohmann::ordered_json buildResultsJson(std::vector<BoardRecord>& records);