"""Export shared resources for the FMI 2 and FMI 3 window model examples."""

from __future__ import annotations

import argparse
import platform
import shutil
from pathlib import Path

import numpy as np
from numpy.typing import NDArray
import torch


EXAMPLE_DIRECTORY = Path(__file__).resolve().parent
REPOSITORY_ROOT = EXAMPLE_DIRECTORY.parents[2]

INPUT_DIM = 3
OUTPUT_DIM = 2
WINDOW = 4

# * Inputs set before exitInitializationMode; they seed the whole window.
INITIAL_INPUT_VALUES = np.array([1.0, 1.0, 1.0], dtype=np.float64)
# * valueReference [INPUT_DIM + OUTPUT_DIM + 1, INPUT_DIM + 2 * OUTPUT_DIM].
INITIAL_OUTPUT_VALUES = np.array([0.5, -0.5], dtype=np.float64)
# * One row per doStep. Keep in sync with both FMI 2 and FMI 3 C++ examples.
STEP_INPUT_VALUES = np.array(
    [
        [1.1, 1.2, 1.3],
        [1.4, 1.5, 1.6],
        [1.7, 1.8, 1.9],
    ],
    dtype=np.float64,
)

MODEL_WEIGHT = np.array(
    [
        [0.5, 0.0, 0.0, 0.5, 0.0],
        [0.0, 0.5, 0.0, 0.0, 0.5],
    ],
    dtype=np.float64,
)
MODEL_BIAS = np.array([0.0, 0.0], dtype=np.float64)


def host_platform_tag() -> str:
    machine = platform.machine().lower()
    system = platform.system()

    if system == "Windows":
        architecture = "arm64" if machine in ("arm64", "aarch64") else "x64"
        return f"win64-{architecture}"
        pass
    if system == "Linux":
        architecture = "aarch64" if machine in ("arm64", "aarch64") else "x64"
        return f"linux64-{architecture}"
        pass
    if system == "Darwin":
        if machine in ("arm64", "aarch64"):
            return "darwin64-arm64"
            pass
        return "darwin64-x64"
        pass
    raise RuntimeError(f"Unsupported host system: {system}")
    pass


def runtime_libraries(runtime_directory: Path, platform_tag: str) -> list[Path]:
    if platform_tag.startswith("win64-"):
        patterns = ("*.dll",)
        pass
    elif platform_tag.startswith("linux64-"):
        patterns = ("*.so", "*.so.*")
        pass
    elif platform_tag.startswith("darwin64-"):
        patterns = ("*.dylib",)
        pass
    else:
        raise RuntimeError(f"Unsupported FMI platform tag: {platform_tag}")
        pass

    libraries = sorted(
        {
            library
            for pattern in patterns
            for library in runtime_directory.glob(pattern)
            if library.is_file()
        }
    )
    if not libraries:
        raise FileNotFoundError(
            f"No ONNX Runtime libraries found in {runtime_directory}. Fetch them "
            f"with: python third_party/onnxruntime/fetch.py --platform {platform_tag}"
        )
        pass
    return libraries
    pass


class WindowModel(torch.nn.Module):
    """Averages the whole window, so a wrong shift or y-backwrite changes the output."""

    def __init__(self) -> None:
        super().__init__()
        self.linear_ = torch.nn.Linear(INPUT_DIM + OUTPUT_DIM, OUTPUT_DIM)
        pass

    def forward(self, window: torch.Tensor) -> torch.Tensor:
        return self.linear_(window.mean(dim=1))
        pass
    pass


def simulate_closed_loop() -> list[NDArray[np.float64]]:
    """Mirrors WindowPureOnnx: warm start, then one shift per doStep."""
    window = np.tile(
        np.concatenate([INITIAL_INPUT_VALUES, INITIAL_OUTPUT_VALUES]), (WINDOW, 1)
    )
    outputs = INITIAL_OUTPUT_VALUES.copy()
    history = [outputs.copy()]
    for step_inputs in STEP_INPUT_VALUES:
        window[-1, :INPUT_DIM] = step_inputs
        outputs = MODEL_WEIGHT @ window.mean(axis=0) + MODEL_BIAS
        history.append(outputs.copy())
        window[:-1] = window[1:].copy()
        window[-1, :INPUT_DIM] = 0.0
        window[-1, INPUT_DIM:] = outputs
        pass
    return history
    pass


def export_model(
    output_directory: Path, platform_tag: str, runtime_directory: Path
) -> None:
    output_directory.mkdir(parents=True, exist_ok=True)

    model = WindowModel()
    model.eval()
    with torch.no_grad():
        model.linear_.weight.copy_(torch.tensor(MODEL_WEIGHT, dtype=torch.float32))
        model.linear_.bias.copy_(torch.tensor(MODEL_BIAS, dtype=torch.float32))
        pass

    torch.onnx.export(
        model,
        torch.zeros((1, WINDOW, INPUT_DIM + OUTPUT_DIM)),
        output_directory / "model.onnx",
        dynamo=False,
        external_data=False,
        input_names=["window"],
        output_names=["output"],
        dynamic_axes={"window": {0: "batch_size"}},
    )

    for library in runtime_libraries(runtime_directory, platform_tag):
        shutil.copy2(library, output_directory / library.name)
        pass

    history = simulate_closed_loop()
    print(f"Example resource directory: {output_directory}")
    print(f"Window: {WINDOW}, inputs: {INPUT_DIM}, outputs: {OUTPUT_DIM}")
    print("Expected results (init, then one line per doStep):")
    for index, values in enumerate(history):
        label = "init" if index == 0 else f"step {index}"
        print(f"  {label:<7} {np.array2string(values, precision=6)}")
        pass
    pass


def parse_arguments() -> argparse.Namespace:
    default_platform_tag = host_platform_tag()
    parser = argparse.ArgumentParser(
        description="Export the window_pure_onnx example."
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=None,
        help="Directory for the model and runtime libraries.",
    )
    parser.add_argument(
        "--platform-tag",
        default=default_platform_tag,
        help="FMI platform tag used to select runtime libraries.",
    )
    parser.add_argument(
        "--runtime-dir",
        type=Path,
        default=None,
        help="Directory containing the fetched ONNX Runtime libraries.",
    )
    arguments = parser.parse_args()
    if arguments.output_dir is None:
        arguments.output_dir = (
            REPOSITORY_ROOT
            / "build"
            / "bin"
            / "resources"
            / "model"
            / "window_pure_onnx"
            / arguments.platform_tag
        )
        pass
    if arguments.runtime_dir is None:
        arguments.runtime_dir = (
            REPOSITORY_ROOT
            / "third_party"
            / "onnxruntime"
            / "lib"
            / arguments.platform_tag
        )
        pass
    return arguments
    pass


if __name__ == "__main__":
    arguments = parse_arguments()
    export_model(
        arguments.output_dir.resolve(),
        arguments.platform_tag,
        arguments.runtime_dir.resolve(),
    )
    pass
