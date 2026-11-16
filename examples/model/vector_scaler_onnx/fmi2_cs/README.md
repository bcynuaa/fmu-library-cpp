# vector_scaler_onnx FMI 2 Co-Simulation example

This directory mirrors the model implementation. The shared export script
[`../export_model.py`](../export_model.py) and both FMI 2 and FMI 3 C++ examples
use the same manually set input vector (`1.1, 1.2, 1.3`) to print their outputs.
Update all three files when changing the example input. The script writes
shared model resources to
`build/bin/resources/model/vector_scaler_onnx/<platform-tag>`. When using a
non-default CMake build directory, pass its matching resource path with
`--output-dir`, for example
`build-custom/bin/resources/model/vector_scaler_onnx/<platform-tag>`.

The first `X` value references are writable inputs; the next `Y` are read-only
outputs. References start at 1. A batch read or write validates every reference
before copying any values, so a rejected request leaves both the model and the
caller's output buffer unchanged.

Call `fmi2EnterInitializationMode`, set the initial inputs, then call
`fmi2ExitInitializationMode` to compute the initial outputs before stepping.
`fmi2SetupExperiment` is optional; omitting it uses start time 0 and no stop
time. When supplied, the start time must be finite, a defined tolerance must be
finite and positive, and a defined stop time must be finite and later than the
start time. Setup is only accepted before entering initialization.

`fmi2DoStep` requires a finite, positive step that advances time, a communication
point matching the last successful time, and an end time within the optional
stop time. Time comparisons allow floating-point roundoff. Invalid step
arguments are rejected before inference and do not advance time.
`fmi2Terminate` is valid only after initialization; final values remain
readable, and further writes and steps
return `fmi2Error`. `fmi2Reset` clears ports, inference buffers, experiment
settings, and lifecycle state, so initialization is required again.

Fetch ONNX Runtime and export the resources:

```powershell
python third_party/onnxruntime/fetch.py
python examples/model/vector_scaler_onnx/export_model.py
```

Build the library and the linked C++ example:

```powershell
cmake --build build --target ex_fmi2_cs_vector_scaler_onnx
```

The resource directory is fixed by CMake, and the executable links directly
to the library. On Windows, CMake stages the linked library beside the example
executable; MinGW builds also link their C++ runtime statically. After
exporting resources and building, launch the executable directly:

```powershell
& 'build/bin/exe/win64-x64/ex_fmi2_cs_vector_scaler_onnx.exe'
```

On Linux, use:

```sh
export LD_LIBRARY_PATH="$(pwd)/build/bin/lib/linux64-x64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
build/bin/exe/linux64-x64/ex_fmi2_cs_vector_scaler_onnx
```

On macOS, run `build/bin/exe/<platform-tag>/ex_fmi2_cs_vector_scaler_onnx`. Its
runtime search path points to
`build/bin/lib/<platform-tag>/`.
