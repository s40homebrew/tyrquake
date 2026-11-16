"""Portable SDK and LLVM tool locations."""

import os
import shutil
from pathlib import Path

PORT = Path(__file__).resolve().parents[1]
SDK_ROOT = Path(os.environ.get("S40_SDK_ROOT", PORT.parents[1] / "main")).resolve()


def llvm_tool(name):
    """Respect explicit LLVM selection, then PATH and Homebrew's keg-only LLVM."""
    if os.environ.get("LLVM"):
        candidate = Path(os.environ["LLVM"]) / name
        if candidate.is_file():
            return str(candidate)
        raise SystemExit(f"Missing LLVM tool: {name}; check LLVM")
    candidate = shutil.which(name)
    if candidate:
        return candidate
    candidate = Path("/opt/homebrew/opt/llvm/bin") / name
    if candidate.is_file():
        return str(candidate)
    raise SystemExit(f"Missing LLVM tool: {name}; add it to PATH or set LLVM")
