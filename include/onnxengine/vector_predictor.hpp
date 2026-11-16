#ifndef ONNXENGINE_VECTOR_PREDICTOR_HPP
#define ONNXENGINE_VECTOR_PREDICTOR_HPP

#include <cstddef>
#include <filesystem>
#include <string>

#include "onnxengine/detail/session.hpp"

namespace onnxengine {

class VectorPredictor {
    public:
    bool loadLibrary(const std::filesystem::path& path) {
        return this->session_.loadLibrary(path);
    }
    bool loadModel(const std::filesystem::path& path) {
        return this->session_.loadModel(path, 2);
    }
    bool predict(const double* input, double* output) {
        return this->session_.predict(input, output);
    }

    bool loaded() const noexcept { return this->session_.loaded(); }
    std::size_t inputSize() const noexcept {
        return this->session_.inputSize();
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

#endif  // ONNXENGINE_VECTOR_PREDICTOR_HPP
