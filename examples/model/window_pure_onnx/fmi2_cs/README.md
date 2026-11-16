# window_pure_onnx FMI 2 Co-Simulation example

This directory mirrors the model implementation. The model consumes a whole
window of `[1, W, X + Y]` frames and returns `[1, Y]`; the FMU keeps the window
on the C++ side and exposes only `X` inputs and `Y` outputs.

valueReference layout (1-based):

| range | meaning |
| --- | --- |
| `[1, X]` | inputs `u(t)` |
| `[X + 1, X + Y]` | outputs `y(t)` |
| `[X + Y + 1, X + 2Y]` | initial outputs `y(0)` |

`warmStart` fills the whole window with `[u(0), y(0)]` and exposes `y(0)` as the
output, without running inference. The first `fmi2DoStep` therefore consumes the
injected `y(0)` as `y(t - 1)`.

Inputs are writable in instantiated, initialization, and step modes. Outputs
are read-only; initial outputs are writable only before
`fmi2ExitInitializationMode` succeeds.
Batch reads and writes validate all references before copying any values, so a
rejected request leaves the model and the caller's output buffer unchanged.

Call `fmi2EnterInitializationMode`, set the initial inputs and outputs, then
call `fmi2ExitInitializationMode` before stepping. `fmi2SetupExperiment` is
optional; omitting it uses start time 0 and no stop time. When supplied, the
start time must be finite, a defined tolerance must be finite and positive,
and a defined stop time must be finite and later than the start time. Setup
is only accepted before entering initialization.

`fmi2DoStep` requires a finite, positive step that advances time, a communication
point matching the last successful time, and an end time within the optional
stop time. Time comparisons allow floating-point roundoff. Invalid step
arguments are rejected before inference and do not advance the window or time.
`fmi2Terminate` is valid only after initialization; final values remain
readable, and further writes and
steps return `fmi2Error`. `fmi2Reset` clears ports, the window, inference buffers,
experiment settings, and lifecycle state, so initialization is required again.

The shared export script [`../export_model.py`](../export_model.py) and both
FMI 2 and FMI 3 C++ examples share the same hand-written values: the initial
inputs `1.0, 1.0, 1.0`, the initial outputs `0.5, -0.5`, and three steps of
inputs. Update all three files when changing them. The script writes shared model
resources to `build/bin/resources/model/window_pure_onnx/<platform-tag>`. When
using a non-default CMake build directory, pass its matching resource path with
`--output-dir`.

Fetch ONNX Runtime and export the resources:

```powershell
python third_party/onnxruntime/fetch.py
python examples/model/window_pure_onnx/export_model.py
```

Build the library and the linked C++ example:

```powershell
cmake --build build --target ex_fmi2_cs_window_pure_onnx
```

After exporting resources and building, launch the executable directly:

```powershell
& 'build/bin/exe/win64-x64/ex_fmi2_cs_window_pure_onnx.exe'
```

On Linux, use:

```sh
export LD_LIBRARY_PATH="$(pwd)/build/bin/lib/linux64-x64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
build/bin/exe/linux64-x64/ex_fmi2_cs_window_pure_onnx
```

On macOS, run `build/bin/exe/<platform-tag>/ex_fmi2_cs_window_pure_onnx`.
