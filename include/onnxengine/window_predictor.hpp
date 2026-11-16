#ifndef ONNXENGINE_WINDOW_PREDICTOR_HPP
#define ONNXENGINE_WINDOW_PREDICTOR_HPP

#include <cstddef>
#include <filesystem>
#include <string>

#include "onnxengine/detail/session.hpp"

namespace onnxengine {

class WindowPredictor {
    public:
    bool loadLibrary(const std::filesystem::path& path) {
        return this->session_.loadLibrary(path);
    }
    bool loadModel(const std::filesystem::path& path) {
        return this->session_.loadModel(path, 3);
    }
    bool predict(const double* window, double* output) {
        return this->session_.predict(window, output);
    }

    bool loaded() const noexcept { return this->session_.loaded(); }
    std::size_t windowSize() const noexcept {
        return this->session_.windowSize();
    }
    std::size_t featureSize() const noexcept {
        return this->session_.featureSize();
    }
    std::size_t inputSize() const noexcept {
        return this->windowSize() * this->featureSize();
    }
    std::size_t outputSize() const noexcept {
        return this->session_.outputSize();
    }
    const std::string& lastError() const noexcept {
        return this->session_.lastError();
    }

    private:
    detail::Session session_;
};

}  // namespace onnxengine

#endif  // ONNXENGINE_WINDOW_PREDICTOR_HPP
