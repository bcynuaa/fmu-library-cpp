#ifndef ONNXENGINE_HPP
#define ONNXENGINE_HPP

#include <filesystem>
#include <system_error>

#include "onnxengine/vector_predictor.hpp"
#include "onnxengine/window_predictor.hpp"

namespace onnxengine {

inline std::filesystem::path locateLibrary(const std::filesystem::path& resource_dir) {
    std::error_code error;
    std::filesystem::path standard_name;
#if defined(_WIN32)
    standard_name = "onnxruntime.dll";
#elif defined(__APPLE__)
    standard_name = "libonnxruntime.dylib";
#elif defined(__linux__)
    standard_name = "libonnxruntime.so";
#else
    return {};
#endif

    const std::filesystem::path standard_path = resource_dir / standard_name;
    if (std::filesystem::is_regular_file(standard_path, error)) return standard_path;

    error.clear();
    std::filesystem::directory_iterator iterator(resource_dir, error);
    const std::filesystem::directory_iterator end;
    std::filesystem::path found;
    while (!error && iterator != end) {
        const auto entry = *iterator;
        std::error_code entry_error;
        if (entry.is_regular_file(entry_error) && !entry_error) {
            const auto name = entry.path().filename().native();
#if defined(_WIN32)
            const bool matches = name == L"onnxruntime.dll";
#elif defined(__APPLE__)
            const bool matches = name.rfind("libonnxruntime.", 0) == 0 &&
                                 entry.path().extension() == ".dylib";
#elif defined(__linux__)
            const bool matches = name.rfind("libonnxruntime.so", 0) == 0;
#else
            const bool matches = false;
#endif
            if (matches && (found.empty() || entry.path().filename() < found.filename())) {
                found = entry.path();
            }
        }
        iterator.increment(error);
    }
    return found;
}

inline std::filesystem::path locateModel(const std::filesystem::path& resource_dir) {
    std::error_code error;
    const std::filesystem::path standard_path = resource_dir / "model.onnx";
    if (std::filesystem::is_regular_file(standard_path, error)) return standard_path;

    error.clear();
    std::filesystem::path found;
    std::filesystem::directory_iterator iterator(resource_dir, error);
    const std::filesystem::directory_iterator end;
    while (!error && iterator != end) {
        const auto entry = *iterator;
        std::error_code entry_error;
        if (entry.is_regular_file(entry_error) && !entry_error &&
            entry.path().extension() == ".onnx" &&
            (found.empty() || entry.path().filename() < found.filename())) {
            found = entry.path();
        }
        iterator.increment(error);
    }
    return found;
}

} // namespace onnxengine

#endif // ONNXENGINE_HPP
