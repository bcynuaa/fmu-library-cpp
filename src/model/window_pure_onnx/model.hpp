#ifndef FMI_MODEL_WINDOW_PURE_ONNX_MODEL_HPP_
#define FMI_MODEL_WINDOW_PURE_ONNX_MODEL_HPP_

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "onnxengine/onnxengine.hpp"

namespace window_pure_onnx {

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
        this->window_size_ = this->predictor_.windowSize();
        this->feature_size_ = this->predictor_.featureSize();
        this->output_size_ = this->predictor_.outputSize();
        if (this->window_size_ == 0 ||
            this->feature_size_ <= this->output_size_)
            return this->fail(
                "Window requires input features plus previous outputs: " +
                this->model_path_.u8string());
        this->input_size_ = this->feature_size_ - this->output_size_;
        this->values_.assign(this->input_size_ + 2 * this->output_size_, 0.0);
        this->window_.assign(this->predictor_.inputSize(), 0.0);
        this->output_.assign(this->output_size_, 0.0);
        return true;
    }

    std::size_t valueReferenceCount() const noexcept {
        return this->values_.size();
    }

    bool supportsVariableCommunicationStepSize() const noexcept {
        return false;
    }

    template <typename Reference>
    bool set(const Reference references[], std::size_t count,
        const double values[], bool initial_values_allowed) {
        if (count > 0 && (!references || !values))
            return this->fail("Null reference or value buffer");
        for (std::size_t index = 0; index < count; ++index) {
            const auto reference = references[index];
            if (reference == 0 || reference > this->values_.size())
                return this->fail(
                    "Invalid valueReference " + std::to_string(reference));
            if (reference > this->input_size_ &&
                (reference <= this->input_size_ + this->output_size_ ||
                    !initial_values_allowed))
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

    bool initialize() {
        // Seed every frame with [u(0), y(0)], without running inference.
        const auto initial_offset = this->input_size_ + this->output_size_;
        for (std::size_t frame = 0; frame < this->window_size_; ++frame) {
            const auto offset = frame * this->feature_size_;
            std::copy_n(this->values_.begin(), this->input_size_,
                this->window_.begin() + offset);
            std::copy_n(this->values_.begin() + initial_offset,
                this->output_size_,
                this->window_.begin() + offset + this->input_size_);
        }
        std::copy_n(this->values_.begin() + initial_offset, this->output_size_,
            this->values_.begin() + this->input_size_);
        return true;
    }

    bool step() {
        const auto last = (this->window_size_ - 1) * this->feature_size_;
        std::copy_n(this->values_.begin(), this->input_size_,
            this->window_.begin() + last);
        if (!this->predictor_.predict(
                this->window_.data(), this->output_.data()))
            return this->fail("Model " + this->model_path_.u8string() + ": " +
                this->predictor_.lastError());
        std::copy(this->output_.begin(), this->output_.end(),
            this->values_.begin() + this->input_size_);
        std::copy(this->window_.begin() + this->feature_size_,
            this->window_.end(), this->window_.begin());
        std::fill_n(this->window_.begin() + last, this->input_size_, 0.0);
        std::copy(this->output_.begin(), this->output_.end(),
            this->window_.begin() + last + this->input_size_);
        return true;
    }

    void reset() {
        std::fill(this->values_.begin(), this->values_.end(), 0.0);
        this->error_.clear();
        std::fill(this->window_.begin(), this->window_.end(), 0.0);
        std::fill(this->output_.begin(), this->output_.end(), 0.0);
    }

    private:
    bool fail(const std::string& message) {
        this->error_ = message;
        return false;
    }

    onnxengine::WindowPredictor predictor_;
    std::vector<double> values_;
    std::size_t input_size_ = 0;
    std::size_t output_size_ = 0;
    std::filesystem::path model_path_;
    std::string error_;
    std::vector<double> window_;
    std::vector<double> output_;
    std::size_t window_size_ = 0;
    std::size_t feature_size_ = 0;
};

}  // namespace window_pure_onnx

#endif  // FMI_MODEL_WINDOW_PURE_ONNX_MODEL_HPP_
