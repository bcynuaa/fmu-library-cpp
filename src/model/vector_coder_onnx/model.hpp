#ifndef FMI_MODEL_VECTOR_CODER_ONNX_MODEL_HPP_
#define FMI_MODEL_VECTOR_CODER_ONNX_MODEL_HPP_

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "onnxengine/onnxengine.hpp"
#include "utils/fmu_io.hpp"

namespace vector_coder_onnx {

class Model {
    public:
    bool load(const std::filesystem::path& resources) {
        this->error_.clear();
        if (resources.empty())
            return this->fail("Resource location is required for this model");
        if (!std::filesystem::is_directory(resources))
            return this->fail(
                "Resource directory does not exist: " + resources.u8string());
        const auto library = onnxengine::locateLibrary(resources);
        if (library.empty())
            return this->fail(
                "ONNX Runtime library is missing in: " + resources.u8string());
        if (!this->predictor_.loadLibrary(library))
            return this->fail("Runtime " + library.u8string() + ": " +
                this->predictor_.lastError());
        this->model_path_ = onnxengine::locateModel(resources);
        if (this->model_path_.empty())
            return this->fail(
                "ONNX model is missing in: " + resources.u8string());
        if (!this->predictor_.loadModel(this->model_path_))
            return this->fail("Model " + this->model_path_.u8string() + ": " +
                this->predictor_.lastError());
        this->input_size_ = this->predictor_.inputSize();
        this->output_size_ = this->predictor_.outputSize();
        if (!this->loadCoder(resources)) return false;
        this->values_.assign(this->input_size_ + this->output_size_, 0.0);
        this->scaled_input_.assign(this->input_size_, 0.0);
        this->scaled_output_.assign(this->output_size_, 0.0);
        return true;
    }

    template <typename Reference>
    bool set(const Reference references[], std::size_t count,
        const double values[], bool) {
        if (count > 0 && (!references || !values))
            return this->fail("Null reference or value buffer");
        for (std::size_t index = 0; index < count; ++index) {
            const auto reference = references[index];
            if (reference == 0 || reference > this->values_.size())
                return this->fail(
                    "Invalid valueReference " + std::to_string(reference));
            if (reference > this->input_size_)
                return this->fail(
                    "Read-only valueReference " + std::to_string(reference));
        }
        for (std::size_t index = 0; index < count; ++index)
            this->values_[references[index] - 1] = values[index];
        return true;
    }

    template <typename Reference>
    bool get(const Reference references[], std::size_t count, double values[]) {
        if (count > 0 && (!references || !values))
            return this->fail("Null reference or value buffer");
        for (std::size_t index = 0; index < count; ++index) {
            if (references[index] == 0 ||
                references[index] > this->values_.size())
                return this->fail("Invalid valueReference " +
                    std::to_string(references[index]));
        }
        for (std::size_t index = 0; index < count; ++index)
            values[index] = this->values_[references[index] - 1];
        return true;
    }

    const std::string& lastError() const noexcept { return this->error_; }
    void clearError() { this->error_.clear(); }

    bool initialize() { return this->step(); }

    bool step() {
        for (std::size_t index = 0; index < this->input_size_; ++index) {
            this->scaled_input_[index] =
                (this->values_[index] - this->input_shift_[index]) /
                this->input_scale_[index];
        }
        if (!this->predictor_.predict(
                this->scaled_input_.data(), this->scaled_output_.data()))
            return this->fail("Model " + this->model_path_.u8string() + ": " +
                this->predictor_.lastError());
        for (std::size_t index = 0; index < this->output_size_; ++index) {
            this->values_[this->input_size_ + index] =
                this->scaled_output_[index] * this->output_scale_[index] +
                this->output_shift_[index];
        }
        return true;
    }

    void reset() {
        std::fill(this->values_.begin(), this->values_.end(), 0.0);
        this->error_.clear();
        std::fill(this->scaled_input_.begin(), this->scaled_input_.end(), 0.0);
        std::fill(
            this->scaled_output_.begin(), this->scaled_output_.end(), 0.0);
    }

    private:
    bool fail(const std::string& message) {
        this->error_ = message;
        return false;
    }

    bool loadCoder(const std::filesystem::path& resources) {
        // A present preferred file must be valid. Other .bin files are tried
        // in name order until one matches the model.
        constexpr const char* kPreferredNames[] = {
            "coder.bin", "scaler.bin", "pptransform.bin"};
        for (const char* name : kPreferredNames) {
            const auto candidate = resources / name;
            if (std::filesystem::exists(candidate))
                return this->readCoder(candidate);
        }
        std::vector<std::filesystem::path> fallback_paths;
        for (const auto& entry :
            std::filesystem::directory_iterator(resources)) {
            if (entry.is_regular_file() && entry.path().extension() == ".bin")
                fallback_paths.push_back(entry.path());
        }
        std::sort(fallback_paths.begin(), fallback_paths.end());
        if (fallback_paths.empty())
            return this->readCoder(resources / "coder.bin");
        for (const auto& path : fallback_paths) {
            if (this->readCoder(path)) {
                this->error_.clear();
                return true;
            }
        }
        return false;
    }

    bool readCoder(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return this->fail("Cannot open coder: " + path.u8string());
        std::int32_t inputs = 0, outputs = 0;
        if (!fmuio::readOne(file, inputs) || !fmuio::readOne(file, outputs) ||
            inputs <= 0 || outputs <= 0 ||
            static_cast<std::size_t>(inputs) != this->input_size_ ||
            static_cast<std::size_t>(outputs) != this->output_size_)
            return this->fail(
                "Coder dimensions do not match model: " + path.u8string());
        if (!fmuio::readDoubles(file, this->input_shift_, this->input_size_) ||
            !fmuio::readDoubles(file, this->input_scale_, this->input_size_) ||
            !fmuio::readDoubles(
                file, this->output_shift_, this->output_size_) ||
            !fmuio::readDoubles(file, this->output_scale_, this->output_size_))
            return this->fail("Truncated coder: " + path.u8string());
        for (const auto* values : {&this->input_shift_, &this->input_scale_,
                 &this->output_shift_, &this->output_scale_}) {
            if (!std::all_of(values->begin(), values->end(),
                    [](double value) { return std::isfinite(value); }))
                return this->fail(
                    "Nonfinite coder coefficient: " + path.u8string());
        }
        if (std::any_of(this->input_scale_.begin(), this->input_scale_.end(),
                [](double value) { return value == 0.0; }))
            return this->fail("Zero input scale: " + path.u8string());
        return true;
    }

    onnxengine::VectorPredictor predictor_;
    std::vector<double> values_;
    std::size_t input_size_ = 0;
    std::size_t output_size_ = 0;
    std::filesystem::path model_path_;
    std::string error_;
    std::vector<double> scaled_input_;
    std::vector<double> scaled_output_;
    std::vector<double> input_shift_;
    std::vector<double> input_scale_;
    std::vector<double> output_shift_;
    std::vector<double> output_scale_;
};

}  // namespace vector_coder_onnx

#endif  // FMI_MODEL_VECTOR_CODER_ONNX_MODEL_HPP_
