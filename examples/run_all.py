"""Recursively run the Python example scripts with the current interpreter."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


script_path: Path = Path(__file__).resolve()
examples_directory: Path = script_path.parent


def find_scripts() -> list[Path]:
    return sorted(
        path
        for path in examples_directory.rglob("*.py")
        if path.is_file()
        and path.resolve() != script_path
        and "__pycache__" not in path.relative_to(examples_directory).parts
    )
    pass


def main() -> int:
    scripts = find_scripts()
    failed_scripts: list[Path] = []

    if not scripts:
        print("No Python example scripts found.")
        return 0
        pass

    for index, script in enumerate(scripts, start=1):
        relative_path = script.relative_to(examples_directory).as_posix()
        print(f"\n[{index}/{len(scripts)}] {relative_path}", flush=True)
        try:
            result = subprocess.run(
                [sys.executable, "-u", str(script)],
                cwd=script.parent,
                check=False,
            )
            pass
        except OSError as error:
            failed_scripts.append(script)
            print(f"[FAIL] {relative_path}: {error}", file=sys.stderr, flush=True)
            continue
            pass

        if result.returncode != 0:
            failed_scripts.append(script)
            print(
                f"[FAIL] {relative_path}: exit code {result.returncode}",
                file=sys.stderr,
                flush=True,
            )
            pass
        else:
            print(f"[OK] {relative_path}", flush=True)
            pass
        pass

    print(
        f"\nCompleted: {len(scripts) - len(failed_scripts)} succeeded, "
        f"{len(failed_scripts)} failed.",
        flush=True,
    )
    return 1 if failed_scripts else 0
    pass


if __name__ == "__main__":
    sys.exit(main())
    pass
