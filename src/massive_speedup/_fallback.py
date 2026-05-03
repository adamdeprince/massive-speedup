"""Pure-Python fallback implementations for source-tree testing."""

from __future__ import annotations

import csv
import gzip
import sys
from pathlib import Path
from types import SimpleNamespace
from typing import Iterator


def read_gzip_lines(path: str | Path):
    with gzip.open(path, "rb") as handle:
        for line in handle:
            yield line.rstrip(b"\r\n")


def gzip_lines(path: str | Path, parallelization: int = 0, chunk_size: int = 1 << 20):
    del parallelization, chunk_size
    yield from read_gzip_lines(path)


def _to_text(payload: bytes | str) -> str:
    if isinstance(payload, bytes):
        return payload.decode("utf-8", errors="replace")
    return payload


def _parse_int(text: str) -> int:
    return int(text) if text else 0


def _parse_float(text: str) -> float:
    return float(text) if text else 0.0


def _parse_bitset96(text: str) -> str:
    if not text:
        return "0" * 96
    if text.startswith(("0x", "0X")):
        return f"{int(text, 16):096b}"[-96:]
    if all(ch in "01" for ch in text) and len(text) <= 96:
        return text.zfill(96)
    if text.isdigit():
        return f"{int(text):096b}"[-96:]

    bits = [0] * 96
    current = []
    for ch in text:
        if ch.isdigit():
            current.append(ch)
            continue
        if current:
            index = int("".join(current))
            if 0 <= index < 96:
                bits[index] = 1
            current.clear()
    if current:
        index = int("".join(current))
        if 0 <= index < 96:
            bits[index] = 1
    return "".join("1" if bit else "0" for bit in reversed(bits))


_BITSET_INDICES_INTERN_CACHE: dict[str, frozenset[int]] = {}


def _bitset_indices(text: str) -> frozenset[int]:
    bit_string = _parse_bitset96(text)
    cached = _BITSET_INDICES_INTERN_CACHE.get(bit_string)
    if cached is not None:
        return cached

    cached = frozenset(index for index, value in enumerate(reversed(bit_string)) if value == "1")
    _BITSET_INDICES_INTERN_CACHE[bit_string] = cached
    return cached


_TICKERS_INTERN_CACHE: dict[str, tuple[str, str]] = {}


def _intern_tickers(ticker: str) -> tuple[str, str]:
    cached = _TICKERS_INTERN_CACHE.get(ticker)
    if cached is not None:
        return cached

    symbol = ticker.partition(":")[2] or ticker
    base, _, quote = symbol.partition("-")
    cached = (sys.intern(base), sys.intern(quote))
    _TICKERS_INTERN_CACHE[ticker] = cached
    return cached


class StockTrade:
    __slots__ = (
        "ticker",
        "conditions",
        "correction",
        "exchange",
        "id",
        "participant_timestamp",
        "price",
        "sequence_number",
        "sip_timestamp",
        "size",
        "tape",
        "trf_id",
        "trf_timestamp",
    )

    def __init__(self, fields: list[str]) -> None:
        if len(fields) != 13:
            raise ValueError(f"StockTrade expected 13 fields, received {len(fields)}")
        self.ticker = fields[0]
        self.conditions = _bitset_indices(fields[1])
        self.correction = _parse_int(fields[2])
        self.exchange = _parse_int(fields[3])
        self.id = _parse_int(fields[4])
        self.participant_timestamp = _parse_int(fields[5])
        self.price = _parse_float(fields[6])
        self.sequence_number = _parse_int(fields[7])
        self.sip_timestamp = _parse_int(fields[8])
        self.size = _parse_float(fields[9])
        self.tape = _parse_int(fields[10])
        self.trf_id = _parse_int(fields[11])
        self.trf_timestamp = _parse_int(fields[12])

    def __iter__(self) -> Iterator[object]:
        yield self.ticker
        yield self.conditions
        yield self.correction
        yield self.exchange
        yield self.id
        yield self.participant_timestamp
        yield self.price
        yield self.sequence_number
        yield self.sip_timestamp
        yield self.size
        yield self.tape
        yield self.trf_id
        yield self.trf_timestamp

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, StockTrade):
            return NotImplemented
        return tuple(self) == tuple(other)

    def __lt__(self, other: object) -> bool:
        if not isinstance(other, StockTrade):
            return NotImplemented
        return self.participant_timestamp < other.participant_timestamp

    def __le__(self, other: object) -> bool:
        if not isinstance(other, StockTrade):
            return NotImplemented
        return self.participant_timestamp <= other.participant_timestamp

    def __gt__(self, other: object) -> bool:
        if not isinstance(other, StockTrade):
            return NotImplemented
        return self.participant_timestamp > other.participant_timestamp

    def __ge__(self, other: object) -> bool:
        if not isinstance(other, StockTrade):
            return NotImplemented
        return self.participant_timestamp >= other.participant_timestamp

    def __hash__(self) -> int:
        return hash(tuple(self))

    def __repr__(self) -> str:
        return (
            "StockTrade("
            f"ticker={self.ticker!r}, conditions={self.conditions!r}, "
            f"correction={self.correction}, exchange={self.exchange}, id={self.id}, "
            f"participant_timestamp={self.participant_timestamp}, price={self.price}, "
            f"sequence_number={self.sequence_number}, sip_timestamp={self.sip_timestamp}, "
            f"size={self.size}, tape={self.tape}, trf_id={self.trf_id}, "
            f"trf_timestamp={self.trf_timestamp})"
        )

    __str__ = __repr__


class StockQuote:
    __slots__ = (
        "ticker",
        "ask_exchange",
        "ask_price",
        "ask_size",
        "bid_exchange",
        "bid_price",
        "bid_size",
        "conditions",
        "indicators",
        "participant_timestamp",
        "sequence_number",
        "sip_timestamp",
        "tape",
        "trf_timestamp",
    )

    def __init__(self, fields: list[str]) -> None:
        if len(fields) != 14:
            raise ValueError(f"StockQuote expected 14 fields, received {len(fields)}")
        self.ticker = fields[0]
        self.ask_exchange = _parse_int(fields[1])
        self.ask_price = _parse_float(fields[2])
        self.ask_size = _parse_int(fields[3])
        self.bid_exchange = _parse_int(fields[4])
        self.bid_price = _parse_float(fields[5])
        self.bid_size = _parse_int(fields[6])
        self.conditions = _bitset_indices(fields[7])
        self.indicators = _bitset_indices(fields[8])
        self.participant_timestamp = _parse_int(fields[9])
        self.sequence_number = _parse_int(fields[10])
        self.sip_timestamp = _parse_int(fields[11])
        self.tape = _parse_int(fields[12])
        self.trf_timestamp = _parse_int(fields[13])

    def __iter__(self) -> Iterator[object]:
        yield self.ticker
        yield self.ask_exchange
        yield self.ask_price
        yield self.ask_size
        yield self.bid_exchange
        yield self.bid_price
        yield self.bid_size
        yield self.conditions
        yield self.indicators
        yield self.participant_timestamp
        yield self.sequence_number
        yield self.sip_timestamp
        yield self.tape
        yield self.trf_timestamp

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, StockQuote):
            return NotImplemented
        return tuple(self) == tuple(other)

    def __lt__(self, other: object) -> bool:
        if not isinstance(other, StockQuote):
            return NotImplemented
        return self.participant_timestamp < other.participant_timestamp

    def __le__(self, other: object) -> bool:
        if not isinstance(other, StockQuote):
            return NotImplemented
        return self.participant_timestamp <= other.participant_timestamp

    def __gt__(self, other: object) -> bool:
        if not isinstance(other, StockQuote):
            return NotImplemented
        return self.participant_timestamp > other.participant_timestamp

    def __ge__(self, other: object) -> bool:
        if not isinstance(other, StockQuote):
            return NotImplemented
        return self.participant_timestamp >= other.participant_timestamp

    def __hash__(self) -> int:
        return hash(tuple(self))

    def __repr__(self) -> str:
        return (
            "StockQuote("
            f"ticker={self.ticker!r}, ask_exchange={self.ask_exchange}, "
            f"ask_price={self.ask_price}, ask_size={self.ask_size}, "
            f"bid_exchange={self.bid_exchange}, bid_price={self.bid_price}, "
            f"bid_size={self.bid_size}, conditions={self.conditions!r}, "
            f"indicators={self.indicators!r}, "
            f"participant_timestamp={self.participant_timestamp}, "
            f"sequence_number={self.sequence_number}, sip_timestamp={self.sip_timestamp}, "
            f"tape={self.tape}, trf_timestamp={self.trf_timestamp})"
        )

    __str__ = __repr__


StockQuotes = StockQuote


class StockAggregate:
    __slots__ = (
        "ticker",
        "volume",
        "open",
        "close",
        "high",
        "low",
        "window_start",
        "transactions",
    )

    def __init__(self, fields: list[str]) -> None:
        if len(fields) != 8:
            raise ValueError(f"StockAggregate expected 8 fields, received {len(fields)}")
        self.ticker = fields[0]
        self.volume = _parse_int(fields[1])
        self.open = _parse_float(fields[2])
        self.close = _parse_float(fields[3])
        self.high = _parse_float(fields[4])
        self.low = _parse_float(fields[5])
        self.window_start = _parse_int(fields[6])
        self.transactions = _parse_int(fields[7])

    def __iter__(self) -> Iterator[object]:
        yield self.ticker
        yield self.volume
        yield self.open
        yield self.close
        yield self.high
        yield self.low
        yield self.window_start
        yield self.transactions

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, StockAggregate):
            return NotImplemented
        return tuple(self) == tuple(other)

    def __lt__(self, other: object) -> bool:
        if not isinstance(other, StockAggregate):
            return NotImplemented
        return self.window_start < other.window_start

    def __le__(self, other: object) -> bool:
        if not isinstance(other, StockAggregate):
            return NotImplemented
        return self.window_start <= other.window_start

    def __gt__(self, other: object) -> bool:
        if not isinstance(other, StockAggregate):
            return NotImplemented
        return self.window_start > other.window_start

    def __ge__(self, other: object) -> bool:
        if not isinstance(other, StockAggregate):
            return NotImplemented
        return self.window_start >= other.window_start

    def __hash__(self) -> int:
        return hash(tuple(self))

    def __repr__(self) -> str:
        return (
            "StockAggregate("
            f"ticker={self.ticker!r}, volume={self.volume}, open={self.open}, "
            f"close={self.close}, high={self.high}, low={self.low}, "
            f"window_start={self.window_start}, transactions={self.transactions})"
        )

    __str__ = __repr__


class CurrencyQuote:
    __slots__ = (
        "ticker",
        "ask_exchange",
        "ask_price",
        "bid_exchange",
        "bid_price",
        "participant_timestamp",
    )

    def __init__(self, fields: list[str]) -> None:
        if len(fields) != 6:
            raise ValueError(f"CurrencyQuote expected 6 fields, received {len(fields)}")
        self.ticker = fields[0]
        self.ask_exchange = _parse_int(fields[1])
        self.ask_price = _parse_float(fields[2])
        self.bid_exchange = _parse_int(fields[3])
        self.bid_price = _parse_float(fields[4])
        self.participant_timestamp = _parse_int(fields[5])

    def __iter__(self) -> Iterator[object]:
        yield self.ticker
        yield self.ask_exchange
        yield self.ask_price
        yield self.bid_exchange
        yield self.bid_price
        yield self.participant_timestamp

    @property
    def tickers(self) -> tuple[str, str]:
        return _intern_tickers(self.ticker)

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, CurrencyQuote):
            return NotImplemented
        return tuple(self) == tuple(other)

    def __lt__(self, other: object) -> bool:
        if not isinstance(other, CurrencyQuote):
            return NotImplemented
        return self.participant_timestamp < other.participant_timestamp

    def __le__(self, other: object) -> bool:
        if not isinstance(other, CurrencyQuote):
            return NotImplemented
        return self.participant_timestamp <= other.participant_timestamp

    def __gt__(self, other: object) -> bool:
        if not isinstance(other, CurrencyQuote):
            return NotImplemented
        return self.participant_timestamp > other.participant_timestamp

    def __ge__(self, other: object) -> bool:
        if not isinstance(other, CurrencyQuote):
            return NotImplemented
        return self.participant_timestamp >= other.participant_timestamp

    def __hash__(self) -> int:
        return hash(tuple(self))

    def __repr__(self) -> str:
        return (
            "CurrencyQuote("
            f"ticker={self.ticker!r}, ask_exchange={self.ask_exchange}, "
            f"ask_price={self.ask_price}, bid_exchange={self.bid_exchange}, "
            f"bid_price={self.bid_price}, participant_timestamp={self.participant_timestamp})"
        )

    __str__ = __repr__


class CurrencyAggregate:
    __slots__ = (
        "ticker",
        "volume",
        "open",
        "close",
        "high",
        "low",
        "window_start",
        "transactions",
    )

    def __init__(self, fields: list[str]) -> None:
        if len(fields) != 8:
            raise ValueError(f"CurrencyAggregate expected 8 fields, received {len(fields)}")
        self.ticker = fields[0]
        self.volume = _parse_int(fields[1])
        self.open = _parse_float(fields[2])
        self.close = _parse_float(fields[3])
        self.high = _parse_float(fields[4])
        self.low = _parse_float(fields[5])
        self.window_start = _parse_int(fields[6])
        self.transactions = _parse_int(fields[7])

    def __iter__(self) -> Iterator[object]:
        yield self.ticker
        yield self.volume
        yield self.open
        yield self.close
        yield self.high
        yield self.low
        yield self.window_start
        yield self.transactions

    @property
    def tickers(self) -> tuple[str, str]:
        return _intern_tickers(self.ticker)

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, CurrencyAggregate):
            return NotImplemented
        return tuple(self) == tuple(other)

    def __lt__(self, other: object) -> bool:
        if not isinstance(other, CurrencyAggregate):
            return NotImplemented
        return self.window_start < other.window_start

    def __le__(self, other: object) -> bool:
        if not isinstance(other, CurrencyAggregate):
            return NotImplemented
        return self.window_start <= other.window_start

    def __gt__(self, other: object) -> bool:
        if not isinstance(other, CurrencyAggregate):
            return NotImplemented
        return self.window_start > other.window_start

    def __ge__(self, other: object) -> bool:
        if not isinstance(other, CurrencyAggregate):
            return NotImplemented
        return self.window_start >= other.window_start

    def __hash__(self) -> int:
        return hash(tuple(self))

    def __repr__(self) -> str:
        return (
            "CurrencyAggregate("
            f"ticker={self.ticker!r}, volume={self.volume}, open={self.open}, "
            f"close={self.close}, high={self.high}, low={self.low}, "
            f"window_start={self.window_start}, transactions={self.transactions})"
        )

    __str__ = __repr__


class Parser:
    parser_group_name = ""
    asset_class_name = ""

    def serialize(self) -> str:
        return (
            f"parser_group={self.parser_group_name};asset_class={self.asset_class_name};"
            f"processor={self.processor_name()}"
        )

    def processor_name(self) -> str:
        return "generic"

    def split_on_commas(self, payload: str, output: list[str]) -> None:
        output.extend(payload.split(","))

    def build_summary(self, payload: bytes | str, operation: str, fmt: str) -> dict[str, object]:
        text = _to_text(payload)
        segments: list[str] = []
        self.split_on_commas(text, segments)
        return {
            "parser_group": self.parser_group_name,
            "asset_class": self.asset_class_name,
            "processor": self.processor_name(),
            "operation": operation,
            "format": fmt,
            "bytes": len(text.encode("utf-8")),
            "commas": text.count(","),
            "newlines": text.count("\n"),
            "json_objects": text.count("{"),
            "segments": len(segments),
            "event_markers": text.count('"ev"'),
        }


class FlatFileParser(Parser):
    parser_group_name = "flatfiles"

    @classmethod
    def parse_quotes(cls, payload: bytes | str) -> dict[str, object]:
        raise NotImplementedError("flatfile quote parsing must be implemented by a concrete parser")

    @classmethod
    def parse_minute_aggregates(cls, payload: bytes | str) -> dict[str, object]:
        raise NotImplementedError(
            "flatfile minute aggregate parsing must be implemented by a concrete parser"
        )

    @classmethod
    def parse_daily_aggregates(cls, payload: bytes | str) -> dict[str, object]:
        raise NotImplementedError(
            "flatfile daily aggregate parsing must be implemented by a concrete parser"
        )

    @classmethod
    def parse_trades(cls, payload: bytes | str) -> dict[str, object]:
        raise NotImplementedError("flatfile trade parsing must be implemented by a concrete parser")


class WebSocketParser(Parser):
    parser_group_name = "websocket"

    @classmethod
    def parse_message(cls, payload: bytes | str) -> dict[str, object]:
        text = _to_text(payload)
        summary = cls().build_summary(text, "parse_message", "json")
        summary["message_frames"] = text.count("},{") + text.count("}{") + (0 if not text else 1)
        return summary


class FlatFileStocksParser(FlatFileParser):
    asset_class_name = "stocks"

    @staticmethod
    def _iter_raw_rows(
        payload: str | Path,
        *,
        participant_timestamp_index: int,
        sip_timestamp_index: int,
        sort_by_participant_timestamp: bool = False,
        sort_by_sip_timestamp: bool = False,
    ) -> Iterator[tuple[bytes, ...]]:
        if sort_by_participant_timestamp and sort_by_sip_timestamp:
            raise ValueError(
                "sort_by_participant_timestamp and sort_by_sip_timestamp cannot both be true"
            )

        with gzip.open(payload, "rt", encoding="utf-8", newline="") as handle:
            reader = csv.reader(handle)
            next(reader, None)

            if not sort_by_participant_timestamp and not sort_by_sip_timestamp:
                for fields in reader:
                    if any(fields):
                        yield tuple(field.encode("utf-8") for field in fields)
                return

            if sort_by_participant_timestamp:
                rows = [tuple(fields) for fields in reader if any(fields)]
                rows.sort(
                    key=lambda row: (
                        _parse_int(row[participant_timestamp_index]),
                        row[0],
                        _parse_int(row[sip_timestamp_index]),
                    )
                )
                for row in rows:
                    yield tuple(field.encode("utf-8") for field in row)
                return

            rows = [tuple(fields) for fields in reader if any(fields)]
            rows.sort(
                key=lambda row: (
                    _parse_int(row[sip_timestamp_index]),
                    row[0],
                    _parse_int(row[participant_timestamp_index]),
                )
            )
            for row in rows:
                yield tuple(field.encode("utf-8") for field in row)

    @staticmethod
    def _iter_rows(
        payload: str | Path,
        row_type,
        sort_by_participant_timestamp: bool = False,
        sort_by_sip_timestamp: bool = False,
    ):
        if sort_by_participant_timestamp and sort_by_sip_timestamp:
            raise ValueError(
                "sort_by_participant_timestamp and sort_by_sip_timestamp cannot both be true"
            )

        with gzip.open(payload, "rt", encoding="utf-8", newline="") as handle:
            reader = csv.reader(handle)
            next(reader, None)

            if not sort_by_participant_timestamp and not sort_by_sip_timestamp:
                for fields in reader:
                    if any(fields):
                        yield row_type(fields)
                return

            if sort_by_participant_timestamp:
                rows = [row_type(fields) for fields in reader if any(fields)]
                rows.sort(key=lambda row: (row.participant_timestamp, row.ticker, row.sip_timestamp))
                yield from rows
                return

            rows = [row_type(fields) for fields in reader if any(fields)]
            rows.sort(key=lambda row: (row.sip_timestamp, row.ticker, row.participant_timestamp))
            yield from rows

    @staticmethod
    def _iter_aggregate_rows(
        payload: str | Path,
        row_type,
        *,
        sort_by_window_start: bool = False,
    ):
        with gzip.open(payload, "rt", encoding="utf-8", newline="") as handle:
            reader = csv.reader(handle)
            next(reader, None)
            rows = [row_type(fields) for fields in reader if any(fields)]

        if sort_by_window_start:
            rows.sort(key=lambda row: (row.window_start, row.ticker))

        yield from rows

    @staticmethod
    def _iter_raw_aggregate_rows(
        payload: str | Path,
        *,
        window_start_index: int,
        sort_by_window_start: bool = False,
    ) -> Iterator[tuple[bytes, ...]]:
        with gzip.open(payload, "rt", encoding="utf-8", newline="") as handle:
            reader = csv.reader(handle)
            next(reader, None)
            rows = [tuple(fields) for fields in reader if any(fields)]

        if sort_by_window_start:
            rows.sort(key=lambda row: (_parse_int(row[window_start_index]), row[0]))

        for row in rows:
            yield tuple(field.encode("utf-8") for field in row)

    @classmethod
    def parse_quotes(
        cls,
        payload: str | Path,
        *,
        sort_by_participant_timestamp: bool = False,
        sort_by_sip_timestamp: bool = False,
    ) -> Iterator[StockQuote]:
        yield from cls._iter_rows(
            payload,
            StockQuote,
            sort_by_participant_timestamp=sort_by_participant_timestamp,
            sort_by_sip_timestamp=sort_by_sip_timestamp,
        )

    @classmethod
    def parse_raw_quotes(
        cls,
        payload: str | Path,
        *,
        sort_by_participant_timestamp: bool = False,
        sort_by_sip_timestamp: bool = False,
    ) -> Iterator[tuple[bytes, ...]]:
        yield from cls._iter_raw_rows(
            payload,
            participant_timestamp_index=9,
            sip_timestamp_index=11,
            sort_by_participant_timestamp=sort_by_participant_timestamp,
            sort_by_sip_timestamp=sort_by_sip_timestamp,
        )

    @classmethod
    def parse_minute_aggregates(
        cls,
        payload: str | Path,
        *,
        sort_by_window_start: bool = False,
    ) -> Iterator[StockAggregate]:
        yield from cls._iter_aggregate_rows(
            payload,
            StockAggregate,
            sort_by_window_start=sort_by_window_start,
        )

    @classmethod
    def parse_daily_aggregates(
        cls,
        payload: str | Path,
        *,
        sort_by_window_start: bool = False,
    ) -> Iterator[StockAggregate]:
        yield from cls.parse_minute_aggregates(
            payload,
            sort_by_window_start=sort_by_window_start,
        )

    @classmethod
    def parse_raw_minute_aggregates(
        cls,
        payload: str | Path,
        *,
        sort_by_window_start: bool = False,
    ) -> Iterator[tuple[bytes, ...]]:
        yield from cls._iter_raw_aggregate_rows(
            payload,
            window_start_index=6,
            sort_by_window_start=sort_by_window_start,
        )

    @classmethod
    def parse_raw_daily_aggregates(
        cls,
        payload: str | Path,
        *,
        sort_by_window_start: bool = False,
    ) -> Iterator[tuple[bytes, ...]]:
        yield from cls.parse_raw_minute_aggregates(
            payload,
            sort_by_window_start=sort_by_window_start,
        )

    @classmethod
    def parse_trades(
        cls,
        payload: str | Path,
        *,
        sort_by_participant_timestamp: bool = False,
        sort_by_sip_timestamp: bool = False,
    ) -> Iterator[StockTrade]:
        yield from cls._iter_rows(
            payload,
            StockTrade,
            sort_by_participant_timestamp=sort_by_participant_timestamp,
            sort_by_sip_timestamp=sort_by_sip_timestamp,
        )

    @classmethod
    def parse_raw_trades(
        cls,
        payload: str | Path,
        *,
        sort_by_participant_timestamp: bool = False,
        sort_by_sip_timestamp: bool = False,
    ) -> Iterator[tuple[bytes, ...]]:
        yield from cls._iter_raw_rows(
            payload,
            participant_timestamp_index=5,
            sip_timestamp_index=8,
            sort_by_participant_timestamp=sort_by_participant_timestamp,
            sort_by_sip_timestamp=sort_by_sip_timestamp,
        )

    @classmethod
    def raw_lines(cls, payload: str | Path) -> Iterator[bytes]:
        with gzip.open(payload, "rb") as handle:
            next(handle, None)
            for line in handle:
                line = line.rstrip(b"\r\n")
                if line:
                    yield line


class FlatFileOptionsParser(FlatFileParser):
    asset_class_name = "options"


class FlatFileFuturesParser(FlatFileParser):
    asset_class_name = "futures"


class FlatFileIndicesParser(FlatFileParser):
    asset_class_name = "indices"


class FlatFileForexParser(FlatFileParser):
    asset_class_name = "forex"


class FlatFileCurrenciesParser(FlatFileParser):
    asset_class_name = "currencies"

    @staticmethod
    def _iter_rows(
        payload: str | Path,
        row_type,
        *,
        sort_by_window_start: bool = False,
    ):
        with gzip.open(payload, "rt", encoding="utf-8", newline="") as handle:
            reader = csv.reader(handle)
            next(reader, None)
            rows = [row_type(fields) for fields in reader if any(fields)]

        if sort_by_window_start:
            rows.sort(key=lambda row: (row.window_start, row.ticker))

        yield from rows

    @staticmethod
    def _iter_raw_rows(
        payload: str | Path,
        *,
        window_start_index: int,
        sort_by_window_start: bool = False,
    ) -> Iterator[tuple[bytes, ...]]:
        with gzip.open(payload, "rt", encoding="utf-8", newline="") as handle:
            reader = csv.reader(handle)
            next(reader, None)
            rows = [tuple(fields) for fields in reader if any(fields)]

        if sort_by_window_start:
            rows.sort(key=lambda row: (_parse_int(row[window_start_index]), row[0]))

        for row in rows:
            yield tuple(field.encode("utf-8") for field in row)

    @classmethod
    def parse_quotes(
        cls,
        payload: str | Path,
        *,
        sort_by_participant_timestamp: bool = False,
        sort_by_sip_timestamp: bool = False,
    ) -> Iterator[CurrencyQuote]:
        if sort_by_sip_timestamp:
            raise ValueError("currency quotes do not support sort_by_sip_timestamp")

        with gzip.open(payload, "rt", encoding="utf-8", newline="") as handle:
            reader = csv.reader(handle)
            next(reader, None)
            rows = [CurrencyQuote(fields) for fields in reader if any(fields)]

        if sort_by_participant_timestamp:
            rows.sort(key=lambda row: (row.participant_timestamp, row.ticker))

        yield from rows

    @classmethod
    def parse_raw_quotes(
        cls,
        payload: str | Path,
        *,
        sort_by_participant_timestamp: bool = False,
        sort_by_sip_timestamp: bool = False,
    ) -> Iterator[tuple[bytes, ...]]:
        if sort_by_sip_timestamp:
            raise ValueError("currency quotes do not support sort_by_sip_timestamp")

        yield from cls._iter_raw_rows(
            payload,
            window_start_index=5,
            sort_by_window_start=sort_by_participant_timestamp,
        )

    @classmethod
    def parse_minute_aggregates(
        cls,
        payload: str | Path,
        *,
        sort_by_window_start: bool = False,
    ) -> Iterator[CurrencyAggregate]:
        yield from cls._iter_rows(
            payload,
            CurrencyAggregate,
            sort_by_window_start=sort_by_window_start,
        )

    @classmethod
    def parse_daily_aggregates(
        cls,
        payload: str | Path,
        *,
        sort_by_window_start: bool = False,
    ) -> Iterator[CurrencyAggregate]:
        yield from cls.parse_minute_aggregates(
            payload,
            sort_by_window_start=sort_by_window_start,
        )

    @classmethod
    def parse_raw_minute_aggregates(
        cls,
        payload: str | Path,
        *,
        sort_by_window_start: bool = False,
    ) -> Iterator[tuple[bytes, ...]]:
        yield from cls._iter_raw_rows(
            payload,
            window_start_index=6,
            sort_by_window_start=sort_by_window_start,
        )

    @classmethod
    def parse_raw_daily_aggregates(
        cls,
        payload: str | Path,
        *,
        sort_by_window_start: bool = False,
    ) -> Iterator[tuple[bytes, ...]]:
        yield from cls.parse_raw_minute_aggregates(
            payload,
            sort_by_window_start=sort_by_window_start,
        )

    @classmethod
    def raw_lines(cls, payload: str | Path) -> Iterator[bytes]:
        with gzip.open(payload, "rb") as handle:
            next(handle, None)
            for line in handle:
                line = line.rstrip(b"\r\n")
                if line:
                    yield line


class FlatFileCryptoParser(FlatFileParser):
    asset_class_name = "crypto"


class WebSocketMessagesParser(WebSocketParser):
    asset_class_name = "messages"


class WebSocketStocksParser(WebSocketParser):
    asset_class_name = "stocks"


class WebSocketOptionsParser(WebSocketParser):
    asset_class_name = "options"


class WebSocketFuturesParser(WebSocketParser):
    asset_class_name = "futures"


class WebSocketIndicesParser(WebSocketParser):
    asset_class_name = "indices"


class WebSocketForexParser(WebSocketParser):
    asset_class_name = "forex"


class WebSocketCryptoParser(WebSocketParser):
    asset_class_name = "crypto"


FlatFiles = SimpleNamespace(
    Stock=FlatFileStocksParser,
    Options=FlatFileOptionsParser,
    Futures=FlatFileFuturesParser,
    Indices=FlatFileIndicesParser,
    Forex=FlatFileForexParser,
    currency=FlatFileCurrenciesParser,
    Crypto=FlatFileCryptoParser,
)

WebSocket = SimpleNamespace(
    Messages=WebSocketMessagesParser,
    Stocks=WebSocketStocksParser,
    Options=WebSocketOptionsParser,
    Futures=WebSocketFuturesParser,
    Indices=WebSocketIndicesParser,
    Forex=WebSocketForexParser,
    Crypto=WebSocketCryptoParser,
)

FlatFiles.StockTrade = StockTrade
FlatFiles.StockQuote = StockQuote
FlatFiles.StockAggregate = StockAggregate
FlatFiles.StockQuotes = StockQuote
FlatFiles.CurrencyQuote = CurrencyQuote
FlatFiles.CurrencyAggregate = CurrencyAggregate

class StockTradeAggregation:
    __slots__ = ()


class StockQuoteAggregation:
    __slots__ = ()


class CurrencyQuoteAggregation:
    __slots__ = ()


class _NativeOnlyAggregator:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("aggregation requires the native massive_speedup extension")


class StockTradeAggregator(_NativeOnlyAggregator):
    pass


class StockQuoteAggregator(_NativeOnlyAggregator):
    pass


class CurrencyQuoteAggregator(_NativeOnlyAggregator):
    pass

FlatFiles.Stock.Trade = SimpleNamespace(
    parse=FlatFileStocksParser.parse_trades,
    parse_raw=FlatFileStocksParser.parse_raw_trades,
    raw_lines=FlatFileStocksParser.raw_lines,
    Aggregator=StockTradeAggregator,
)
FlatFiles.Stock.Quote = SimpleNamespace(
    parse=FlatFileStocksParser.parse_quotes,
    parse_raw=FlatFileStocksParser.parse_raw_quotes,
    raw_lines=FlatFileStocksParser.raw_lines,
    Aggregator=StockQuoteAggregator,
)
FlatFiles.Stock.Aggregate = SimpleNamespace(
    parse=FlatFileStocksParser.parse_minute_aggregates,
    parse_raw=FlatFileStocksParser.parse_raw_minute_aggregates,
    raw_lines=FlatFileStocksParser.raw_lines,
)

FlatFiles.currency.Quote = SimpleNamespace(
    parse=FlatFileCurrenciesParser.parse_quotes,
    parse_raw=FlatFileCurrenciesParser.parse_raw_quotes,
    raw_lines=FlatFileCurrenciesParser.raw_lines,
    Aggregator=CurrencyQuoteAggregator,
)
FlatFiles.currency.Aggregate = SimpleNamespace(
    parse=FlatFileCurrenciesParser.parse_minute_aggregates,
    parse_raw=FlatFileCurrenciesParser.parse_raw_minute_aggregates,
    raw_lines=FlatFileCurrenciesParser.raw_lines,
)


def build_database_file(*args, **kwargs):
    raise RuntimeError("build_database_file requires the native massive_speedup extension")


class StockTradeDatabase:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("StockTradeDatabase requires the native massive_speedup extension")


class StockQuoteDatabase:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("StockQuoteDatabase requires the native massive_speedup extension")


class CurrencyQuoteDatabase:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("CurrencyQuoteDatabase requires the native massive_speedup extension")
