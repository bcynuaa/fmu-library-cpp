#ifndef ONNXENGINE_DETAIL_SESSION_HPP
#define ONNXENGINE_DETAIL_SESSION_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <onnxruntime_c_api.h>

namespace onnxengine::detail {

class Session {
    public:
    Session() { this->error_.reserve(128); }
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    ~Session() { unload(); }

    bool loadLibrary(const std::filesystem::path& path) {
        try {
            return loadLibraryImpl(path);
        } catch (...) {
            unload();
            return fail("Could not initialize ONNX Runtime");
        }
    }

    bool loadModel(const std::filesystem::path& path, std::size_t input_rank) {
        try {
            return loadModelImpl(path, input_rank);
        } catch (...) {
            releaseModel();
            return fail("Could not load ONNX model");
        }
    }

    bool predict(const double* input, double* output) {
        this->error_.clear();
        if (!loaded() || !input || !output)
            return fail("Predictor is not loaded or a buffer is null");
        if (this->input_type_ == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) {
            for (std::size_t i = 0; i < this->input_size_; ++i)
                this->input_f32_[i] = static_cast<float>(input[i]);
        } else {
            std::copy(
                input, input + this->input_size_, this->input_f64_.begin());
        }
        if (!run()) return false;
        if (this->output_type_ == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) {
            for (std::size_t i = 0; i < this->output_size_; ++i)
                output[i] = static_cast<double>(this->output_f32_[i]);
        } else {
            std::copy(
                this->output_f64_.begin(), this->output_f64_.end(), output);
        }
        return true;
    }

    bool loaded() const noexcept {
        return this->session_ && this->input_value_ && this->output_value_ &&
            this->binding_;
    }
    std::size_t inputSize() const noexcept { return this->input_size_; }
    std::size_t outputSize() const noexcept { return this->output_size_; }
    std::size_t windowSize() const noexcept {
        return this->input_rank_ == 3
            ? static_cast<std::size_t>(this->input_shape_[1])
            : 0;
    }
    std::size_t featureSize() const noexcept {
        return this->input_rank_ == 3
            ? static_cast<std::size_t>(this->input_shape_[2])
            : 0;
    }
    const std::string& lastError() const noexcept { return this->error_; }

    void unload() noexcept {
        releaseModel();
        if (this->api_) {
            if (this->options_)
                this->api_->ReleaseSessionOptions(this->options_);
            if (this->env_) this->api_->ReleaseEnv(this->env_);
        }
        this->options_ = nullptr;
        this->env_ = nullptr;
        this->api_ = nullptr;
#ifdef _WIN32
        if (this->library_) FreeLibrary(this->library_);
#else
        if (this->library_) dlclose(this->library_);
#endif
        this->library_ = nullptr;
    }

    private:
    bool loadLibraryImpl(const std::filesystem::path& path) {
        unload();
        this->error_.clear();
#ifdef _WIN32
        this->library_ = LoadLibraryExW(
            path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        auto get_api_base = this->library_
            ? reinterpret_cast<GetApiBase>(
                  GetProcAddress(this->library_, "OrtGetApiBase"))
            : nullptr;
#else
        const std::string library_path = path.string();
        this->library_ = dlopen(library_path.c_str(), RTLD_NOW | RTLD_LOCAL);
        auto get_api_base = this->library_
            ? reinterpret_cast<GetApiBase>(
                  dlsym(this->library_, "OrtGetApiBase"))
            : nullptr;
#endif
        if (!get_api_base) {
#ifdef _WIN32
            const auto error_code = GetLastError();
            this->error_ =
                "Could not load ONNX Runtime library or OrtGetApiBase"
                " (Windows error " +
                std::to_string(error_code) + ")";
#else
            const char* error = dlerror();
            this->error_ =
                "Could not load ONNX Runtime library or OrtGetApiBase";
            if (error) this->error_ += std::string(": ") + error;
#endif
            return failLibrary("Could not load ONNX Runtime library");
        }
        const OrtApiBase* base = get_api_base();
        this->api_ = base ? base->GetApi(ORT_API_VERSION) : nullptr;
        if (!this->api_ ||
            !check(this->api_->CreateEnv(
                       ORT_LOGGING_LEVEL_WARNING, "onnxengine", &this->env_),
                "CreateEnv") ||
            !check(this->api_->CreateSessionOptions(&this->options_),
                "CreateSessionOptions") ||
            !check(this->api_->SetIntraOpNumThreads(this->options_, 1),
                "SetIntraOpNumThreads") ||
            !check(this->api_->SetSessionGraphOptimizationLevel(
                       this->options_, ORT_ENABLE_ALL),
                "SetGraphOptimization"))
            return failLibrary("Could not initialize ONNX Runtime");
        return true;
    }

    bool loadModelImpl(
        const std::filesystem::path& path, std::size_t input_rank) {
        releaseModel();
        this->error_.clear();
        if (!this->api_) return fail("Load the ONNX Runtime library first");
        if (input_rank != 2 && input_rank != 3)
            return fail("Unsupported input rank");

        if (!check(this->api_->CreateSession(this->env_, path.c_str(),
                       this->options_, &this->session_),
                "CreateSession") ||
            !check(this->api_->CreateCpuMemoryInfo(OrtArenaAllocator,
                       OrtMemTypeDefault, &this->memory_info_),
                "CreateMemoryInfo"))
            return failModelCurrent();

        std::size_t input_count = 0, output_count = 0;
        if (!check(
                this->api_->SessionGetInputCount(this->session_, &input_count),
                "ReadInputCount") ||
            !check(this->api_->SessionGetOutputCount(
                       this->session_, &output_count),
                "ReadOutputCount"))
            return failModelCurrent();
        if (input_count != 1 || output_count != 1)
            return failModel("Model must have one input and one output");
        if (!readName(true, this->input_name_) ||
            !readName(false, this->output_name_) ||
            !readSpec(true, this->input_shape_, this->input_type_,
                this->input_rank_) ||
            !readSpec(false, this->output_shape_, this->output_type_,
                this->output_rank_))
            return failModelCurrent();
        if (this->input_rank_ != input_rank ||
            (this->output_rank_ != 1 && this->output_rank_ != 2))
            return failModel("Model input or output rank is unsupported");
        if (!validType(this->input_type_) || !validType(this->output_type_))
            return failModel(
                "Model input and output must use float32 or float64");
        if (!validBatch(this->input_shape_[0]) ||
            (this->output_rank_ == 2 && !validBatch(this->output_shape_[0])))
            return failModel("Model batch dimension must be dynamic or 1");
        this->input_shape_[0] = 1;
        if (this->output_rank_ == 2) this->output_shape_[0] = 1;
        if (!fixedElements(
                this->input_shape_, this->input_rank_, this->input_size_) ||
            !fixedElements(
                this->output_shape_, this->output_rank_, this->output_size_))
            return failModel("Model dimensions must be fixed positive values");

        if (this->input_type_ == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
            this->input_f32_.assign(this->input_size_, 0.0f);
        else
            this->input_f64_.assign(this->input_size_, 0.0);
        if (this->output_type_ == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
            this->output_f32_.assign(this->output_size_, 0.0f);
        else
            this->output_f64_.assign(this->output_size_, 0.0);

        void* input_data =
            this->input_type_ == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT
            ? static_cast<void*>(this->input_f32_.data())
            : static_cast<void*>(this->input_f64_.data());
        void* output_data =
            this->output_type_ == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT
            ? static_cast<void*>(this->output_f32_.data())
            : static_cast<void*>(this->output_f64_.data());
        const std::size_t input_bytes = this->input_size_ *
            (this->input_type_ == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT
                    ? sizeof(float)
                    : sizeof(double));
        const std::size_t output_bytes = this->output_size_ *
            (this->output_type_ == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT
                    ? sizeof(float)
                    : sizeof(double));
        if (!check(
                this->api_->CreateTensorWithDataAsOrtValue(this->memory_info_,
                    input_data, input_bytes, this->input_shape_.data(),
                    this->input_rank_, this->input_type_, &this->input_value_),
                "CreateInputTensor") ||
            !check(this->api_->CreateTensorWithDataAsOrtValue(
                       this->memory_info_, output_data, output_bytes,
                       this->output_shape_.data(), this->output_rank_,
                       this->output_type_, &this->output_value_),
                "CreateOutputTensor") ||
            !check(this->api_->CreateIoBinding(this->session_, &this->binding_),
                "CreateIoBinding") ||
            !check(this->api_->BindInput(this->binding_,
                       this->input_name_.c_str(), this->input_value_),
                "BindInput") ||
            !check(this->api_->BindOutput(this->binding_,
                       this->output_name_.c_str(), this->output_value_),
                "BindOutput") ||
            !run())
            return failModelCurrent();
        return true;
    }

    using GetApiBase = const OrtApiBase*(ORT_API_CALL*)();
#ifdef _WIN32
    HMODULE library_ = nullptr;
#else
    void* library_ = nullptr;
#endif
    const OrtApi* api_ = nullptr;
    OrtEnv* env_ = nullptr;
    OrtSessionOptions* options_ = nullptr;
    OrtSession* session_ = nullptr;
    OrtMemoryInfo* memory_info_ = nullptr;
    OrtValue* input_value_ = nullptr;
    OrtValue* output_value_ = nullptr;
    OrtIoBinding* binding_ = nullptr;
    std::string input_name_, output_name_, error_;
    std::array<std::int64_t, 3> input_shape_{}, output_shape_{};
    ONNXTensorElementDataType input_type_ =
        ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
    ONNXTensorElementDataType output_type_ =
        ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
    std::size_t input_rank_ = 0, output_rank_ = 0, input_size_ = 0,
                output_size_ = 0;
    std::vector<float> input_f32_, output_f32_;
    std::vector<double> input_f64_, output_f64_;

    bool fail(const char* message) {
        this->error_.assign(message, std::char_traits<char>::length(message));
        return false;
    }

    bool check(OrtStatus* status, const char* message) {
        if (!status) return true;
        try {
            this->error_ = message;
            const char* detail = this->api_->GetErrorMessage(status);
            if (detail && detail[0]) this->error_ += std::string(": ") + detail;
        } catch (...) {
            this->api_->ReleaseStatus(status);
            throw;
        }
        this->api_->ReleaseStatus(status);
        return false;
    }

    bool failLibrary(const char* message) {
        if (this->error_.empty()) fail(message);
        unload();
        return false;
    }

    bool failModel(const char* message) {
        fail(message);
        releaseModel();
        return false;
    }

    bool failModelCurrent() {
        releaseModel();
        return false;
    }

    void releaseModel() noexcept {
        if (this->api_) {
            if (this->binding_) this->api_->ReleaseIoBinding(this->binding_);
            if (this->output_value_)
                this->api_->ReleaseValue(this->output_value_);
            if (this->input_value_)
                this->api_->ReleaseValue(this->input_value_);
            if (this->memory_info_)
                this->api_->ReleaseMemoryInfo(this->memory_info_);
            if (this->session_) this->api_->ReleaseSession(this->session_);
        }
        this->binding_ = nullptr;
        this->output_value_ = this->input_value_ = nullptr;
        this->memory_info_ = nullptr;
        this->session_ = nullptr;
        this->input_name_.clear();
        this->output_name_.clear();
        this->input_size_ = this->output_size_ = this->input_rank_ =
            this->output_rank_ = 0;
        this->input_f32_.clear();
        this->input_f64_.clear();
        this->output_f32_.clear();
        this->output_f64_.clear();
    }

    bool readName(bool input, std::string& name) {
        OrtAllocator* allocator = nullptr;
        if (!check(this->api_->GetAllocatorWithDefaultOptions(&allocator),
                "GetAllocator") ||
            !allocator)
            return false;
        char* value = nullptr;
        OrtStatus* status = input ? this->api_->SessionGetInputName(
                                        this->session_, 0, allocator, &value)
                                  : this->api_->SessionGetOutputName(
                                        this->session_, 0, allocator, &value);
        if (!check(status, "ReadTensorName") || !value) return false;
        try {
            name = value;
        } catch (...) {
            allocator->Free(allocator, value);
            throw;
        }
        allocator->Free(allocator, value);
        return true;
    }

    bool readSpec(bool input, std::array<std::int64_t, 3>& shape,
        ONNXTensorElementDataType& type, std::size_t& rank) {
        OrtTypeInfo* type_info = nullptr;
        const OrtStatus* status = nullptr;
        status = input
            ? this->api_->SessionGetInputTypeInfo(this->session_, 0, &type_info)
            : this->api_->SessionGetOutputTypeInfo(
                  this->session_, 0, &type_info);
        if (!check(const_cast<OrtStatus*>(status), "ReadTensorType") ||
            !type_info)
            return false;
        const OrtTensorTypeAndShapeInfo* tensor_info = nullptr;
        bool ok =
            check(this->api_->CastTypeInfoToTensorInfo(type_info, &tensor_info),
                "ReadTensorInfo") &&
            tensor_info;
        if (ok)
            ok = check(this->api_->GetTensorElementType(tensor_info, &type),
                "ReadTensorElementType");
        if (ok)
            ok = check(this->api_->GetDimensionsCount(tensor_info, &rank),
                "ReadTensorRank");
        if (ok && rank <= shape.size())
            ok = check(
                this->api_->GetDimensions(tensor_info, shape.data(), rank),
                "ReadTensorShape");
        else if (ok)
            ok = fail("Model tensor rank is unsupported");
        this->api_->ReleaseTypeInfo(type_info);
        return ok;
    }

    static bool validBatch(std::int64_t value) noexcept {
        return value == -1 || value == 1;
    }

    static bool validType(ONNXTensorElementDataType type) noexcept {
        return type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
            type == ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE;
    }

    static bool fixedElements(const std::array<std::int64_t, 3>& shape,
        std::size_t rank, std::size_t& count) noexcept {
        count = 1;
        for (std::size_t i = 0; i < rank; ++i) {
            if (shape[i] <= 0 ||
                count > static_cast<std::size_t>(-1) /
                        static_cast<std::size_t>(shape[i]))
                return false;
            count *= static_cast<std::size_t>(shape[i]);
        }
        return true;
    }

    bool run() {
        return check(
            this->api_->RunWithBinding(this->session_, nullptr, this->binding_),
            "RunModel");
    }
};

}  // namespace onnxengine::detail

#endif  // ONNXENGINE_DETAIL_SESSION_HPP
