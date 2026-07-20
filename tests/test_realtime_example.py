"""Tests for the real-time example using only invented protocol records."""

from __future__ import annotations

import importlib.util
from pathlib import Path
from types import ModuleType

from massive_speedup import WebSocket


def load_example() -> ModuleType:
    path = Path(__file__).resolve().parents[1] / "examples" / "howto_realtime_stock_signals.py"
    specification = importlib.util.spec_from_file_location(
        "howto_realtime_stock_signals",
        path,
    )
    assert specification is not None
    assert specification.loader is not None
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


def test_realtime_example_prints_signals_and_tracks_its_own_inventory(capsys) -> None:
    example = load_example()
    endpoint = example.PrintSignals()
    frames = [
        '[{"ev":"Q","sym":"ZZTEST","bp":10.0,"ap":11.0,"t":1000},'
        '{"ev":"T","sym":"ZZTEST","p":10.0,"s":1,"t":1100},'
        '{"ev":"T","sym":"ZZTEST","p":10.5,"s":1,"t":1200},'
        '{"ev":"T","sym":"ZZTEST","p":11.0,"s":1,"t":1300}]'
    ]
    market = WebSocket.Stocks.market(frames, endpoint, fast=True)

    inventory = example.run_signals(market)

    assert capsys.readouterr().out.splitlines() == ["buy", "sell"]
    assert inventory == {"ZZTEST": 0}
    assert vars(endpoint) == {}
