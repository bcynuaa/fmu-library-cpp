from __future__ import annotations

import argparse
import platform
import shutil
from pathlib import Path

import torch


EXAMPLE_DIRECTORY = Path(__file__).resolve().parent
REPOSITORY_ROOT = EXAMPLE_DIRECTORY.parents[2]

WINDOW = 4
FEATURE_DIM = 3
OUTPUT_DIM = 2

# * ex_window_predictor.cpp fills the window with 1.0 + 0.1 * (i + 1).
MANUAL_WINDOW_VALUES = [
    1.0 + 0.1 * (index + 1) for index in range(WINDOW * FEATURE_DIM)
]


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
    def __init__(self) -> None:
        super().__init__()
        self.linear_ = torch.nn.Linear(FEATURE_DIM, OUTPUT_DIM)
        self.register_buffer("bias_", torch.tensor([-1.0, -2.0]))
        pass

    def forward(self, window: torch.Tensor) -> torch.Tensor:
        return self.linear_(window.mean(dim=1)) + self.bias_
        pass
    pass


def export_model(
    output_directory: Path, platform_tag: str, runtime_directory: Path
) -> None:
    output_directory.mkdir(parents=True, exist_ok=True)

    model = WindowModel()
    model.eval()
    with torch.no_grad():
        model.linear_.weight.copy_(
            torch.tensor([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]], dtype=torch.float32)
        )
        model.linear_.bias.copy_(torch.tensor([1.0, 2.0], dtype=torch.float32))
        pass

    torch.onnx.export(
        model,
        torch.zeros((1, WINDOW, FEATURE_DIM), dtype=torch.float32),
        output_directory / "model.onnx",
        dynamo=False,
        external_data=False,
        input_names=["input"],
        output_names=["output"],
    )

    for library in runtime_libraries(runtime_directory, platform_tag):
        shutil.copy2(library, output_directory / library.name)
        pass

    window = torch.tensor([MANUAL_WINDOW_VALUES], dtype=torch.float32).reshape(
        1, WINDOW, FEATURE_DIM
    )
    with torch.no_grad():
        expected = model(window)
        pass
    print(f"Example resource directory: {output_directory}")
    print(f"Window: {WINDOW}, features: {FEATURE_DIM}")
    print(f"Expected output: {expected.numpy().reshape(-1).tolist()}")
    pass


def parse_arguments() -> argparse.Namespace:
    default_platform_tag = host_platform_tag()
    parser = argparse.ArgumentParser(
        description="Export the window_predictor example."
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
            / "onnxengine"
            / "window_predictor"
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
