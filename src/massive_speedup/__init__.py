"""Python bootstrap for the native nanobind module."""

from __future__ import annotations

import importlib
import os
import sys
from types import ModuleType


def _install_stock_api(module: ModuleType) -> None:
    if getattr(module, "_massive_speedup_stock_api_installed", False):
        return

    stock = module.FlatFiles.Stock
    if hasattr(stock, "Trade") and hasattr(stock, "Quote") and hasattr(stock, "Aggregate"):
        module._massive_speedup_stock_api_installed = True
        return

    class Trade:
        @classmethod
        def parse(
            cls,
            path,
            *,
            sort_by_participant_timestamp: bool = False,
            sort_by_sip_timestamp: bool = False,
        ):
            return stock.parse_trades(
                path,
                sort_by_participant_timestamp=sort_by_participant_timestamp,
                sort_by_sip_timestamp=sort_by_sip_timestamp,
            )

        @classmethod
        def parse_raw(
            cls,
            path,
            *,
            sort_by_participant_timestamp: bool = False,
            sort_by_sip_timestamp: bool = False,
        ):
            return stock.parse_raw_trades(
                path,
                sort_by_participant_timestamp=sort_by_participant_timestamp,
                sort_by_sip_timestamp=sort_by_sip_timestamp,
            )

        @classmethod
        def raw_lines(cls, path):
            return stock.raw_lines(path)

    class Quote:
        @classmethod
        def parse(
            cls,
            path,
            *,
            sort_by_participant_timestamp: bool = False,
            sort_by_sip_timestamp: bool = False,
        ):
            return stock.parse_quotes(
                path,
                sort_by_participant_timestamp=sort_by_participant_timestamp,
                sort_by_sip_timestamp=sort_by_sip_timestamp,
            )

        @classmethod
        def parse_raw(
            cls,
            path,
            *,
            sort_by_participant_timestamp: bool = False,
            sort_by_sip_timestamp: bool = False,
        ):
            return stock.parse_raw_quotes(
                path,
                sort_by_participant_timestamp=sort_by_participant_timestamp,
                sort_by_sip_timestamp=sort_by_sip_timestamp,
            )

        @classmethod
        def raw_lines(cls, path):
            return stock.raw_lines(path)

    class Aggregate:
        @classmethod
        def parse(cls, path, *, sort_by_window_start: bool = False):
            return stock.parse_minute_aggregates(
                path,
                sort_by_window_start=sort_by_window_start,
            )

        @classmethod
        def parse_raw(cls, path, *, sort_by_window_start: bool = False):
            return stock.parse_raw_minute_aggregates(
                path,
                sort_by_window_start=sort_by_window_start,
            )

        @classmethod
        def raw_lines(cls, path):
            return stock.raw_lines(path)

    stock.Trade = Trade
    stock.Quote = Quote
    stock.Aggregate = Aggregate
    module._massive_speedup_stock_api_installed = True


def _install_currency_api(module: ModuleType) -> None:
    if getattr(module, "_massive_speedup_currency_api_installed", False):
        return

    currency = getattr(module.FlatFiles, "currency", None)
    if currency is None:
        module._massive_speedup_currency_api_installed = True
        return

    if not hasattr(currency, "Quote"):
        class Quote:
            @classmethod
            def parse(
                cls,
                path,
                *,
                sort_by_participant_timestamp: bool = False,
                sort_by_sip_timestamp: bool = False,
            ):
                return currency.parse_quotes(
                    path,
                    sort_by_participant_timestamp=sort_by_participant_timestamp,
                    sort_by_sip_timestamp=sort_by_sip_timestamp,
                )

            @classmethod
            def parse_raw(
                cls,
                path,
                *,
                sort_by_participant_timestamp: bool = False,
                sort_by_sip_timestamp: bool = False,
            ):
                return currency.parse_raw_quotes(
                    path,
                    sort_by_participant_timestamp=sort_by_participant_timestamp,
                    sort_by_sip_timestamp=sort_by_sip_timestamp,
                )

            @classmethod
            def raw_lines(cls, path):
                return currency.raw_lines(path)

        currency.Quote = Quote

    if not hasattr(currency, "Aggregate"):
        class Aggregate:
            @classmethod
            def parse(cls, path, *, sort_by_window_start: bool = False):
                return currency.parse_minute_aggregates(
                    path,
                    sort_by_window_start=sort_by_window_start,
                )

            @classmethod
            def parse_raw(cls, path, *, sort_by_window_start: bool = False):
                return currency.parse_raw_minute_aggregates(
                    path,
                    sort_by_window_start=sort_by_window_start,
                )

            @classmethod
            def raw_lines(cls, path):
                return currency.raw_lines(path)

        currency.Aggregate = Aggregate

    module._massive_speedup_currency_api_installed = True


_BACKEND_ENV_VAR = "MASSIVE_SPEEDUP_BACKEND"

# Ordered priority for backend selection when no override is given. Each entry
# names the BackendKind member followed by the suffix of the compiled module.
_BACKEND_PRIORITY: tuple[tuple[str, str], ...] = (
    ("X86_AVX512BW", "_avx512bw"),
    ("X86_AVX2", "_avx2"),
    ("AARCH64_NEON", "_neon"),
    ("SWAR", "_swar"),
    ("GENERIC", "_generic"),
)


def _resolve_backend() -> tuple[str, ModuleType] | None:
    try:
        from . import _cpu  # type: ignore[attr-defined]
    except ImportError:
        _cpu = None  # type: ignore[assignment]

    override = os.environ.get(_BACKEND_ENV_VAR, "").strip().lower()
    if override:
        suffix = "_" + override.lstrip("_")
        try:
            module = importlib.import_module(f"{__name__}.{suffix}")
        except ImportError as exc:
            raise ImportError(
                f"{_BACKEND_ENV_VAR}={override!r} requested but module "
                f"{__name__}.{suffix} is not available"
            ) from exc
        return suffix.lstrip("_"), module

    detected: str | None = None
    if _cpu is not None:
        try:
            detected = _cpu.detect_best_backend().name
        except Exception:  # pragma: no cover - defensive
            detected = None

    ordered = list(_BACKEND_PRIORITY)
    if detected is not None:
        ordered.sort(key=lambda entry: 0 if entry[0] == detected else 1)

    for kind_name, suffix in ordered:
        try:
            module = importlib.import_module(f"{__name__}.{suffix}")
            return kind_name.lower(), module
        except ImportError:
            continue
    return None


_resolution = _resolve_backend()
if _resolution is not None:
    _BACKEND_KIND, _backend = _resolution
    for _name in dir(_backend):
        if not _name.startswith("_"):
            globals()[_name] = getattr(_backend, _name)
    sys.modules[__name__]._native = _backend  # type: ignore[attr-defined]
else:
    _BACKEND_KIND = "fallback"
    from ._fallback import *  # noqa: F403


_install_stock_api(sys.modules[__name__])
_install_currency_api(sys.modules[__name__])


def read_gzip_lines(path, parallelization=0, chunk_size=1 << 20):
    yield from gzip_lines(  # type: ignore[name-defined]
        path,
        parallelization=parallelization,
        chunk_size=chunk_size,
    )


__all__ = [
    "FlatFiles",
    "StockTrade",
    "StockQuote",
    "StockAggregate",
    "StockTradeAggregation",
    "StockQuoteAggregation",
    "StockTradeAggregator",
    "StockQuoteAggregator",
    "StockTradeDatabase",
    "StockQuoteDatabase",
    "StockQuotes",
    "CurrencyQuote",
    "CurrencyAggregate",
    "CurrencyQuoteAggregation",
    "CurrencyQuoteAggregator",
    "CurrencyQuoteDatabase",
    "WebSocket",
    "gzip_lines",
    "read_gzip_lines",
    "build_database_file",
]
