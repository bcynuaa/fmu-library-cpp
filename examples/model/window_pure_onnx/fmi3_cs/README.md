# window_pure_onnx FMI 3 Co-Simulation example

This example and the FMI 2 example use the shared exporter
[`../export_model.py`](../export_model.py) for the ONNX model, runtime, initial
values, and three manual input steps. Export the shared resources and build:

```sh
python third_party/onnxruntime/fetch.py
python examples/model/window_pure_onnx/export_model.py
cmake --build build --target ex_fmi3_cs_window_pure_onnx
```

Run `build/bin/exe/win64-x64/ex_fmi3_cs_window_pure_onnx.exe` on Windows, or
`build/bin/exe/<platform-tag>/ex_fmi3_cs_window_pure_onnx` on Linux/macOS.
For a custom CMake build directory, pass its matching resource directory to
the exporter using `--output-dir`.

The library is `fmi3_cs_window_pure_onnx.dll` / `.so` / `.dylib`, without a
`lib` prefix. The example passes `example-token`; the model does not validate it.
`resourcePath` is an absolute UTF-8 filesystem path. This example does not
generate `modelDescription.xml` or a complete `.fmu` archive.

Float64 scalar references remain 1-based:

| References | Meaning | Writable |
| --- | --- | --- |
| `[1, X]` | Current inputs | Before termination |
| `[X + 1, X + Y]` | Current outputs | No |
| `[X + Y + 1, X + 2Y]` | Initial outputs | Before initialization completes |

Each Get/Set request must have `nValues == nValueReferences`. Exiting
initialization fills every window frame with `[u(0), y(0)]` and exposes
`y(0)` without inference. Each successful `doStep` predicts once, shifts the
window once, and writes the prediction back as feedback for the next frame.
The expected initialization and step results match the FMI 2 example.

Communication points must follow the previous successful step. Step sizes must
be finite and positive and stay within a defined stop time. Every step advances
one frame regardless of its duration; the host must choose a communication
period matching the model's training data. Reset clears the window and values
and allows a new initialization sequence.

Event mode and required intermediate variables are rejected at instantiation.
Early return, FMU state save/restore, derivatives, configuration, clocks, and
non-Float64 variables are not implemented. The host may allow early return;
this model always completes the requested step.
