#!/usr/bin/env python3
"""Collects the build and ONNX Runtime outputs into a single artifacts tree.

Layout produced:

    artifacts/
    ├── SOURCE.txt                     commit, timestamp, platforms, file count
    ├── binary/                        shared libraries built from src/
    │   ├── win64-x64/
    │   │   ├── fmi2_cs_vector_scaler_onnx.dll
    │   │   └── fmi2_cs_window_pure_onnx.dll
    │   └── linux64-x64/
    │       ├── fmi2_cs_vector_scaler_onnx.so
    │       └── fmi2_cs_window_pure_onnx.so
    └── runtime/
        └── onnxruntime/               everything under ONNX Runtime's lib/
            ├── win64-x64/
            │   ├── onnxruntime.dll
            │   └── onnxruntime_providers_shared.dll
            └── linux64-x64/
                ├── libonnxruntime.so
                └── libonnxruntime_providers_shared.so

The platform subdirectory mirrors the FMU layout, so `binary/<tag>/` maps to
`binaries/<tag>/` and `runtime/onnxruntime/<tag>/` maps to `resources/`. Use
`--flat` to drop the subdirectory and merge everything into one level.

Collecting merges into whatever is already there, so a Windows build and a
Linux build gathered on different machines end up in the same tree. Only files
with the same platform and name are replaced; everything else is left alone.
Legacy `lib`-prefixed model copies are removed when their replacements are
collected. Rebuild older model libraries before packing; ONNX Runtime libraries
keep their upstream names.
Pass `--clean` to start from an empty tree.

Run it after `cmake --build build` and `python third_party/onnxruntime/fetch.py`.
Files are copied, not moved, so the build tree stays usable.
"""

from __future__ import annotations

import argparse
import datetime
import shutil
import subprocess
from pathlib import Path
from typing import Callable

REPOSITORY_ROOT = Path(__file__).resolve().parent

DEFAULT_OUTPUT_DIR = REPOSITORY_ROOT / "artifacts"
DEFAULT_BUILD_DIR = REPOSITORY_ROOT / "build"
DEFAULT_RUNTIME_DIR = REPOSITORY_ROOT / "third_party" / "onnxruntime" / "lib"

BINARY_SUBDIRECTORY = Path("binary")
RUNTIME_SUBDIRECTORY = Path("runtime") / "onnxruntime"
SOURCE_MANIFEST = "SOURCE.txt"


def is_shared_library(path: Path) -> bool:
    """Matches foo.dll, foo.dylib, foo.so and versioned foo.so.1.2."""
    name = path.name.lower()
    if name.endswith(".dll") or name.endswith(".dylib"):
        return True
        pass
    return name.endswith(".so") or ".so." in name
    pass


def is_any_file(path: Path) -> bool:
    return path.is_file()
    pass


def is_model_library(path: Path) -> bool:
    return (
        path.is_file()
        and is_shared_library(path)
        and not path.name.startswith("lib")
    )
    pass


def collect(
    source_root: Path,
    target_root: Path,
    predicate: Callable[[Path], bool],
    keep_structure: bool,
) -> list[tuple[Path, bool]]:
    """Copies matching files over, returning each target and whether it existed."""
    collected: list[tuple[Path, bool]] = []
    for source in sorted(source_root.rglob("*")):
        if not predicate(source):
            continue
            pass
        relative = source.relative_to(source_root)
        if keep_structure and len(relative.parts) > 1:
            target = target_root.joinpath(*relative.parts)
            pass
        else:
            target = target_root / source.name
            pass
        replaced = target.is_file()
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        collected.append((target, replaced))
        pass
    return collected
    pass


def collect_runtime(
    runtime_directory: Path, target_directory: Path, keep_structure: bool
) -> list[tuple[Path, bool]]:
    if not runtime_directory.is_dir():
        raise FileNotFoundError(
            f"ONNX Runtime libraries not found in {runtime_directory}. Fetch them "
            f"with: python third_party/onnxruntime/fetch.py"
        )
        pass
    collected = collect(
        runtime_directory, target_directory, is_any_file, keep_structure
    )
    if not collected:
        raise FileNotFoundError(f"No runtime files found in {runtime_directory}.")
        pass
    return collected
    pass


def collect_binaries(
    build_directory: Path, target_directory: Path, keep_structure: bool
) -> list[tuple[Path, bool]]:
    library_directory = build_directory / "bin" / "lib"
    if not library_directory.is_dir():
        raise FileNotFoundError(
            f"Built libraries not found in {library_directory}. Build them with: "
            f"cmake --build build"
        )
        pass
    collected = collect(
        library_directory, target_directory, is_model_library, keep_structure
    )
    if not collected:
        raise FileNotFoundError(
            f"No model .dll/.so/.dylib without a 'lib' prefix found in "
            f"{library_directory}. Rebuild the model libraries with: "
            f"cmake --build {build_directory}"
        )
        pass
    for target, _ in collected:
        legacy_target = target.with_name(f"lib{target.name}")
        if legacy_target.is_file():
            legacy_target.unlink()
            pass
        pass
    return collected
    pass


def git_revision(repository_root: Path) -> tuple[str, bool]:
    """Returns the short commit and whether the work tree has local changes."""
    try:
        commit = subprocess.run(
            ["git", "-C", str(repository_root), "rev-parse", "--short", "HEAD"],
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip()
        changes = subprocess.run(
            ["git", "-C", str(repository_root), "status", "--porcelain"],
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip()
        pass
    except (OSError, subprocess.CalledProcessError):
        return "", False
        pass
    return commit, bool(changes)
    pass


def detect_platforms(output_directory: Path) -> list[str]:
    """Reads the platform tags back from the collected tree."""
    platforms: set[str] = set()
    for subdirectory in (BINARY_SUBDIRECTORY, RUNTIME_SUBDIRECTORY):
        root = output_directory / subdirectory
        if not root.is_dir():
            continue
            pass
        for child in root.iterdir():
            if child.is_dir():
                platforms.add(child.name)
                pass
            pass
        pass
    if platforms:
        return sorted(platforms)
        pass

    # * Flat layout: fall back to the file extensions.
    extensions = {".dll": "win64", ".so": "linux64", ".dylib": "darwin64"}
    for path in output_directory.rglob("*"):
        if path.is_file():
            platform = extensions.get(path.suffix.lower())
            if platform:
                platforms.add(platform)
                pass
            pass
        pass
    return sorted(platforms)
    pass


def write_source_manifest(
    output_directory: Path, repository_root: Path, file_count: int
) -> Path:
    """Records which commit produced the collected binaries."""
    commit, dirty = git_revision(repository_root)
    platforms = detect_platforms(output_directory)
    lines = [
        "source fmu-library-cpp",
        f"commit {commit or 'unknown'}",
        f"dirty {'true' if dirty else 'false'}",
        f"packed {datetime.datetime.now().astimezone().isoformat(timespec='seconds')}",
        f"platforms {', '.join(platforms) if platforms else 'unknown'}",
        f"files {file_count}",
    ]
    output_directory.mkdir(parents=True, exist_ok=True)
    manifest = output_directory / SOURCE_MANIFEST
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return manifest
    pass


def clear_directories(directories: list[Path]) -> None:
    for directory in directories:
        if directory.is_dir():
            shutil.rmtree(directory)
            pass
        pass
    pass


def print_report(
    label: str, target_root: Path, entries: list[tuple[Path, bool]]
) -> None:
    print(f"{label} -> {target_root}")
    for target, replaced in entries:
        suffix = "  (replaced)" if replaced else ""
        print(f"  {target.relative_to(target_root).as_posix()}{suffix}")
        pass
    pass


def pack(
    output_directory: Path,
    build_directory: Path,
    runtime_directory: Path,
    clean_existing: bool,
    flat: bool,
) -> None:
    binary_target = output_directory / BINARY_SUBDIRECTORY
    runtime_target = output_directory / RUNTIME_SUBDIRECTORY
    if clean_existing:
        clear_directories([binary_target, runtime_target])
        pass

    keep_structure = not flat
    runtime_files = collect_runtime(runtime_directory, runtime_target, keep_structure)
    binary_files = collect_binaries(build_directory, binary_target, keep_structure)

    print_report("Runtime", runtime_target, runtime_files)
    print_report("Binaries", binary_target, binary_files)

    entries = runtime_files + binary_files
    replaced = sum(1 for _, existed in entries if existed)
    manifest = write_source_manifest(output_directory, REPOSITORY_ROOT, len(entries))
    print(f"Source -> {manifest}")
    print(
        f"Total: {len(entries)} files ({len(entries) - replaced} new, "
        f"{replaced} replaced) in {output_directory}"
    )
    pass


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Collect build and ONNX Runtime outputs into artifacts/."
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DEFAULT_OUTPUT_DIR,
        help="Directory that receives the artifacts tree.",
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=DEFAULT_BUILD_DIR,
        help="CMake build directory; libraries are read from <build-dir>/bin/lib.",
    )
    parser.add_argument(
        "--runtime-dir",
        type=Path,
        default=DEFAULT_RUNTIME_DIR,
        help="Directory holding the fetched ONNX Runtime libraries.",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Remove the previous binary/ and runtime/ artifacts before copying.",
    )
    parser.add_argument(
        "--flat",
        action="store_true",
        help="Drop the platform subdirectory and merge every file into one level.",
    )
    return parser.parse_args()
    pass


if __name__ == "__main__":
    arguments = parse_arguments()
    pack(
        arguments.output_dir.resolve(),
        arguments.build_dir.resolve(),
        arguments.runtime_dir.resolve(),
        arguments.clean,
        arguments.flat,
    )
    pass
