"""Python fallback for CPU detection when the native module is unavailable."""

from __future__ import annotations

from ._fallback import (
    BackendKind,
    BackendRecord,
    ProcessorType,
    available_backends,
    backend_is_available,
    detect_best_backend,
    detect_processor_type,
)

__all__ = [
    "BackendKind",
    "BackendRecord",
    "ProcessorType",
    "available_backends",
    "backend_is_available",
    "detect_best_backend",
    "detect_processor_type",
]
