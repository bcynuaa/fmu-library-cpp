#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "fmi3/fmi3_declare.h"

#ifndef FMI_EXAMPLE_RESOURCE_DIRECTORY
#error "FMI_EXAMPLE_RESOURCE_DIRECTORY must be defined by CMake."
#endif

namespace {

constexpr char kResourceDirectory[] = FMI_EXAMPLE_RESOURCE_DIRECTORY;

// * The FMI 3 instantiation token travels from modelDescription.xml to
// * fmi3InstantiateCoSimulation, but the model does not validate it. Any
// * non-empty string works here.
constexpr char kExampleToken[] = "example-token";
constexpr std::array<fmi3Float64, 3> kManualInputValues = {1.1, 1.2, 1.3};

void logMessage(
    fmi3InstanceEnvironment, fmi3Status, fmi3String, fmi3String message) {
    if (message) std::cerr << message << '\n';
}

class InstanceGuard {
    public:
    explicit InstanceGuard(fmi3Instance component) : component_(component) {}

    InstanceGuard(const InstanceGuard&) = delete;
    InstanceGuard& operator=(const InstanceGuard&) = delete;

    ~InstanceGuard() {
        if (this->component_) fmi3FreeInstance(this->component_);
    }

    fmi3Instance get() const noexcept { return this->component_; }

    private:
    fmi3Instance component_;
};

void checkStatus(const char* function_name, fmi3Status status) {
    if (status != fmi3OK) {
        throw std::runtime_error(std::string(function_name) +
            " returned FMI status " + std::to_string(static_cast<int>(status)) +
            ".");
    }
}

double doStep(fmi3Instance instance, double time) {
    fmi3Boolean event_handling_needed = fmi3False;
    fmi3Boolean terminate_simulation = fmi3False;
    fmi3Boolean early_return = fmi3False;
    fmi3Float64 last_successful_time = time;
    checkStatus("fmi3DoStep",
        fmi3DoStep(instance, time, 0.1, fmi3True, &event_handling_needed,
            &terminate_simulation, &early_return, &last_successful_time));
    if (event_handling_needed || terminate_simulation || early_return ||
        std::abs(last_successful_time - (time + 0.1)) > 1e-12) {
        throw std::runtime_error("Unexpected FMI 3 step result.");
    }
    return last_successful_time;
}

void runExample(const std::array<fmi3Float64, 3>& input_values) {
    const auto resource_directory = std::filesystem::u8path(kResourceDirectory);
    if (!std::filesystem::is_directory(resource_directory)) {
        throw std::runtime_error("FMU resource directory does not exist: " +
            resource_directory.string());
    }

    const fmi3String version = fmi3GetVersion();
    if (!version || std::string(version) != "3.0") {
        throw std::runtime_error(
            "Linked library did not report FMI version 3.0.");
    }

    const std::string resource_path =
        std::filesystem::absolute(resource_directory).u8string();
    InstanceGuard component(fmi3InstantiateCoSimulation(
        "fmi3_cs_vector_scaler_onnx_example", kExampleToken,
        resource_path.c_str(), fmi3False, fmi3False, fmi3False, fmi3False,
        nullptr, 0, nullptr, logMessage, nullptr));
    if (!component.get()) {
        throw std::runtime_error("fmi3InstantiateCoSimulation failed.");
    }

    checkStatus("fmi3EnterInitializationMode",
        fmi3EnterInitializationMode(
            component.get(), fmi3False, 0.0, 0.0, fmi3False, 0.0));
    checkStatus("fmi3ExitInitializationMode",
        fmi3ExitInitializationMode(component.get()));

    const fmi3ValueReference input_references[] = {1, 2, 3};
    checkStatus("fmi3SetFloat64",
        fmi3SetFloat64(component.get(), input_references, input_values.size(),
            input_values.data(), input_values.size()));
    doStep(component.get(), 0.0);

    const fmi3ValueReference output_references[] = {4, 5};
    fmi3Float64 output_values[] = {0.0, 0.0};
    checkStatus("fmi3GetFloat64",
        fmi3GetFloat64(
            component.get(), output_references, 2, output_values, 2));

    checkStatus("fmi3Terminate", fmi3Terminate(component.get()));
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
