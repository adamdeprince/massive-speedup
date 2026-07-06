"""Process-title helpers for console scripts."""

from __future__ import annotations


def set_process_title(title: str) -> None:
    from setproctitle import setproctitle

    setproctitle(title)
