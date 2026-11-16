# AGENTS.md

## Coding style

### C++

Strictly follow the google coding style.

- `ClassName`
- `functionName`
- `variable_name`
- `this->class_variable_name_`
- `folder_name`
- `file_name.cpp`
- `MACRO_NAME`
- `ACRONYM`: like `ONNX` not `Onnx`

4 char's blank indent.

exception: `onnxengine` keeps the name as it is.

### Python

Follow the coding style below:

- `ClassName`
- `function_name`
- `variable_name`
- `class_variable_name_`
- `struct_variable_name` (struct is decorated by `@dataclass`)
- `CONSTANT_VARIABLE_NAME`
- `decorator_name`

and:

1. add `pass` as much as possible to make code blocks clear (for, if, def, even after return...)
2. `import` order:
   1. `__future__`
   2. standard library like `os`, `pathlib`, `json`...
   3. third party library like `numpy`, `pandas`, `torch`
   4. other file or module inside package
3. specify input and outputs's type

## FMU contracts

1. `valueReference` index starts from 1
2. The FMI 3 `instantiationToken` is **not** validated by the model. It exists
   for the importing tool; the C API only rejects a null or empty token.

## Include Folder

- `onnxengine`:  the pure header-only file for an onnx evaluator engine
  - `onnxengine.hpp`: entry of the pure header-only file
  - `vector_predictor.hpp`: `[1, Nx]` -> `[1, Ny]` onnx engine
  - `window_predictor.hpp`: `[1, W, Nx]` -> `[1, Ny]` onnx engine
  - `detail/session.hpp`: 
- `fmi2`: fmi2 basis function
  - `fmi2_declare.h`: fmi2 api declaration
  - `fmi2_platform`: the platform macro
  - `fmi2_base.hpp`: basic class for the api declaration
  - `cs_base.hpp`: the co-simulation format class
  - `me_base.hpp`: the model-exchange format class
  - `factory.hpp`: the factory function for specific class
- `fmi3`: FMI 3 common and Co-Simulation interfaces
  - `fmi3_declare.h`: FMI 3 types, callbacks, and C API declarations
  - `fmi3_platform.h`: platform export macros
  - `fmi3_base.hpp`: common interface; optional capabilities default to errors
  - `cs_base.hpp`: Co-Simulation instantiation and stepping interface
  - `factory.hpp`: factory for concrete FMI 3 models
- `utils`:
  - `fmu_io.hpp`: read formatted files

## Src Folder

- `core`: the core export api
  - `fmi2`
    - `fmi2_wrapper.cpp`: the fmi2 export api
  - `fmi3`
    - `fmi3_wrapper.cpp`: common/CS C API exports and exception handling; ME/SE entry points return unsupported
- `model`: the implement of different model
  - `vector_scaler_onnx`: the model relies on onnx with input/output scaler of `[1, Nx]` -> `[1, Ny]` relies on scaler
    - `fmi2_cs`: co-simulation mode
      - `fmi2_cs_vector_scaler_onnx.cpp`: implement of fmi2, cs's `vector_scaler_onnx`
    - `fmi3_cs`: FMI 3 CS implementation with scalar Float64 ports; shares resources with `fmi2_cs`
  - `vector_pure_onnx`: the model relies on onnx with input/output scaler of `[1, Nx]` -> `[1, Ny]`, no extra scaler for input and output
    - `fmi2_cs`: co-simulation mode
      - `fmi2_cs_vector_pure_onnx.cpp`: implement of fmi2, cs's `vector_pure_onnx`
    - `fmi3_cs`: TODO, similar to `fmi2_cs`
  - `rbf`: the model by hand-writing predictor of rbf of `[1, Nx]` -> `[1, Ny]`
    - `fmi2_cs`: co-simulation mode
      - `fmi2_cs_rbf.cpp`: implement of fmi2, cs's `rbf`
    - `fmi3_cs`: TODO, similar to `fmi3_cs`
  - `window_pure_onnx`: the model relies on onnx with a rolling window of `[1, W, Nx + Ny]` -> `[1, Ny]`, no extra scaler; the C++ side keeps the window and exposes `Nx` inputs plus `Ny` initial outputs
    - `fmi2_cs`: co-simulation mode
      - `fmi2_cs_window_pure_onnx.cpp`: implement of fmi2, cs's `window_pure_onnx`
    - `fmi3_cs`: FMI 3 CS implementation with scalar Float64 ports; shares resources with `fmi2_cs`
  - TODO: `neuralode`, `window_scaler_onnx`...

## Examples Folder

- `run_all.py`: recursively runs all Python example scripts except itself and `__pycache__`, in path order, using the current interpreter and each script's directory. It continues after failures, summarizes results, and returns a nonzero exit code if any script fails.
- `model`: a copy of src's folder to verify each implement. Each model keeps a shared `export_model.py` at `examples/model/<model>/`, alongside the `fmi2_cs` and `fmi3_cs` C++ example directories. The script exports resources to `build/bin/resources/model/<model>/<platform-tag>` for both FMI versions, uses manual inputs, and prints the outputs. C++ examples use the same inputs and receive the shared resource path from CMake.
- `onnxengine`: console drivers for the header-only predictors in `include/onnxengine`, one folder per predictor. Same rule as `model`: `export_model.py` writes `model.onnx` and the ONNX Runtime library into `build/bin/resources/onnxengine/<example>/<platform-tag>`, so no generated file is stored in the source tree.

## Build Folder

Only focus on the `bin` folder:

- `lib/{platform}/{**.dll/**.so}`
- `exe/{platform}/{**.exe/**.a}`

`lib` should from `src`, and `exe` should from `examples`. Examples should be linked to the lib.

## Others

1. `cmake` should include any standard library used here
2. `valueReference` is 1-based in this repository: models index `values_[vr - 1]`
   and reject `vr == 0`. Any `modelDescription.xml` generator must emit 1-based
   references, otherwise every port shifts by one.
3. Project-built model libraries use `<modelIdentifier>.dll` on Windows,
   `<modelIdentifier>.so` on Linux, and `<modelIdentifier>.dylib` on macOS,
   without a `lib` prefix. `fmi_add_model` sets `PREFIX` and `IMPORT_PREFIX` to
   empty strings on every platform, so variants must not override them.
   Third-party ONNX Runtime libraries keep their upstream filenames.
4. `fmi_add_model` requires an explicit `CORE` argument naming an existing
   OBJECT library, for example `CORE fmi2_core` or `CORE fmi3_core`. There is no
   default FMI version.
5. ...
