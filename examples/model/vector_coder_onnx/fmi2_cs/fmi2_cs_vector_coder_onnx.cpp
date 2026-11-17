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
constexpr std::array<fmi2Real, 3> kManualInputValues = {1.1, 1.2, 1.3};

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

void runExample(const std::array<fmi2Real, 3>& input_values) {
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
    ComponentGuard component(
        fmi2Instantiate("fmi2_cs_vector_coder_onnx_example", fmi2CoSimulation,
            "{fmi2_cs_vector_coder_onnx-example}", resource_uri.c_str(),
            &callbacks, fmi2False, fmi2False));
    if (!component.get()) {
        throw std::runtime_error("fmi2Instantiate failed.");
    }

    checkStatus("fmi2SetupExperiment",
        fmi2SetupExperiment(
            component.get(), fmi2False, 0.0, 0.0, fmi2False, 0.0));
    checkStatus("fmi2EnterInitializationMode",
        fmi2EnterInitializationMode(component.get()));
    checkStatus("fmi2ExitInitializationMode",
        fmi2ExitInitializationMode(component.get()));

    const fmi2ValueReference input_references[] = {1, 2, 3};
    checkStatus("fmi2SetReal",
        fmi2SetReal(component.get(), input_references, input_values.size(),
            input_values.data()));
    checkStatus("fmi2DoStep", fmi2DoStep(component.get(), 0.0, 0.1, fmi2True));

    const fmi2ValueReference output_references[] = {4, 5};
    fmi2Real output_values[] = {0.0, 0.0};
    checkStatus("fmi2GetReal",
        fmi2GetReal(component.get(), output_references, 2, output_values));

    checkStatus("fmi2Terminate", fmi2Terminate(component.get()));
    std::cout << "Input values: " << input_values[0] << ", " << input_values[1];
    std::cout << ", " << input_values[2] << '\n';
    std::cout << "Output values: " << output_values[0];
    std::cout << ", " << output_values[1] << '\n';
}

}  // namespace

int main() {
    try {
        runExample(kManualInputValues);
    } catch (const std::exception& error) {
        std::cerr << "FMI example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
