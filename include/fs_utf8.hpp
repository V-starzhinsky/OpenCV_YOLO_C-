#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <iterator>
#include <filesystem>

#include <windows.h>

#include <opencv2/opencv.hpp>


// utf8ToWide: std::string (UTF-8) -> std::wstring (UTF-16, "родной" формат
// Windows). MultiByteToWideChar — стандартная функция WinAPI:
//   CP_UTF8       — исходная кодировка (что мы конвертируем ИЗ);
//   0             — флаги (0 = стандартное поведение);
//   c_str()       — входная строка;
//   -1            — длина: -1 значит "до нулевого терминатора", включает его;
//   nullptr, 0    — сначала вызываем с nullptr, чтобы УЗНАТЬ нужный размер
//                   (функция вернёт количество требуемых wchar_t);
// далее выделяем буфер и вызываем второй раз уже с буфером.
namespace fsu8 {

    
    inline  std::wstring utf8toWide(const std::string& utf8) {
        if (utf8.empty()) {return std::wstring();}
        int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
        if (needed <= 0) {return std::wstring();}

        std::wstring result(static_cast<size_t>(needed) - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
            -1, &result[0], needed);
        return result;
    }

    // wideToUtf8: обратное преобразование std::wstring (UTF-16) -> std::string
    // (UTF-8). Нужно, например, чтобы перевести путь, выбранный пользователем
    // в системном диалоге выбора папки (диалог возвращает wchar_t*), в наш
    // внутренний формат UTF-8.

    inline std::string wideToUtf8(const std::wstring& wide) {
        if (wide.empty()) {return std::string();}
        int needed = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (needed <= 0) {return std::string();}
        std::string result(static_cast<size_t>(needed) - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),-1, &ewsult[0], needed, nullptr, nullptr);
        return result;
    }

    // wideToUtf8: обратное преобразование std::wstring (UTF-16) -> std::string
    // (UTF-8). Нужно, например, чтобы перевести путь, выбранный пользователем
    // в системном диалоге выбора папки (диалог возвращает wchar_t*), в наш
    // внутренний формат UTF-8.

    inline std::filesystem::path pathFromUtf8 (const std::string& utf8) {
        return std::filesystem::path(utf8toWide(utf8));
    }

    // pathToUtf8: путь -> UTF-8 строка (для отображения в ImGui/консоли).
    inline std::string pathToUtf8 (const std::filesystem::path& path) {
        return wideToUtf8(path.wstring());
    }

    // imreadUtf8: безопасное чтение картинки по UTF-8 пути.
    //
    // Алгоритм:
    //   1. Открываем файл std::ifstream, передав ему fs::path (он принимает path
    //      напрямую и внутри использует UTF-16 версии WinAPI — проблем с кодовой
    //      страницей нет).
    //   2. Читаем ВСЕ байты файла в std::vector<unsigned char> через
    //      istreambuf_iterator (итератор по "сырым" символам потока).
    //   3. cv::imdecode разбирает байты в cv::Mat, сам определяя формат
    //      (JPEG/PNG/BMP) по содержимому — расширение ему не нужно.
    //
    // Возвращает пустой cv::Mat (проверяется через .empty()), если файл не
    // открылся или не является картинкой — так же, как это делает cv::imread.
    inline cv::Mat imreadUtf8(const std::string& utf8Path) {
        std::ifstream file(pathFromUtf8(utf8Path), std::ios::binary);
        if (!file.is_open()) {return cv::Mat();}

        std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
        if (buffer.empty()) {return cv::Mat();}
        return cv::imdecode(buffer, cv::IMREAD_COLOR);
    }

    //  imwriteUtf8: безопасная запись картинки по UTF-8 пути.
    //
    // cv::imencode требует РАСШИРЕНИЕ файла (".jpg", ".png"...) — по нему он
    // выбирает кодек. Затем пишет байты обычным ofstream'ом.

    inline bool imwriteUtf8(const std::string& utf8Path, const cv::Mat& image) {
        std::filesystem::path p = pathFromUtf8(utf8Path);
        std::string ext = p.extension().string();
        if (ext.empty()) {ext = ".jpg";}

        std::vector<unsigned char> encoded;
        if (!cv::imencode(ext, image, encoded)) {return false;}

        std::ofstream file(p, std::ios::binary);
        if (!file.is_open()) {return false;}
        file.write(reinterpret_cast<const char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
        return file.good();
    }




}