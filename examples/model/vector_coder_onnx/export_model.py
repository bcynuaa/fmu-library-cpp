"""Export shared resources for the FMI 2 and FMI 3 vector coder examples."""

from __future__ import annotations

import argparse
import platform
import shutil
import struct
from pathlib import Path

import numpy as np
from numpy.typing import NDArray
import torch


EXAMPLE_DIRECTORY = Path(__file__).resolve().parent
REPOSITORY_ROOT = EXAMPLE_DIRECTORY.parents[2]
MANUAL_INPUT_VALUES = np.array(
    [1.1, 1.2, 1.3], dtype=np.float64
)  # Keep in sync with both FMI 2 and FMI 3 C++ examples.


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


def byte_array(values: NDArray[np.float64]) -> bytes:
    if values.ndim == 1:
        ordered_values = values
        pass
    elif values.ndim == 2:
        ordered_values = values.ravel(order="F")
        pass
    else:
        raise ValueError("Only one- and two-dimensional arrays are supported.")
        pass
    return np.ascontiguousarray(ordered_values, dtype=np.float64).tobytes()
    pass


class OnnxModel(torch.nn.Module):
    def __init__(self) -> None:
        super().__init__()
        self.linear_ = torch.nn.Linear(3, 2)
        self.register_buffer("bias_", torch.tensor([-1.0, -2.0]))
        pass

    def forward(self, values: torch.Tensor) -> torch.Tensor:
        return self.linear_(values) + self.bias_
        pass
    pass


def export_coder(
    input_shift: NDArray[np.float64],
    input_scale: NDArray[np.float64],
    output_shift: NDArray[np.float64],
    output_scale: NDArray[np.float64],
    output_path: Path,
) -> None:
    with output_path.open("wb") as output_file:
        output_file.write(struct.pack("<ii", input_shift.size, output_shift.size))
        output_file.write(byte_array(input_shift))
        output_file.write(byte_array(input_scale))
        output_file.write(byte_array(output_shift))
        output_file.write(byte_array(output_scale))
        pass
    pass


def export_model(output_directory: Path, platform_tag: str, runtime_directory: Path) -> None:
    output_directory.mkdir(parents=True, exist_ok=True)

    model = OnnxModel()
    model.eval()
    with torch.no_grad():
        model.linear_.weight.copy_(torch.tensor([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]))
        model.linear_.bias.copy_(torch.tensor([1.0, 2.0]))
        pass

    torch.onnx.export(
        model,
        torch.zeros((1, 3)),
        output_directory / "model.onnx",
        dynamo=False,
        external_data=False,
        input_names=["input"],
        output_names=["output"],
        dynamic_axes={
            "input": {0: "batch_size"},
            "output": {0: "batch_size"},
        },
    )

    input_shift = np.array([0.1, 0.2, 0.3], dtype=np.float64)
    input_scale = np.array([1.0, 2.0, 3.0], dtype=np.float64)
    output_shift = np.array([0.4, 0.5], dtype=np.float64)
    output_scale = np.array([4.0, 5.0], dtype=np.float64)
    export_coder(
        input_shift,
        input_scale,
        output_shift,
        output_scale,
        output_directory / "coder.bin",
    )

    for library in runtime_libraries(runtime_directory, platform_tag):
        shutil.copy2(library, output_directory / library.name)
        pass

    input_values = MANUAL_INPUT_VALUES.copy()
    normalized_values = (input_values - input_shift) / input_scale
    with torch.no_grad():
        predicted_values = model(
            torch.from_numpy(normalized_values.astype(np.float32)).reshape(1, -1)
        ).numpy().reshape(-1)
        pass
    output_values = predicted_values * output_scale + output_shift
    print(f"Example resource directory: {output_directory}")
    print(f"Expected results: {output_values}")
    pass


def parse_arguments() -> argparse.Namespace:
    default_platform_tag = host_platform_tag()
    parser = argparse.ArgumentParser(description="Export the vector_coder_onnx example.")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=None,
        help="Directory for the model, coder data, and runtime libraries.",
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
            / "vector_coder_onnx"
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
