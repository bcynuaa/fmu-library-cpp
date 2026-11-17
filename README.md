# fmu-library-cpp

C++ model libraries for FMI 2.0 and FMI 3.0 Co-Simulation that wrap ONNX models
and handwritten predictors.

Each algorithm is written once as a plain C++ class with no FMI types or
callbacks, then exposed through two thin adapters, one per FMI version, as a
shared library that an FMI host can load.

**Not included:** these outputs are the `binaries/` layer of an FMU only.
Generating `modelDescription.xml` and packaging a complete `.fmu` archive
remains separate work, so an FMI host cannot load the libraries on their own
yet.

See [AGENTS.md](AGENTS.md) for the folder layout and coding style,
[MEMO.md](MEMO.md) for why the DLL build and the FMU packaging stay separate,
and [Adding an algorithm](#adding-an-algorithm-for-fmi-2-and-fmi-3) for the
model contract plus the steps to add one.

## Contents

- [Requirements](#requirements)
- [Quick start](#quick-start)
- [Models](#models)
- [Output layout](#output-layout)
- [Packaging](#packaging)
- [Verification](#verification)
- [Behavior notes](#behavior-notes)
- [Adding an algorithm for FMI 2 and FMI 3](#adding-an-algorithm-for-fmi-2-and-fmi-3)

## Requirements

| Requirement | Minimum | Needed for |
| --- | --- | --- |
| CMake | 3.23 | Configure and build |
| C++ compiler | C++17 | Configure and build |
| Python | 3.9 | `fetch.py`, `pack.py`, exporters |
| ONNX Runtime | fetched | Configure, see the next step |

`third_party/onnxruntime/fetch.py` and `pack.py` use the Python standard
library only. The third-party packages in `requirements.txt` (torch, onnx,
numpy) are needed to export example models and to run the CTest resource
fixtures; a plain build does not need them.

## Quick start

```sh
python third_party/onnxruntime/fetch.py
cmake -S . -B build
cmake --build build
```

The fetch step is not optional: CMake stops at configure time when the ONNX
Runtime headers are missing from `third_party/onnxruntime/include`. Libraries
land in `build/bin/lib/<platform-tag>/` and example clients in
`build/bin/exe/<platform-tag>/`.

Model resources and the console examples need the Python packages:

```sh
python -m pip install -r requirements.txt
python examples/model/vector_coder_onnx/export_model.py
python examples/model/window_pure_onnx/export_model.py
```

Each model has one shared exporter at
`examples/model/<model>/export_model.py`, used by both FMI versions. It writes
`model.onnx` and any coder file to
`build/bin/resources/model/<model>/<platform-tag>` by default.

Run every Python example script recursively with:

```sh
python examples/run_all.py
```

The runner uses the current Python interpreter and each script's own directory
as its working directory. It skips itself and `__pycache__`, runs scripts in
path order, and continues after failures. It prints a final summary and returns
a nonzero exit code if any script fails.

## Models

The vector coder and window models support FMI 2 and FMI 3 Co-Simulation.

| Model | Mapping | Resources |
| --- | --- | --- |
| `vector_coder_onnx` | `[1, Nx]` -> `[1, Ny]` with a per-port coder | `model.onnx`, `coder.bin` |
| `window_pure_onnx` | `[1, W, Nx + Ny]` -> `[1, Ny]` rolling window | `model.onnx` |

Each algorithm owns its implementation in `src/model/<model>/model.hpp`,
including port storage, validation, resource loading, errors, and reset.
FMI 2 and FMI 3 use the same implementation of that algorithm. ONNX algorithms
reuse `onnxengine` for inference. Port, loading, and error logic stays local
to each algorithm's model. The adapters in
`src/model/detail/fmi2_cs_model.hpp` and `fmi3_cs_model.hpp` handle each
protocol's lifecycle and C API data conversion.

FMI 3 models expose scalar Float64 ports. Model Exchange and Scheduled
Execution entry points return unsupported; their interfaces are not
implemented. See the FMI 3 examples for details:
[vector coder](examples/model/vector_coder_onnx/fmi3_cs/README.md) and
[window model](examples/model/window_pure_onnx/fmi3_cs/README.md). These
targets build shared libraries and console examples only.

## Output layout

Model libraries use the same basename on every platform, without a `lib`
prefix:

| Platform | Model library filename |
| --- | --- |
| Windows | `<modelIdentifier>.dll` |
| Linux | `<modelIdentifier>.so` |
| macOS | `<modelIdentifier>.dylib` |

For example, the Linux vector coder library is
`build/bin/lib/linux64-x64/fmi2_cs_vector_coder_onnx.so`.
CMake also links examples using these names. ONNX Runtime keeps its upstream
filenames, including `libonnxruntime.so` on Linux.

## Packaging

`python pack.py` collects the build and ONNX Runtime outputs into one
`artifacts/` tree:

```text
artifacts/
├── SOURCE.txt                     commit, timestamp, platforms, file count
├── binary/<platform-tag>/         model libraries built from src/
└── runtime/onnxruntime/<tag>/     ONNX Runtime files
```

The platform subdirectories mirror the FMU layout, so `binary/<tag>/` maps to
`binaries/<tag>/` and `runtime/onnxruntime/<tag>/` maps to `resources/`. This
is staging only: it writes no `modelDescription.xml` and no `.fmu` archive.

Rebuild existing build directories with `cmake --build build` before running
`python pack.py`. Packing skips old `lib`-prefixed model libraries and removes
the old copy when collecting its replacement in `artifacts/binary/`. It also
skips stale `vector_scaler_onnx` libraries left in an existing build tree and
removes their artifact copies when the renamed `vector_coder_onnx` libraries
are collected.

## Verification

CTest coverage is available with `BUILD_TESTING=ON` (off by default). The
resource fixtures invoke the example exporters, so install `requirements.txt`
first:

```sh
python -m pip install -r requirements.txt
python third_party/onnxruntime/fetch.py
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

For multi-configuration generators, add `--config Debug` to the build command
and `-C Debug` to CTest. Tests also build with `FMI_BUILD_EXAMPLES=OFF`.
CTest fixtures run each model's shared exporter into the configured build
directory and copy the already-fetched ONNX Runtime library there. Tests do
not download dependencies. The ten CTest entries cover resource export and,
for each model, FMI 2 contracts, FMI 3 contracts, numerical parity, and resource
checks. They exercise lifecycle order, port permissions, atomic batch access,
time validation, reset, FMI 3 result pointers, and unsupported capabilities.
They compare predictions with expected values before and after reset, then
check Chinese paths, missing/corrupt resources, coder validation, and logging
callbacks. Test executables stay under the build directory's `tests/` folder.

## Behavior notes

Failed calls report their API name and reason through the host's FMI logger
using `logStatusError`. Resource errors include UTF-8 paths; ONNX Runtime
errors retain the runtime's diagnostic text. Error messages are emitted even
when debug logging is disabled. Successful calls do not emit debug messages.
Missing log callbacks are supported, and callback exceptions cannot escape the
C API.

Resource directories support UTF-8 paths, including Chinese characters. FMI 2
also accepts percent-escaped `file://` URIs; plain paths keep literal percent
sequences. Coder loading validates dimensions, complete coefficient arrays,
finite coefficients, and nonzero input scales. The resource filename priority is
`coder.bin`, `scaler.bin`, then `pptransform.bin`. If none exists, the model
tries every other `*.bin` file in the resource directory in filename order
until it finds valid coefficients. A present but invalid preferred file causes
failure instead of silently selecting another file.

A missing resource location is legal in both FMI versions. Models that need
resources reject it from `load()`, and the C API reports that reason through
the logger.

On Windows, building an example or test refreshes its model DLL copies even
when changes in a model do not require relinking the executable.

## Adding an algorithm for FMI 2 and FMI 3

Implement the computation once and connect it to both Co-Simulation adapters.
The following example adds `linear_gain`, with one input `u`, one output
`y = 2 * u`, and no external model files. The same structure works for an
ONNX model or a handwritten predictor such as RBF.

### 1. Add the model files

```text
src/model/linear_gain/
├── CMakeLists.txt
├── model.hpp
├── fmi2_cs/
│   └── fmi2_cs_linear_gain.cpp
└── fmi3_cs/
    └── fmi3_cs_linear_gain.cpp

examples/model/linear_gain/
├── CMakeLists.txt
├── export_model.py                 # If the algorithm needs generated resources
├── fmi2_cs/
│   └── fmi2_cs_linear_gain.cpp
└── fmi3_cs/
    └── fmi3_cs_linear_gain.cpp
```

Use the existing
[vector coder](src/model/vector_coder_onnx/model.hpp) and
[window model](src/model/window_pure_onnx/model.hpp) as resource-based examples.
Keep the computation independent of FMI types and callbacks. Follow
[AGENTS.md](AGENTS.md), including four-space indentation and
`this->class_variable_name_` for member access.

### 2. Implement the algorithm's model

Create `src/model/linear_gain/model.hpp`:

```cpp
#ifndef FMI_MODEL_LINEAR_GAIN_MODEL_HPP_
#define FMI_MODEL_LINEAR_GAIN_MODEL_HPP_

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>

namespace linear_gain {

class Model {
    public:
    bool load(const std::filesystem::path&) {
        this->reset();
        return true;
    }

    bool initialize() { return this->step(); }

    bool step() {
        this->values_[1] = 2.0 * this->values_[0];
        return true;
    }

    void reset() {
        this->values_.fill(0.0);
        this->error_.clear();
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
            if (reference != 1)
                return this->fail("Output is read-only");
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

    private:
    bool fail(const std::string& message) {
        this->error_ = message;
        return false;
    }

    std::array<double, 2> values_{};
    std::string error_;
};

}  // namespace linear_gain

#endif  // FMI_MODEL_LINEAR_GAIN_MODEL_HPP_
```

The adapters default-construct `Model` and call the following interface.
Implement these methods in each algorithm's own class; no model inheritance
is required. The `Reference` template parameter accepts the value-reference
type supplied by either FMI adapter.

| Method | Contract |
| --- | --- |
| `bool load(const std::filesystem::path& resources)` | Load and validate resources; establish dimensions and port storage. Called during instantiation. |
| `bool initialize()` | Initialize algorithm state and initial outputs. Called when leaving initialization mode. |
| `bool step()` | Calculate one synchronous step. Return `false` on failure; publish outputs and advance algorithm history only after successful computation. |
| `void reset()` | Restore port values and algorithm history so the instance can initialize again. Keep loaded resources available. |
| `bool set(const Reference[], std::size_t, const double[], bool)` | Validate the entire write request before applying it; the last argument permits writes to initial-value ports. |
| `bool get(const Reference[], std::size_t, double[])` | Validate the entire read request before changing the caller's buffer. |
| `const std::string& lastError() const noexcept` | Provide a useful failure reason for the FMI logger. |
| `void clearError()` | Clear the previous diagnostic before a new API call. |

Each algorithm chooses its port storage and validates its own reads and writes.
The two existing ONNX models use the following reference layout:

| `valueReference` | Port | Write permission |
| --- | --- | --- |
| `1 .. Nx` | Inputs | Before and during stepping |
| `Nx + 1 .. Nx + Ny` | Outputs | Read-only |
| `Nx + Ny + 1 .. values_.size()` | Optional initial-value ports | Before leaving initialization mode |

For `linear_gain`, reference `1` is `u` and reference `2` is `y`.
References start at **1** and must reject `0`; the array in this example maps
them to `values_[vr - 1]`. The window model allocates `Nx + 2 * Ny` values to
expose `Ny` initial outputs and uses the final `set()` argument to lock those
ports after initialization. The gain model has no initial-value ports and
ignores that argument.

For resource-based algorithms, validate files, dimensions, and coefficients
in `load()`. Return `this->fail("reason including the resource path")` on
failure. Each ONNX algorithm locates its files with
`onnxengine::locateLibrary()` and `onnxengine::locateModel()`, then calls its
predictor's `loadLibrary()` and `loadModel()`. Preserve the predictor's
`lastError()` and the file path when reporting failures. Keep this loading
logic inside the algorithm's own model. Handwritten algorithms use their own
resource reader and need no ONNX headers, as shown by `linear_gain`.

### 3. Add one factory per FMI version

Create `src/model/linear_gain/fmi2_cs/fmi2_cs_linear_gain.cpp`:

```cpp
#include "../../detail/fmi2_cs_model.hpp"
#include "../model.hpp"
#include "fmi2/factory.hpp"

namespace fmi2 {

Fmi2Base* createFmi2Instance(fmi2Type type) {
    if (type == fmi2CoSimulation) {
        return new model::detail::FMI2CSModel<linear_gain::Model>();
    }
    return nullptr;
}

}  // namespace fmi2
```

Create `src/model/linear_gain/fmi3_cs/fmi3_cs_linear_gain.cpp`:

```cpp
#include "../../detail/fmi3_cs_model.hpp"
#include "../model.hpp"
#include "fmi3/factory.hpp"

namespace fmi3 {

Fmi3Base* createFmi3Instance(InterfaceType type) {
    if (type == InterfaceType::kCoSimulation) {
        return new model::detail::FMI3CSModel<linear_gain::Model>(
            "fmi3_cs_linear_gain");
    }
    return nullptr;
}

}  // namespace fmi3
```

The FMI 3 `instantiationToken` is a handshake between `modelDescription.xml`
and the importing tool. The model does not validate it, so any non-empty string
is accepted at `fmi3InstantiateCoSimulation`. A `.fmu` generator should still
keep it stable for a given artifact; `surrogate-fmu-py` derives it from the
payload.
Each shared library contains exactly one model factory and one FMI core;
build the two factories into separate libraries.

### 4. Register both libraries in CMake

Create `src/model/linear_gain/CMakeLists.txt`:

```cmake
fmi_add_model(fmi2_cs_linear_gain CORE fmi2_core
    fmi2_cs/fmi2_cs_linear_gain.cpp
)

fmi_add_model(fmi3_cs_linear_gain CORE fmi3_core
    fmi3_cs/fmi3_cs_linear_gain.cpp
)
```

Add `add_subdirectory(linear_gain)` to `src/model/CMakeLists.txt`, then run
these commands from the repository root:

```sh
cmake -S . -B build
cmake --build build --target fmi2_cs_linear_gain fmi3_cs_linear_gain
```

`CORE` is required and must name an existing OBJECT library. Declare any
additional algorithm dependencies with `target_link_libraries()` on both
model targets. Keep `PREFIX` and `IMPORT_PREFIX` as configured by
`fmi_add_model()`. The resulting libraries are
`build/bin/lib/<platform-tag>/fmi2_cs_linear_gain.<extension>` and
`fmi3_cs_linear_gain.<extension>`, where the extension is `dll`, `so`, or
`dylib` for the target platform.

### 5. Add resources and examples

For an algorithm with generated resources, add one shared exporter at
`examples/model/linear_gain/export_model.py`. Follow an existing exporter,
including its `--output-dir`, `--platform-tag`, and `--runtime-dir` arguments,
so tests can use a separate build directory. Write generated files under
`build/bin/resources/model/linear_gain/<platform-tag>` by default. Both FMI
versions read the same resources. Copy ONNX Runtime only if the algorithm
uses it, and follow the Python style in `AGENTS.md`.

The pure C++ gain example above requires no exporter or resource files, so its
`load()` can accept an empty resource location and `fmi2Instantiate` /
`fmi3InstantiateCoSimulation` may be called with a `NULL` resource location or
path. Resource-based models still require one; they reject a missing location
from `load()` and the C API reports that reason through the FMI logger.

Add console clients using the existing model examples as a starting point.
Change the model identifier, port references, dimensions, inputs, and expected
results to match the new algorithm. Both clients should use the
same input sequence. Their normal call order is:

| FMI 2 | FMI 3 |
| --- | --- |
| `fmi2Instantiate` | `fmi3InstantiateCoSimulation` |
| `fmi2SetupExperiment`, `fmi2EnterInitializationMode` | `fmi3EnterInitializationMode` |
| `fmi2SetReal`, `fmi2ExitInitializationMode` | `fmi3SetFloat64`, `fmi3ExitInitializationMode` |
| Repeat: set inputs, `fmi2DoStep`, `fmi2GetReal` | Repeat: set inputs, `fmi3DoStep`, `fmi3GetFloat64` |
| `fmi2Terminate`, `fmi2FreeInstance` | `fmi3Terminate`, `fmi3FreeInstance` |

Use a percent-escaped `file://` resource URI for the FMI 2 client and a UTF-8
directory path for FMI 3. Pass all four required output pointers to
`fmi3DoStep`. `reset` returns either version to the instantiated state;
repeat experiment setup and initialization before stepping again.

Create `examples/model/linear_gain/CMakeLists.txt`:

```cmake
fmi_add_example(ex_fmi2_cs_linear_gain
    fmi2_cs_linear_gain
    fmi2_cs/fmi2_cs_linear_gain.cpp
)

fmi_add_example(ex_fmi3_cs_linear_gain
    fmi3_cs_linear_gain
    fmi3_cs/fmi3_cs_linear_gain.cpp
)
```

Add `add_subdirectory(linear_gain)` to `examples/model/CMakeLists.txt`.
The helper links each client to its model library, stages Windows DLLs, and
defines `FMI_EXAMPLE_RESOURCE_DIRECTORY` from the target name. Keep the
`fmi2_cs_<algorithm>` / `fmi3_cs_<algorithm>` naming convention so that resource
path is correct. Configure with `FMI_BUILD_EXAMPLES=ON` and build the
`ex_fmi2_cs_linear_gain` and `ex_fmi3_cs_linear_gain` targets. Executables go
under `build/bin/exe/<platform-tag>`.

### 6. Add verification for the new algorithm

Register the new algorithm's tests in `tests/CMakeLists.txt`. Test the public
C APIs for both versions: expected numerical outputs, reference `0` and
out-of-range references, output write rejection, atomic batch access, invalid
lifecycle/time calls, reset reproducibility, and FMI 2/3 parity. For
resource-based algorithms, also test malformed resources and logger messages.
For the gain example, setting input reference `1` to `3.0` should produce
`6.0` at output reference `2` after initialization and after a valid step.

The current `tests/cs_contract_test.cpp` hardcodes three inputs, two outputs,
the window model's initial ports, ONNX resources, and model-specific expected
results. Adding a name to the CMake model loop alone is insufficient. Supply
an algorithm-specific test source and resource fixture, or generalize those
assumptions first. Link the test to both model libraries and use
`fmi_stage_model_libraries()` for Windows, following the existing test setup.

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The shared adapters currently cover synchronous Co-Simulation with scalar
Real/Float64 ports. Their `step()` interface receives neither simulation time
nor step size. An algorithm that integrates over time needs an explicit
time/step interface and corresponding adapter changes, or dedicated subclasses
of `fmi2::CsBase` and `fmi3::CsBase`. Arrays, additional scalar types, events,
and early-return behavior also require suitable protocol implementations.
Model Exchange and Scheduled Execution need their own interfaces and core
support. A normal scalar predictor only needs the computation, factories,
and registrations described above; the C API wrappers can be reused.
