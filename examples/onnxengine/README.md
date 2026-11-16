# onnxengine examples

Two console drivers for the header-only predictors in `include/onnxengine`:

- `vector_predictor`: `[1, Nx] -> [1, Ny]`
- `window_predictor`: `[1, W, Nx] -> [1, Ny]`

The ONNX model and the ONNX Runtime library are **not** stored in the source
tree. Each `export_model.py` writes them into
`build/bin/resources/onnxengine/<example>/<platform-tag>`, which is exactly the
directory the executable is compiled to look in. Both scripts print the expected
output, and the drivers print the values they actually compute.

Fetch ONNX Runtime and export both models:

```powershell
python third_party/onnxruntime/fetch.py
python examples/onnxengine/vector_predictor/export_model.py
python examples/onnxengine/window_predictor/export_model.py
```

Build and run:

```powershell
cmake --build build --target ex_vector_predictor ex_window_predictor
& 'build/bin/exe/win64-x64/ex_vector_predictor.exe'
& 'build/bin/exe/win64-x64/ex_window_predictor.exe'
```

On Linux, use `build/bin/exe/linux64-x64/...`. The predictors dlopen the ONNX
Runtime library from the resource directory, so no `LD_LIBRARY_PATH` is needed.
