#!/usr/bin/env python3
"""Download the prebuilt ONNX Runtime for one or more platforms.

Upstream ships a separate archive per platform, each containing the same public
headers plus that platform's shared library. This script unpacks them into a
single tree so CMake can point at one include directory regardless of host:

    third_party/onnxruntime/
        include/                        headers, identical across platforms
        lib/win64-x64/onnxruntime.dll   runtime, one directory per platform
        lib/linux64-x64/libonnxruntime.so.1.28.0
        lib/darwin64-arm64/libonnxruntime.1.28.0.dylib
        VERSION                         platform tag and version, one per line

Nothing here is linked at build time: only include/ is needed to compile, and
the FMU opens the runtime by absolute path at fmi2Instantiate time. The lib/
directories exist so the packaging step can copy the right one next to the
model, which is why the tree keeps them separated per platform.

Run without arguments to fetch the host platform only; pass --all to fetch every
platform (needed when building FMUs for machines other than this one).

    python third_party/onnxruntime/fetch.py
    python third_party/onnxruntime/fetch.py --all
    python third_party/onnxruntime/fetch.py --platform linux64-x64 --version 1.28.0

Only the standard library is used, so no pip install is required.
"""

from __future__ import annotations

import argparse
import hashlib
import platform
import shutil
import sys
import tarfile
import tempfile
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

# Pinned rather than resolved to "latest" so a build is reproducible and an
# upstream release cannot silently change what gets linked. Bump deliberately.
DEFAULT_VERSION = "1.28.0"

RELEASE_URL = (
    "https://github.com/microsoft/onnxruntime/releases/download/v{version}/{asset}"
)

# Maps our platform tag -> upstream archive infix. Our tags follow the FMI 2.0
# platform directory names (win64/linux64/darwin64) with the architecture
# appended, because FMI has no standard tag for anything but x86-64 and the
# packaging step needs to tell the variants apart.
PLATFORMS = {
    "win64-x64": "win-x64",
    "win64-arm64": "win-arm64",
    "linux64-x64": "linux-x64",
    "linux64-aarch64": "linux-aarch64",
    "darwin64-arm64": "osx-arm64",
}

# Intel macOS was dropped upstream after 1.23.x: releases from 1.24.0 on ship
# only osx-arm64. Fetching it therefore has to pin an older version, which means
# a different C API version than the other platforms in the same tree.
LEGACY_PLATFORMS = {
    "darwin64-x64": ("osx-x86_64", "1.23.0"),
    "darwin64-universal2": ("osx-universal2", "1.23.0"),
}

ROOT = Path(__file__).resolve().parent


def host_platform() -> str:
    machine = platform.machine().lower()
    system = platform.system()

    if system == "Windows":
        return "win64-arm64" if machine in ("arm64", "aarch64") else "win64-x64"
        pass
    if system == "Linux":
        return "linux64-aarch64" if machine in ("aarch64", "arm64") else "linux64-x64"
        pass
    if system == "Darwin":
        if machine in ("arm64", "aarch64"):
            return "darwin64-arm64"
            pass
        raise SystemExit(
            "Intel macOS is not covered by current ONNX Runtime releases.\n"
            "Fetch the last version that supported it explicitly:\n"
            "    python fetch.py --platform darwin64-x64"
        )
        pass
    raise SystemExit(f"Unsupported host system: {system}")
    pass


def asset_name(infix: str, version: str) -> str:
    suffix = "zip" if infix.startswith("win-") else "tgz"
    return f"onnxruntime-{infix}-{version}.{suffix}"
    pass


def resolve(tag: str, version_override: str | None) -> tuple[str, str]:
    """Return (upstream infix, version) for one of our platform tags."""
    if tag in PLATFORMS:
        return PLATFORMS[tag], version_override or DEFAULT_VERSION
        pass
    if tag in LEGACY_PLATFORMS:
        infix, pinned = LEGACY_PLATFORMS[tag]
        if version_override:
            return infix, version_override
            pass
        print(
            f"note: {tag} is only available up to {pinned}; using that instead "
            f"of {DEFAULT_VERSION}",
            file=sys.stderr,
        )
        return infix, pinned
        pass
    known = ", ".join(sorted({**PLATFORMS, **LEGACY_PLATFORMS}))
    raise SystemExit(f"Unknown platform '{tag}'. Known platforms: {known}")
    pass


# The Windows archives are around 80 MB, so a stalled connection is a real
# possibility. Resume from what already arrived instead of starting over.
ATTEMPTS = 5
TIMEOUT_SECONDS = 60


def remote_size(url: str) -> int | None:
    """Content length of the asset, or None if the server will not say.

    Used to tell a complete cached download from a truncated one, since a
    partial file is otherwise indistinguishable from a finished one.
    """
    request = urllib.request.Request(url, method="HEAD")
    try:
        with urllib.request.urlopen(request, timeout=TIMEOUT_SECONDS) as response:
            length = response.headers.get("Content-Length")
            return int(length) if length else None
            pass
        pass
    except (urllib.error.URLError, TimeoutError, ConnectionError, ValueError):
        return None
        pass
    pass


def download_once(url: str, target: Path, expected_size: int | None) -> None:
    resume_from = target.stat().st_size if target.is_file() else 0
    if expected_size is not None and resume_from >= expected_size:
        return
        pass

    request = urllib.request.Request(url)
    mode = "wb"
    if resume_from:
        request.add_header("Range", f"bytes={resume_from}-")
        mode = "ab"
        pass

    with urllib.request.urlopen(request, timeout=TIMEOUT_SECONDS) as response:
        # A server that ignores the Range header replies 200 with the whole
        # body; appending that to a partial file would corrupt it.
        if resume_from and response.status != 206:
            mode = "wb"
            pass
        with target.open(mode) as out:
            shutil.copyfileobj(response, out)
            pass
        pass
    pass


def download(url: str, target: Path, expected_size: int | None = None) -> None:
    print(f"  fetching {url}")

    for attempt in range(1, ATTEMPTS + 1):
        try:
            download_once(url, target, expected_size)
            have = target.stat().st_size if target.is_file() else 0
            # A connection dropped mid-body raises nothing, so the size is the
            # only signal that the transfer was cut short. Retry to resume.
            if expected_size is None or have >= expected_size:
                break
                pass
            if attempt == ATTEMPTS:
                raise SystemExit(
                    f"  incomplete after {ATTEMPTS} attempts: got "
                    f"{have / 1e6:.1f} of {expected_size / 1e6:.1f} MB.\n"
                    f"  Re-run to resume, or download the archive by hand and "
                    f"pass --archive <path>."
                )
                pass
            print(
                f"  attempt {attempt}/{ATTEMPTS} truncated at "
                f"{have / 1e6:.1f}/{expected_size / 1e6:.1f} MB; resuming"
            )
            continue
            pass
        except urllib.error.HTTPError as error:
            if error.code == 404:
                raise SystemExit(
                    f"  not found: {url}\n"
                    "  This version and platform combination does not exist "
                    "upstream."
                ) from error
                pass
            # 416 means the range request started past the end of the file,
            # so what is on disk is already complete or longer than the asset.
            if error.code == 416 and target.is_file():
                break
                pass
            raise
            pass
        except (urllib.error.URLError, TimeoutError, ConnectionError) as error:
            have = target.stat().st_size if target.is_file() else 0
            if attempt == ATTEMPTS:
                raise SystemExit(
                    f"  giving up after {ATTEMPTS} attempts: {error}\n"
                    f"  Got {have / 1e6:.1f} MB. Re-run to resume, or download "
                    f"the archive by hand and pass --archive <path>."
                ) from error
                pass
            print(
                f"  attempt {attempt}/{ATTEMPTS} failed ({error}); "
                f"resuming from {have / 1e6:.1f} MB"
            )
            pass
        pass

    digest = hashlib.sha256(target.read_bytes()).hexdigest()
    size_mb = target.stat().st_size / 1e6
    print(f"  {size_mb:.1f} MB, sha256 {digest[:16]}...")
    pass


def extract(archive: Path, into: Path) -> Path:
    """Unpack the archive and return its single top-level directory."""
    if archive.suffix == ".zip":
        with zipfile.ZipFile(archive) as handle:
            handle.extractall(into)
            pass
        pass
    else:
        # filter="data" refuses absolute paths and traversal entries. Required
        # from Python 3.14 on, where the default became an error.
        with tarfile.open(archive) as handle:
            handle.extractall(into, filter="data")
            pass
        pass

    entries = [path for path in into.iterdir() if path.is_dir()]
    if len(entries) != 1:
        raise SystemExit(f"  unexpected archive layout: {[p.name for p in entries]}")
        pass
    return entries[0]
    pass


# Files we keep from each archive's lib/ directory. Upstream also ships import
# libraries, static archives and provider stubs we have no use for: the FMU
# loads the runtime with dlopen/LoadLibrary at run time, so only the shared
# library itself has to travel with it.
def is_runtime_library(name: str) -> bool:
    if name.endswith(".dll"):
        return True
        pass
    if name.endswith(".dylib") or ".dylib" in name:
        return True
        pass
    return name.endswith(".so") or ".so." in name
    pass


def install_headers(source: Path, destination: Path) -> int:
    """Copy the public headers, which are identical across platforms."""
    include_dir = source / "include"
    if not include_dir.is_dir():
        raise SystemExit(f"  archive has no include/ directory: {source}")
        pass

    destination.mkdir(parents=True, exist_ok=True)
    count = 0
    for header in sorted(include_dir.rglob("*.h")):
        target = destination / header.relative_to(include_dir)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(header, target)
        count += 1
        pass

    if not (destination / "onnxruntime_c_api.h").is_file():
        raise SystemExit("  onnxruntime_c_api.h missing from the archive headers")
        pass
    return count
    pass


def install_libraries(source: Path, destination: Path) -> list[str]:
    lib_dir = source / "lib"
    if not lib_dir.is_dir():
        raise SystemExit(f"  archive has no lib/ directory: {source}")
        pass

    if destination.exists():
        shutil.rmtree(destination)
        pass
    destination.mkdir(parents=True)

    # Linux ships libonnxruntime.so and .so.1 as symlinks to .so.1.28.0, and
    # extracting on a host that cannot create symlinks turns each into a full
    # copy of the same 20 MB. Deduplicate by content and keep the shortest name,
    # which is the plain unversioned one the FMU loader probes for. Comparing
    # bytes rather than testing for symlinks makes this independent of how the
    # extraction happened to handle them.
    candidates = [
        entry
        for entry in sorted(lib_dir.iterdir())
        if entry.is_file() and is_runtime_library(entry.name)
    ]

    chosen: dict[str, Path] = {}
    for entry in candidates:
        digest = hashlib.sha256(entry.read_bytes()).hexdigest()
        previous = chosen.get(digest)
        if previous is None or len(entry.name) < len(previous.name):
            chosen[digest] = entry
            pass
        pass

    installed: list[str] = []
    for entry in sorted(chosen.values(), key=lambda path: path.name):
        shutil.copy2(entry, destination / entry.name)
        installed.append(entry.name)
        pass

    if not installed:
        raise SystemExit(f"  no shared library found in {lib_dir}")
        pass
    return installed
    pass


VERSION_FILE = ROOT / "VERSION.txt"


def read_versions() -> dict[str, str]:
    """Platform tag -> version, as recorded by earlier runs."""
    if not VERSION_FILE.is_file():
        return {}
        pass

    versions: dict[str, str] = {}
    for line in VERSION_FILE.read_text().splitlines():
        parts = line.split()
        if len(parts) == 2:
            versions[parts[0]] = parts[1]
            pass
        pass
    return versions
    pass


def fetch(
    tag: str,
    version_override: str | None,
    keep_headers: bool,
    local_archive: Path | None = None,
) -> str:
    infix, version = resolve(tag, version_override)
    asset = asset_name(infix, version)

    print(f"{tag} (onnxruntime {version}, {infix})")

    if local_archive is not None:
        archive = local_archive
        if not archive.is_file():
            raise SystemExit(f"  no such archive: {archive}")
            pass
        print(f"  using {archive}")
        pass
    else:
        # Kept outside the temp directory so an interrupted run can resume
        # rather than re-download from zero.
        cache = ROOT / ".cache"
        cache.mkdir(parents=True, exist_ok=True)
        archive = cache / asset
        url = RELEASE_URL.format(version=version, asset=asset)
        download(url, archive, expected_size=remote_size(url))
        pass

    with tempfile.TemporaryDirectory() as scratch:
        try:
            unpacked = extract(archive, Path(scratch))
            pass
        except (tarfile.TarError, zipfile.BadZipFile, OSError) as error:
            raise SystemExit(
                f"  {archive.name} is not a readable archive ({error}).\n"
                f"  Delete it and re-run: {archive}"
            ) from error
            pass

        if not keep_headers:
            count = install_headers(unpacked, ROOT / "include")
            print(f"  include/: {count} headers")
            pass

        libraries = install_libraries(unpacked, ROOT / "lib" / tag)
        for name in libraries:
            size_mb = (ROOT / "lib" / tag / name).stat().st_size / 1e6
            print(f"  lib/{tag}/{name}  ({size_mb:.1f} MB)")
            pass
        pass

    return version
    pass


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Download prebuilt ONNX Runtime libraries and headers.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Platforms: " + ", ".join(sorted({**PLATFORMS, **LEGACY_PLATFORMS})),
    )
    parser.add_argument(
        "--platform",
        action="append",
        dest="platforms",
        metavar="TAG",
        help="platform to fetch; repeatable. Defaults to the host platform.",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="fetch every currently supported platform",
    )
    parser.add_argument(
        "--version",
        help=f"onnxruntime version to fetch (default {DEFAULT_VERSION})",
    )
    parser.add_argument(
        "--archive",
        type=Path,
        metavar="PATH",
        help="unpack an already-downloaded archive instead of fetching it; "
        "requires exactly one --platform",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="print what is already present and exit",
    )
    arguments = parser.parse_args()

    if arguments.list:
        versions = read_versions()
        if not versions:
            print("nothing fetched yet")
            return 0
            pass

        headers = ROOT / "include" / "onnxruntime_c_api.h"
        print(f"headers: {'present' if headers.is_file() else 'MISSING'}")
        for tag, version in sorted(versions.items()):
            lib_dir = ROOT / "lib" / tag
            if lib_dir.is_dir():
                names = ", ".join(sorted(path.name for path in lib_dir.iterdir()))
                pass
            else:
                names = "MISSING"
                pass
            print(f"  {tag}  {version}  {names}")
            pass
        return 0
        pass

    if arguments.all:
        targets = list(PLATFORMS)
        pass
    elif arguments.platforms:
        targets = arguments.platforms
        pass
    else:
        targets = [host_platform()]
        pass

    if arguments.archive and len(targets) != 1:
        parser.error("--archive needs exactly one --platform to unpack it as")
        pass

    # Preserve what is already on disk so fetching one platform does not drop
    # the record of the others.
    versions = read_versions()
    for index, tag in enumerate(targets):
        # The headers are identical in every archive, so unpack them once.
        versions[tag] = fetch(
            tag,
            arguments.version,
            keep_headers=index > 0,
            local_archive=arguments.archive,
        )
        pass

    VERSION_FILE.write_text(
        "\n".join(f"{tag} {version}" for tag, version in sorted(versions.items()))
        + "\n"
    )

    distinct = sorted(set(versions.values()))
    if len(distinct) > 1:
        print(
            f"\nwarning: this tree mixes versions ({', '.join(distinct)}) but "
            "holds only one copy of the headers. Build each platform against a "
            "tree fetched at its own version if the C API versions differ.",
            file=sys.stderr,
        )
        pass

    print("\nDone. Configure with: cmake -S . -B build")
    return 0
    pass


if __name__ == "__main__":
    sys.exit(main())
    pass
