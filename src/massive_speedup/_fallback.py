"""Pure-Python fallback implementations for source-tree testing."""

from __future__ import annotations

import csv
import datetime as dt
import gzip
import math
import sys
from pathlib import Path
from types import SimpleNamespace
from typing import Iterator

from ._condition_enums import make_condition_enum


StockTradeCondition = make_condition_enum(
    "StockTradeCondition",
    {
        "ACQUISITION": 1,
        "AVERAGE_PRICE_TRADE": 2,
        "AUTOMATIC_EXECUTION": 3,
        "BUNCHED_TRADE": 4,
        "BUNCHED_SOLD_TRADE": 5,
        "CAP_ELECTION": 6,
        "CASH_SALE": 7,
        "CLOSING_PRINTS": 8,
        "CROSS_TRADE": 9,
        "DERIVATIVELY_PRICED": 10,
        "DISTRIBUTION": 11,
        "FORM_T_EXTENDED_HOURS": 12,
        "EXTENDED_HOURS_SOLD_OUT_OF_SEQUENCE": 13,
        "INTERMARKET_SWEEP": 14,
        "MARKET_CENTER_OFFICIAL_CLOSE": 15,
        "MARKET_CENTER_OFFICIAL_OPEN": 16,
        "MARKET_CENTER_OPENING_TRADE": 17,
        "MARKET_CENTER_REOPENING_TRADE": 18,
        "MARKET_CENTER_CLOSING_TRADE": 19,
        "NEXT_DAY": 20,
        "PRICE_VARIATION_TRADE": 21,
        "PRIOR_REFERENCE_PRICE": 22,
        "RULE_155_TRADE_AMEX": 23,
        "RULE_127_NYSE_ONLY": 24,
        "OPENING_PRINTS": 25,
        "STOPPED_STOCK_REGULAR_TRADE": 27,
        "RE_OPENING_PRINTS": 28,
        "SELLER": 29,
        "SOLD_LAST": 30,
        "SOLD_LAST_AND_STOPPED_STOCK": 31,
        "SOLD_OUT_OF_SEQUENCE": 32,
        "SOLD_OUT_OF_SEQUENCE_AND_STOPPED_STOCK": 33,
        "SPLIT_TRADE": 34,
        "STOCK_OPTION": 35,
        "YELLOW_FLAG_REGULAR_TRADE": 36,
        "ODD_LOT_TRADE": 37,
        "CORRECTED_CONSOLIDATED_CLOSE_PER_LISTING_MARKET": 38,
        "TRADE_THRU_EXEMPT": 41,
        "CONTINGENT_TRADE": 52,
        "QUALIFIED_CONTINGENT_TRADE": 53,
        "OPENING_REOPENING_TRADE_DETAIL": 55,
        "SHORT_SALE_RESTRICTION_ACTIVATED": 57,
        "SHORT_SALE_RESTRICTION_CONTINUED": 58,
        "SHORT_SALE_RESTRICTION_DEACTIVATED": 59,
        "SHORT_SALE_RESTRICTION_IN_EFFECT": 60,
        "FINANCIAL_STATUS_BANKRUPT": 62,
        "FINANCIAL_STATUS_DEFICIENT": 63,
        "FINANCIAL_STATUS_DELINQUENT": 64,
        "FINANCIAL_STATUS_BANKRUPT_AND_DEFICIENT": 65,
        "FINANCIAL_STATUS_BANKRUPT_AND_DELINQUENT": 66,
        "FINANCIAL_STATUS_DEFICIENT_AND_DELINQUENT": 67,
        "FINANCIAL_STATUS_DEFICIENT_DELINQUENT_AND_BANKRUPT": 68,
        "FINANCIAL_STATUS_LIQUIDATION": 69,
        "FINANCIAL_STATUS_CREATIONS_SUSPENDED": 70,
        "FINANCIAL_STATUS_REDEMPTIONS_SUSPENDED": 71,
    },
)

StockQuoteCondition = make_condition_enum(
    "StockQuoteCondition",
    {
        "REGULAR_TWO_SIDED_OPEN": 1,
        "REGULAR_ONE_SIDED_OPEN": 2,
        "SLOW_ASK": 3,
        "SLOW_BID": 4,
        "SLOW_BID_AND_ASK": 5,
        "SLOW_DUE_LRP_BID": 6,
        "SLOW_DUE_LRP_ASK": 7,
        "SLOW_DUE_SET_SLOW_LIST_BID_ASK": 9,
        "MANUAL_ASK_AUTOMATED_BID": 10,
        "MANUAL_BID_AUTOMATED_ASK": 11,
        "MANUAL_BID_AND_ASK": 12,
        "OPENING": 13,
        "CLOSING": 14,
        "CLOSED": 15,
        "RESUME": 16,
        "FAST_TRADING": 17,
        "TRADING_RANGE_INDICATION": 18,
        "MARKET_MAKER_QUOTES_CLOSED": 19,
        "NON_FIRM": 20,
        "NEWS_DISSEMINATION": 21,
        "ORDER_INFLUX": 22,
        "ORDER_IMBALANCE": 23,
        "ADDITIONAL_INFORMATION": 26,
        "NEWS_PENDING": 27,
        "ADDITIONAL_INFORMATION_DUE_TO_RELATED_SECURITY": 28,
        "DUE_TO_RELATED_SECURITY": 29,
        "IN_VIEW_OF_COMMON": 30,
        "NO_OPEN_NO_RESUME": 32,
        "ON_DEMAND_AUCTION": 40,
        "CASH_ONLY_SETTLEMENT": 41,
        "NEXT_DAY_SETTLEMENT": 42,
        "LULD_TRADING_PAUSE": 43,
        "SLOW_DUE_LRP_BID_AND_ASK": 71,
        "CORRECTED_PRICE_INDICATION": 81,
        "SIP_GENERATED": 82,
        "CROSSED_MARKET": 84,
        "LOCKED_MARKET": 85,
        "CQS_GENERATED": 94,
    },
)


def read_gzip_lines(path: str | Path):
    with gzip.open(path, "rb") as handle:
        for line in handle:
            yield line.rstrip(b"\r\n")


def gzip_lines(path: str | Path, parallelization: int = 0, chunk_size: int = 1 << 20):
    del parallelization, chunk_size
    yield from read_gzip_lines(path)


def _parse_int(text: str) -> int:
    return int(text) if text else 0


def _parse_float(text: str) -> float:
    return float(text) if text else 0.0


def _parse_nullable_float(text: str) -> float:
    return float(text) if text else float("nan")


def _parse_decimal_quantity(text: str) -> tuple[str, int, int]:
    if not text:
        return "0", 0, 0
    if text.startswith("-"):
        raise ValueError(f"decimal quantity must be non-negative: {text}")
    if text.startswith("+"):
        text = text[1:]
    if not text or text.count(".") > 1:
        raise ValueError(f"invalid decimal quantity: {text}")

    whole, point, fractional = text.partition(".")
    if not whole and not fractional:
        raise ValueError(f"invalid decimal quantity: {text}")
    if (whole and not whole.isdigit()) or (fractional and not fractional.isdigit()):
        raise ValueError(f"invalid decimal quantity: {text}")

    scale = len(fractional) if point else 0
    coefficient = int((whole or "0") + fractional)
    while scale and coefficient % 10 == 0:
        coefficient //= 10
        scale -= 1
    if coefficient == 0:
        scale = 0
    if coefficient > (1 << 64) - 1:
        raise OverflowError(f"decimal quantity coefficient out of range: {text}")
    if scale > 255:
        raise OverflowError(f"decimal quantity scale out of range: {text}")

    digits = str(coefficient)
    if not scale:
        canonical = digits
    elif len(digits) <= scale:
        canonical = "0." + "0" * (scale - len(digits)) + digits
    else:
        canonical = digits[:-scale] + "." + digits[-scale:]
    return canonical, coefficient, scale


def _parse_bitset96(text: str) -> str:
    if not text:
        return "0" * 96
    if text.startswith(("0x", "0X")):
        return f"{int(text, 16):096b}"[-96:]
    if all(ch in "01" for ch in text) and len(text) <= 96:
        return text.zfill(96)
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


_BITSET_INDICES_INTERN_CACHE: dict[tuple[object, str], frozenset[int]] = {}


def _bitset_indices(text: str, enum_type: type[int] | None = None) -> frozenset[int]:
    bit_string = _parse_bitset96(text)
    cache_key = (enum_type, bit_string)
    cached = _BITSET_INDICES_INTERN_CACHE.get(cache_key)
    if cached is not None:
        return cached

    values = []
    for index, value in enumerate(reversed(bit_string)):
        if value != "1":
            continue
        if enum_type is None:
            values.append(index)
            continue
        try:
            values.append(enum_type(index))
        except ValueError:
            values.append(index)
    cached = frozenset(values)
    _BITSET_INDICES_INTERN_CACHE[cache_key] = cached
    return cached


def _conditions_clear(conditions: frozenset[int], excluded: frozenset[int]) -> bool:
    return not any(int(condition) in excluded for condition in conditions)


_STOCK_TRADE_HIGH_LOW_EXCLUDED = frozenset({2, 7, 12, 13, 15, 16, 20, 22, 38})
_STOCK_TRADE_OPEN_CLOSE_EXCLUDED = frozenset({2, 5, 7, 10, 12, 13, 15, 16, 20, 22, 38})
_STOCK_TRADE_VOLUME_EXCLUDED = frozenset({15, 16, 38})

_STOCK_QUOTE_HIGH_LOW_EXCLUDED = frozenset()
_STOCK_QUOTE_OPEN_CLOSE_EXCLUDED = frozenset()
_STOCK_QUOTE_VOLUME_EXCLUDED = frozenset()


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
        "decimal_size",
        "size_coefficient",
        "size_scale",
        "tape",
        "trf_id",
        "trf_timestamp",
    )

    def __init__(self, fields: list[str]) -> None:
        if len(fields) != 13:
            raise ValueError(f"StockTrade expected 13 fields, received {len(fields)}")
        self.ticker = fields[0]
        self.conditions = _bitset_indices(fields[1], StockTradeCondition)
        self.correction = _parse_int(fields[2])
        self.exchange = _parse_int(fields[3])
        self.id = _parse_int(fields[4])
        self.participant_timestamp = _parse_int(fields[5])
        self.price = _parse_float(fields[6])
        self.sequence_number = _parse_int(fields[7])
        self.sip_timestamp = _parse_int(fields[8])
        (
            self.decimal_size,
            self.size_coefficient,
            self.size_scale,
        ) = _parse_decimal_quantity(fields[9])
        self.size = float(self.decimal_size)
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
        return tuple(self) == tuple(other) and self.decimal_size == other.decimal_size

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
        return hash((tuple(self), self.decimal_size))

    def updates_high_low(self) -> bool:
        return _conditions_clear(self.conditions, _STOCK_TRADE_HIGH_LOW_EXCLUDED)

    def updates_open_close(self) -> bool:
        return _conditions_clear(self.conditions, _STOCK_TRADE_OPEN_CLOSE_EXCLUDED)

    def updates_volume(self) -> bool:
        return _conditions_clear(self.conditions, _STOCK_TRADE_VOLUME_EXCLUDED)

    def __repr__(self) -> str:
        return (
            "StockTrade("
            f"ticker={self.ticker!r}, conditions={self.conditions!r}, "
            f"correction={self.correction}, exchange={self.exchange}, id={self.id}, "
            f"participant_timestamp={self.participant_timestamp}, price={self.price}, "
            f"sequence_number={self.sequence_number}, sip_timestamp={self.sip_timestamp}, "
            f"size={self.decimal_size}, tape={self.tape}, trf_id={self.trf_id}, "
            f"trf_timestamp={self.trf_timestamp})"
        )

    __str__ = __repr__


class CryptoTrade:
    __slots__ = (
        "ticker",
        "conditions",
        "exchange",
        "id",
        "participant_timestamp",
        "price",
        "size",
    )

    def __init__(self, fields: list[str]) -> None:
        if len(fields) != 7:
            raise ValueError(f"CryptoTrade expected 7 fields, received {len(fields)}")
        self.ticker = fields[0]
        self.conditions = _bitset_indices(fields[1])
        self.exchange = _parse_int(fields[2])
        self.id = _parse_int(fields[3])
        self.participant_timestamp = _parse_int(fields[4])
        self.price = _parse_float(fields[5])
        self.size = _parse_float(fields[6])

    def __iter__(self) -> Iterator[object]:
        yield self.ticker
        yield self.conditions
        yield self.exchange
        yield self.id
        yield self.participant_timestamp
        yield self.price
        yield self.size

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, CryptoTrade):
            return NotImplemented
        return tuple(self) == tuple(other)

    def __lt__(self, other: object) -> bool:
        if not isinstance(other, CryptoTrade):
            return NotImplemented
        return self.participant_timestamp < other.participant_timestamp

    def __le__(self, other: object) -> bool:
        if not isinstance(other, CryptoTrade):
            return NotImplemented
        return self.participant_timestamp <= other.participant_timestamp

    def __gt__(self, other: object) -> bool:
        if not isinstance(other, CryptoTrade):
            return NotImplemented
        return self.participant_timestamp > other.participant_timestamp

    def __ge__(self, other: object) -> bool:
        if not isinstance(other, CryptoTrade):
            return NotImplemented
        return self.participant_timestamp >= other.participant_timestamp

    def __hash__(self) -> int:
        return hash(tuple(self))

    def __repr__(self) -> str:
        return (
            "CryptoTrade("
            f"ticker={self.ticker!r}, conditions={self.conditions!r}, "
            f"exchange={self.exchange}, id={self.id}, "
            f"participant_timestamp={self.participant_timestamp}, "
            f"price={self.price}, size={self.size})"
        )

    __str__ = __repr__


def _option_conditions(value: str) -> frozenset[int]:
    if not value:
        return frozenset()
    result: set[int] = set()
    for item in value.split(","):
        if not item:
            raise ValueError("empty option trade condition code")
        code = _parse_int(item)
        if code < 201 or code > 248:
            raise ValueError(f"option trade condition code out of range: {code}")
        result.add(code)
    return frozenset(result)


def _parse_option_symbol(ticker: str) -> tuple[str, str, str, float]:
    if not ticker.startswith("O:"):
        raise ValueError(f"option ticker must start with O: {ticker}")

    body = ticker[2:]
    suffix_size = 15
    if len(body) <= suffix_size:
        raise ValueError(f"option ticker is too short: {ticker}")

    expiration = body[-suffix_size:-suffix_size + 6]
    right = body[-9]
    strike_text = body[-8:]
    if right not in {"C", "P"}:
        raise ValueError(f"option ticker right must be C or P: {ticker}")
    if not expiration.isdecimal() or not strike_text.isdecimal():
        raise ValueError(f"option ticker contains invalid numeric fields: {ticker}")

    year = 2000 + int(expiration[:2])
    month = int(expiration[2:4])
    day = int(expiration[4:6])
    return (
        body[:-suffix_size],
        dt.date(year, month, day).isoformat(),
        right,
        int(strike_text) / 1000.0,
    )


class OptionTrade:
    __slots__ = (
        "root",
        "expiration",
        "right",
        "strike",
        "conditions",
        "correction",
        "exchange",
        "price",
        "sip_timestamp",
        "size",
    )

    def __init__(self, fields: list[str]) -> None:
        if len(fields) != 7:
            raise ValueError(f"OptionTrade expected 7 fields, received {len(fields)}")
        self.root, self.expiration, self.right, self.strike = _parse_option_symbol(fields[0])
        self.conditions = _option_conditions(fields[1])
        self.correction = _parse_int(fields[2])
        self.exchange = _parse_int(fields[3])
        self.price = _parse_float(fields[4])
        self.sip_timestamp = _parse_int(fields[5])
        self.size = _parse_int(fields[6])

    def __iter__(self) -> Iterator[object]:
        yield self.root
        yield self.expiration
        yield self.right
        yield self.strike
        yield self.conditions
        yield self.correction
        yield self.exchange
        yield self.price
        yield self.sip_timestamp
        yield self.size

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, OptionTrade):
            return NotImplemented
        return tuple(self) == tuple(other)

    def __lt__(self, other: object) -> bool:
        if not isinstance(other, OptionTrade):
            return NotImplemented
        return self.sip_timestamp < other.sip_timestamp

    def __le__(self, other: object) -> bool:
        if not isinstance(other, OptionTrade):
            return NotImplemented
        return self.sip_timestamp <= other.sip_timestamp

    def __gt__(self, other: object) -> bool:
        if not isinstance(other, OptionTrade):
            return NotImplemented
        return self.sip_timestamp > other.sip_timestamp

    def __ge__(self, other: object) -> bool:
        if not isinstance(other, OptionTrade):
            return NotImplemented
        return self.sip_timestamp >= other.sip_timestamp

    def __hash__(self) -> int:
        return hash(tuple(self))

    def __repr__(self) -> str:
        return (
            "OptionTrade("
            f"root={self.root!r}, expiration={self.expiration!r}, "
            f"right={self.right!r}, strike={self.strike}, "
            f"conditions={self.conditions!r}, "
            f"correction={self.correction}, exchange={self.exchange}, "
            f"price={self.price}, sip_timestamp={self.sip_timestamp}, "
            f"size={self.size})"
        )

    __str__ = __repr__


class OptionQuote:
    __slots__ = (
        "root",
        "expiration",
        "right",
        "strike",
        "ask_exchange",
        "ask_price",
        "ask_size",
        "bid_exchange",
        "bid_price",
        "bid_size",
        "sequence_number",
        "sip_timestamp",
    )

    def __init__(self, fields: list[str]) -> None:
        if len(fields) != 9:
            raise ValueError(f"OptionQuote expected 9 fields, received {len(fields)}")
        self.root, self.expiration, self.right, self.strike = _parse_option_symbol(fields[0])
        self.ask_exchange = _parse_int(fields[1])
        self.ask_price = _parse_nullable_float(fields[2])
        self.ask_size = _parse_int(fields[3])
        self.bid_exchange = _parse_int(fields[4])
        self.bid_price = _parse_nullable_float(fields[5])
        self.bid_size = _parse_int(fields[6])
        self.sequence_number = _parse_int(fields[7])
        self.sip_timestamp = _parse_int(fields[8])

    def __iter__(self) -> Iterator[object]:
        yield self.root
        yield self.expiration
        yield self.right
        yield self.strike
        yield self.ask_exchange
        yield self.ask_price
        yield self.ask_size
        yield self.bid_exchange
        yield self.bid_price
        yield self.bid_size
        yield self.sequence_number
        yield self.sip_timestamp

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, OptionQuote):
            return NotImplemented
        ask_prices_equal = self.ask_price == other.ask_price or (
            math.isnan(self.ask_price) and math.isnan(other.ask_price)
        )
        bid_prices_equal = self.bid_price == other.bid_price or (
            math.isnan(self.bid_price) and math.isnan(other.bid_price)
        )
        return (
            self.root == other.root
            and self.expiration == other.expiration
            and self.right == other.right
            and self.strike == other.strike
            and self.ask_exchange == other.ask_exchange
            and ask_prices_equal
            and self.ask_size == other.ask_size
            and self.bid_exchange == other.bid_exchange
            and bid_prices_equal
            and self.bid_size == other.bid_size
            and self.sequence_number == other.sequence_number
            and self.sip_timestamp == other.sip_timestamp
        )

    def __lt__(self, other: object) -> bool:
        if not isinstance(other, OptionQuote):
            return NotImplemented
        return self.sip_timestamp < other.sip_timestamp

    def __le__(self, other: object) -> bool:
        if not isinstance(other, OptionQuote):
            return NotImplemented
        return self.sip_timestamp <= other.sip_timestamp

    def __gt__(self, other: object) -> bool:
        if not isinstance(other, OptionQuote):
            return NotImplemented
        return self.sip_timestamp > other.sip_timestamp

    def __ge__(self, other: object) -> bool:
        if not isinstance(other, OptionQuote):
            return NotImplemented
        return self.sip_timestamp >= other.sip_timestamp

    def __hash__(self) -> int:
        return hash(tuple(self))

    def __repr__(self) -> str:
        return (
            "OptionQuote("
            f"root={self.root!r}, expiration={self.expiration!r}, "
            f"right={self.right!r}, strike={self.strike}, "
            f"ask_exchange={self.ask_exchange}, ask_price={self.ask_price}, "
            f"ask_size={self.ask_size}, bid_exchange={self.bid_exchange}, "
            f"bid_price={self.bid_price}, bid_size={self.bid_size}, "
            f"sequence_number={self.sequence_number}, "
            f"sip_timestamp={self.sip_timestamp})"
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
        self.conditions = _bitset_indices(fields[7], StockQuoteCondition)
        self.indicators = _bitset_indices(fields[8], StockQuoteCondition)
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

    def updates_high_low(self) -> bool:
        return _conditions_clear(
            self.conditions | self.indicators, _STOCK_QUOTE_HIGH_LOW_EXCLUDED
        )

    def updates_open_close(self) -> bool:
        return _conditions_clear(
            self.conditions | self.indicators, _STOCK_QUOTE_OPEN_CLOSE_EXCLUDED
        )

    def updates_volume(self) -> bool:
        return _conditions_clear(
            self.conditions | self.indicators, _STOCK_QUOTE_VOLUME_EXCLUDED
        )

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


class IndexValue:
    __slots__ = ("ticker", "value", "timestamp")

    def __init__(self, fields: list[str]) -> None:
        if len(fields) != 3:
            raise ValueError(f"IndexValue expected 3 fields, received {len(fields)}")
        self.ticker = fields[0]
        self.value = _parse_float(fields[1])
        self.timestamp = _parse_int(fields[2])

    def __iter__(self) -> Iterator[object]:
        yield self.ticker
        yield self.value
        yield self.timestamp

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, IndexValue):
            return NotImplemented
        return tuple(self) == tuple(other)

    def __lt__(self, other: object) -> bool:
        if not isinstance(other, IndexValue):
            return NotImplemented
        return self.timestamp < other.timestamp

    def __le__(self, other: object) -> bool:
        if not isinstance(other, IndexValue):
            return NotImplemented
        return self.timestamp <= other.timestamp

    def __gt__(self, other: object) -> bool:
        if not isinstance(other, IndexValue):
            return NotImplemented
        return self.timestamp > other.timestamp

    def __ge__(self, other: object) -> bool:
        if not isinstance(other, IndexValue):
            return NotImplemented
        return self.timestamp >= other.timestamp

    def __hash__(self) -> int:
        return hash(tuple(self))

    def __repr__(self) -> str:
        return (
            "IndexValue("
            f"ticker={self.ticker!r}, value={self.value}, timestamp={self.timestamp})"
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


class FlatFileParser(Parser):
    parser_group_name = "flatfiles"

    @classmethod
    def parse_quotes(cls, payload: bytes | str) -> dict[str, object]:
        raise NotImplementedError("flatfile quote parsing must be implemented by a concrete parser")

    @classmethod
    def parse_trades(cls, payload: bytes | str) -> dict[str, object]:
        raise NotImplementedError("flatfile trade parsing must be implemented by a concrete parser")


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

    @classmethod
    def parse_trades(
        cls,
        payload: str | Path,
        *,
        sort_by_sip_timestamp: bool = False,
    ) -> Iterator[OptionTrade]:
        del sort_by_sip_timestamp
        with gzip.open(payload, "rt", encoding="utf-8", newline="") as handle:
            reader = csv.reader(handle)
            next(reader, None)
            current_root: str | None = None
            rows: list[OptionTrade] = []
            for fields in reader:
                if not any(fields):
                    continue
                row = OptionTrade(fields)
                if current_root is None:
                    current_root = row.root
                if row.root != current_root:
                    rows.sort(
                        key=lambda item: (
                            item.sip_timestamp,
                            item.expiration,
                            item.right,
                            item.strike,
                        )
                    )
                    yield from rows
                    rows = [row]
                    current_root = row.root
                else:
                    rows.append(row)

            rows.sort(
                key=lambda item: (
                    item.sip_timestamp,
                    item.expiration,
                    item.right,
                    item.strike,
                )
            )
            yield from rows

    @classmethod
    def parse_raw_trades(
        cls,
        payload: str | Path,
        *,
        sort_by_sip_timestamp: bool = False,
    ) -> Iterator[tuple[bytes, ...]]:
        del sort_by_sip_timestamp
        with gzip.open(payload, "rt", encoding="utf-8", newline="") as handle:
            reader = csv.reader(handle)
            next(reader, None)
            for fields in reader:
                if any(fields):
                    yield tuple(field.encode("utf-8") for field in fields)

    @classmethod
    def parse_quotes(
        cls,
        payload: str | Path,
        *,
        sort_by_sip_timestamp: bool = False,
    ) -> Iterator[OptionQuote]:
        del sort_by_sip_timestamp
        with gzip.open(payload, "rt", encoding="utf-8", newline="") as handle:
            reader = csv.reader(handle)
            next(reader, None)
            current_root: str | None = None
            rows: list[OptionQuote] = []
            for fields in reader:
                if not any(fields):
                    continue
                row = OptionQuote(fields)
                if current_root is None:
                    current_root = row.root
                if row.root != current_root:
                    rows.sort(
                        key=lambda item: (
                            item.sip_timestamp,
                            item.expiration,
                            item.right,
                            item.strike,
                            item.sequence_number,
                        )
                    )
                    yield from rows
                    rows = [row]
                    current_root = row.root
                else:
                    rows.append(row)

            rows.sort(
                key=lambda item: (
                    item.sip_timestamp,
                    item.expiration,
                    item.right,
                    item.strike,
                    item.sequence_number,
                )
            )
            yield from rows

    @classmethod
    def parse_raw_quotes(
        cls,
        payload: str | Path,
        *,
        sort_by_sip_timestamp: bool = False,
    ) -> Iterator[tuple[bytes, ...]]:
        del sort_by_sip_timestamp
        with gzip.open(payload, "rt", encoding="utf-8", newline="") as handle:
            reader = csv.reader(handle)
            next(reader, None)
            for fields in reader:
                if any(fields):
                    yield tuple(field.encode("utf-8") for field in fields)

    @classmethod
    def raw_lines(cls, payload: str | Path) -> Iterator[bytes]:
        with gzip.open(payload, "rb") as handle:
            next(handle, None)
            for line in handle:
                line = line.rstrip(b"\r\n")
                if line:
                    yield line


class FlatFileFuturesParser(FlatFileParser):
    asset_class_name = "futures"


class FlatFileIndicesParser(FlatFileParser):
    asset_class_name = "indices"

    @classmethod
    def parse_values(
        cls,
        payload: str | Path,
        *,
        sort_by_timestamp: bool = False,
    ) -> Iterator[IndexValue]:
        with gzip.open(payload, "rt", encoding="utf-8", newline="") as handle:
            reader = csv.reader(handle)
            next(reader, None)
            rows = [IndexValue(fields) for fields in reader if any(fields)]

        if sort_by_timestamp:
            rows.sort(key=lambda row: (row.timestamp, row.ticker))

        yield from rows

    @classmethod
    def parse_raw_values(
        cls,
        payload: str | Path,
        *,
        sort_by_timestamp: bool = False,
    ) -> Iterator[tuple[bytes, ...]]:
        with gzip.open(payload, "rt", encoding="utf-8", newline="") as handle:
            reader = csv.reader(handle)
            next(reader, None)
            rows = [tuple(fields) for fields in reader if any(fields)]

        if sort_by_timestamp:
            rows.sort(key=lambda row: (_parse_int(row[2]), row[0]))

        for row in rows:
            yield tuple(field.encode("utf-8") for field in row)

    @classmethod
    def raw_lines(cls, payload: str | Path) -> Iterator[bytes]:
        with gzip.open(payload, "rb") as handle:
            next(handle, None)
            for line in handle:
                line = line.rstrip(b"\r\n")
                if line:
                    yield line


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
    def raw_lines(cls, payload: str | Path) -> Iterator[bytes]:
        with gzip.open(payload, "rb") as handle:
            next(handle, None)
            for line in handle:
                line = line.rstrip(b"\r\n")
                if line:
                    yield line


class FlatFileCryptoParser(FlatFileParser):
    asset_class_name = "crypto"

    @classmethod
    def parse_trades(
        cls,
        payload: str | Path,
        *,
        sort_by_participant_timestamp: bool = False,
    ) -> Iterator[CryptoTrade]:
        with gzip.open(payload, "rt", encoding="utf-8", newline="") as handle:
            reader = csv.reader(handle)
            next(reader, None)
            rows = [CryptoTrade(fields) for fields in reader if any(fields)]

        if sort_by_participant_timestamp:
            rows.sort(key=lambda row: (row.participant_timestamp, row.ticker, row.id))

        yield from rows

    @classmethod
    def parse_raw_trades(
        cls,
        payload: str | Path,
        *,
        sort_by_participant_timestamp: bool = False,
    ) -> Iterator[tuple[bytes, ...]]:
        with gzip.open(payload, "rt", encoding="utf-8", newline="") as handle:
            reader = csv.reader(handle)
            next(reader, None)
            rows = [tuple(fields) for fields in reader if any(fields)]

        if sort_by_participant_timestamp:
            rows.sort(key=lambda row: (_parse_int(row[4]), row[0], _parse_int(row[3])))

        for row in rows:
            yield tuple(field.encode("utf-8") for field in row)

    @classmethod
    def raw_lines(cls, payload: str | Path) -> Iterator[bytes]:
        with gzip.open(payload, "rb") as handle:
            next(handle, None)
            for line in handle:
                line = line.rstrip(b"\r\n")
                if line:
                    yield line


FlatFiles = SimpleNamespace(
    Stock=FlatFileStocksParser,
    Options=FlatFileOptionsParser,
    Futures=FlatFileFuturesParser,
    Indices=FlatFileIndicesParser,
    Forex=FlatFileForexParser,
    currency=FlatFileCurrenciesParser,
    Crypto=FlatFileCryptoParser,
)

FlatFiles.StockTrade = StockTrade
FlatFiles.CryptoTrade = CryptoTrade
FlatFiles.OptionTrade = OptionTrade
FlatFiles.OptionQuote = OptionQuote
FlatFiles.StockQuote = StockQuote
FlatFiles.StockQuotes = StockQuote
FlatFiles.CurrencyQuote = CurrencyQuote
FlatFiles.IndexValue = IndexValue

class StockTradeAggregation:
    __slots__ = ()


class StockQuoteAggregation:
    __slots__ = ()


class CurrencyQuoteAggregation:
    __slots__ = ()


class ValueAggregation:
    __slots__ = ()


CryptoTradeAggregation = StockTradeAggregation
FuturesTradeAggregation = StockTradeAggregation
OptionTradeAggregation = StockTradeAggregation
FuturesQuoteAggregation = StockQuoteAggregation
OptionQuoteAggregation = StockQuoteAggregation
IndexValueAggregation = ValueAggregation


class _NativeOnlyAggregator:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("aggregation requires the native massive_speedup extension")


class StockTradeAggregator(_NativeOnlyAggregator):
    pass


class StockQuoteAggregator(_NativeOnlyAggregator):
    pass


class CurrencyQuoteAggregator(_NativeOnlyAggregator):
    pass


class CryptoTradeAggregator(_NativeOnlyAggregator):
    pass


class IndexValueAggregator(_NativeOnlyAggregator):
    pass


class FuturesTradeAggregator(_NativeOnlyAggregator):
    pass


class FuturesQuoteAggregator(_NativeOnlyAggregator):
    pass


class OptionTradeAggregator(_NativeOnlyAggregator):
    pass


class OptionQuoteAggregator(_NativeOnlyAggregator):
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
FlatFiles.currency.Quote = SimpleNamespace(
    parse=FlatFileCurrenciesParser.parse_quotes,
    parse_raw=FlatFileCurrenciesParser.parse_raw_quotes,
    raw_lines=FlatFileCurrenciesParser.raw_lines,
    Aggregator=CurrencyQuoteAggregator,
)
FlatFiles.Crypto.Trade = SimpleNamespace(
    parse=FlatFileCryptoParser.parse_trades,
    parse_raw=FlatFileCryptoParser.parse_raw_trades,
    raw_lines=FlatFileCryptoParser.raw_lines,
    Aggregator=CryptoTradeAggregator,
)

FlatFiles.Options.Trade = SimpleNamespace(
    parse=FlatFileOptionsParser.parse_trades,
    parse_raw=FlatFileOptionsParser.parse_raw_trades,
    raw_lines=FlatFileOptionsParser.raw_lines,
    Aggregator=OptionTradeAggregator,
)

FlatFiles.Options.Quote = SimpleNamespace(
    parse=FlatFileOptionsParser.parse_quotes,
    parse_raw=FlatFileOptionsParser.parse_raw_quotes,
    raw_lines=FlatFileOptionsParser.raw_lines,
    Aggregator=OptionQuoteAggregator,
)

FlatFiles.Indices.Value = SimpleNamespace(
    parse=FlatFileIndicesParser.parse_values,
    parse_raw=FlatFileIndicesParser.parse_raw_values,
    raw_lines=FlatFileIndicesParser.raw_lines,
    Aggregator=IndexValueAggregator,
)

FlatFiles.Futures.Trade = SimpleNamespace(Aggregator=FuturesTradeAggregator)
FlatFiles.Futures.Quote = SimpleNamespace(Aggregator=FuturesQuoteAggregator)


class FuturesTrade:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("FuturesTrade requires the native massive_speedup extension")


class FuturesQuote:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("FuturesQuote requires the native massive_speedup extension")


def build_database_file(*args, **kwargs):
    raise RuntimeError("build_database_file requires the native massive_speedup extension")


def build_database_file_inferred(*args, **kwargs):
    raise RuntimeError(
        "build_database_file_inferred requires the native massive_speedup extension"
    )


def build_database_file_inferred_with_stats(*args, **kwargs):
    raise RuntimeError(
        "build_database_file_inferred_with_stats requires the native "
        "massive_speedup extension"
    )


class StockTradeDatabase:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("StockTradeDatabase requires the native massive_speedup extension")


class StockQuoteDatabase:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("StockQuoteDatabase requires the native massive_speedup extension")


class CryptoTradeDatabase:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("CryptoTradeDatabase requires the native massive_speedup extension")


class StockTradeQuoteTimeline:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("StockTradeQuoteTimeline requires the native massive_speedup extension")


class SimpleMarketBroker:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("SimpleMarketBroker requires the native massive_speedup extension")


class SimpleMarket:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("SimpleMarket requires the native massive_speedup extension")


class TradeEmulator:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("TradeEmulator requires the native massive_speedup extension")


def stock_trade_quote_timeline(*args, **kwargs):
    raise RuntimeError("stock_trade_quote_timeline requires the native massive_speedup extension")


class CurrencyQuoteDatabase:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("CurrencyQuoteDatabase requires the native massive_speedup extension")


class IndexValueDatabase:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("IndexValueDatabase requires the native massive_speedup extension")


class FuturesTradeDatabase:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("FuturesTradeDatabase requires the native massive_speedup extension")


class FuturesQuoteDatabase:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("FuturesQuoteDatabase requires the native massive_speedup extension")


class OptionTradeDatabase:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("OptionTradeDatabase requires the native massive_speedup extension")


class OptionQuoteDatabase:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("OptionQuoteDatabase requires the native massive_speedup extension")


class FuturesMarketBroker:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("FuturesMarketBroker requires the native massive_speedup extension")


class FuturesMarket:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("FuturesMarket requires the native massive_speedup extension")


class OptionMarketBroker:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("OptionMarketBroker requires the native massive_speedup extension")


class OptionMarket:
    def __init__(self, *args, **kwargs):
        raise RuntimeError("OptionMarket requires the native massive_speedup extension")
