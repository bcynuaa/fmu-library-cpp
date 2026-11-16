#ifndef FMI_UTILS_FMU_IO_HPP_
#define FMI_UTILS_FMU_IO_HPP_

#include <cctype>
#include <cstddef>
#include <filesystem>
#include <istream>
#include <string>
#include <type_traits>
#include <vector>

namespace fmuio {

inline int hexValue(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

inline std::string percentDecode(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '%' && index + 2 < value.size()) {
            const int high = hexValue(value[index + 1]);
            const int low = hexValue(value[index + 2]);
            if (high >= 0 && low >= 0) {
                result.push_back(static_cast<char>((high << 4) | low));
                index += 2;
                continue;
            }
        }
        result.push_back(value[index]);
    }
    return result;
}

inline std::filesystem::path resourceLocationToPath(
    const char* resource_location) {
    if (!resource_location || !resource_location[0]) return {};

    std::string path(resource_location);
    const bool had_file_scheme = path.rfind("file://", 0) == 0;
    if (had_file_scheme) {
        path.erase(0, 7);
        if (path.rfind("localhost/", 0) == 0) path.erase(0, 10);
        path = percentDecode(path);
    }

#ifdef _WIN32
    if (path.size() >= 3 && path[0] == '/' &&
        std::isalpha(static_cast<unsigned char>(path[1])) && path[2] == ':') {
        path.erase(0, 1);
    } else if (had_file_scheme && !path.empty() && path[0] != '/' &&
        path.find(':') == std::string::npos) {
        path = "//" + path;
    }
#endif

    return std::filesystem::u8path(path);
}

template <typename T>
bool readOne(std::istream& file, T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    file.read(reinterpret_cast<char*>(&value),
        static_cast<std::streamsize>(sizeof(T)));
    return static_cast<bool>(file);
}

inline bool readDoubles(
    std::istream& file, std::vector<double>& values, std::size_t count) {
    values.resize(count);
    if (count == 0) return true;
    file.read(reinterpret_cast<char*>(values.data()),
        static_cast<std::streamsize>(count * sizeof(double)));
    return static_cast<bool>(file);
}

}  // namespace fmuio

#endif  // FMI_UTILS_FMU_IO_HPP_
