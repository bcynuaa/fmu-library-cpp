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

// * valueReference layout (1-based), see fmi3_cs_window_pure_onnx.cpp.
constexpr std::size_t kInputCount = 3;
constexpr std::size_t kOutputCount = 2;
constexpr std::array<fmi3ValueReference, kInputCount> kInputReferences = {
    1, 2, 3};
constexpr std::array<fmi3ValueReference, kOutputCount> kOutputReferences = {
    4, 5};
constexpr std::array<fmi3ValueReference, kOutputCount>
    kInitialOutputReferences = {6, 7};

// * Set before exitInitializationMode; they seed the whole window.
constexpr std::array<fmi3Float64, kInputCount> kInitialInputValues = {
    1.0, 1.0, 1.0};
constexpr std::array<fmi3Float64, kOutputCount> kInitialOutputValues = {
    0.5, -0.5};

// * One row per doStep. Keep in sync with ../export_model.py.
constexpr std::array<std::array<fmi3Float64, kInputCount>, 3> kStepInputValues =
    {{
        {1.1, 1.2, 1.3},
        {1.4, 1.5, 1.6},
        {1.7, 1.8, 1.9},
    }};

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

void printValues(
    const char* label, const std::array<fmi3Float64, kOutputCount>& values) {
    std::cout << "  " << label << ":";
    for (const fmi3Float64 value : values) std::cout << ' ' << value;
    std::cout << '\n';
}

void runExample() {
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
        "fmi3_cs_window_pure_onnx_example", kExampleToken,
        resource_path.c_str(), fmi3False, fmi3False, fmi3False, fmi3False,
        nullptr, 0, nullptr, logMessage, nullptr));
    if (!component.get()) {
        throw std::runtime_error("fmi3InstantiateCoSimulation failed.");
    }

    checkStatus("fmi3EnterInitializationMode",
        fmi3EnterInitializationMode(
            component.get(), fmi3False, 0.0, 0.0, fmi3False, 0.0));

    // * Seed both the inputs and the initial outputs before warm start runs.
    checkStatus("fmi3SetFloat64(inputs)",
        fmi3SetFloat64(component.get(), kInputReferences.data(), kInputCount,
            kInitialInputValues.data(), kInputCount));
    checkStatus("fmi3SetFloat64(y0)",
        fmi3SetFloat64(component.get(), kInitialOutputReferences.data(),
            kOutputCount, kInitialOutputValues.data(), kOutputCount));

    checkStatus("fmi3ExitInitializationMode",
        fmi3ExitInitializationMode(component.get()));

    std::array<fmi3Float64, kOutputCount> output_values = {0.0, 0.0};
    checkStatus("fmi3GetFloat64",
        fmi3GetFloat64(component.get(), kOutputReferences.data(), kOutputCount,
            output_values.data(), kOutputCount));

    std::cout << "Inputs  : " << kInitialInputValues[0] << ", "
              << kInitialInputValues[1] << ", " << kInitialInputValues[2]
              << '\n';
    std::cout << "Outputs (init = injected y0, no inference):\n";
    printValues("init", output_values);

    fmi3Float64 time = 0.0;
    for (std::size_t step = 0; step < kStepInputValues.size(); ++step) {
        const auto& step_inputs = kStepInputValues[step];
        checkStatus("fmi3SetFloat64(inputs)",
            fmi3SetFloat64(component.get(), kInputReferences.data(),
                kInputCount, step_inputs.data(), kInputCount));
        time = doStep(component.get(), time);
        checkStatus("fmi3GetFloat64",
            fmi3GetFloat64(component.get(), kOutputReferences.data(),
                kOutputCount, output_values.data(), kOutputCount));
        const std::string label = "step " + std::to_string(step + 1) +
            " (u=" + std::to_string(step_inputs[0]) + "," +
            std::to_string(step_inputs[1]) + "," +
            std::to_string(step_inputs[2]) + ")";
        printValues(label.c_str(), output_values);
    }

    checkStatus("fmi3Terminate", fmi3Terminate(component.get()));
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
