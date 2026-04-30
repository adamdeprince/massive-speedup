"""Python bootstrap for the native nanobind module."""

from types import ModuleType
import sys


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


try:
    from ._native import *  # noqa: F403
except ImportError:
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
    "StockQuotes",
    "CurrencyQuote",
    "CurrencyAggregate",
    "WebSocket",
    "gzip_lines",
    "read_gzip_lines",
    "build_database_file",
]
