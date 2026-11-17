# vector_coder_onnx FMI 3 Co-Simulation example

This example uses the same model, coder, manual inputs (`1.1, 1.2, 1.3`), and
resource directory as the FMI 2 example. Both versions use
[`../export_model.py`](../export_model.py). Export the shared resources once:

```sh
python third_party/onnxruntime/fetch.py
python examples/model/vector_coder_onnx/export_model.py
cmake --build build --target ex_fmi3_cs_vector_coder_onnx
```

Run `build/bin/exe/win64-x64/ex_fmi3_cs_vector_coder_onnx.exe` on Windows, or
`build/bin/exe/<platform-tag>/ex_fmi3_cs_vector_coder_onnx` on Linux/macOS.
For a custom CMake build directory, pass its matching resource directory to
the exporter using `--output-dir`.

The library is `fmi3_cs_vector_coder_onnx.dll` / `.so` / `.dylib`, without a
`lib` prefix. The example passes `example-token`; the model does not validate it.
`resourcePath` is an absolute UTF-8 filesystem path. This example does not
generate `modelDescription.xml` or a complete `.fmu` archive.

The API exposes scalar Float64 variables with 1-based references: inputs are
`[1, X]`, outputs are `[X + 1, X + Y]`. Only inputs are writable, and each
Get/Set request must have `nValues == nValueReferences`.

Initialization evaluates the current inputs once. Each successful `doStep`
evaluates the latest inputs and reports `lastSuccessfulTime = currentTime +
stepSize`. Communication points must follow the previous successful step;
the step size must be finite and positive and stay within a defined stop time.
Reset clears values and returns the instance to its initial lifecycle state.

Event mode and required intermediate variables are rejected at instantiation.
Early return, FMU state save/restore, derivatives, configuration, clocks, and
non-Float64 variables are not implemented. The host may allow early return;
this model always completes the requested step.
