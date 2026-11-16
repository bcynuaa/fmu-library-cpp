#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "fmi2/fmi2_declare.h"

#ifndef FMI_EXAMPLE_RESOURCE_DIRECTORY
#error "FMI_EXAMPLE_RESOURCE_DIRECTORY must be defined by CMake."
#endif

namespace {

constexpr char kResourceDirectory[] = FMI_EXAMPLE_RESOURCE_DIRECTORY;

// * valueReference layout (1-based), see fmi2_cs_window_pure_onnx.cpp.
constexpr std::size_t kInputCount = 3;
constexpr std::size_t kOutputCount = 2;
constexpr std::array<fmi2ValueReference, kInputCount> kInputReferences = {
    1, 2, 3};
constexpr std::array<fmi2ValueReference, kOutputCount> kOutputReferences = {
    4, 5};
constexpr std::array<fmi2ValueReference, kOutputCount>
    kInitialOutputReferences = {6, 7};

// * Set before exitInitializationMode; they seed the whole window.
constexpr std::array<fmi2Real, kInputCount> kInitialInputValues = {
    1.0, 1.0, 1.0};
constexpr std::array<fmi2Real, kOutputCount> kInitialOutputValues = {0.5, -0.5};

// * One row per doStep. Keep in sync with ../export_model.py.
constexpr std::array<std::array<fmi2Real, kInputCount>, 3> kStepInputValues = {{
    {1.1, 1.2, 1.3},
    {1.4, 1.5, 1.6},
    {1.7, 1.8, 1.9},
}};

void* FMI2_CALL allocateMemory(size_t count, size_t size) {
    return std::calloc(count, size);
}

void FMI2_CALL freeMemory(void* memory) { std::free(memory); }

void FMI2_CALL logMessage(fmi2ComponentEnvironment, fmi2String, fmi2Status,
    fmi2String, fmi2String, ...) {}

void FMI2_CALL stepFinished(fmi2ComponentEnvironment, fmi2Status) {}

class ComponentGuard {
    public:
    explicit ComponentGuard(fmi2Component component) : component_(component) {}

    ComponentGuard(const ComponentGuard&) = delete;
    ComponentGuard& operator=(const ComponentGuard&) = delete;

    ~ComponentGuard() {
        if (this->component_) fmi2FreeInstance(this->component_);
    }

    fmi2Component get() const noexcept { return this->component_; }

    private:
    fmi2Component component_;
};

std::string fileUri(const std::filesystem::path& path) {
    const std::string generic_path =
        std::filesystem::absolute(path).lexically_normal().generic_u8string();

#ifdef _WIN32
    std::string uri = generic_path.rfind("//", 0) == 0 ? "file:" : "file:///";
#else
    std::string uri = "file://";
#endif

    constexpr char hex_digits[] = "0123456789ABCDEF";
    for (const unsigned char character : generic_path) {
        const bool is_unreserved = (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '-' ||
            character == '_' || character == '.' || character == '~';
        if (is_unreserved || character == '/' || character == ':') {
            uri.push_back(static_cast<char>(character));
        } else {
            uri.push_back('%');
            uri.push_back(hex_digits[character >> 4]);
            uri.push_back(hex_digits[character & 0x0f]);
        }
    }
    return uri;
}

void checkStatus(const char* function_name, fmi2Status status) {
    if (status != fmi2OK) {
        throw std::runtime_error(std::string(function_name) +
            " returned FMI status " + std::to_string(static_cast<int>(status)) +
            ".");
    }
}

void printValues(
    const char* label, const std::array<fmi2Real, kOutputCount>& values) {
    std::cout << "  " << label << ":";
    for (const fmi2Real value : values) std::cout << ' ' << value;
    std::cout << '\n';
}

void runExample() {
    const std::filesystem::path resource_directory(kResourceDirectory);
    if (!std::filesystem::is_directory(resource_directory)) {
        throw std::runtime_error("FMU resource directory does not exist: " +
            resource_directory.string());
    }

    const fmi2String version = fmi2GetVersion();
    if (!version || std::string(version) != "2.0") {
        throw std::runtime_error(
            "Linked library did not report FMI version 2.0.");
    }

    const fmi2CallbackFunctions callbacks = {
        logMessage,
        allocateMemory,
        freeMemory,
        stepFinished,
        nullptr,
    };
    const std::string resource_uri = fileUri(resource_directory);
    ComponentGuard component(fmi2Instantiate("fmi2_cs_window_pure_onnx_example",
        fmi2CoSimulation, "{fmi2_cs_window_pure_onnx-example}",
        resource_uri.c_str(), &callbacks, fmi2False, fmi2False));
    if (!component.get()) {
        throw std::runtime_error("fmi2Instantiate failed.");
    }

    checkStatus("fmi2SetupExperiment",
        fmi2SetupExperiment(
            component.get(), fmi2False, 0.0, 0.0, fmi2False, 0.0));
    checkStatus("fmi2EnterInitializationMode",
        fmi2EnterInitializationMode(component.get()));

    // * Seed both the inputs and the initial outputs before warm start runs.
    checkStatus("fmi2SetReal(inputs)",
        fmi2SetReal(component.get(), kInputReferences.data(), kInputCount,
            kInitialInputValues.data()));
    checkStatus("fmi2SetReal(y0)",
        fmi2SetReal(component.get(), kInitialOutputReferences.data(),
            kOutputCount, kInitialOutputValues.data()));

    checkStatus("fmi2ExitInitializationMode",
        fmi2ExitInitializationMode(component.get()));

    std::array<fmi2Real, kOutputCount> output_values = {0.0, 0.0};
    checkStatus("fmi2GetReal",
        fmi2GetReal(component.get(), kOutputReferences.data(), kOutputCount,
            output_values.data()));

    std::cout << "Inputs  : " << kInitialInputValues[0] << ", "
              << kInitialInputValues[1] << ", " << kInitialInputValues[2]
              << '\n';
    std::cout << "Outputs (init = injected y0, no inference):\n";
    printValues("init", output_values);

    fmi2Real time = 0.0;
    for (std::size_t step = 0; step < kStepInputValues.size(); ++step) {
        const auto& step_inputs = kStepInputValues[step];
        checkStatus("fmi2SetReal(inputs)",
            fmi2SetReal(component.get(), kInputReferences.data(), kInputCount,
                step_inputs.data()));
        checkStatus(
            "fmi2DoStep", fmi2DoStep(component.get(), time, 0.1, fmi2True));
        time += 0.1;
        checkStatus("fmi2GetReal",
            fmi2GetReal(component.get(), kOutputReferences.data(), kOutputCount,
                output_values.data()));
        const std::string label = "step " + std::to_string(step + 1) +
            " (u=" + std::to_string(step_inputs[0]) + "," +
            std::to_string(step_inputs[1]) + "," +
            std::to_string(step_inputs[2]) + ")";
        printValues(label.c_str(), output_values);
    }

    checkStatus("fmi2Terminate", fmi2Terminate(component.get()));
}

}  // namespace

int main() {
    try {
        runExample();
    } catch (const std::exception& error) {
        std::cerr << "FMI example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
