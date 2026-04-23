"""Python bootstrap for processor-specialized nanobind modules."""

from importlib import import_module, machinery, util
from types import ModuleType
from pathlib import Path
import sys

from ._cpu import BackendKind, available_backends, backend_is_available, detect_best_backend, detect_processor_type

_BACKEND_MODULES = {
    BackendKind.GENERIC: "massive_speedup._generic",
    BackendKind.X86_SSE42: "massive_speedup._sse",
    BackendKind.X86_AVX2: "massive_speedup._avx",
    BackendKind.X86_AVX512: "massive_speedup._avx512",
    BackendKind.LINUX_AARCH64_NEON: "massive_speedup._neon",
    BackendKind.LINUX_AARCH64_SVE: "massive_speedup._sve",
    BackendKind.LINUX_AARCH64_SVE2: "massive_speedup._sve2",
    BackendKind.LINUX_LOONGARCH64_LSX: "massive_speedup._lsx",
    BackendKind.LINUX_LOONGARCH64_LASX: "massive_speedup._lasx",
}

_backend_module: ModuleType | None = None


def _install_stock_api(module: ModuleType) -> ModuleType:
    if getattr(module, "_massive_speedup_stock_api_installed", False):
        return module

    stocks = module.FlatFiles.Stocks
    if hasattr(stocks, "Trade") and hasattr(stocks, "Quote"):
        module._massive_speedup_stock_api_installed = True
        return module

    class Trade:
        @classmethod
        def parse(
            cls,
            path,
            *,
            sort_by_participant_timestamp: bool = False,
            sort_by_sip_timestamp: bool = False,
        ):
            return stocks.parse_trades(
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
            return stocks.parse_raw_trades(
                path,
                sort_by_participant_timestamp=sort_by_participant_timestamp,
                sort_by_sip_timestamp=sort_by_sip_timestamp,
            )

        @classmethod
        def raw_lines(cls, path):
            return stocks.raw_lines(path)

    class Quote:
        @classmethod
        def parse(
            cls,
            path,
            *,
            sort_by_participant_timestamp: bool = False,
            sort_by_sip_timestamp: bool = False,
        ):
            return stocks.parse_quotes(
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
            return stocks.parse_raw_quotes(
                path,
                sort_by_participant_timestamp=sort_by_participant_timestamp,
                sort_by_sip_timestamp=sort_by_sip_timestamp,
            )

        @classmethod
        def raw_lines(cls, path):
            return stocks.raw_lines(path)

    stocks.Trade = Trade
    stocks.Quote = Quote
    module._massive_speedup_stock_api_installed = True
    return module


def _load_local_backend_module(module_name: str) -> ModuleType | None:
    package_dir = Path(__file__).resolve().parent
    module_basename = module_name.rsplit(".", 1)[-1]

    for suffix in machinery.EXTENSION_SUFFIXES:
        candidate = package_dir / f"{module_basename}{suffix}"
        if not candidate.exists():
            continue

        spec = util.spec_from_file_location(module_name, candidate)
        if spec is None or spec.loader is None:
            continue

        module = util.module_from_spec(spec)
        sys.modules[module_name] = module
        spec.loader.exec_module(module)
        return module

    return None


def _load_backend_module():
    global _backend_module

    if _backend_module is not None:
        return _backend_module

    backend = detect_best_backend()
    module_name = _BACKEND_MODULES.get(backend, "massive_speedup._generic")

    try:
        _backend_module = _load_local_backend_module(module_name) or import_module(module_name)
    except ImportError:
        _backend_module = import_module("massive_speedup._fallback")

    return _install_stock_api(_backend_module)


def __getattr__(name):
    if name in {"FlatFiles", "WebSocket", "StockTrade", "StockQuote", "StockQuotes"}:
        return getattr(_load_backend_module(), name)
    raise AttributeError(name)


def gzip_lines(path, parallelization=0, chunk_size=1 << 20):
    yield from _load_backend_module().gzip_lines(
        path,
        parallelization=parallelization,
        chunk_size=chunk_size,
    )


def read_gzip_lines(path, parallelization=0, chunk_size=1 << 20):
    yield from gzip_lines(path, parallelization=parallelization, chunk_size=chunk_size)


__all__ = [
    "BackendKind",
    "FlatFiles",
    "StockTrade",
    "StockQuote",
    "StockQuotes",
    "WebSocket",
    "available_backends",
    "backend_is_available",
    "detect_best_backend",
    "detect_processor_type",
    "gzip_lines",
    "read_gzip_lines",
]
