"""Resolve configured storage roots for database and downloaded data files."""

from __future__ import annotations

import os
from pathlib import Path


DATABASE_PATH_ENV = "MASSIVE_SPEEDUP_DB_PATH"
DOWNLOAD_PATH_ENV = "MASSIVE_SPEEDUP_DOWNLOAD_PATH"


def _configured_path(
    value: str | os.PathLike[str] | None,
    *,
    environment_variable: str,
    argument_name: str,
) -> Path:
    if value is None:
        value = os.environ.get(environment_variable)
    if value is None or not os.fspath(value):
        raise RuntimeError(
            f"{argument_name} is not configured; set {environment_variable} "
            f"or pass {argument_name}=..."
        )
    return Path(value).expanduser().resolve()


def database_path(value: str | os.PathLike[str] | None = None) -> Path:
    """Return an explicit database root or the configured default."""

    return _configured_path(
        value,
        environment_variable=DATABASE_PATH_ENV,
        argument_name="database_path",
    )


def download_path(value: str | os.PathLike[str] | None = None) -> Path:
    """Return an explicit download root or the configured default."""

    return _configured_path(
        value,
        environment_variable=DOWNLOAD_PATH_ENV,
        argument_name="download_path",
    )
