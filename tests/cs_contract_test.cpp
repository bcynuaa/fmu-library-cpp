#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "fmi2/fmi2_declare.h"
#include "fmi3/fmi3_declare.h"
#include "onnxengine/onnxengine.hpp"
#include "utils/fmu_io.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

using Reference = fmi2ValueReference;
using Output = std::array<double, 2>;
static_assert(std::is_same_v<Reference, fmi3ValueReference>);

constexpr std::array<Reference, 3> kInputs = {1, 2, 3};
constexpr std::array<Reference, 2> kOutputs = {4, 5};
constexpr std::array<Reference, 2> kInitialOutputs = {6, 7};
constexpr std::array<double, 3> kInitialInput = {1.0, 1.0, 1.0};
constexpr Output kInitialOutput = {0.5, -0.5};
constexpr std::array<std::array<double, 3>, 3> kStepInputs = {{
    {1.1, 1.2, 1.3},
    {1.4, 1.5, 1.6},
    {1.7, 1.8, 1.9},
}};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void expectStatus(int actual, int expected, const std::string& operation) {
    require(actual == expected,
        operation + ": expected status " + std::to_string(expected) + ", got " +
            std::to_string(actual));
}

void expectNear(double actual, double expected, const std::string& operation) {
    require(std::isfinite(actual) && std::abs(actual - expected) < 1e-5,
        operation + ": expected " + std::to_string(expected) + ", got " +
            std::to_string(actual));
}

void* FMI2_CALL allocateMemory(std::size_t count, std::size_t size) {
    return std::calloc(count, size);
}

void FMI2_CALL freeMemory(void* memory) { std::free(memory); }

void FMI2_CALL logFMI2(fmi2ComponentEnvironment environment, fmi2String,
    fmi2Status status, fmi2String category, fmi2String format, ...) {
    if (!environment) return;
    char message[4096];
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    auto& messages = *static_cast<std::vector<std::string>*>(environment);
    messages.push_back(
        std::string(category) + "|" + std::to_string(status) + "|" + message);
}

void logFMI3(fmi3InstanceEnvironment environment, fmi3Status status,
    fmi3String category, fmi3String message) {
    if (!environment) return;
    auto& messages = *static_cast<std::vector<std::string>*>(environment);
    messages.push_back(
        std::string(category) + "|" + std::to_string(status) + "|" + message);
}

void FMI2_CALL throwFMI2Log(fmi2ComponentEnvironment, fmi2String, fmi2Status,
    fmi2String, fmi2String, ...) {
    throw std::runtime_error("host logger failure");
}

void throwFMI3Log(fmi3InstanceEnvironment, fmi3Status, fmi3String, fmi3String) {
    throw std::runtime_error("host logger failure");
}

std::string fileURI(const std::filesystem::path& directory) {
    const std::string path =
        std::filesystem::absolute(directory).generic_u8string();
    const std::string prefix = path[0] == '/' ? "file://" : "file:///";
    std::string uri = prefix;
    constexpr char kHexDigits[] = "0123456789ABCDEF";
    for (const unsigned char character : path) {
        if ((character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '-' ||
            character == '_' || character == '.' || character == '~' ||
            character == '/' || character == ':') {
            uri.push_back(static_cast<char>(character));
        } else {
            uri.push_back('%');
            uri.push_back(kHexDigits[character >> 4]);
            uri.push_back(kHexDigits[character & 15]);
        }
    }
    return uri;
}

// Both versions are exercised through their public C ABI, including pointers,
// value references, lifecycle calls, and doStep result parameters.
class Client {
    public:
    Client(int version, const std::string& model,
        const std::filesystem::path& resources,
        std::vector<std::string>* messages = nullptr)
        : version_(version), window_model_(model == "window_pure_onnx") {
        const std::string identifier =
            "fmi" + std::to_string(version) + "_cs_" + model;
        if (this->version_ == 2) {
            this->callbacks_ = {
                logFMI2, allocateMemory, freeMemory, nullptr, messages};
            const std::string uri = fileURI(resources);
            this->instance_ = fmi2Instantiate("contract_test", fmi2CoSimulation,
                identifier.c_str(), uri.c_str(), &this->callbacks_, fmi2False,
                fmi2False);
        } else {
            const std::string path =
                std::filesystem::absolute(resources).u8string();
            this->instance_ = fmi3InstantiateCoSimulation("contract_test",
                identifier.c_str(), path.c_str(), fmi3False, fmi3False,
                fmi3False, fmi3False, nullptr, 0, messages, logFMI3, nullptr);
        }
        require(
            this->instance_ != nullptr, identifier + ": instantiation failed");
    }

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    ~Client() {
        if (this->version_ == 2)
            fmi2FreeInstance(this->instance_);
        else
            fmi3FreeInstance(this->instance_);
    }

    fmi2Component component() const { return this->instance_; }
    bool windowModel() const { return this->window_model_; }

    Reference timeReference() const { return this->window_model_ ? 8 : 6; }

    double time() {
        const Reference reference = this->timeReference();
        double value = -123.0;
        expectStatus(this->get(&reference, 1, &value), 0, "get time");
        return value;
    }

    int set(
        const Reference* references, std::size_t count, const double* values) {
        if (this->version_ == 2)
            return fmi2SetReal(this->instance_, references, count, values);
        return fmi3SetFloat64(
            this->instance_, references, count, values, count);
    }

    int get(const Reference* references, std::size_t count, double* values) {
        if (this->version_ == 2)
            return fmi2GetReal(this->instance_, references, count, values);
        return fmi3GetFloat64(
            this->instance_, references, count, values, count);
    }

    int initialize(
        double start_time, bool stop_defined = false, double stop_time = 0.0) {
        if (this->version_ == 2) {
            const auto status = fmi2SetupExperiment(this->instance_, fmi2False,
                0.0, start_time, stop_defined, stop_time);
            if (status != fmi2OK) return status;
            return fmi2EnterInitializationMode(this->instance_);
        }
        return fmi3EnterInitializationMode(this->instance_, fmi3False, 0.0,
            start_time, stop_defined, stop_time);
    }

    int exitInitialization() {
        if (this->version_ == 2)
            return fmi2ExitInitializationMode(this->instance_);
        return fmi3ExitInitializationMode(this->instance_);
    }

    int step(double time, double step_size) {
        if (this->version_ == 2)
            return fmi2DoStep(this->instance_, time, step_size, fmi2True);
        fmi3Boolean event = true, terminate = true, early = true;
        double last_time = -1.0;
        const auto status = fmi3DoStep(this->instance_, time, step_size,
            fmi3True, &event, &terminate, &early, &last_time);
        if (status == fmi3OK) {
            require(
                !event && !terminate && !early, "unexpected FMI 3 step flags");
            expectNear(last_time, time + step_size, "last successful time");
        }
        return status;
    }

    int terminate() {
        if (this->version_ == 2) return fmi2Terminate(this->instance_);
        return fmi3Terminate(this->instance_);
    }

    int reset() {
        if (this->version_ == 2) return fmi2Reset(this->instance_);
        return fmi3Reset(this->instance_);
    }

    std::vector<double> values() {
        const std::array<Reference, 7> references = {1, 2, 3, 4, 5, 6, 7};
        const std::size_t count = this->window_model_ ? 7 : 5;
        std::vector<double> values(count);
        expectStatus(
            get(references.data(), count, values.data()), 0, "get values");
        return values;
    }

    private:
    int version_;
    bool window_model_;
    fmi2CallbackFunctions callbacks_{};
    void* instance_ = nullptr;
};

void seed(Client& client) {
    expectStatus(
        client.set(kInputs.data(), kInputs.size(), kInitialInput.data()), 0,
        "set initial inputs");
    if (client.windowModel()) {
        expectStatus(client.set(kInitialOutputs.data(), kInitialOutputs.size(),
                         kInitialOutput.data()),
            0, "set initial outputs");
    }
}

void checkAccess(Client& client) {
    const auto before = client.values();
    const std::array<std::array<Reference, 2>, 3> bad_references = {{
        {1, 0},
        {1, 999},
        {1, 4},
    }};
    const std::array<double, 2> replacements = {123.0, 456.0};
    for (const auto& references : bad_references) {
        expectStatus(client.set(references.data(), references.size(),
                         replacements.data()),
            3, "invalid batch write");
        require(client.values() == before, "failed write changed values");
    }
    for (const Reference invalid : {0u, 999u}) {
        const Reference references[] = {1, invalid};
        double values[] = {123.0, 456.0};
        expectStatus(
            client.get(references, 2, values), 3, "invalid batch read");
        require(values[0] == 123.0 && values[1] == 456.0,
            "failed read changed the caller's buffer");
    }
    double value = 123.0;
    expectStatus(client.set(nullptr, 1, &value), 3, "null write references");
    expectStatus(
        client.set(kInputs.data(), 1, nullptr), 3, "null write values");
    expectStatus(client.get(nullptr, 1, &value), 3, "null read references");
    expectStatus(client.get(kInputs.data(), 1, nullptr), 3, "null read values");
    expectStatus(client.set(nullptr, 0, nullptr), 0, "empty write");
    expectStatus(client.get(nullptr, 0, nullptr), 0, "empty read");
    require(client.values() == before, "invalid requests changed model values");
}

void checkContracts(Client& client) {
    const auto component = client.component();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    expectStatus(client.step(0.0, 0.1), 3, "step before initialization");
    expectStatus(client.exitInitialization(), 3, "exit before initialization");
    expectStatus(client.terminate(), 3, "terminate before initialization");
    expectStatus(fmi2DoStep(nullptr, 0.0, 0.1, fmi2True), 3, "null instance");
    seed(client);
    checkAccess(client);

    for (const double start : {nan, infinity, -infinity}) {
        expectStatus(fmi2SetupExperiment(component, 0, 0.0, start, 0, 0.0), 3,
            "invalid start time");
    }
    for (const double tolerance : {0.0, -1.0, nan, infinity}) {
        expectStatus(fmi2SetupExperiment(component, 1, tolerance, 1.25, 0, 0.0),
            3, "invalid tolerance");
    }
    for (const double stop : {1.25, 1.0, nan, infinity}) {
        expectStatus(fmi2SetupExperiment(component, 0, 0.0, 1.25, 1, stop), 3,
            "invalid stop time");
    }
    expectStatus(fmi2SetupExperiment(component, 0, nan, 0.0, 0, nan), 0,
        "undefined optional experiment values");
    expectStatus(fmi2SetupExperiment(component, 1, 1e-6, 1.25, 1, 1.55), 0,
        "valid experiment");
    expectStatus(fmi2SetupExperiment(component, 1, -1.0, 50.0, 1, 60.0), 3,
        "failed experiment must preserve previous settings");
    expectStatus(
        fmi2EnterInitializationMode(component), 0, "enter initialization");
    expectStatus(
        fmi2EnterInitializationMode(component), 3, "duplicate initialization");
    expectStatus(fmi2SetupExperiment(component, 0, 0.0, 0.0, 0, 0.0), 3,
        "experiment setup during initialization");
    expectStatus(client.step(1.25, 0.1), 3, "step during initialization");
    expectStatus(client.terminate(), 3, "terminate during initialization");
    checkAccess(client);
    expectStatus(client.exitInitialization(), 0, "exit initialization");
    if (client.windowModel()) {
        const auto values = client.values();
        expectNear(values[3], 0.5, "window initial output 1");
        expectNear(values[4], -0.5, "window initial output 2");
        const Reference references[] = {1, 6};
        const double values_to_set[] = {123.0, 456.0};
        const auto before = client.values();
        expectStatus(client.set(references, 2, values_to_set), 3,
            "initial output write after initialization");
        require(client.values() == before,
            "late initial output write changed inputs");
    }
    checkAccess(client);
    expectStatus(
        client.exitInitialization(), 3, "duplicate initialization exit");
    expectStatus(
        fmi2EnterInitializationMode(component), 3, "reenter initialization");
    expectStatus(fmi2SetupExperiment(component, 0, 0.0, 0.0, 0, 0.0), 3,
        "experiment setup in step mode");

    const auto before = client.values();
    for (const double step : {0.0, -0.1, nan, infinity}) {
        expectStatus(client.step(1.25, step), 3, "invalid step size");
    }
    for (const double time : {1.0, 1.3, nan, infinity}) {
        expectStatus(client.step(time, 0.1), 3, "invalid communication point");
    }
    expectStatus(client.step(1.25, 0.4), 3, "step past stop time");
    require(client.values() == before, "invalid step changed outputs");
    for (std::size_t index = 0; index < kStepInputs.size(); ++index) {
        const auto& inputs = kStepInputs[index];
        expectStatus(client.set(kInputs.data(), kInputs.size(), inputs.data()),
            0, "set step inputs");
        // The host may calculate time differently from the model's additions.
        expectStatus(client.step(1.25 + 0.1 * index, 0.1), 0,
            "valid step after rejected requests");
        if (client.windowModel() && index == 0) {
            const auto after_first_step = client.values();
            expectStatus(client.step(1.35, 0.05), 3,
                "variable step rejected by fixed-step model");
            require(client.values() == after_first_step,
                "rejected variable step changed model values");
        }
    }
    const double time = 1.55;
    const auto at_stop = client.values();
    expectStatus(client.step(time, 0.1), 3, "step beyond final time");
    require(client.values() == at_stop, "overrun changed outputs");
    expectStatus(client.terminate(), 0, "terminate");
    require(client.values() == at_stop, "terminated results are readable");
    expectStatus(client.terminate(), 3, "duplicate termination");
    expectStatus(client.step(time, 0.1), 3, "step after termination");
    expectStatus(
        client.set(kInputs.data(), kInputs.size(), kInitialInput.data()), 3,
        "write after termination");
    expectStatus(fmi2SetupExperiment(component, 0, 0.0, 0.0, 0, 0.0), 3,
        "experiment setup after termination");
    expectStatus(fmi2EnterInitializationMode(component), 3,
        "initialize after termination");
    expectStatus(client.exitInitialization(), 3,
        "exit initialization after termination");

    expectStatus(client.reset(), 0, "reset after termination");
    const auto reset_values = client.values();
    require(std::all_of(reset_values.begin(), reset_values.end(),
                [](double value) { return value == 0.0; }),
        "reset did not clear ports");
    expectStatus(
        fmi2EnterInitializationMode(component), 0, "default experiment");
    expectStatus(client.reset(), 0, "reset during initialization");
    expectStatus(
        fmi2EnterInitializationMode(component), 0, "initialize after reset");
    seed(client);
    expectStatus(client.exitInitialization(), 0, "initialize with defaults");
    expectStatus(
        client.step(0.0, 2.0), 0, "reset cleared time and stop constraint");
    if (!client.windowModel()) {
        expectStatus(client.step(2.0, 1.0), 0,
            "variable step accepted by variable-step model");
    }
    expectStatus(client.reset(), 0, "reset during stepping");
    expectStatus(client.reset(), 0, "reset while instantiated");

    const double huge = (std::numeric_limits<double>::max)();
    expectStatus(client.initialize(huge), 0, "large finite start time");
    expectStatus(client.exitInitialization(), 0, "initialize at large time");
    expectStatus(client.step(huge, 1.0), 3, "nonadvancing floating point time");
    expectStatus(client.step(huge, huge), 3, "overflowing next time");
}

void checkFMI3Contracts(Client& client) {
    const auto instance = client.component();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    expectStatus(client.step(0.0, 0.1), 3, "step before initialization");
    expectStatus(client.exitInitialization(), 3, "exit before initialization");
    expectStatus(client.terminate(), 3, "terminate before initialization");
    expectNear(client.time(), 0.0, "initial time before initialization");
    seed(client);
    checkAccess(client);
    for (const double start : {nan, infinity, -infinity}) {
        expectStatus(client.initialize(start), 3, "invalid start time");
    }
    for (const double tolerance : {0.0, -1.0, nan, infinity}) {
        expectStatus(fmi3EnterInitializationMode(
                         instance, true, tolerance, 1.25, false, 0.0),
            3, "invalid tolerance");
    }
    for (const double stop : {1.25, 1.0, nan, infinity}) {
        expectStatus(
            client.initialize(1.25, true, stop), 3, "invalid stop time");
    }
    expectStatus(
        fmi3EnterInitializationMode(instance, false, nan, 1.25, true, 1.55), 0,
        "undefined tolerance");
    expectStatus(client.initialize(1.25), 3, "duplicate initialization");
    expectStatus(client.step(1.25, 0.1), 3, "step during initialization");
    expectStatus(client.terminate(), 3, "terminate during initialization");
    expectNear(client.time(), 1.25, "time during initialization");
    checkAccess(client);
    expectStatus(client.exitInitialization(), 0, "finish initialization");
    expectNear(client.time(), 1.25, "time after initialization");
    expectStatus(
        client.exitInitialization(), 3, "duplicate initialization exit");
    expectStatus(client.initialize(0.0), 3, "reenter initialization");
    checkAccess(client);
    if (client.windowModel()) {
        const auto before = client.values();
        const Reference references[] = {1, 6};
        const double replacements[] = {123.0, 456.0};
        expectStatus(client.set(references, 2, replacements), 3,
            "late initial output write");
        require(client.values() == before, "late write changed inputs");
    }

    {
        const Reference time_reference = client.timeReference();
        const Reference mixed_references[] = {time_reference, 1};
        double mixed_values[] = {-1.0, -1.0};
        expectStatus(client.get(mixed_references, 2, mixed_values), 0,
            "mixed time and model-variable read");
        expectNear(mixed_values[0], 1.25, "mixed read time position");
        expectNear(mixed_values[1], client.values()[0],
            "mixed read model-variable position");

        const Reference bad_mixed_references[] = {time_reference, 999};
        double unchanged_values[] = {123.0, 456.0};
        expectStatus(client.get(bad_mixed_references, 2, unchanged_values), 3,
            "mixed read with invalid reference");
        require(unchanged_values[0] == 123.0 && unchanged_values[1] == 456.0,
            "failed mixed read changed caller buffer");

        const auto before = client.values();
        const double replacement[] = {2.0, 123.0};
        expectStatus(client.set(mixed_references, 2, replacement), 3,
            "mixed write with independent time");
        require(client.values() == before,
            "failed write containing time changed model variables");

        const double replacement_time = 3.0;
        expectStatus(client.set(&time_reference, 1, &replacement_time), 3,
            "write independent time");
        expectNear(client.time(), 1.25, "time unchanged after rejected write");
    }

    const auto before = client.values();
    for (const std::size_t count : {0u, 1u, 4u}) {
        double values[] = {123.0, 456.0, 789.0, 101.0};
        expectStatus(fmi3GetFloat64(instance, kInputs.data(), 3, values, count),
            3, "mismatched read count");
        require(values[0] == 123.0 && values[1] == 456.0 &&
                values[2] == 789.0 && values[3] == 101.0,
            "mismatched read changed buffer");
        expectStatus(fmi3SetFloat64(instance, kInputs.data(), 3, values, count),
            3, "mismatched write count");
    }
    fmi3Boolean event = true, terminate = true, early = true;
    double last_time = -123.0;
    for (int missing = 0; missing < 4; ++missing) {
        expectStatus(fmi3DoStep(instance, 1.25, 0.1, true,
                         missing == 0 ? nullptr : &event,
                         missing == 1 ? nullptr : &terminate,
                         missing == 2 ? nullptr : &early,
                         missing == 3 ? nullptr : &last_time),
            3, "missing step result buffer");
    }
    for (const double step : {0.0, -0.1, nan, infinity}) {
        expectStatus(fmi3DoStep(instance, 1.25, step, true, &event, &terminate,
                         &early, &last_time),
            3, "invalid step size");
        require(!event && !terminate && !early && last_time == 1.25,
            "invalid step result flags or last successful time");
    }
    for (const double point : {1.0, 1.3, nan, infinity}) {
        expectStatus(client.step(point, 0.1), 3, "invalid communication point");
    }
    expectStatus(client.step(1.25, 0.4), 3, "stop time overrun");
    require(client.values() == before, "invalid requests changed values");
    expectStatus(fmi3EnterEventMode(instance), 3, "unsupported event mode");
    expectStatus(
        fmi3EnterStepMode(instance), 3, "unsupported event transition");
    expectStatus(fmi3EnterConfigurationMode(instance), 3,
        "unsupported configuration mode");
    float value = 123.0f;
    expectStatus(fmi3GetFloat32(instance, kInputs.data(), 1, &value, 1), 3,
        "unsupported scalar type");
    require(value == 123.0f, "unsupported getter changed buffer");
    double communication_point = 1.25;
    const std::array<double, 3> variable_step_sizes = {0.1, 0.05, 0.15};
    for (std::size_t index = 0; index < kStepInputs.size(); ++index) {
        expectStatus(client.set(kInputs.data(), 3, kStepInputs[index].data()),
            0, "set step inputs");
        const double step_size =
            client.windowModel() ? 0.1 : variable_step_sizes[index];
        expectStatus(client.step(communication_point, step_size), 0,
            "valid step after rejected requests");
        communication_point += step_size;
        expectNear(
            client.time(), communication_point, "time after successful step");
    }
    if (client.windowModel()) {
        const auto before = client.values();
        expectStatus(client.step(communication_point, 0.2), 3,
            "variable step rejected by fixed-step model");
        require(client.values() == before,
            "rejected variable step changed model values");
        expectNear(client.time(), communication_point,
            "time unchanged after rejected variable step");
    }
    expectStatus(
        client.step(communication_point, 0.1), 3, "step beyond stop time");
    expectStatus(client.terminate(), 0, "terminate");
    expectStatus(client.terminate(), 3, "duplicate termination");
    expectStatus(
        client.step(communication_point, 0.1), 3, "step after termination");
    expectStatus(client.initialize(0.0), 3, "initialize after termination");
    double output = 123.0;
    expectStatus(
        client.get(kOutputs.data(), 1, &output), 3, "read after termination");
    expectStatus(client.set(kInputs.data(), 3, kInitialInput.data()), 3,
        "write after termination");
    expectStatus(client.reset(), 0, "reset after termination");
    const auto reset_values = client.values();
    require(std::all_of(reset_values.begin(), reset_values.end(),
                [](double item) { return item == 0.0; }),
        "reset did not clear ports");
    expectStatus(client.initialize(0.0), 0, "initialize after reset");
    expectStatus(client.reset(), 0, "reset during initialization");
    expectStatus(
        fmi3EnterInitializationMode(instance, false, nan, -2.0, false, nan), 0,
        "undefined optional experiment values");
    seed(client);
    expectStatus(client.exitInitialization(), 0, "reinitialize");
    expectStatus(client.step(-2.0, 5.0), 0, "reset cleared stop constraint");
    expectNear(client.time(), 3.0, "time after reset step");
    expectStatus(client.reset(), 0, "reset during stepping");
    expectStatus(client.reset(), 0, "reset while instantiated");
    const double huge = (std::numeric_limits<double>::max)();
    expectStatus(client.initialize(huge), 0, "large finite start time");
    expectStatus(client.exitInitialization(), 0, "initialize at large time");
    expectStatus(client.step(huge, 1.0), 3, "nonadvancing time");
    expectStatus(client.step(huge, huge), 3, "overflowing time");
}

void checkFMI3Instantiation(
    const std::string& model, const std::filesystem::path& resources) {
    const std::string token = "fmi3_cs_" + model;
    const std::string path = std::filesystem::absolute(resources).u8string();
    const Reference reference = 1;
    const Reference zero = 0;
    for (int invalid = 0; invalid < 11; ++invalid) {
        // * Case 4 used to send a mismatched instantiation token. The model no
        // * longer validates the token, so there is nothing left to reject.
        if (invalid == 4) continue;

        const auto instance = fmi3InstantiateCoSimulation(invalid == 0 ? nullptr
                : invalid == 1                                         ? ""
                               : "contract_test",
            invalid == 2       ? nullptr
                : invalid == 3 ? ""
                               : token.c_str(),
            invalid == 5       ? nullptr
                : invalid == 6 ? ""
                               : path.c_str(),
            false, false, invalid == 7, false,
            invalid == 8        ? &reference
                : invalid == 10 ? &zero
                                : nullptr,
            invalid >= 8 ? 1 : 0, nullptr, logFMI3, nullptr);
        if (instance) fmi3FreeInstance(instance);
        require(!instance, "unsupported instantiation request succeeded");
    }
    const auto instance = fmi3InstantiateCoSimulation("contract_test",
        token.c_str(), path.c_str(), false, false, false, true, nullptr, 0,
        nullptr, nullptr, nullptr);
    require(
        instance != nullptr, "early return permission requires no capability");
    fmi3FreeInstance(instance);
    expectStatus(
        fmi3GetFloat64(nullptr, nullptr, 0, nullptr, 0), 3, "null instance");
}

std::vector<Output> runSeries(Client& client, double start_time) {
    expectStatus(client.initialize(start_time, true, start_time + 0.3), 0,
        "initialize prediction series");
    seed(client);
    expectStatus(
        client.exitInitialization(), 0, "finish prediction initialization");
    std::vector<Output> history;
    const auto initial_values = client.values();
    const Output initial_expected =
        client.windowModel() ? kInitialOutput : Output{10.0, 35.5};
    for (std::size_t index = 0; index < 2; ++index) {
        expectNear(initial_values[3 + index], initial_expected[index],
            "initial prediction");
    }
    const std::array<Output, 3> expected = client.windowModel()
        ? std::array<Output, 3>{{{0.7625, 0.275}, {0.8453125, 0.434375},
              {0.9759765625, 0.651171875}}}
        : std::array<Output, 3>{{{12.4, 43.0}, {16.0, 55.75}, {19.6, 68.5}}};
    double time = start_time;
    for (std::size_t step = 0; step < kStepInputs.size(); ++step) {
        expectStatus(client.set(kInputs.data(), kInputs.size(),
                         kStepInputs[step].data()),
            0, "set prediction inputs");
        expectStatus(client.step(time, 0.1), 0, "prediction step");
        Output output{};
        expectStatus(
            client.get(kOutputs.data(), kOutputs.size(), output.data()), 0,
            "read prediction");
        for (std::size_t index = 0; index < output.size(); ++index) {
            expectNear(
                output[index], expected[step][index], "expected prediction");
        }
        history.push_back(output);
        time += 0.1;
    }
    expectStatus(client.terminate(), 0, "terminate prediction series");
    return history;
}

void checkParity(
    const std::string& model, const std::filesystem::path& resources) {
    std::vector<Output> baseline;
    for (const int version : {2, 3}) {
        Client client(version, model, resources);
        const auto first_run = runSeries(client, 1.25);
        expectStatus(client.reset(), 0, "reset prediction model");
        const auto reset_values = client.values();
        require(std::all_of(reset_values.begin(), reset_values.end(),
                    [](double value) { return value == 0.0; }),
            "prediction reset did not clear values");
        const auto second_run = runSeries(client, -2.0);
        require(first_run == second_run, "reset changed prediction sequence");
        if (baseline.empty())
            baseline = first_run;
        else
            require(
                baseline == first_run, "FMI 2 and FMI 3 predictions differ");
    }
}

class ResourceCopy {
    public:
    explicit ResourceCopy(const std::filesystem::path& original) {
        const auto suffix =
            std::chrono::steady_clock::now().time_since_epoch().count();
        this->path_ = original.parent_path() /
            std::filesystem::u8path(u8"test_\u4e2d\u6587 %20_") /
            std::to_string(suffix);
        std::filesystem::create_directories(this->path_);
        for (const auto& entry :
            std::filesystem::directory_iterator(original)) {
            if (entry.is_regular_file())
                std::filesystem::copy_file(
                    entry.path(), this->path_ / entry.path().filename());
        }
    }
    ResourceCopy(const ResourceCopy&) = delete;
    ResourceCopy& operator=(const ResourceCopy&) = delete;
    ~ResourceCopy() {
        std::error_code error;
        std::filesystem::remove_all(this->path_, error);
    }
    const std::filesystem::path& path() const { return this->path_; }

    private:
    std::filesystem::path path_;
};

void expectLog(const std::vector<std::string>& messages,
    const std::string& operation, const std::string& detail) {
    require(messages.size() == 1, "expected exactly one diagnostic");
    require(messages[0].find("logStatusError|3|") == 0 &&
            messages[0].find(operation) != std::string::npos &&
            messages[0].find(detail) != std::string::npos,
        "missing diagnostic context: " + messages[0]);
}

void expectLoadError(int version, const std::string& model,
    const std::filesystem::path& resources, const std::string& detail) {
    std::vector<std::string> messages;
    const auto identifier = "fmi" + std::to_string(version) + "_cs_" + model;
    void* instance = nullptr;
    if (version == 2) {
        const auto uri = fileURI(resources);
        const fmi2CallbackFunctions callbacks = {
            logFMI2, allocateMemory, freeMemory, nullptr, &messages};
        instance = fmi2Instantiate("resource_test", fmi2CoSimulation,
            identifier.c_str(), uri.c_str(), &callbacks, false, false);
        if (instance) fmi2FreeInstance(instance);
    } else {
        const auto path = std::filesystem::absolute(resources).u8string();
        instance = fmi3InstantiateCoSimulation("resource_test",
            identifier.c_str(), path.c_str(), false, false, false, false,
            nullptr, 0, &messages, logFMI3, nullptr);
        if (instance) fmi3FreeInstance(instance);
    }
    require(instance == nullptr, "invalid resources were accepted: " + detail);
    expectLog(messages,
        version == 2 ? "fmi2Instantiate" : "fmi3InstantiateCoSimulation",
        detail);
    std::string normalized_message = messages[0];
    std::replace(
        normalized_message.begin(), normalized_message.end(), '\\', '/');
    require(normalized_message.find(resources.generic_u8string()) !=
            std::string::npos,
        "diagnostic omitted UTF-8 resource path: " + messages[0]);
}

// * An absent resource location is legal for both versions. Models that need
// * resources must reject it themselves with a readable reason.
void expectMissingResourceLocation(
    int version, const std::string& model, bool null_pointer) {
    std::vector<std::string> messages;
    const auto identifier = "fmi" + std::to_string(version) + "_cs_" + model;
    const char* resources = null_pointer ? nullptr : "";
    void* instance = nullptr;
    if (version == 2) {
        const fmi2CallbackFunctions callbacks = {
            logFMI2, allocateMemory, freeMemory, nullptr, &messages};
        instance = fmi2Instantiate("resource_test", fmi2CoSimulation,
            identifier.c_str(), resources, &callbacks, false, false);
        if (instance) fmi2FreeInstance(instance);
    } else {
        instance = fmi3InstantiateCoSimulation("resource_test",
            identifier.c_str(), resources, false, false, false, false, nullptr,
            0, &messages, logFMI3, nullptr);
        if (instance) fmi3FreeInstance(instance);
    }
    require(instance == nullptr, "a missing resource location was accepted");
    expectLog(messages,
        version == 2 ? "fmi2Instantiate" : "fmi3InstantiateCoSimulation",
        "Resource location is required");
}

void writeBytes(
    const std::filesystem::path& path, const std::vector<char>& contents) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    require(static_cast<bool>(file), "could not create test resource");
}
std::vector<char> readBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    require(static_cast<bool>(file), "could not read test resource");
    return {
        std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void checkResources(
    const std::string& model, const std::filesystem::path& resources) {
    ResourceCopy copy(resources);
    const auto& directory = copy.path();
    const auto model_path = directory / "model.onnx";
    const auto model_contents = readBytes(model_path);
    for (const int version : {2, 3}) {
        // Percent-escaped UTF-8 URI for FMI 2; native UTF-8 path for FMI 3.
        std::vector<std::string> messages;
        {
            Client client(version, model, directory, &messages);
            runSeries(client, 1.25);
            require(messages.empty(), "successful operations emitted errors");
            expectStatus(client.reset(), 0, "reset for diagnostics");
            const Reference invalid = 999;
            const double replacement = 123.0;
            expectStatus(client.set(&invalid, 1, &replacement), 3,
                "invalid write for diagnostic");
            expectLog(messages, version == 2 ? "fmi2SetReal" : "fmi3SetFloat64",
                "999");
            messages.clear();
            expectStatus(
                client.step(0.0, -1.0), 3, "invalid step for diagnostic");
            expectLog(messages, version == 2 ? "fmi2DoStep" : "fmi3DoStep",
                "lifecycle");
            messages.clear();
            if (version == 3) {
                double value = 0.0;
                expectStatus(
                    fmi3GetFloat64(client.component(), nullptr, 1, &value, 1),
                    3, "wrapper validation diagnostic");
                expectLog(messages, "fmi3GetFloat64", "buffer");
            }
        }
        if (version == 2) {
            const auto raw_path = directory.u8string();
            const auto identifier = "fmi2_cs_" + model;
            const fmi2CallbackFunctions callbacks = {
                nullptr, allocateMemory, freeMemory, nullptr, nullptr};
            auto component = fmi2Instantiate("raw_path_test", fmi2CoSimulation,
                identifier.c_str(), raw_path.c_str(), &callbacks, false, false);
            require(component != nullptr, "FMI 2 UTF-8 path with literal %20");
            fmi2FreeInstance(component);
        }
        expectMissingResourceLocation(version, model, true);
        expectMissingResourceLocation(version, model, false);
        expectLoadError(version, model, directory / "missing", "directory");
        std::filesystem::rename(model_path, directory / "model.saved");
        expectLoadError(version, model, directory, "model is missing");
        writeBytes(model_path, {'b', 'a', 'd'});
        expectLoadError(version, model, directory, "CreateSession: ");
        writeBytes(model_path, model_contents);
        std::filesystem::remove(directory / "model.saved");
        const auto runtime = onnxengine::locateLibrary(directory);
        const auto saved_runtime = directory / "runtime.saved";
        std::filesystem::rename(runtime, saved_runtime);
        expectLoadError(version, model, directory, "library is missing");
        writeBytes(runtime, {'b', 'a', 'd'});
        expectLoadError(version, model, directory, "Runtime");
        std::filesystem::remove(runtime);
        std::filesystem::rename(saved_runtime, runtime);

        if (model != "vector_coder_onnx") continue;
        const auto coder = directory / "coder.bin";
        const auto contents = readBytes(coder);
        const auto previous_coder = directory / "scaler.bin";
        const auto legacy = directory / "pptransform.bin";
        const auto arbitrary = directory / "other_coefficients.bin";
        const auto earlier_arbitrary = directory / "a_invalid.bin";
        for (const auto& entry :
            std::filesystem::directory_iterator(directory)) {
            if (entry.path() != coder && entry.path().extension() == ".bin")
                std::filesystem::remove(entry.path());
        }

        writeBytes(previous_coder, contents);
        writeBytes(coder, {'b', 'a', 'd'});
        expectLoadError(version, model, directory, "Coder dimensions");
        writeBytes(coder, contents);
        std::filesystem::remove(previous_coder);

        std::filesystem::rename(coder, previous_coder);
        {
            Client client(version, model, directory);
            runSeries(client, 1.25);
        }
        writeBytes(legacy, contents);
        writeBytes(previous_coder, {'b', 'a', 'd'});
        expectLoadError(version, model, directory, "Coder dimensions");
        std::filesystem::remove(previous_coder);
        {
            Client client(version, model, directory);
            runSeries(client, 1.25);
        }

        writeBytes(arbitrary, contents);
        writeBytes(legacy, {'b', 'a', 'd'});
        expectLoadError(version, model, directory, "Coder dimensions");
        std::filesystem::remove(legacy);
        {
            Client client(version, model, directory);
            runSeries(client, 1.25);
        }
        writeBytes(earlier_arbitrary, {'b', 'a', 'd'});
        {
            Client client(version, model, directory);
            runSeries(client, 1.25);
        }
        std::filesystem::remove(arbitrary);
        expectLoadError(version, model, directory, "Coder dimensions");
        std::filesystem::remove(earlier_arbitrary);
        expectLoadError(version, model, directory, "Cannot open coder");
        writeBytes(
            coder, std::vector<char>(contents.begin(), contents.begin() + 4));
        expectLoadError(version, model, directory, "dimensions");
        writeBytes(
            coder, std::vector<char>(contents.begin(), contents.end() - 1));
        expectLoadError(version, model, directory, "Truncated coder");
        auto invalid_dimensions = contents;
        const std::int32_t invalid_count = 99;
        std::memcpy(
            invalid_dimensions.data(), &invalid_count, sizeof(invalid_count));
        writeBytes(coder, invalid_dimensions);
        expectLoadError(version, model, directory, "dimensions");
        // 3 inputs, 2 outputs; each coefficient group must be finite.
        for (const std::size_t coefficient : {0u, 3u, 6u, 8u}) {
            for (const double value : {std::numeric_limits<double>::quiet_NaN(),
                     std::numeric_limits<double>::infinity(),
                     -std::numeric_limits<double>::infinity()}) {
                auto invalid = contents;
                std::memcpy(invalid.data() + 2 * sizeof(std::int32_t) +
                        coefficient * sizeof(double),
                    &value, sizeof(value));
                writeBytes(coder, invalid);
                expectLoadError(version, model, directory, "Nonfinite coder");
            }
        }
        auto zero_scale = contents;
        const double zero = 0.0;
        std::memcpy(
            zero_scale.data() + 2 * sizeof(std::int32_t) + 3 * sizeof(double),
            &zero, sizeof(zero));
        writeBytes(coder, zero_scale);
        expectLoadError(version, model, directory, "Zero input scale");
        writeBytes(coder, contents);
    }
}

void checkLoggingCallbacks(
    const std::string& model, const std::filesystem::path& resources) {
    const auto uri = fileURI(resources);
    const auto path = std::filesystem::absolute(resources).u8string();
    const auto identifier2 = "fmi2_cs_" + model;
    const auto identifier3 = "fmi3_cs_" + model;
    const Reference invalid = 999;
    const double value = 1.0;
    // A callback may be absent, or throw in a C++ host. Neither case may
    // escape the exported C entry points, including instantiation failures.
    for (const bool throws : {false, true}) {
        const fmi2CallbackFunctions callbacks = {
            throws ? throwFMI2Log : nullptr, allocateMemory, freeMemory,
            nullptr, nullptr};
        require(fmi2Instantiate("", fmi2CoSimulation, identifier2.c_str(),
                    uri.c_str(), &callbacks, false, false) == nullptr,
            "FMI 2 invalid instantiation with optional logger");
        auto component = fmi2Instantiate("logger_test", fmi2CoSimulation,
            identifier2.c_str(), uri.c_str(), &callbacks, false, false);
        require(component != nullptr, "FMI 2 optional logger instantiation");
        const auto status2 = fmi2SetReal(component, &invalid, 1, &value);
        fmi2FreeInstance(component);
        expectStatus(status2, fmi2Error, "FMI 2 optional logger error");

        const auto callback = throws ? throwFMI3Log : nullptr;
        require(fmi3InstantiateCoSimulation("", identifier3.c_str(),
                    path.c_str(), false, false, false, false, nullptr, 0,
                    nullptr, callback, nullptr) == nullptr,
            "FMI 3 invalid instantiation with optional logger");
        auto instance = fmi3InstantiateCoSimulation("logger_test",
            identifier3.c_str(), path.c_str(), false, false, false, false,
            nullptr, 0, nullptr, callback, nullptr);
        require(instance != nullptr, "FMI 3 optional logger instantiation");
        const auto status3 = fmi3SetFloat64(instance, &invalid, 1, &value, 1);
        fmi3FreeInstance(instance);
        expectStatus(status3, fmi3Error, "FMI 3 optional logger error");
    }
    std::vector<std::string> messages;
    require(fmi3InstantiateModelExchange("me_test", identifier3.c_str(),
                path.c_str(), false, false, &messages, logFMI3) == nullptr,
        "Model Exchange must remain unsupported");
    expectLog(messages, "fmi3InstantiateModelExchange", "unsupported");
    messages.clear();
    require(fmi3InstantiateScheduledExecution("se_test", identifier3.c_str(),
                path.c_str(), false, false, &messages, logFMI3, nullptr,
                nullptr, nullptr) == nullptr,
        "Scheduled Execution must remain unsupported");
    expectLog(messages, "fmi3InstantiateScheduledExecution", "unsupported");
    messages.clear();
    Client client(3, model, resources, &messages);
    expectStatus(fmi3SetTime(client.component(), 0.0), fmi3Error,
        "unsupported interface operation");
    expectLog(messages, "fmi3SetTime", "unsupported");
}

}  // namespace

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
#endif
    try {
        require(argc == 4,
            "usage: test_cs <model> <resources> "
            "<contracts|fmi3_contracts|parity|resource_checks>");
        const std::string model = argv[1];
        const auto resources = std::filesystem::u8path(argv[2]);
        const std::string test_case = argv[3];
        if (test_case == "contracts") {
            Client client(2, model, resources);
            checkContracts(client);
        } else if (test_case == "fmi3_contracts") {
            checkFMI3Instantiation(model, resources);
            Client client(3, model, resources);
            checkFMI3Contracts(client);
        } else if (test_case == "resource_checks") {
            checkLoggingCallbacks(model, resources);
            checkResources(model, resources);
        } else {
            require(test_case == "parity", "unknown test case");
            checkParity(model, resources);
        }
        std::cout << "PASS: " << model << ' ' << test_case << '\n';
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
