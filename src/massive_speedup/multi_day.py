"""Row-at-a-time access across date-partitioned databases."""

from __future__ import annotations

import datetime as dt
import math
from bisect import bisect_left, bisect_right
from collections import OrderedDict
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterator

from ._paths import database_path as configured_database_path


_SIMPLE_DATABASE_TYPES = {
    "stock_trade": "StockTradeDatabase",
    "stock_quote": "StockQuoteDatabase",
    "crypto_trade": "CryptoTradeDatabase",
    "currency_quote": "CurrencyQuoteDatabase",
    "index_value": "IndexValueDatabase",
}
_OPTION_RECORD_TYPES = {
    "option_trade": "OptionTradeDatabase",
    "option_quote": "OptionQuoteDatabase",
}
_FUTURES_KINDS = {"trade": "FuturesTradeDatabase", "quote": "FuturesQuoteDatabase"}


@dataclass(frozen=True, slots=True)
class DatabaseLocation:
    """A record and its local position in one daily database."""

    date: str
    index: int
    record: Any


def _date_text(value: str | dt.date | None, name: str) -> str | None:
    if value is None:
        return None
    if isinstance(value, dt.datetime):
        value = value.date()
    if isinstance(value, dt.date):
        return value.isoformat()
    if isinstance(value, str):
        try:
            return dt.date.fromisoformat(value).isoformat()
        except ValueError as error:
            raise ValueError(f"{name} must be a valid YYYY-MM-DD date") from error
    raise TypeError(f"{name} must be a YYYY-MM-DD string or datetime.date")


def _timestamp_ns(value: int | float | dt.date | dt.datetime) -> int:
    if isinstance(value, bool):
        raise TypeError("timestamp must not be bool")
    if isinstance(value, int):
        if value < 0:
            raise ValueError("timestamp must be non-negative")
        return value
    if isinstance(value, float):
        if not math.isfinite(value) or value < 0:
            raise ValueError("timestamp must be finite and non-negative")
        return int(value)
    if isinstance(value, dt.datetime):
        if value.tzinfo is None:
            value = value.replace(tzinfo=dt.UTC)
        epoch = dt.datetime(1970, 1, 1, tzinfo=dt.UTC)
        delta = value.astimezone(dt.UTC) - epoch
    elif isinstance(value, dt.date):
        epoch_date = dt.date(1970, 1, 1)
        return (value - epoch_date).days * 86_400_000_000_000
    else:
        raise TypeError(
            "timestamp must be integer/float nanoseconds, datetime.date, or datetime.datetime"
        )
    return (
        delta.days * 86_400_000_000_000
        + delta.seconds * 1_000_000_000
        + delta.microseconds * 1_000
    )


def _utc_date_from_ns(timestamp_ns: int) -> str:
    day = timestamp_ns // 86_400_000_000_000
    try:
        return (dt.date(1970, 1, 1) + dt.timedelta(days=day)).isoformat()
    except OverflowError as error:
        raise ValueError("timestamp is outside datetime.date's supported range") from error


def _validate_key(key: str) -> str:
    if not key:
        raise ValueError("key must not be empty")
    path = Path(key)
    if path.is_absolute() or any(part in {"", ".", ".."} for part in path.parts):
        raise ValueError("key must be a relative database record key")
    return path.as_posix()


def _futures_record_type(record_type: str) -> tuple[str, str]:
    parts = record_type.split("_")
    if len(parts) == 2 and parts[0] == "future" and parts[1] in _FUTURES_KINDS:
        return parts[1], ""
    if (
        len(parts) == 3
        and parts[0] == "future"
        and parts[1] in {"cbot", "cme", "comex", "nymex"}
        and parts[2] in _FUTURES_KINDS
    ):
        return parts[2], parts[1]
    raise ValueError(f"unsupported database record type: {record_type}")


class MultiDayDatabase:
    """Coordinate a bounded set of daily mmap databases as one row sequence.

    ``key`` is the filename below each date directory. Option contracts use the
    database key form ``ROOT/YYYY-MM-DD/C|P/SSSSSSSS``, where the final component
    is strike price in thousandths.
    """

    def __init__(
        self,
        record_type: str,
        key: str,
        *,
        database_path: str | Path | None = None,
        start_date: str | dt.date | None = None,
        end_date: str | dt.date | None = None,
        max_open_days: int = 1,
    ) -> None:
        if isinstance(max_open_days, bool) or not isinstance(max_open_days, int):
            raise TypeError("max_open_days must be an integer")
        if max_open_days < 1:
            raise ValueError("max_open_days must be at least 1")

        self.database_path = configured_database_path(database_path)
        self.record_type = record_type
        self.key = _validate_key(key)
        self.max_open_days = max_open_days
        self.start_date = _date_text(start_date, "start_date")
        self.end_date = _date_text(end_date, "end_date")
        if self.start_date and self.end_date and self.start_date > self.end_date:
            raise ValueError("start_date must not be after end_date")

        self._validate_record_type()
        self.dates = self._discover_dates()
        self._open: OrderedDict[str, Any] = OrderedDict()

    @property
    def open_dates(self) -> tuple[str, ...]:
        return tuple(self._open)

    def _validate_record_type(self) -> None:
        if self.record_type in _SIMPLE_DATABASE_TYPES:
            return
        if self.record_type in _OPTION_RECORD_TYPES:
            self._option_parts()
            return
        _futures_record_type(self.record_type)

    def _option_parts(self) -> tuple[str, str, str, float]:
        parts = self.key.split("/")
        if len(parts) != 4:
            raise ValueError(
                "option keys must use ROOT/YYYY-MM-DD/C|P/SSSSSSSS format"
            )
        root, expiration, right, strike_text = parts
        _date_text(expiration, "option expiration")
        if right not in {"C", "P"}:
            raise ValueError("option key right must be C or P")
        if len(strike_text) != 8 or not strike_text.isdigit():
            raise ValueError("option key strike must contain exactly 8 digits")
        return root, expiration, right, int(strike_text) / 1000.0

    def _discover_dates(self) -> tuple[str, ...]:
        root = self.database_path / self.record_type
        if not root.is_dir():
            return ()
        dates: list[str] = []
        for candidate in root.iterdir():
            if not candidate.is_dir():
                continue
            try:
                date = dt.date.fromisoformat(candidate.name)
            except ValueError:
                continue
            date_text = date.isoformat()
            if self.start_date and date_text < self.start_date:
                continue
            if self.end_date and date_text > self.end_date:
                continue
            if (candidate / self.key).is_file():
                dates.append(date_text)
        dates.sort()
        return tuple(dates)

    def _make_database(self, date: str):
        import massive_speedup

        if self.record_type in _SIMPLE_DATABASE_TYPES:
            database_type = getattr(
                massive_speedup,
                _SIMPLE_DATABASE_TYPES[self.record_type],
            )
            return database_type(date, self.key, database_path=self.database_path)
        if self.record_type in _OPTION_RECORD_TYPES:
            database_type = getattr(
                massive_speedup,
                _OPTION_RECORD_TYPES[self.record_type],
            )
            root, expiration, right, strike = self._option_parts()
            return database_type(
                date,
                root,
                expiration,
                right,
                strike,
                database_path=self.database_path,
            )

        kind, exchange = _futures_record_type(self.record_type)
        database_type = getattr(massive_speedup, _FUTURES_KINDS[kind])
        return database_type(
            date,
            self.key,
            exchange=exchange,
            database_path=self.database_path,
        )

    def database_for_date(self, date: str | dt.date):
        date_text = _date_text(date, "date")
        assert date_text is not None
        if date_text not in self.dates:
            raise FileNotFoundError(
                f"no {self.record_type} database for {self.key!r} on {date_text}"
            )
        cached = self._open.pop(date_text, None)
        if cached is None:
            cached = self._make_database(date_text)
        self._open[date_text] = cached
        while len(self._open) > self.max_open_days:
            self._open.popitem(last=False)
        return cached

    def close(self) -> None:
        self._open.clear()

    def __enter__(self) -> MultiDayDatabase:
        return self

    def __exit__(self, *exc_info: object) -> None:
        self.close()

    def __iter__(self) -> Iterator[Any]:
        for date in self.dates:
            yield from self.database_for_date(date)

    def __len__(self) -> int:
        return sum(len(self.database_for_date(date)) for date in self.dates)

    def __getitem__(self, index: int):
        if isinstance(index, bool) or not isinstance(index, int):
            raise TypeError("multi-day database indices must be integers")
        if index < 0:
            index += len(self)
        if index < 0:
            raise IndexError("multi-day database index out of range")
        for date in self.dates:
            database = self.database_for_date(date)
            if index < len(database):
                return database[index]
            index -= len(database)
        raise IndexError("multi-day database index out of range")

    def iterate_bounded(
        self,
        start_timestamp: int | float | dt.date | dt.datetime,
        stop_timestamp: int | float | dt.date | dt.datetime | None = None,
    ) -> Iterator[Any]:
        start_ns = _timestamp_ns(start_timestamp)
        stop_ns = None if stop_timestamp is None else _timestamp_ns(stop_timestamp)
        if stop_ns is not None and start_ns > stop_ns:
            raise ValueError("start_timestamp must not be after stop_timestamp")
        for date in self.dates:
            database = self.database_for_date(date)
            if stop_ns is None:
                yield from database.iterate_bounded(start_ns)
            else:
                yield from database.iterate_bounded(start_ns, stop_ns)

    def locate_before_timestamp(
        self,
        timestamp: int | float | dt.date | dt.datetime,
        *,
        on: bool = True,
        galloping: DatabaseLocation | None = None,
    ) -> DatabaseLocation:
        timestamp_ns = _timestamp_ns(timestamp)
        if not on:
            if timestamp_ns == 0:
                raise IndexError("no record before timestamp")
            timestamp_ns -= 1
        target_date = _utc_date_from_ns(timestamp_ns)
        start = min(bisect_right(self.dates, target_date), len(self.dates) - 1)
        for position in range(start, -1, -1):
            date = self.dates[position]
            database = self.database_for_date(date)
            hint = galloping.index if galloping and galloping.date == date else None
            index = database.index_before_timestamp(timestamp_ns, galloping=hint)
            if index >= 0:
                return DatabaseLocation(date, index, database[index])
        raise IndexError("no record before timestamp")

    def locate_after_timestamp(
        self,
        timestamp: int | float | dt.date | dt.datetime,
        *,
        on: bool = True,
        galloping: DatabaseLocation | None = None,
    ) -> DatabaseLocation:
        timestamp_ns = _timestamp_ns(timestamp)
        if not on:
            if timestamp_ns == (1 << 64) - 1:
                raise IndexError("no record after timestamp")
            timestamp_ns += 1
        target_date = _utc_date_from_ns(timestamp_ns)
        start = max(0, bisect_left(self.dates, target_date) - 1)
        for position in range(start, len(self.dates)):
            date = self.dates[position]
            database = self.database_for_date(date)
            hint = galloping.index if galloping and galloping.date == date else None
            index = database.index_after_timestamp(timestamp_ns, galloping=hint)
            if index >= 0:
                return DatabaseLocation(date, index, database[index])
        raise IndexError("no record after timestamp")

    def find_before_timestamp(
        self,
        timestamp: int | float | dt.date | dt.datetime,
        *,
        on: bool = True,
        galloping: DatabaseLocation | None = None,
    ):
        return self.locate_before_timestamp(
            timestamp,
            on=on,
            galloping=galloping,
        ).record

    def find_after_timestamp(
        self,
        timestamp: int | float | dt.date | dt.datetime,
        *,
        on: bool = True,
        galloping: DatabaseLocation | None = None,
    ):
        return self.locate_after_timestamp(
            timestamp,
            on=on,
            galloping=galloping,
        ).record
