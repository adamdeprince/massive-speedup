from pathlib import Path
from importlib import import_module
import datetime as dt
import gzip
import math
import struct
import sys
import types

import pytest
from massive_speedup import (
    CryptoTrade,
    CurrencyQuote,
    FlatFiles,
    IndexValue,
    OptionQuote,
    OptionTrade,
    StockQuote,
    StockQuoteCondition,
    StockTrade,
    StockTradeCondition,
    WebSocket,
    gzip_lines,
    read_gzip_lines,
)


@pytest.fixture(autouse=True)
def _stub_process_title_dependency(monkeypatch: pytest.MonkeyPatch) -> None:
    from massive_speedup import build_database, download

    monkeypatch.setattr(build_database, "set_process_title", lambda title: None)
    monkeypatch.setattr(download, "set_process_title", lambda title: None)


def test_process_title_helper_uses_setproctitle(monkeypatch: pytest.MonkeyPatch) -> None:
    from massive_speedup._process_title import set_process_title

    calls: list[str] = []
    monkeypatch.setitem(
        sys.modules,
        "setproctitle",
        types.SimpleNamespace(setproctitle=calls.append),
    )

    set_process_title("massive-dl")

    assert calls == ["massive-dl"]


def test_flatfiles_stocks_parse_quotes_reads_gzip_records(tmp_path: Path) -> None:
    path = tmp_path / "quotes.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,ask_exchange,ask_price,ask_size,bid_exchange,bid_price,bid_size,"
            "conditions,indicators,participant_timestamp,sequence_number,"
            "sip_timestamp,tape,trf_timestamp\n"
        )
        handle.write('A,19,0.0,0,19,0.0,0,"1,81",,1770800340065735000,324,1770800340066046795,1,0\n')

    quotes = list(FlatFiles.Stock.parse_quotes(path))

    assert len(quotes) == 1
    assert isinstance(quotes[0], StockQuote)
    assert quotes[0].ticker == "A"
    assert quotes[0].conditions == frozenset({1, 81})
    assert quotes[0].participant_timestamp == 1770800340065735000


def test_flatfiles_stocks_nested_quote_api_parse_and_parse_raw(tmp_path: Path) -> None:
    path = tmp_path / "quotes.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,ask_exchange,ask_price,ask_size,bid_exchange,bid_price,bid_size,"
            "conditions,indicators,participant_timestamp,sequence_number,"
            "sip_timestamp,tape,trf_timestamp\n"
        )
        handle.write('A,19,0.0,0,19,0.0,0,"1,81",,1770800340065735000,324,1770800340066046795,1,0\n')

    quote = next(FlatFiles.Stock.Quote.parse(path))
    raw_quote = next(FlatFiles.Stock.Quote.parse_raw(path))

    assert isinstance(quote, StockQuote)
    assert raw_quote == (
        b"A",
        b"19",
        b"0.0",
        b"0",
        b"19",
        b"0.0",
        b"0",
        b"1,81",
        b"",
        b"1770800340065735000",
        b"324",
        b"1770800340066046795",
        b"1",
        b"0",
    )


def test_flatfiles_stocks_parse_trades_reads_gzip_records(tmp_path: Path) -> None:
    path = tmp_path / "trades.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,conditions,correction,exchange,id,participant_timestamp,price,"
            "sequence_number,sip_timestamp,size,tape,trf_id,trf_timestamp\n"
        )
        handle.write("A,12,0,8,52983525035849,1770810300032981000,129.79,6876,1770810300033243132,100,1,0,0\n")

    trades = list(FlatFiles.Stock.parse_trades(path))

    assert len(trades) == 1
    assert isinstance(trades[0], StockTrade)
    assert trades[0].ticker == "A"
    assert trades[0].exchange == 8
    assert trades[0].participant_timestamp == 1770810300032981000
    assert trades[0].price == 129.79


def test_flatfiles_stocks_parse_trades_accepts_decimal_size_field(tmp_path: Path) -> None:
    path = tmp_path / "recent_trades.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,conditions,correction,exchange,id,participant_timestamp,price,"
            "sequence_number,sip_timestamp,size,tape,trf_id,trf_timestamp\n"
        )
        handle.write(
            'A,"12,37",0,4,52983526340682,1775023619906000000,113.950000,'
            "4410,1775030437400930176,0.123456789012345678,1,201,"
            "1775030437400903786\n"
        )

    trade = next(FlatFiles.Stock.Trade.parse(path))

    assert trade.conditions == frozenset({12, 37})
    assert trade.exchange == 4
    assert trade.price == 113.95
    assert trade.size == pytest.approx(0.123456789012345678)
    assert isinstance(trade.size, float)
    assert trade.decimal_size == "0.123456789012345678"
    assert trade.size_coefficient == 123456789012345678
    assert trade.size_scale == 18
    assert trade.trf_id == 201


def test_flatfiles_stocks_nested_trade_api_parse_and_parse_raw(tmp_path: Path) -> None:
    path = tmp_path / "trades.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,conditions,correction,exchange,id,participant_timestamp,price,"
            "sequence_number,sip_timestamp,size,tape,trf_id,trf_timestamp\n"
        )
        handle.write("A,12,0,8,52983525035849,1770810300032981000,129.79,6876,1770810300033243132,100,1,0,0\n")

    trade = next(FlatFiles.Stock.Trade.parse(path))
    raw_trade = next(FlatFiles.Stock.Trade.parse_raw(path))

    assert isinstance(trade, StockTrade)
    assert raw_trade == (
        b"A",
        b"12",
        b"0",
        b"8",
        b"52983525035849",
        b"1770810300032981000",
        b"129.79",
        b"6876",
        b"1770810300033243132",
        b"100",
        b"1",
        b"0",
        b"0",
    )


def test_flatfiles_stocks_nested_trade_api_parse_raw_reads_quoted_conditions(tmp_path: Path) -> None:
    path = tmp_path / "quoted_raw_trades.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,conditions,correction,exchange,id,participant_timestamp,price,"
            "sequence_number,sip_timestamp,size,tape,trf_id,trf_timestamp\n"
        )
        handle.write(
            'A,"12,37",0,12,62879131135034,1769161728012624580,137.73,'
            "4798,1769161728012983416,15,1,0,0\n"
        )

    raw_trade = next(FlatFiles.Stock.Trade.parse_raw(path))

    assert raw_trade == (
        b"A",
        b"12,37",
        b"0",
        b"12",
        b"62879131135034",
        b"1769161728012624580",
        b"137.73",
        b"4798",
        b"1769161728012983416",
        b"15",
        b"1",
        b"0",
        b"0",
    )


def test_flatfiles_stocks_parse_raw_reuses_repeated_ticker_bytes(tmp_path: Path) -> None:
    path = tmp_path / "trades.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,conditions,correction,exchange,id,participant_timestamp,price,"
            "sequence_number,sip_timestamp,size,tape,trf_id,trf_timestamp\n"
        )
        handle.write("A,12,0,8,1,2,3.5,4,5,6,7,8,9\n")
        handle.write("A,13,0,8,10,11,12.5,13,14,15,16,17,18\n")
        handle.write("B,14,0,8,19,20,21.5,22,23,24,25,26,27\n")

    rows = list(FlatFiles.Stock.Trade.parse_raw(path))

    assert rows[0][0] is rows[1][0]
    assert rows[1][0] is not rows[2][0]


def test_flatfiles_stocks_nested_trade_api_raw_lines(tmp_path: Path) -> None:
    path = tmp_path / "trades.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,conditions,correction,exchange,id,participant_timestamp,price,"
            "sequence_number,sip_timestamp,size,tape,trf_id,trf_timestamp\n"
        )
        handle.write("A,12,0,8,1,2,3.5,4,5,6,7,8,9\n")
        handle.write("B,13,0,9,10,11,12.5,13,14,15,16,17,18\n")

    assert list(FlatFiles.Stock.Trade.raw_lines(path)) == [
        b"A,12,0,8,1,2,3.5,4,5,6,7,8,9",
        b"B,13,0,9,10,11,12.5,13,14,15,16,17,18",
    ]


def test_flatfiles_stocks_parse_trades_reads_quoted_condition_sets(tmp_path: Path) -> None:
    path = tmp_path / "quoted_conditions_trades.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,conditions,correction,exchange,id,participant_timestamp,price,"
            "sequence_number,sip_timestamp,size,tape,trf_id,trf_timestamp\n"
        )
        handle.write(
            'A,"1,81",0,8,52983525035849,1770810300032981000,129.79,6876,'
            "1770810300033243132,100,1,0,0\n"
        )

    trades = list(FlatFiles.Stock.parse_trades(path))

    assert len(trades) == 1
    assert trades[0].conditions == frozenset({1, 81})


def test_flatfiles_stocks_nested_quote_api_parse_raw_reads_quoted_condition_fields(
    tmp_path: Path,
) -> None:
    path = tmp_path / "quoted_raw_quotes.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,ask_exchange,ask_price,ask_size,bid_exchange,bid_price,bid_size,"
            "conditions,indicators,participant_timestamp,sequence_number,"
            "sip_timestamp,tape,trf_timestamp\n"
        )
        handle.write('A,19,0.0,0,19,0.0,0,"1,81",,1770800340065735000,324,1770800340066046795,1,0\n')

    raw_quote = next(FlatFiles.Stock.Quote.parse_raw(path))

    assert raw_quote[7] == b"1,81"
    assert raw_quote[8] == b""
    assert len(raw_quote) == 14


def test_flatfiles_currencies_quote_parse_and_parse_raw(tmp_path: Path) -> None:
    path = tmp_path / "currencies.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,ask_exchange,ask_price,bid_exchange,bid_price,participant_timestamp\n"
        )
        handle.write(
            "C:AED-AUD,48,0.412060465749694,48,0.411836123587859,1757552407000000000\n"
        )

    quote = next(FlatFiles.currency.Quote.parse(path))
    raw_quote = next(FlatFiles.currency.Quote.parse_raw(path))

    assert isinstance(quote, CurrencyQuote)
    assert quote.ticker == "C:AED-AUD"
    assert quote.ask_exchange == 48
    assert quote.bid_price == 0.411836123587859
    assert quote.participant_timestamp == 1757552407000000000
    assert raw_quote == (
        b"C:AED-AUD",
        b"48",
        b"0.412060465749694",
        b"48",
        b"0.411836123587859",
        b"1757552407000000000",
    )
    assert quote.tickers == ("AED", "AUD")


def test_flatfiles_indices_value_parse_and_parse_raw(tmp_path: Path) -> None:
    path = tmp_path / "index_values.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write("ticker,value,timestamp\n")
        handle.write("I:AAPLCW,87.27,1784295061137000000\n")

    value = next(FlatFiles.Indices.Value.parse(path))
    raw_value = next(FlatFiles.Indices.Value.parse_raw(path))

    assert isinstance(value, IndexValue)
    assert tuple(value) == ("I:AAPLCW", 87.27, 1784295061137000000)
    assert raw_value == (b"I:AAPLCW", b"87.27", b"1784295061137000000")
    assert list(FlatFiles.Indices.Value.raw_lines(path)) == [
        b"I:AAPLCW,87.27,1784295061137000000"
    ]


def test_flatfiles_indices_values_sort_by_timestamp(tmp_path: Path) -> None:
    path = tmp_path / "index_values.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write("ticker,value,timestamp\n")
        handle.write("I:LATE,2.0,20\n")
        handle.write("I:EARLY,1.0,10\n")

    values = list(FlatFiles.Indices.Value.parse(path, sort_by_timestamp=True))
    raw_values = list(FlatFiles.Indices.Value.parse_raw(path, sort_by_timestamp=True))

    assert [row.ticker for row in values] == ["I:EARLY", "I:LATE"]
    assert [row[0] for row in raw_values] == [b"I:EARLY", b"I:LATE"]


def test_flatfiles_currencies_quote_rejects_sort_by_sip_timestamp(tmp_path: Path) -> None:
    path = tmp_path / "currencies.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,ask_exchange,ask_price,bid_exchange,bid_price,participant_timestamp\n"
        )
        handle.write(
            "C:AED-AUD,48,0.412060465749694,48,0.411836123587859,1757552407000000000\n"
        )

    with pytest.raises(Exception):
        list(FlatFiles.currency.Quote.parse(path, sort_by_sip_timestamp=True))


def test_flatfiles_stocks_parse_trades_is_unsorted_by_default(tmp_path: Path) -> None:
    path = tmp_path / "sorted_trades.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,conditions,correction,exchange,id,participant_timestamp,price,"
            "sequence_number,sip_timestamp,size,tape,trf_id,trf_timestamp\n"
        )
        handle.write("A,1,0,8,1,300,10.0,1,300,100,1,0,300\n")
        handle.write("A,1,0,8,2,100,11.0,2,500,100,1,0,500\n")
        handle.write("B,1,0,8,3,200,12.0,3,200,100,1,0,200\n")
        handle.write("B,1,0,8,4,50,13.0,4,400,100,1,0,400\n")

    trades = list(FlatFiles.Stock.parse_trades(path))

    assert [trade.id for trade in trades] == [1, 2, 3, 4]


def test_flatfiles_stocks_parse_trades_can_sort_by_sip_timestamp(tmp_path: Path) -> None:
    path = tmp_path / "sorted_trades.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,conditions,correction,exchange,id,participant_timestamp,price,"
            "sequence_number,sip_timestamp,size,tape,trf_id,trf_timestamp\n"
        )
        handle.write("A,1,0,8,1,300,10.0,1,300,100,1,0,300\n")
        handle.write("A,1,0,8,2,100,11.0,2,500,100,1,0,500\n")
        handle.write("B,1,0,8,3,200,12.0,3,200,100,1,0,200\n")
        handle.write("B,1,0,8,4,50,13.0,4,400,100,1,0,400\n")

    trades = list(FlatFiles.Stock.parse_trades(path, sort_by_sip_timestamp=True))

    assert [trade.id for trade in trades] == [3, 1, 4, 2]
    assert [trade.sip_timestamp for trade in trades] == [200, 300, 400, 500]


def test_flatfiles_stocks_parse_trades_can_sort_by_participant_timestamp(tmp_path: Path) -> None:
    path = tmp_path / "participant_sorted_trades.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,conditions,correction,exchange,id,participant_timestamp,price,"
            "sequence_number,sip_timestamp,size,tape,trf_id,trf_timestamp\n"
        )
        handle.write("A,1,0,8,1,300,10.0,1,300,100,1,0,300\n")
        handle.write("A,1,0,8,2,100,11.0,2,500,100,1,0,500\n")
        handle.write("B,1,0,8,3,200,12.0,3,200,100,1,0,200\n")
        handle.write("B,1,0,8,4,50,13.0,4,400,100,1,0,400\n")

    trades = list(FlatFiles.Stock.parse_trades(path, sort_by_participant_timestamp=True))

    assert [trade.id for trade in trades] == [4, 2, 3, 1]
    assert [trade.participant_timestamp for trade in trades] == [50, 100, 200, 300]


def test_flatfiles_stocks_parse_trades_rejects_conflicting_sort_flags(tmp_path: Path) -> None:
    path = tmp_path / "conflicting_flags_trades.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,conditions,correction,exchange,id,participant_timestamp,price,"
            "sequence_number,sip_timestamp,size,tape,trf_id,trf_timestamp\n"
        )
        handle.write("A,1,0,8,1,300,10.0,1,300,100,1,0,300\n")

    with pytest.raises(Exception):
        list(
            FlatFiles.Stock.parse_trades(
                path,
                sort_by_participant_timestamp=True,
                sort_by_sip_timestamp=True,
            )
        )


def test_flatfiles_stocks_parse_trades_sort_flags_are_keyword_only(tmp_path: Path) -> None:
    path = tmp_path / "keyword_only_trades.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,conditions,correction,exchange,id,participant_timestamp,price,"
            "sequence_number,sip_timestamp,size,tape,trf_id,trf_timestamp\n"
        )
        handle.write("A,1,0,8,1,300,10.0,1,300,100,1,0,300\n")

    with pytest.raises(TypeError):
        list(FlatFiles.Stock.parse_trades(path, True))


def test_massive_flatfile_aggregate_apis_are_not_exposed() -> None:
    package = import_module("massive_speedup")

    assert not hasattr(package, "StockAggregate")
    assert not hasattr(package, "CurrencyAggregate")
    assert not hasattr(FlatFiles.Stock, "Aggregate")
    assert not hasattr(FlatFiles.currency, "Aggregate")
    for parser in (FlatFiles.Stock, FlatFiles.currency, FlatFiles.Forex):
        assert not hasattr(parser, "parse_minute_aggregates")
        assert not hasattr(parser, "parse_daily_aggregates")
        assert not hasattr(parser, "parse_raw_minute_aggregates")
        assert not hasattr(parser, "parse_raw_daily_aggregates")


def test_websocket_messages_parse_message_is_class_based() -> None:
    message = WebSocket.Messages.parse_message(
        '[{"ev":"status"},{"ev":"trade"}]'
    )

    assert message.asset_class == "messages"
    assert len(message) == 2
    assert [event.event_type for event in message] == ["status", "trade"]


def test_websocket_crypto_parse_message_uses_selected_processor_module() -> None:
    message = WebSocket.Crypto.parse_message('{"ev":"XT"}')
    assert message.asset_class == "crypto"
    assert WebSocket.Crypto.processor_name() in {
        "generic",
        "sse42",
        "avx2",
        "avx512",
        "neon",
        "sve",
        "sve2",
        "lsx",
        "lasx",
    }


def test_websocket_message_owns_payload_and_lazily_caches_fields() -> None:
    payload = (
        b'[{"ev":"T","sym":"AAPL","p":197.25,"c":[1,37],'
        b'"details":{"source":"sip"},"nullable":null},'
        b'{"ev":"Q","sym":"MSFT"}]'
    )
    message = WebSocket.Stocks.parse(payload)
    event = message[0]
    del payload

    assert len(message) == 2
    assert message.events[0] is event
    assert message[-1].event_type == "Q"
    assert event.cached_fields == ()
    assert not event.is_cached("sym")

    assert event.event_type == "T"
    assert event.cached_fields == ("ev",)
    assert event["p"] == pytest.approx(197.25)
    assert event["sym"] == "AAPL"
    assert event["sym"] is event["sym"]
    assert event["c"] == [1, 37]
    assert event["details"] == {"source": "sip"}
    assert event["nullable"] is None
    assert event["nullable"] is None
    assert event.get("absent") is None
    sentinel = object()
    assert event.get("another_absent", sentinel) is sentinel
    assert "details" in event
    assert "not_there" not in event
    assert event.message_bytes == message.raw_json
    assert event.raw_json.startswith(b'{"ev":"T"')


def test_websocket_message_accepts_one_object_and_sequence_indexing() -> None:
    message = WebSocket.Crypto.parse_message(
        b'{"ev":"XT","pair":"BTC-USD","i":184467440737095516150}'
    )

    assert len(message) == 1
    assert message[0]["pair"] == "BTC-USD"
    assert message[:] == message.events
    assert message[0]["i"] == 184467440737095516150


@pytest.mark.parametrize(
    "payload",
    [
        b"",
        b"42",
        b'[{}, 3]',
        b'[{"ev":"T"}',
        b'{"ev":3}',
    ],
)
def test_websocket_message_rejects_invalid_shapes(payload: bytes) -> None:
    with pytest.raises((ValueError, RuntimeError)):
        WebSocket.Messages.parse_message(payload)


def test_gzip_lines_yields_hello_world_lines() -> None:
    path = Path(__file__).resolve().parent / "data" / "hello_world.txt.gz"
    assert list(gzip_lines(path)) == [b"Hello", b"World!"]


def test_read_gzip_lines_alias_yields_hello_world_lines() -> None:
    path = Path(__file__).resolve().parent / "data" / "hello_world.txt.gz"
    assert list(read_gzip_lines(path)) == [b"Hello", b"World!"]


def test_stock_trade_row_model_is_iterable_hashable_and_ordered() -> None:
    newer = StockTrade(
        [
            "AAPL",
            "0,2",
            "0",
            "11",
            "42",
            "200",
            "191.25",
            "99",
            "210",
            "10",
            "2",
            "7",
            "220",
        ]
    )
    older = StockTrade(
        [
            "AAPL",
            "0,2",
            "0",
            "11",
            "42",
            "100",
            "191.25",
            "99",
            "210",
            "10",
            "2",
            "7",
            "220",
        ]
    )

    assert newer.ticker == "AAPL"
    assert newer.conditions == frozenset({0, 2})
    assert list(newer)[5] == 200
    assert newer > older
    assert hash(newer) == hash(
        StockTrade(
            [
                "AAPL",
                "0,2",
                "0",
                "11",
                "42",
                "200",
                "191.25",
                "99",
                "210",
                "10",
                "2",
                "7",
                "220",
            ]
        )
    )
    assert "participant_timestamp=200" in repr(newer)
    assert newer.conditions is older.conditions


def test_stock_quote_row_model_exposes_read_only_fields() -> None:
    quote = StockQuote(
        [
            "MSFT",
            "4",
            "410.5",
            "25",
            "12",
            "410.25",
            "30",
            "0,1",
            "0,3",
            "500",
            "501",
            "502",
            "1",
            "503",
        ]
    )

    assert quote.ask_exchange == 4
    assert quote.bid_price == 410.25
    assert quote.conditions == frozenset({0, 1})
    assert quote.indicators == frozenset({0, 3})
    assert list(quote)[9] == 500
    assert "StockQuote(" in str(quote)


def test_currency_quote_row_model_is_iterable_hashable_and_ordered() -> None:
    newer = CurrencyQuote(
        [
            "C:AED-AUD",
            "48",
            "0.412060465749694",
            "48",
            "0.411836123587859",
            "1757552407000000001",
        ]
    )
    older = CurrencyQuote(
        [
            "C:AED-AUD",
            "48",
            "0.412060465749694",
            "48",
            "0.411836123587859",
            "1757552407000000000",
        ]
    )

    assert newer.ask_exchange == 48
    assert list(newer)[5] == 1757552407000000001
    assert newer > older
    assert hash(newer) == hash(
        CurrencyQuote(
            [
                "C:AED-AUD",
                "48",
                "0.412060465749694",
                "48",
                "0.411836123587859",
                "1757552407000000001",
            ]
        )
    )
    assert "CurrencyQuote(" in repr(newer)
    assert newer.tickers == ("AED", "AUD")


def test_condition_frozensets_use_symbolic_enums_and_are_kind_interned() -> None:
    trade = StockTrade(
        [
            "AAPL",
            "1,81",
            "0",
            "11",
            "42",
            "200",
            "191.25",
            "99",
            "210",
            "10",
            "2",
            "7",
            "220",
        ]
    )
    quote = StockQuote(
        [
            "MSFT",
            "4",
            "410.5",
            "25",
            "12",
            "410.25",
            "30",
            "1,81",
            "",
            "500",
            "501",
            "502",
            "1",
            "503",
        ]
    )

    assert StockTradeCondition.ACQUISITION in trade.conditions
    assert 81 in trade.conditions
    assert StockQuoteCondition.REGULAR_TWO_SIDED_OPEN in quote.conditions
    assert StockQuoteCondition.CORRECTED_PRICE_INDICATION in quote.conditions
    assert trade.conditions is trade.conditions
    assert quote.conditions is quote.conditions
    assert trade.conditions is not quote.conditions


def test_stock_condition_rule_methods_are_exposed() -> None:
    def trade_with_conditions(conditions: str) -> StockTrade:
        return StockTrade(
            [
                "AAPL",
                conditions,
                "0",
                "11",
                "42",
                "200",
                "191.25",
                "99",
                "210",
                "10",
                "2",
                "7",
                "220",
            ]
        )

    regular = trade_with_conditions("1")
    assert regular.updates_high_low()
    assert regular.updates_open_close()
    assert regular.updates_volume()

    extended_hours = trade_with_conditions("12")
    assert not extended_hours.updates_high_low()
    assert not extended_hours.updates_open_close()
    assert extended_hours.updates_volume()

    official_close = trade_with_conditions("15")
    assert not official_close.updates_high_low()
    assert not official_close.updates_open_close()
    assert not official_close.updates_volume()

    unknown = trade_with_conditions("81")
    assert unknown.updates_high_low()
    assert unknown.updates_open_close()
    assert unknown.updates_volume()

    quote = StockQuote(
        [
            "MSFT",
            "4",
            "410.5",
            "25",
            "12",
            "410.25",
            "30",
            "1,81",
            "84",
            "500",
            "501",
            "502",
            "1",
            "503",
        ]
    )
    assert quote.updates_high_low()
    assert quote.updates_open_close()
    assert quote.updates_volume()


def test_currency_tickers_are_globally_interned() -> None:
    quote = CurrencyQuote(
        [
            "C:AED-AUD",
            "48",
            "0.412060465749694",
            "48",
            "0.411836123587859",
            "1757552407000000000",
        ]
    )
    second_quote = CurrencyQuote(
        [
            "C:AED-AUD",
            "48",
            "0.412060465749694",
            "48",
            "0.411836123587859",
            "1769133600000000000",
        ]
    )

    assert quote.tickers is second_quote.tickers


def test_build_database_parser_accepts_positional_files() -> None:
    from massive_speedup import build_database

    parser = build_database.build_parser()
    args = parser.parse_args(
        [
            "--database",
            "db",
            "stock_quotes.csv.gz",
            "currency_quotes.csv.gz",
        ]
    )

    assert args.input_files == [Path("stock_quotes.csv.gz"), Path("currency_quotes.csv.gz")]
    assert args.database_path == Path("db")
    assert args.benchmark is False
    assert args.force is False
    assert not hasattr(args, "record_type")
    assert not hasattr(args, "input_stdin")
    assert not hasattr(args, "input_file")


def test_build_database_expand_input_files_recurses_directories(tmp_path: Path) -> None:
    from massive_speedup import build_database

    nested = tmp_path / "nested"
    nested.mkdir()
    top_csv = tmp_path / "a.csv.gz"
    nested_csv = nested / "b.csv.gz"
    ignored = nested / "c.txt"
    top_csv.write_bytes(b"")
    nested_csv.write_bytes(b"")
    ignored.write_text("x", encoding="utf-8")

    files = build_database.expand_input_files([tmp_path])

    assert files == sorted([top_csv.resolve(), nested_csv.resolve()])


def test_build_database_infers_record_type_from_header(tmp_path: Path) -> None:
    from massive_speedup import build_database

    cases = {
        build_database.STOCK_TRADE_HEADER: "stock_trade",
        build_database.STOCK_QUOTE_HEADER: "stock_quote",
        build_database.CURRENCY_QUOTE_HEADER: "currency_quote",
        build_database.CRYPTO_TRADE_HEADER: "crypto_trade",
        build_database.INDEX_VALUE_HEADER: "index_value",
        build_database.OPTION_TRADE_HEADER: "option_trade",
        build_database.OPTION_QUOTE_HEADER: "option_quote",
    }

    for index, (header, expected_type) in enumerate(cases.items()):
        path = tmp_path / f"{index}.csv.gz"
        with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
            handle.write(header + "\n")

        assert build_database.infer_record_type(path) == expected_type

    futures_trade_dir = tmp_path / "future_cme_trade"
    futures_trade_dir.mkdir()
    futures_trade_path = futures_trade_dir / "2026-05-21.csv.gz"
    with gzip.open(futures_trade_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.FUTURES_TRADE_HEADER + "\n")

    futures_quote_dir = tmp_path / "future_nymex_quote"
    futures_quote_dir.mkdir()
    futures_quote_path = futures_quote_dir / "2026-05-21.csv.gz"
    with gzip.open(futures_quote_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.FUTURES_QUOTE_HEADER + "\n")

    assert build_database.infer_record_type(futures_trade_path) == "future_cme_trade"
    assert build_database.infer_record_type(futures_quote_path) == "future_nymex_quote"


def test_build_database_main_processes_mixed_positional_files(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    from massive_speedup import build_database

    stock_path = tmp_path / "stock_quote" / "1970-01-01.csv.gz"
    currency_path = tmp_path / "currency_quote" / "1970-01-01.csv.gz"
    stock_path.parent.mkdir()
    currency_path.parent.mkdir()
    with gzip.open(stock_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.STOCK_QUOTE_HEADER + "\n")
    with gzip.open(currency_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.CURRENCY_QUOTE_HEADER + "\n")

    calls = []
    titles: list[str] = []

    def fake_write_database_file(
        input_path: Path,
        record_type: str,
        *,
        database_path: Path,
        force: bool = False,
    ) -> int:
        calls.append(("write", input_path, database_path, record_type, force))
        return 1

    monkeypatch.setattr(build_database, "set_process_title", titles.append)
    monkeypatch.setattr(build_database, "write_database_file", fake_write_database_file)

    result = build_database.main(
        [
            "--database",
            str(tmp_path / "db"),
            str(stock_path),
            str(currency_path),
        ]
    )

    assert result == 0
    assert titles == ["massive-builddb"]
    assert calls == [
        ("write", stock_path, tmp_path / "db", "stock_quote", False),
        ("write", currency_path, tmp_path / "db", "currency_quote", False),
    ]


def test_build_database_main_skips_bad_header_and_logs(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
) -> None:
    from massive_speedup import build_database

    bad_path = tmp_path / "bad" / "1970-01-01.csv.gz"
    good_path = tmp_path / "stock_quote" / "1970-01-01.csv.gz"
    bad_path.parent.mkdir()
    good_path.parent.mkdir()
    with gzip.open(bad_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write("not,a,supported,header\n")
    with gzip.open(good_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.STOCK_QUOTE_HEADER + "\n")

    calls = []

    def fake_write_database_file(
        input_path: Path,
        record_type: str,
        *,
        database_path: Path,
        force: bool = False,
    ) -> int:
        calls.append((input_path, record_type))
        return 0

    monkeypatch.setattr(build_database, "write_database_file", fake_write_database_file)

    result = build_database.main(
        [
            "--database",
            str(tmp_path / "db"),
            str(bad_path),
            str(good_path),
        ]
    )

    assert result == 0
    assert calls == [(good_path.resolve(), "stock_quote")]
    output = capsys.readouterr().out
    assert "unsupported input header" in output
    assert str(bad_path.resolve()) in output


def test_build_database_main_force_passes_through(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    from massive_speedup import build_database

    stock_path = tmp_path / "stock_quote" / "1970-01-01.csv.gz"
    stock_path.parent.mkdir()
    with gzip.open(stock_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.STOCK_QUOTE_HEADER + "\n")

    calls = []

    def fake_write_database_file(
        input_path: Path,
        record_type: str,
        *,
        database_path: Path,
        force: bool = False,
    ) -> int:
        calls.append((input_path, database_path, record_type, force))
        return 1

    monkeypatch.setattr(build_database, "write_database_file", fake_write_database_file)

    result = build_database.main(
        [
            "--force",
            "--database",
            str(tmp_path / "db"),
            str(stock_path),
        ]
    )

    assert result == 0
    assert calls == [(stock_path, tmp_path / "db", "stock_quote", True)]


def test_build_database_main_delegates_existing_date_directory_to_native_builder(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    from massive_speedup import build_database

    stock_path = tmp_path / "stock_quote" / "1970-01-01.csv.gz"
    stock_path.parent.mkdir()
    with gzip.open(stock_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.STOCK_QUOTE_HEADER + "\n")
        handle.write("A,8,0.0,0,8,0.0,0,1,,0,322,0,1,0\n")

    database = tmp_path / "db"
    (database / "stock_quote" / "1970-01-01").mkdir(parents=True)

    calls = []

    def fake_write_database_file(
        input_path: Path,
        record_type: str,
        *,
        database_path: Path,
        force: bool = False,
    ) -> int:
        calls.append((input_path, database_path, record_type, force))
        return 1

    monkeypatch.setattr(build_database, "write_database_file", fake_write_database_file)

    result = build_database.main(["--database", str(database), str(stock_path)])

    assert result == 0
    assert calls == [(stock_path, database, "stock_quote", False)]


def test_build_database_main_force_overrides_complete_date_directory_skip(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    from massive_speedup import build_database

    stock_path = tmp_path / "stock_quote" / "1970-01-01.csv.gz"
    stock_path.parent.mkdir()
    with gzip.open(stock_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.STOCK_QUOTE_HEADER + "\n")
        handle.write("A,8,0.0,0,8,0.0,0,1,,0,322,0,1,0\n")

    database = tmp_path / "db"
    (database / "stock_quote" / "1970-01-01").mkdir(parents=True)

    calls = []

    def fake_write_database_file(
        input_path: Path,
        record_type: str,
        *,
        database_path: Path,
        force: bool = False,
    ) -> int:
        calls.append((input_path, database_path, record_type, force))
        return 1

    monkeypatch.setattr(build_database, "write_database_file", fake_write_database_file)

    result = build_database.main(
        [
            "--force",
            "--database",
            str(database),
            str(stock_path),
        ]
    )

    assert result == 0
    assert calls == [(stock_path, database, "stock_quote", True)]


def test_build_database_main_malformed_gzip_logs_and_continues(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
) -> None:
    from massive_speedup import build_database

    stock_path = tmp_path / "stock_quote" / "1970-01-01.csv.gz"
    stock_path.parent.mkdir()
    with gzip.open(stock_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.STOCK_QUOTE_HEADER + "\n")
        handle.write("A,8,0.0,0,8,0.0,0,1,,0,322,0,1,0\n")

    database = tmp_path / "db"
    def fake_write_database_file(*args, **kwargs):
        raise gzip.BadGzipFile("corrupt stream")

    monkeypatch.setattr(build_database, "write_database_file", fake_write_database_file)

    result = build_database.main(["--database", str(database), str(stock_path)])

    assert result == 0
    output = capsys.readouterr().out
    assert "Skipping malformed gzip" in output
    assert "corrupt stream" in output


def test_build_database_main_benchmark_prints_metrics(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
) -> None:
    from massive_speedup import build_database

    stock_path = tmp_path / "stock_quote" / "1970-01-01.csv.gz"
    stock_path.parent.mkdir()
    with gzip.open(stock_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.STOCK_QUOTE_HEADER + "\n")

    timer_values = iter([10.0, 12.0])

    monkeypatch.setattr(
        build_database,
        "write_database_file",
        lambda input_path, record_type, *, database_path, force=False: 2,
    )
    monkeypatch.setattr(build_database.time, "perf_counter", lambda: next(timer_values))

    result = build_database.main(
        [
            "--benchmark",
            "--database",
            str(tmp_path / "db"),
            str(stock_path),
        ]
    )

    assert result == 0
    stdout = capsys.readouterr().out
    assert f"file={stock_path}" in stdout
    assert "type=stock_quote" in stdout
    assert "lines=2 lines" in stdout
    assert "seconds=2.000000 s" in stdout
    assert "throughput=0.000001 Mlines/s" in stdout


def test_build_database_main_benchmark_reports_native_skip_count(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
) -> None:
    from massive_speedup import build_database

    stock_path = tmp_path / "stock_quote" / "1970-01-01.csv.gz"
    stock_path.parent.mkdir()
    with gzip.open(stock_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.STOCK_QUOTE_HEADER + "\n")
        handle.write("A,8,0.0,0,8,0.0,0,1,,0,322,0,1,0\n")

    database = tmp_path / "db"
    (database / "stock_quote" / "1970-01-01").mkdir(parents=True)
    timer_values = iter([10.0, 12.0])
    monkeypatch.setattr(build_database, "write_database_file", lambda *args, **kwargs: 0)
    monkeypatch.setattr(build_database.time, "perf_counter", lambda: next(timer_values))

    result = build_database.main(
        [
            "--benchmark",
            "--database",
            str(database),
            str(stock_path),
        ]
    )

    assert result == 0
    output = capsys.readouterr().out
    assert "lines=0 lines" in output


def test_build_database_file_native_groups_records_by_ticker_using_filename_date(
    tmp_path: Path,
) -> None:
    from massive_speedup import build_database
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    one_day_ns = 86_400_000_000_000
    path = tmp_path / "stock_trade" / "1970-01-01.csv.gz"
    path.parent.mkdir()
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.STOCK_TRADE_HEADER + "\n")
        handle.write("A,12,0,8,52983525035849,0,129.79,6876,0,100,1,0,0\n")
        handle.write(
            f"B,37,0,8,52983525035850,{one_day_ns},129.80,6877,{one_day_ns},100,1,0,0\n"
        )

    rows_written = build_database.write_database_file(
        path,
        "stock_trade",
        database_path=tmp_path / "db",
    )

    root = tmp_path / "db" / "stock_trade" / "1970-01-01"
    assert rows_written == 2
    assert len((root / "A").read_bytes()) == module.StockTrade.packed_size
    assert len((root / "B").read_bytes()) == module.StockTrade.packed_size
    assert module.StockTrade.from_packed((root / "A").read_bytes(), "A").ticker == "A"
    assert module.StockTrade.from_packed((root / "B").read_bytes(), "B").ticker == "B"
    assert not (tmp_path / "db" / "stock_trade" / "1970-01-02").exists()


def test_build_database_file_native_skips_existing_tickers_unless_forced(
    tmp_path: Path,
) -> None:
    from massive_speedup import build_database
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    database = tmp_path / "db"
    first_path = tmp_path / "stock_trade" / "1970-01-01.csv.gz"
    first_path.parent.mkdir()
    with gzip.open(first_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.STOCK_TRADE_HEADER + "\n")
        handle.write("A,12,0,8,52983525035849,0,129.79,6876,0,100,1,0,0\n")

    assert (
        build_database.write_database_file(
            first_path,
            "stock_trade",
            database_path=database,
        )
        == 1
    )
    output = database / "stock_trade" / "1970-01-01" / "A"
    original = module.StockTrade.from_packed(output.read_bytes(), "A")
    assert original.price == pytest.approx(129.79)

    second_path = tmp_path / "stock_trade_replacement" / "1970-01-01.csv.gz"
    second_path.parent.mkdir()
    with gzip.open(second_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.STOCK_TRADE_HEADER + "\n")
        handle.write("A,12,0,8,52983525035849,0,200.25,6876,0,100,1,0,0\n")

    assert (
        build_database.write_database_file(
            second_path,
            "stock_trade",
            database_path=database,
        )
        == 0
    )
    skipped = module.StockTrade.from_packed(output.read_bytes(), "A")
    assert skipped.price == pytest.approx(129.79)

    assert (
        build_database.write_database_file(
            second_path,
            "stock_trade",
            database_path=database,
            force=True,
        )
        == 1
    )
    overwritten = module.StockTrade.from_packed(output.read_bytes(), "A")
    assert overwritten.price == pytest.approx(200.25)


def test_build_database_file_native_publishes_only_completed_symbol_files(
    tmp_path: Path,
) -> None:
    from massive_speedup import build_database
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    path = tmp_path / "stock_trade" / "1970-01-01.csv.gz"
    path.parent.mkdir()
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.STOCK_TRADE_HEADER + "\n")
        handle.write("A,12,0,8,1,1000,10.0,1,1000,1.25,1,0,0\n")
        handle.write("B,12,0,8,2,2000,20.0,1,2000,2.5,1,0,0\n")
        handle.write("B,12,0,8,3,3000,21.0,2,3000,invalid,1,0,0\n")

    database = tmp_path / "db"
    with pytest.raises(ValueError, match="decimal quantity"):
        build_database.write_database_file(
            path,
            "stock_trade",
            database_path=database,
        )

    root = database / "stock_trade" / "1970-01-01"
    assert module.StockTrade.from_packed((root / "A").read_bytes(), "A").decimal_size == "1.25"
    assert not (root / "A.incomplete").exists()
    assert not (root / "B").exists()
    assert len((root / "B.incomplete").read_bytes()) == module.StockTrade.packed_size


def test_build_database_file_native_force_keeps_old_file_until_replacement_completes(
    tmp_path: Path,
) -> None:
    from massive_speedup import build_database
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    database = tmp_path / "db"
    initial = tmp_path / "initial" / "1970-01-01.csv.gz"
    initial.parent.mkdir()
    with gzip.open(initial, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.STOCK_TRADE_HEADER + "\n")
        handle.write("A,12,0,8,1,1000,10.0,1,1000,1,1,0,0\n")
    assert (
        build_database.write_database_file(
            initial,
            "stock_trade",
            database_path=database,
        )
        == 1
    )

    replacement = tmp_path / "replacement" / "1970-01-01.csv.gz"
    replacement.parent.mkdir()
    with gzip.open(replacement, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.STOCK_TRADE_HEADER + "\n")
        handle.write("A,12,0,8,1,1000,99.0,1,1000,1,1,0,0\n")
        handle.write("A,12,0,8,2,2000,100.0,2,2000,invalid,1,0,0\n")

    with pytest.raises(ValueError, match="decimal quantity"):
        build_database.write_database_file(
            replacement,
            "stock_trade",
            database_path=database,
            force=True,
        )

    output = database / "stock_trade" / "1970-01-01" / "A"
    assert module.StockTrade.from_packed(output.read_bytes(), "A").price == pytest.approx(10.0)
    assert (output.parent / "A.incomplete").exists()


def test_build_database_file_native_uses_configured_database_root(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    from massive_speedup import build_database
    try:
        import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    database = tmp_path / "db"
    monkeypatch.setenv("MASSIVE_SPEEDUP_DB_PATH", str(database))
    path = tmp_path / "stock_trade" / "1970-01-01.csv.gz"
    path.parent.mkdir()
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.STOCK_TRADE_HEADER + "\n")

    rows_written = build_database.write_database_file(
        path,
        "stock_trade",
    )

    assert rows_written == 0
    assert database.is_dir()


def test_build_database_file_native_uses_filename_date_for_currency_quote(
    tmp_path: Path,
) -> None:
    from massive_speedup import build_database
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    path = tmp_path / "currency_quote" / "2026-06-17.csv.gz"
    path.parent.mkdir()
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.CURRENCY_QUOTE_HEADER + "\n")
        handle.write("C:AED-AUD,48,0.412060465749694,48,0.411836123587859,0\n")

    rows_written = build_database.write_database_file(
        path,
        "currency_quote",
        database_path=tmp_path / "db",
    )

    assert rows_written == 1
    output = tmp_path / "db" / "currency_quote" / "2026-06-17" / "C:AED-AUD"
    assert len(output.read_bytes()) == module.CurrencyQuote.packed_size


def test_build_database_file_native_writes_searchable_index_values(
    tmp_path: Path,
) -> None:
    from massive_speedup import build_database
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    path = tmp_path / "index_value" / "2026-07-17.csv.gz"
    path.parent.mkdir()
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.INDEX_VALUE_HEADER + "\n")
        handle.write("I:AAPLCW,87.27,1784295061137000000\n")
        handle.write("I:AAPLCW,87.26,1784320540557000000\n")

    rows_written = build_database.write_database_file(
        path,
        "index_value",
        database_path=tmp_path / "db",
    )
    records = module.IndexValueDatabase(
        "2026-07-17",
        "I:AAPLCW",
        database_path=tmp_path / "db",
    )

    assert rows_written == 2
    assert len(records) == 2
    assert records.record_type == "index_value"
    assert records[0].value == pytest.approx(87.27)
    assert records[0].timestamp == 1784295061137000000
    assert records.index_before_timestamp(1784320540556999999) == 0
    assert records.index_after_timestamp(1784295061137000001) == 1
    packed = records[1].pack()
    assert len(packed) == module.IndexValue.packed_size == 16
    assert module.IndexValue.timestamp_from_packed(packed) == 1784320540557000000
    assert module.IndexValue.from_packed(packed, "I:AAPLCW") == records[1]


def test_build_database_file_native_writes_crypto_trade_databases(
    tmp_path: Path,
) -> None:
    from massive_speedup import build_database
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    path = tmp_path / "crypto_trade" / "2026-06-17.csv.gz"
    path.parent.mkdir()
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.CRYPTO_TRADE_HEADER + "\n")
        handle.write("X:00-USD,2,1,3641495,0,0.0036,30000.0\n")
        handle.write("X:00-USD,2,1,3641496,10,0.0037,7575.0\n")
        handle.write("X:01-USD,2,1,3641497,20,0.0038,72854.49\n")

    rows_written = build_database.write_database_file(
        path,
        "crypto_trade",
        database_path=tmp_path / "db",
    )

    root = tmp_path / "db" / "crypto_trade" / "2026-06-17"
    first = root / "X:00-USD"
    second = root / "X:01-USD"
    records = module.CryptoTradeDatabase(
        "2026-06-17",
        "X:00-USD",
        database_path=tmp_path / "db",
    )

    assert rows_written == 3
    assert len(first.read_bytes()) == module.CryptoTrade.packed_size * 2
    assert len(second.read_bytes()) == module.CryptoTrade.packed_size
    assert len(records) == 2
    assert records[0].ticker == "X:00-USD"
    assert records[0].id == 3641495
    assert records[1].participant_timestamp == 10
    assert records.index_before_timestamp(10) == 1
    assert records.index_after_timestamp(1) == 1
    assert list(records.iterate_bounded(5)) == [records[1]]


def test_build_database_file_native_rejects_misordered_crypto_trade_timestamps(
    tmp_path: Path,
) -> None:
    from massive_speedup import build_database
    try:
        import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    path = tmp_path / "crypto_trade" / "2026-06-17.csv.gz"
    path.parent.mkdir()
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.CRYPTO_TRADE_HEADER + "\n")
        handle.write("X:00-USD,2,1,3641496,10,0.0037,7575.0\n")
        handle.write("X:00-USD,2,1,3641495,9,0.0036,30000.0\n")

    with pytest.raises(ValueError, match="ticker,participant_timestamp"):
        build_database.write_database_file(
            path,
            "crypto_trade",
            database_path=tmp_path / "db",
        )


def test_build_database_file_native_writes_futures_trade_databases(
    tmp_path: Path,
) -> None:
    from massive_speedup import build_database
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    path = tmp_path / "future_cme_trade" / "2026-05-21.csv.gz"
    path.parent.mkdir()
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.FUTURES_TRADE_HEADER + "\n")
        handle.write(
            "0BTZ9,1779368715688046681,104906685,259,100.000000000,1,0,4,2026-05-21\n"
        )
        handle.write(
            "1BTZ9,1779368715688046682,104906686,260,101.000000000,2,0,4,2026-05-21\n"
        )

    rows_written = build_database.write_database_file(
        path,
        build_database.infer_record_type(path),
        database_path=tmp_path / "db",
    )

    assert rows_written == 2
    root = tmp_path / "db" / "future_cme_trade" / "2026-05-21"
    first = root / "0BTZ9"
    second = root / "1BTZ9"
    assert len(first.read_bytes()) == module.FuturesTrade.packed_size
    assert len(second.read_bytes()) == module.FuturesTrade.packed_size
    assert module.FuturesTrade.from_packed(first.read_bytes(), "0BTZ9").price == 100.0
    second_row = module.FuturesTrade.from_packed(second.read_bytes(), "1BTZ9")
    assert second_row.size == 2
    assert second_row.session_end_date == "2026-05-21"


def test_futures_trade_database_reads_mmap_records(tmp_path: Path) -> None:
    from massive_speedup import build_database
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    path = tmp_path / "future_cme_trade" / "2026-05-21.csv.gz"
    path.parent.mkdir()
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.FUTURES_TRADE_HEADER + "\n")
        handle.write(
            "0BTZ9,1779368715688046681,104906685,259,100.000000000,1,0,4,2026-05-21\n"
        )
        handle.write(
            "0BTZ9,1779368715688046683,104906686,260,101.000000000,2,0,4,2026-05-21\n"
        )

    rows_written = build_database.write_database_file(
        path,
        build_database.infer_record_type(path),
        database_path=tmp_path / "db",
    )

    records = module.FuturesTradeDatabase(
        dt.date(2026, 5, 21),
        "0BTZ9",
        exchange="cme",
        database_path=tmp_path / "db",
    )

    assert rows_written == 2
    assert records.record_type == "future_cme_trade"
    assert len(records) == 2
    assert records[0].ticker == "0BTZ9"
    assert records[0].timestamp == 1779368715688046681
    assert records[0].session_end_date == "2026-05-21"
    assert records[-1].price == 101.0
    assert [row.size for row in records] == [1, 2]
    assert records.index_before_timestamp(1779368715688046682) == 0
    assert records.index_after_timestamp(1779368715688046682) == 1
    assert records.index_before_timestamp(
        1779368715688046682,
        galloping=0,
    ) == 0
    assert [
        row.timestamp
        for row in records.iterate_bounded(
            1779368715688046681,
            1779368715688046681,
        )
    ] == [1779368715688046681]


def test_build_database_file_native_writes_futures_quote_databases(
    tmp_path: Path,
) -> None:
    from massive_speedup import build_database
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    path = tmp_path / "future_nymex_quote" / "2026-05-21.csv.gz"
    path.parent.mkdir()
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.FUTURES_QUOTE_HEADER + "\n")
        handle.write(
            "0BTZ9,1779368715560670607,104906684,257,"
            "1779368715560670607,100.000000000,1,"
            "1779047085970577269,,0,4,2026-05-21\n"
        )

    rows_written = build_database.write_database_file(
        path,
        build_database.infer_record_type(path),
        database_path=tmp_path / "db",
    )

    assert rows_written == 1
    output = tmp_path / "db" / "future_nymex_quote" / "2026-05-21" / "0BTZ9"
    packed = output.read_bytes()
    quote = module.FuturesQuote.from_packed(packed, "0BTZ9")
    assert len(packed) == module.FuturesQuote.packed_size
    assert quote.ask_price == 100.0
    assert math.isnan(quote.bid_price)
    assert quote.session_end_date == "2026-05-21"
    quote_bar = next(module.FuturesQuoteAggregator([quote], interval_seconds=60))
    assert quote_bar.ask_avg == 100.0
    assert math.isnan(quote_bar.bid_open)
    assert math.isnan(quote_bar.bid_avg)
    assert math.isnan(quote_bar.spread_avg)


def test_futures_quote_database_reads_mmap_records(tmp_path: Path) -> None:
    from massive_speedup import build_database
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    path = tmp_path / "future_nymex_quote" / "2026-05-21.csv.gz"
    path.parent.mkdir()
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.FUTURES_QUOTE_HEADER + "\n")
        handle.write(
            "0BTZ9,1779368715560670607,104906684,257,"
            "1779368715560670607,100.000000000,1,"
            "1779047085970577269,,0,4,2026-05-21\n"
        )
        handle.write(
            "0BTZ9,1779368715560670610,104906685,258,"
            "1779368715560670610,101.000000000,2,"
            "1779047085970577270,99.000000000,3,4,2026-05-21\n"
        )

    rows_written = build_database.write_database_file(
        path,
        build_database.infer_record_type(path),
        database_path=tmp_path / "db",
    )

    records = module.FuturesQuoteDatabase(
        "2026-05-21",
        "0BTZ9",
        exchange="nymex",
        database_path=tmp_path / "db",
    )

    assert rows_written == 2
    assert records.record_type == "future_nymex_quote"
    assert len(records) == 2
    assert records[0].ticker == "0BTZ9"
    assert records[0].timestamp == 1779368715560670607
    assert records[0].session_end_date == "2026-05-21"
    assert math.isnan(records[0].bid_price)
    assert records[1].bid_price == 99.0
    quote_bar = next(module.FuturesQuoteAggregator(records, interval_seconds=60))
    assert quote_bar.ask_avg == pytest.approx(100.5)
    assert quote_bar.bid_open == 99.0
    assert quote_bar.bid_avg == 99.0
    assert quote_bar.spread_avg == 2.0
    assert records.index_before_timestamp(1779368715560670609) == 0
    assert records.index_after_timestamp(1779368715560670609, galloping=0) == 1
    assert [
        row.ask_price
        for row in records.iterate_bounded(
            1779368715560670608,
            1779368715560670610,
        )
    ] == [101.0]


def test_futures_database_time_search_maps_evening_to_prior_calendar_day(
    tmp_path: Path,
) -> None:
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    session_end_date = "2026-07-17"
    epoch = dt.datetime(1970, 1, 1, tzinfo=dt.UTC)
    prior_evening = dt.datetime(2026, 7, 16, 22, 30, tzinfo=dt.UTC)
    session_day = dt.datetime(2026, 7, 17, 15, 0, tzinfo=dt.UTC)
    prior_evening_ns = int((prior_evening - epoch).total_seconds()) * 1_000_000_000
    session_day_ns = int((session_day - epoch).total_seconds()) * 1_000_000_000
    rows = [
        module.FuturesTrade(
            [
                "ESU6",
                str(timestamp),
                str(index),
                str(index),
                str(6000 + index),
                "1",
                "0",
                "4",
                session_end_date,
            ]
        )
        for index, timestamp in enumerate((prior_evening_ns, session_day_ns), start=1)
    ]
    path = tmp_path / "db" / "future_cme_trade" / session_end_date / "ESU6"
    path.parent.mkdir(parents=True)
    path.write_bytes(b"".join(row.pack() for row in rows))
    records = module.FuturesTradeDatabase(
        session_end_date,
        "ESU6",
        exchange="cme",
        database_path=tmp_path / "db",
    )

    assert records.index_after_timestamp(dt.time(22, 30, tzinfo=dt.UTC)) == 0
    assert records.index_before_timestamp(dt.time(22, 30, tzinfo=dt.UTC)) == 0
    assert records.index_after_timestamp(dt.time(15, 0, tzinfo=dt.UTC)) == 1
    assert next(records.iterate_bounded(dt.time(22, 30, tzinfo=dt.UTC))) == rows[0]


def test_build_database_file_native_writes_option_databases(tmp_path: Path) -> None:
    from massive_speedup import build_database
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    trade_path = tmp_path / "option_trade" / "2026-06-18.csv.gz"
    trade_path.parent.mkdir()
    with gzip.open(trade_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.OPTION_TRADE_HEADER + "\n")
        handle.write(
            "O:A260618C00115000,209,0,320,10.7,1781791123025000000,2\n"
        )
        handle.write(
            "O:A260618C00115000,227,0,308,11.2,1781791123025000001,1\n"
        )
        handle.write(
            "O:MSFT260618P00250000,227,0,308,12.25,1781791123025000002,3\n"
        )

    quote_path = tmp_path / "option_quote" / "2026-06-18.csv.gz"
    quote_path.parent.mkdir()
    with gzip.open(quote_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.OPTION_QUOTE_HEADER + "\n")
        handle.write(
            "O:A260618C00115000,320,11.0,1,320,10.5,1,10,1781791123024999000\n"
        )
        handle.write(
            "O:A260618C00115000,320,11.5,2,320,10.7,2,11,1781791123025001000\n"
        )

    trade_rows = build_database.write_database_file(
        trade_path,
        build_database.infer_record_type(trade_path),
        database_path=tmp_path / "db",
    )
    quote_rows = build_database.write_database_file(
        quote_path,
        build_database.infer_record_type(quote_path),
        database_path=tmp_path / "db",
    )

    root = tmp_path / "db" / "option_trade" / "2026-06-18"
    trade_file = root / "A" / "2026-06-18" / "C" / "00115000"
    other_trade_file = root / "MSFT" / "2026-06-18" / "P" / "00250000"
    quote_file = (
        tmp_path
        / "db"
        / "option_quote"
        / "2026-06-18"
        / "A"
        / "2026-06-18"
        / "C"
        / "00115000"
    )

    assert trade_rows == 3
    assert quote_rows == 2
    assert len(trade_file.read_bytes()) == module.OptionTrade.packed_size * 2
    assert len(other_trade_file.read_bytes()) == module.OptionTrade.packed_size
    assert len(quote_file.read_bytes()) == module.OptionQuote.packed_size * 2

    trades = module.OptionTradeDatabase(
        "2026-06-18",
        "A",
        "2026-06-18",
        "C",
        115.0,
        database_path=tmp_path / "db",
    )
    quotes = module.OptionQuoteDatabase(
        dt.date(2026, 6, 18),
        "A",
        "2026-06-18",
        "C",
        115.0,
        database_path=tmp_path / "db",
    )

    assert trades.record_type == "option_trade"
    assert trades.contract_key == "A/2026-06-18/C/00115000"
    assert len(trades) == 2
    assert trades[0].root == "A"
    assert trades[0].expiration == "2026-06-18"
    assert trades[0].right == "C"
    assert trades[0].strike == 115.0
    assert trades[1].price == 11.2
    assert trades.index_before_timestamp(1781791123025000000) == 0
    assert trades.index_after_timestamp(1781791123025000000, galloping=0) == 0
    assert [row.price for row in trades.iterate_bounded(1781791123025000001)] == [11.2]

    assert quotes.record_type == "option_quote"
    assert len(quotes) == 2
    assert quotes[0].bid_price == 10.5
    assert quotes[1].ask_price == 11.5


def test_build_database_file_native_sorts_option_contract_rows_within_root(
    tmp_path: Path,
) -> None:
    from massive_speedup import build_database
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    path = tmp_path / "option_trade" / "2026-06-18.csv.gz"
    path.parent.mkdir()
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.OPTION_TRADE_HEADER + "\n")
        handle.write(
            "O:A260618P00135000,209,0,320,13.5,1781791123025000002,2\n"
        )
        handle.write(
            "O:A260618P00080000,227,0,308,8.0,1781791123025000001,1\n"
        )
        handle.write(
            "O:A260618P00135000,227,0,308,13.0,1781791123025000000,1\n"
        )

    quote_path = tmp_path / "option_quote" / "2026-06-18.csv.gz"
    quote_path.parent.mkdir()
    with gzip.open(quote_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.OPTION_QUOTE_HEADER + "\n")
        handle.write(
            "O:A260618P00135000,320,13.6,2,320,13.4,2,12,1781791123025000002\n"
        )
        handle.write(
            "O:A260618P00080000,308,8.1,1,308,7.9,1,13,1781791123025000001\n"
        )
        handle.write(
            "O:A260618P00135000,308,13.1,1,308,12.9,1,11,1781791123025000000\n"
        )

    trade_rows_written = build_database.write_database_file(
        path,
        build_database.infer_record_type(path),
        database_path=tmp_path / "db",
    )
    quote_rows_written = build_database.write_database_file(
        quote_path,
        build_database.infer_record_type(quote_path),
        database_path=tmp_path / "db",
    )

    high_strike = module.OptionTradeDatabase(
        "2026-06-18",
        "A",
        "2026-06-18",
        "P",
        135.0,
        database_path=tmp_path / "db",
    )
    low_strike = module.OptionTradeDatabase(
        "2026-06-18",
        "A",
        "2026-06-18",
        "P",
        80.0,
        database_path=tmp_path / "db",
    )
    high_strike_quotes = module.OptionQuoteDatabase(
        "2026-06-18",
        "A",
        "2026-06-18",
        "P",
        135.0,
        database_path=tmp_path / "db",
    )

    assert trade_rows_written == 3
    assert quote_rows_written == 3
    assert [row.sip_timestamp for row in high_strike] == [
        1781791123025000000,
        1781791123025000002,
    ]
    assert [row.price for row in high_strike] == [13.0, 13.5]
    assert [row.sip_timestamp for row in low_strike] == [1781791123025000001]
    assert [row.sip_timestamp for row in high_strike_quotes] == [
        1781791123025000000,
        1781791123025000002,
    ]
    assert [row.ask_price for row in high_strike_quotes] == [13.1, 13.6]


def test_direct_native_module_exports_api() -> None:
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    assert hasattr(module, "FlatFiles")
    assert hasattr(module, "WebSocket")
    assert hasattr(module, "StockTrade")
    assert hasattr(module, "StockQuote")
    assert not hasattr(module, "StockAggregate")
    assert hasattr(module, "FuturesTrade")
    assert hasattr(module, "FuturesQuote")
    assert hasattr(module, "FuturesTradeDatabase")
    assert hasattr(module, "FuturesQuoteDatabase")
    assert hasattr(module, "FuturesMarket")
    assert hasattr(module, "FuturesMarketBroker")
    assert hasattr(module, "OptionTradeDatabase")
    assert hasattr(module, "OptionQuoteDatabase")
    assert hasattr(module, "OptionMarket")
    assert hasattr(module, "OptionMarketBroker")
    assert hasattr(module, "StockTradeDatabase")
    assert hasattr(module, "StockQuoteDatabase")
    assert hasattr(module, "StockTradeQuoteTimeline")
    assert hasattr(module, "stock_trade_quote_timeline")
    assert hasattr(module, "CurrencyQuote")
    assert not hasattr(module, "CurrencyAggregate")
    assert hasattr(module, "CurrencyQuoteDatabase")
    assert hasattr(module, "IndexValue")
    assert hasattr(module, "IndexValueDatabase")
    assert hasattr(module, "gzip_lines")
    assert hasattr(module, "build_database_file")


def test_direct_native_module_classes_are_callable_when_built() -> None:
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    path = Path(__file__).resolve().parent / "data" / "hello_world.txt.gz"
    message = module.WebSocket.Messages.parse_message(b'{"ev":"status"}')

    assert hasattr(module.FlatFiles.Stock, "parse_quotes")
    assert hasattr(module.FlatFiles.Stock, "parse_trades")
    assert hasattr(module.FlatFiles.Stock, "parse_raw_quotes")
    assert hasattr(module.FlatFiles.Stock, "parse_raw_trades")
    assert not hasattr(module.FlatFiles.Stock, "parse_minute_aggregates")
    assert not hasattr(module.FlatFiles.Stock, "parse_daily_aggregates")
    assert not hasattr(module.FlatFiles.Stock, "parse_raw_minute_aggregates")
    assert not hasattr(module.FlatFiles.Stock, "parse_raw_daily_aggregates")
    assert not hasattr(module.FlatFiles.Stock, "Aggregate")
    assert hasattr(module.FlatFiles.Stock, "raw_lines")
    assert not hasattr(module.FlatFiles, "Stocks")
    assert hasattr(module.FlatFiles.currency, "parse_quotes")
    assert hasattr(module.FlatFiles.currency, "parse_raw_quotes")
    assert not hasattr(module.FlatFiles.currency, "parse_minute_aggregates")
    assert not hasattr(module.FlatFiles.currency, "parse_daily_aggregates")
    assert not hasattr(module.FlatFiles.currency, "parse_raw_minute_aggregates")
    assert not hasattr(module.FlatFiles.currency, "parse_raw_daily_aggregates")
    assert not hasattr(module.FlatFiles.currency, "Aggregate")
    assert hasattr(module.FlatFiles.currency, "raw_lines")
    assert hasattr(module.FlatFiles.Options, "parse_quotes")
    assert hasattr(module.FlatFiles.Options, "parse_raw_quotes")
    assert hasattr(module.FlatFiles.Options, "Quote")
    assert hasattr(module, "read_gzip_lines_bytes")
    assert list(module.read_gzip_lines_bytes(path)) == [b"Hello", b"World!"]
    assert message.asset_class == "messages"
    assert message[0].event_type == "status"


def test_native_row_models_pack_roundtrip_and_extract_timestamps_when_built() -> None:
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    def assert_roundtrip(row_type, fields):
        row = row_type(fields)
        packed = row.pack()
        assert isinstance(packed, bytes)
        assert len(packed) == row_type.packed_size
        assert row_type.from_packed(packed, row.ticker) == row
        assert row_type(packed, row.ticker) == row
        return row, packed

    trade, packed_trade = assert_roundtrip(
        module.StockTrade,
        [
            "Brk.bBb",
            "12,37",
            "0",
            "12",
            "62879131135034",
            "1769161728012624580",
            "137.73",
            "4798",
            "1769161728012983416",
            "16713336.0",
            "1",
            "0",
            "0",
        ],
    )
    assert trade.size == 16713336.0
    assert isinstance(trade.size, float)
    assert trade.decimal_size == "16713336"
    assert trade.size_coefficient == 16713336
    assert trade.size_scale == 0
    size_offset = module.StockTrade.packed_size_offset
    assert struct.unpack("<Q", packed_trade[size_offset:size_offset + 8])[0] == 16713336
    assert packed_trade[module.StockTrade.packed_size_scale_offset] == 0
    quote, packed_quote = assert_roundtrip(
        module.StockQuote,
        [
            "Brk.bBb",
            "8",
            "0.0",
            "0",
            "8",
            "0.0",
            "0",
            "1,81",
            "",
            "1764147540102233000",
            "322",
            "1764147540102526248",
            "1",
            "0",
        ],
    )
    currency_quote, packed_currency_quote = assert_roundtrip(
        module.CurrencyQuote,
        [
            "C:AED-AUD",
            "48",
            "0.412060465749694",
            "48",
            "0.411836123587859",
            "1757552407000000000",
        ],
    )
    assert (
        module.StockTrade.participant_timestamp_from_packed(packed_trade)
        == trade.participant_timestamp
    )
    assert module.StockTrade.sip_timestamp_from_packed(packed_trade) == trade.sip_timestamp
    assert (
        module.StockQuote.participant_timestamp_from_packed(packed_quote)
        == quote.participant_timestamp
    )
    assert module.StockQuote.sip_timestamp_from_packed(packed_quote) == quote.sip_timestamp
    assert (
        module.CurrencyQuote.participant_timestamp_from_packed(packed_currency_quote)
        == currency_quote.participant_timestamp
    )

    trade_offset = module.StockTrade.packed_participant_timestamp_offset
    quote_offset = module.StockQuote.packed_sip_timestamp_offset
    assert (
        int.from_bytes(packed_trade[trade_offset:trade_offset + 8], "little")
        == trade.participant_timestamp
    )
    assert (
        int.from_bytes(packed_quote[quote_offset:quote_offset + 8], "little")
        == quote.sip_timestamp
    )


def test_native_row_attributes_are_immutable_and_cached_when_built() -> None:
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    trade = module.StockTrade(
        [
            "A",
            "12",
            "0",
            "8",
            "52983525035849",
            "1770810300032981000",
            "129.79",
            "6876",
            "1770810300033243132",
            "100",
            "1",
            "0",
            "0",
        ]
    )
    assert trade.price is trade.price
    assert trade.id is trade.id
    assert trade.conditions is trade.conditions
    assert list(trade)[6] is trade.price
    with pytest.raises(AttributeError):
        trade.price = 1.0

    quote = module.StockQuote(
        [
            "A",
            "8",
            "0.0",
            "0",
            "8",
            "0.0",
            "0",
            "1",
            "",
            "1764147540102233000",
            "322",
            "1764147540102526248",
            "1",
            "0",
        ]
    )
    assert quote.ask_price is quote.ask_price
    assert quote.participant_timestamp is quote.participant_timestamp
    assert quote.indicators is quote.indicators

    currency_quote = module.CurrencyQuote(
        ["C:AED-AUD", "48", "0.412", "48", "0.411", "1000"]
    )
    assert currency_quote.ask_price is currency_quote.ask_price
    assert currency_quote.tickers is currency_quote.tickers


def test_native_quote_and_trade_aggregators_yield_native_result_objects() -> None:
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    assert module.FlatFiles.Stock.Trade.Aggregator is module.StockTradeAggregator
    assert module.FlatFiles.Stock.Quote.Aggregator is module.StockQuoteAggregator
    assert module.FlatFiles.currency.Quote.Aggregator is module.CurrencyQuoteAggregator
    assert module.FlatFiles.Crypto.Trade.Aggregator is module.CryptoTradeAggregator
    assert module.FlatFiles.Indices.Value.Aggregator is module.IndexValueAggregator
    assert module.FlatFiles.Futures.Trade.Aggregator is module.FuturesTradeAggregator
    assert module.FlatFiles.Futures.Quote.Aggregator is module.FuturesQuoteAggregator
    assert module.FlatFiles.Options.Trade.Aggregator is module.OptionTradeAggregator
    assert module.FlatFiles.Options.Quote.Aggregator is module.OptionQuoteAggregator

    trade_rows = [
        module.StockTrade(
            [
                "A",
                "",
                "0",
                "8",
                "1",
                "1000000000",
                "10.0",
                "1",
                "1000000000",
                "100",
                "1",
                "0",
                "0",
            ]
        ),
        module.StockTrade(
            [
                "A",
                "",
                "0",
                "8",
                "2",
                "1500000000",
                "14.0",
                "2",
                "1500000000",
                "300.5",
                "1",
                "0",
                "0",
            ]
        ),
        module.StockTrade(
            [
                "A",
                "",
                "0",
                "8",
                "3",
                "2100000000",
                "7.0",
                "3",
                "2100000000",
                "50",
                "1",
                "0",
                "0",
            ]
        ),
    ]
    trade_aggregates = list(module.StockTradeAggregator(trade_rows, interval_seconds=1))
    assert len(trade_aggregates) == 2
    first_trade = trade_aggregates[0]
    assert isinstance(first_trade, module.StockTradeAggregation)
    assert first_trade.open is first_trade.open
    assert first_trade.ticker is first_trade.ticker
    assert first_trade.ticker == "A"
    assert first_trade.open == 10.0
    assert first_trade.close == 14.0
    assert first_trade.high == 14.0
    assert first_trade.low == 10.0
    assert first_trade.avg == 12.0
    assert first_trade.volume_weighted_avg == pytest.approx(13.001248439450686)
    assert first_trade.volume == 400.5
    assert isinstance(first_trade.volume, float)
    assert first_trade.window_start == 1_000_000_000
    assert first_trade.transactions == 2
    assert first_trade.stddev == 2.0
    assert first_trade.dollar_volume == 5207.0
    assert first_trade.avg_trade_size == 200.25
    assert first_trade.min_trade_size == 100.0
    assert first_trade.max_trade_size == 300.5
    assert first_trade.price_change == 4.0
    assert first_trade.return_bps == pytest.approx(4000.0)
    assert first_trade.price_range == 4.0
    assert first_trade.range_bps == pytest.approx(4000.0)
    assert first_trade.first_timestamp == 1_000_000_000
    assert first_trade.last_timestamp == 1_500_000_000
    assert first_trade.duration_ns == 500_000_000
    assert trade_aggregates[1].window_start == 2_000_000_000

    custom_start = list(
        module.StockTradeAggregator(
            trade_rows,
            interval_seconds=1,
            start_timestamp=1_250_000_000,
        )
    )
    assert len(custom_start) == 1
    assert custom_start[0].window_start == 1_250_000_000
    assert custom_start[0].open == 14.0
    assert custom_start[0].close == 7.0

    index_values = [
        module.IndexValue(["I:TEST", "100", "100000000"]),
        module.IndexValue(["I:TEST", "104", "900000000"]),
    ]
    index_bar = next(module.IndexValueAggregator(index_values, interval_seconds=1))
    assert isinstance(index_bar, module.ValueAggregation)
    assert isinstance(index_bar, module.IndexValueAggregation)
    assert index_bar.ticker == "I:TEST"
    assert index_bar.open == 100.0
    assert index_bar.close == 104.0
    assert index_bar.avg == 102.0
    assert index_bar.value_change == 4.0
    assert index_bar.transactions == 2

    quote_rows = [
        module.StockQuote(
            [
                "A",
                "8",
                "10.0",
                "2",
                "8",
                "8.0",
                "1",
                "",
                "",
                "100000000",
                "1",
                "100000000",
                "1",
                "0",
            ]
        ),
        module.StockQuote(
            [
                "A",
                "8",
                "14.0",
                "6",
                "8",
                "10.0",
                "3",
                "",
                "",
                "900000000",
                "2",
                "900000000",
                "1",
                "0",
            ]
        ),
    ]
    quote = next(module.StockQuoteAggregator(quote_rows, interval_seconds=1))
    assert isinstance(quote, module.StockQuoteAggregation)
    assert quote.ask_avg is quote.ask_avg
    assert quote.microprice_avg is quote.microprice_avg
    assert quote.ask_avg == 12.0
    assert quote.ask_volume_weighted_avg == 13.0
    assert quote.ask_volume == 8
    assert quote.ask_stddev == 2.0
    assert quote.bid_avg == 9.0
    assert quote.bid_volume_weighted_avg == pytest.approx(9.5)
    assert quote.bid_volume == 4
    assert quote.bid_stddev == 1.0
    assert quote.ask_change == 4.0
    assert quote.ask_return_bps == pytest.approx(4000.0)
    assert quote.ask_range == 4.0
    assert quote.ask_range_bps == pytest.approx(4000.0)
    assert quote.bid_change == 2.0
    assert quote.bid_return_bps == pytest.approx(2500.0)
    assert quote.bid_range == 2.0
    assert quote.bid_range_bps == pytest.approx(2500.0)
    assert quote.spread_open == 2.0
    assert quote.spread_close == 4.0
    assert quote.spread_avg == 3.0
    assert quote.spread_stddev == 1.0
    assert quote.mid_open == 9.0
    assert quote.mid_close == 12.0
    assert quote.mid_avg == 10.5
    assert quote.mid_stddev == 1.5
    assert quote.locked_count == 0
    assert quote.crossed_count == 0
    assert quote.zero_ask_size_count == 0
    assert quote.zero_bid_size_count == 0
    assert quote.size_imbalance_avg == pytest.approx(-1.0 / 3.0)
    assert quote.microprice_avg == pytest.approx(10.0)
    assert quote.time_weighted_ask_avg == pytest.approx(10.444444444444445)
    assert quote.time_weighted_bid_avg == pytest.approx(8.222222222222221)
    assert quote.time_weighted_mid_avg == pytest.approx(9.333333333333334)
    assert quote.time_weighted_spread_avg == pytest.approx(2.2222222222222223)
    assert quote.first_timestamp == 100_000_000
    assert quote.last_timestamp == 900_000_000
    assert quote.duration_ns == 800_000_000

    currency_rows = [
        module.CurrencyQuote(["C:AED-AUD", "48", "1.0", "48", "0.8", "100000000"]),
        module.CurrencyQuote(["C:AED-AUD", "48", "1.4", "48", "1.0", "900000000"]),
    ]
    currency = next(module.CurrencyQuoteAggregator(currency_rows, interval_seconds=1))
    assert isinstance(currency, module.CurrencyQuoteAggregation)
    assert not hasattr(currency, "volume")
    assert currency.ticker is currency.ticker
    assert currency.mid_avg is currency.mid_avg
    assert currency.ticker == "C:AED-AUD"
    assert currency.ask_avg == pytest.approx(1.2)
    assert currency.ask_stddev == pytest.approx(0.2)
    assert currency.bid_avg == pytest.approx(0.9)
    assert currency.bid_stddev == pytest.approx(0.1)
    assert currency.window_start == 0
    assert currency.transactions == 2
    assert currency.spread_open == pytest.approx(0.2)
    assert currency.spread_close == pytest.approx(0.4)
    assert currency.spread_avg == pytest.approx(0.3)
    assert currency.mid_open == pytest.approx(0.9)
    assert currency.mid_close == pytest.approx(1.2)
    assert currency.mid_avg == pytest.approx(1.05)
    assert currency.time_weighted_ask_avg == pytest.approx(1.0444444444444445)
    assert currency.time_weighted_bid_avg == pytest.approx(0.8222222222222222)
    assert currency.time_weighted_mid_avg == pytest.approx(0.9333333333333333)
    assert currency.time_weighted_spread_avg == pytest.approx(0.22222222222222224)
    assert currency.first_timestamp == 100_000_000
    assert currency.last_timestamp == 900_000_000
    assert currency.duration_ns == 800_000_000


def test_native_database_record_files_mmap_iter_index_search_and_market_calendar(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    database = tmp_path / "db"
    date = "1970-01-01"
    date_object = dt.date(1970, 1, 1)

    def write_records(record_type: str, ticker: str, rows) -> Path:
        path = database / record_type / date / ticker
        path.parent.mkdir(parents=True)
        path.write_bytes(b"".join(row.pack() for row in rows))
        return path

    trade_rows = [
        module.StockTrade(
            [
                "A",
                "12",
                "0",
                "8",
                "52983525035849",
                "1000",
                "129.79",
                "6876",
                "1000",
                "100",
                "1",
                "0",
                "0",
            ]
        ),
        module.StockTrade(
            [
                "A",
                "37",
                "0",
                "8",
                "52983525035850",
                "2000",
                "129.80",
                "6877",
                "2000",
                "100",
                "1",
                "0",
                "0",
            ]
        ),
    ]
    write_records("stock_trade", "A", trade_rows)

    trade_records = module.StockTradeDatabase(
        date_object,
        "A",
        database_path=database,
    )
    assert len(trade_records) == 2
    assert trade_records.ticker == "A"
    assert trade_records.date == date
    assert trade_records.path == database / "stock_trade" / date / "A"
    assert trade_records[0] == trade_rows[0]
    assert trade_records[-1] == trade_rows[1]
    assert list(trade_records) == trade_rows
    assert trade_records.index_before_timestamp(999) == -1
    assert trade_records.index_before_timestamp(1000) == 0
    assert trade_records.index_before_timestamp(1999) == 0
    assert trade_records.index_before_timestamp(2000) == 1
    assert trade_records.index_before_timestamp(2000, galloping=0) == 1
    assert trade_records.index_before_timestamp(2000, galloping=99) == 1
    saved_index = trade_records.index_before_timestamp(1000)
    assert trade_records.index_before_timestamp(2000, galloping=saved_index + 1) == 1
    assert trade_records.index_after_timestamp(999) == 0
    assert trade_records.index_after_timestamp(1000) == 0
    assert trade_records.index_after_timestamp(1001) == 1
    assert trade_records.index_after_timestamp(2000) == 1
    assert trade_records.index_after_timestamp(2001) == -1
    assert trade_records.index_after_timestamp(1000, galloping=99) == 0
    assert trade_records.index_after_timestamp(1000, galloping=-99) == 0
    assert trade_records.index_before_timestamp(dt.time(0, 0, 0, 1)) == 0
    assert list(trade_records.iterate_bounded(1000)) == trade_rows
    assert list(trade_records.iterate_bounded(dt.time(0, 0, 0, 2))) == [trade_rows[1]]
    assert list(trade_records.iterate_bounded(1000, 1999)) == [trade_rows[0]]
    assert list(trade_records.iterate_bounded(dt.time(0, 0, 0, 1), dt.time(0, 0, 0, 1))) == [
        trade_rows[0]
    ]
    assert list(trade_records.iterate_bounded(1001, 1999)) == []
    assert trade_records.find_before_participant_timestamp(1500, 1000) == trade_rows[0]
    assert trade_records.find_after_participant_timestamp(1500, 1000) == trade_rows[1]
    assert (
        trade_records.find_before_participant_timestamp(dt.time(0, 0, 0, 1))
        == trade_rows[0]
    )
    assert (
        trade_records.find_after_participant_timestamp(1000.0, 1000, on=True)
        == trade_rows[0]
    )
    assert (
        trade_records.find_after_participant_timestamp(1500, 1000, galloping=0)
        == trade_rows[1]
    )
    trade_bar = next(module.StockTradeAggregator(trade_records, interval_seconds=1))
    assert trade_bar.volume == 200
    assert trade_bar.transactions == 2
    assert trade_bar.volume_weighted_avg == pytest.approx(129.795)
    started_trade_bar = next(
        module.StockTradeAggregator(
            trade_records,
            interval_seconds=1,
            start_timestamp=dt.time(0, 0, 0, 2),
        )
    )
    assert started_trade_bar.window_start == 2000
    assert started_trade_bar.transactions == 1
    assert started_trade_bar.volume_weighted_avg == pytest.approx(129.80)

    quote_rows = [
        module.StockQuote(
            [
                "A",
                "8",
                "0.0",
                "0",
                "8",
                "0.0",
                "0",
                "1",
                "",
                "1000",
                "322",
                "1000",
                "1",
                "0",
            ]
        ),
        module.StockQuote(
            [
                "A",
                "8",
                "0.0",
                "0",
                "8",
                "0.0",
                "0",
                "1",
                "",
                "2000",
                "323",
                "2000",
                "1",
                "0",
            ]
        ),
    ]
    write_records("stock_quote", "A", quote_rows)
    quote_records = module.StockQuoteDatabase(date, "A", database_path=database)
    assert quote_records[1] == quote_rows[1]
    assert quote_records.index_before_timestamp(1999) == 0
    assert quote_records.index_before_timestamp(dt.time(0, 0, 0, 2), galloping=0) == 1
    assert quote_records.index_after_timestamp(dt.time(0, 0, 0, 2), galloping=0) == 1
    assert list(quote_records.iterate_bounded(1000, 1999)) == [quote_rows[0]]
    assert list(quote_records.iterate_bounded(dt.time(0, 0, 0, 2))) == [quote_rows[1]]
    assert quote_records.find_after_participant_timestamp(1500, 1000) == quote_rows[1]
    assert quote_records.find_before_participant_timestamp(2000, on=False) == quote_rows[0]
    quote_bar = next(module.StockQuoteAggregator(quote_records, interval_seconds=1))
    assert quote_bar.transactions == 2
    assert quote_bar.ask_volume == 0
    assert quote_bar.bid_volume == 0

    timeline = list(
        module.StockTradeQuoteTimeline(
            date_object,
            "A",
            database_path=database,
        )
    )
    assert timeline == [
        (None, quote_rows[0]),
        (trade_rows[0], quote_rows[0]),
        (None, quote_rows[1]),
        (trade_rows[1], quote_rows[1]),
    ]
    assert (
        list(module.stock_trade_quote_timeline(date, "A", database_path=database))
        == timeline
    )

    currency_rows = [
        module.CurrencyQuote(["C:AED-AUD", "48", "0.412", "48", "0.411", "1000"]),
        module.CurrencyQuote(["C:AED-AUD", "48", "0.413", "48", "0.412", "2000"]),
    ]
    write_records("currency_quote", "C:AED-AUD", currency_rows)
    currency_records = module.CurrencyQuoteDatabase(
        date,
        "C:AED-AUD",
        database_path=database,
    )
    assert currency_records[0] == currency_rows[0]
    assert currency_records.index_before_timestamp(1999) == 0
    assert currency_records.index_before_timestamp(dt.time(0, 0, 0, 2), galloping=0) == 1
    assert currency_records.index_after_timestamp(dt.time(0, 0, 0, 2), galloping=0) == 1
    assert list(currency_records.iterate_bounded(1000, 1999)) == [currency_rows[0]]
    assert list(currency_records.iterate_bounded(dt.time(0, 0, 0, 2))) == [currency_rows[1]]
    assert currency_records.find_before_participant_timestamp(1500, 1000) == currency_rows[0]
    assert currency_records.find_after_participant_timestamp(1500, galloping=0) == currency_rows[1]
    currency_bar = next(module.CurrencyQuoteAggregator(currency_records, interval_seconds=1))
    assert currency_bar.transactions == 2
    assert currency_bar.ask_avg == pytest.approx(0.4125)

    class FakeTimestamp:
        def __init__(self, value: int) -> None:
            self.value = value

    class FakeRow:
        def __getitem__(self, key: str) -> FakeTimestamp:
            return FakeTimestamp({"market_open": 123, "market_close": 456}[key])

    class FakeILoc:
        def __getitem__(self, index: int) -> FakeRow:
            assert index == 0
            return FakeRow()

    class FakeSchedule:
        empty = False
        iloc = FakeILoc()

    class FakeCalendar:
        def schedule(self, *, start_date: str, end_date: str) -> FakeSchedule:
            assert start_date == date
            assert end_date == date
            return FakeSchedule()

    fake_pmc = types.ModuleType("pandas_market_calendars")
    fake_pmc.get_calendar = lambda name: FakeCalendar()
    monkeypatch.setitem(sys.modules, "pandas_market_calendars", fake_pmc)

    assert trade_records.market_open == 123
    assert trade_records.market_close == 456
    assert quote_records.market_open == 123


def test_multi_day_database_streams_and_searches_across_dates(tmp_path: Path) -> None:
    import massive_speedup

    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    database = tmp_path / "db"
    epoch = dt.datetime(1970, 1, 1, tzinfo=dt.UTC)

    def timestamp_ns(day: int, hour: int) -> int:
        value = dt.datetime(2026, 7, day, hour, tzinfo=dt.UTC)
        return int((value - epoch).total_seconds()) * 1_000_000_000

    def trade(timestamp: int, identifier: int):
        return module.StockTrade(
            [
                "A",
                "",
                "0",
                "8",
                str(identifier),
                str(timestamp),
                str(100 + identifier),
                str(identifier),
                str(timestamp),
                "1",
                "1",
                "0",
                "0",
            ]
        )

    rows = [
        trade(timestamp_ns(16, 14), 1),
        trade(timestamp_ns(16, 15), 2),
        trade(timestamp_ns(18, 14), 3),
    ]
    for date, daily_rows in (
        ("2026-07-16", rows[:2]),
        ("2026-07-18", rows[2:]),
    ):
        path = database / "stock_trade" / date / "A"
        path.parent.mkdir(parents=True)
        path.write_bytes(b"".join(row.pack() for row in daily_rows))

    other = database / "stock_trade" / "2026-07-17" / "B"
    other.parent.mkdir(parents=True)
    other.write_bytes(rows[0].pack())

    records = massive_speedup.MultiDayDatabase(
        "stock_trade",
        "A",
        database_path=database,
        max_open_days=1,
    )

    assert records.dates == ("2026-07-16", "2026-07-18")
    assert len(records) == 3
    assert records[-1] == rows[2]
    assert list(records) == rows
    assert records.open_dates == ("2026-07-18",)

    between = timestamp_ns(17, 12)
    before = records.locate_before_timestamp(between)
    after = records.locate_after_timestamp(between)
    assert (before.date, before.index, before.record) == ("2026-07-16", 1, rows[1])
    assert (after.date, after.index, after.record) == ("2026-07-18", 0, rows[2])
    assert records.find_before_timestamp(rows[1].sip_timestamp, on=False) == rows[0]
    assert records.find_after_timestamp(rows[1].sip_timestamp, on=False) == rows[2]
    assert list(
        records.iterate_bounded(rows[1].sip_timestamp, rows[2].sip_timestamp)
    ) == rows[1:]

    records.close()
    assert records.open_dates == ()


def test_database_loaders_use_environment_and_reject_positional_path(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    import massive_speedup

    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    database = tmp_path / "db"
    date = "1970-01-01"
    row = module.StockTrade(
        [
            "A",
            "",
            "0",
            "8",
            "1",
            "1000",
            "10",
            "1",
            "1000",
            "1",
            "1",
            "0",
            "0",
        ]
    )
    path = database / "stock_trade" / date / "A"
    path.parent.mkdir(parents=True)
    path.write_bytes(row.pack())
    monkeypatch.setenv("MASSIVE_SPEEDUP_DB_PATH", str(database))

    records = module.StockTradeDatabase(date, "A")
    multi_day = massive_speedup.MultiDayDatabase("stock_trade", "A")

    assert records.database_path == database
    assert list(records) == [row]
    assert list(multi_day) == [row]
    with pytest.raises(TypeError):
        module.StockTradeDatabase(database, date, "A")


def test_multi_day_database_uses_futures_session_date_for_evening_rows(
    tmp_path: Path,
) -> None:
    import massive_speedup

    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    database = tmp_path / "db"
    session_date = "2026-07-20"
    timestamp = 1784586600000000000  # 2026-07-19 22:30:00 UTC
    row = module.FuturesTrade(
        ["ESU6", str(timestamp), "1", "1", "6000", "1", "0", "4", session_date]
    )
    path = database / "future_cme_trade" / session_date / "ESU6"
    path.parent.mkdir(parents=True)
    path.write_bytes(row.pack())
    records = massive_speedup.MultiDayDatabase(
        "future_cme_trade",
        "ESU6",
        database_path=database,
    )

    location = records.locate_after_timestamp(timestamp)

    assert location.date == session_date
    assert location.record == row


def test_simple_market_hides_fills_until_session_summary(tmp_path: Path) -> None:
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    database = tmp_path / "db"
    date = "1970-01-01"

    def write_records(record_type: str, ticker: str, rows) -> None:
        path = database / record_type / date / ticker
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"".join(row.pack() for row in rows))

    def trade(ticker: str, sip_timestamp: int, price: float):
        return module.StockTrade(
            [
                ticker,
                "",
                "0",
                "8",
                str(52983525035849 + sip_timestamp),
                str(sip_timestamp),
                str(price),
                str(sip_timestamp),
                str(sip_timestamp),
                "100",
                "1",
                "0",
                "0",
            ]
        )

    def quote(ticker: str, sip_timestamp: int, bid: float, ask: float):
        return module.StockQuote(
            [
                ticker,
                "8",
                str(ask),
                "10",
                "8",
                str(bid),
                "10",
                "",
                "",
                str(sip_timestamp),
                str(sip_timestamp),
                str(sip_timestamp),
                "1",
                "0",
            ]
        )

    write_records("stock_trade", "A", [trade("A", 1000, 10.0), trade("A", 3000, 12.0)])
    write_records(
        "stock_quote",
        "A",
        [quote("A", 900, 9.0, 11.0), quote("A", 2500, 10.0, 12.0)],
    )
    write_records("stock_trade", "B", [trade("B", 1500, 20.0)])
    write_records(
        "stock_quote",
        "B",
        [quote("B", 1000, 19.0, 21.0), quote("B", 2000, 20.0, 22.0)],
    )

    market = module.SimpleMarket(
        date,
        ["A", "B"],
        1000,
        database_path=database,
        quotes=True,
    )
    (
        first_symbol,
        first_timestamp,
        first_trade,
        first_quote,
        first_trades,
        first_quotes,
        first_broker,
    ) = next(market)
    assert first_symbol == "A"
    assert first_timestamp == pytest.approx(0.0000009)
    assert first_trade is None
    assert first_quote.sip_timestamp == 900
    assert first_trades == {}
    assert first_quotes == {"A": first_quote}
    assert first_broker.symbol == "A"
    assert first_broker.sip_timestamp == 900

    (
        second_symbol,
        second_timestamp,
        second_trade,
        second_quote,
        second_trades,
        second_quotes,
        second_broker,
    ) = next(market)
    assert second_symbol == "A"
    assert second_timestamp == pytest.approx(0.000001)
    assert second_trade.sip_timestamp == 1000
    assert second_quote is None
    assert second_trades == {"A": second_trade}
    assert second_quotes == {"A": first_quote}
    inventory = {"A": 0.0, "B": 0.0}
    assert second_broker.buy(2) is None
    inventory["A"] += 2
    assert second_broker.sell(1, "B") is None
    inventory["B"] -= 1

    assert inventory == {"A": 2.0, "B": -1.0}
    with pytest.raises(RuntimeError, match="iterator is exhausted"):
        market.summary()
    with pytest.raises(TypeError):
        market["A"]
    assert not hasattr(market, "as_dict")

    remaining = list(market)
    assert [
        (
            symbol,
            timestamp,
            trade.sip_timestamp if trade else None,
            quote.sip_timestamp if quote else None,
        )
        for symbol, timestamp, trade, quote, _, _, _ in remaining
    ] == [
        ("B", 0.000001, None, 1000),
        ("B", 0.0000015, 1500, None),
        ("B", 0.000002, None, 2000),
        ("A", 0.0000025, None, 2500),
        ("A", 0.000003, 3000, None),
    ]

    summary = market.summary()
    assert summary == {
        "session_date": date,
        "trade_latency_ns": 1000,
        "order_count": 2,
        "fill_count": 2,
        "rejection_count": 0,
        "cash_flow": pytest.approx(-2.0),
        "orders": [
            {
                "instrument": "A",
                "side": "buy",
                "quantity": 2.0,
                "submitted_timestamp": 1000,
                "execution_timestamp": 2000,
                "status": "filled",
                "quote_timestamp": 900,
                "price": 11.0,
                "notional": 22.0,
            },
            {
                "instrument": "B",
                "side": "sell",
                "quantity": 1.0,
                "submitted_timestamp": 1000,
                "execution_timestamp": 2000,
                "status": "filled",
                "quote_timestamp": 2000,
                "price": 20.0,
                "notional": 20.0,
            },
        ],
    }
    with pytest.raises(RuntimeError, match="session ends"):
        second_broker.buy(1)


def test_simple_market_skips_quote_events_by_default(tmp_path: Path) -> None:
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    database = tmp_path / "db"
    date = "1970-01-01"
    trade_row = module.StockTrade(
        ["A", "", "0", "8", "1", "1000", "10.0", "1", "1000", "100", "1", "0", "0"]
    )
    quote_row = module.StockQuote(
        ["A", "8", "11.0", "10", "8", "9.0", "10", "", "", "900", "1", "900", "1", "0"]
    )
    trade_path = database / "stock_trade" / date / "A"
    quote_path = database / "stock_quote" / date / "A"
    trade_path.parent.mkdir(parents=True, exist_ok=True)
    quote_path.parent.mkdir(parents=True, exist_ok=True)
    trade_path.write_bytes(trade_row.pack())
    quote_path.write_bytes(quote_row.pack())

    market = module.SimpleMarket(
        dt.date(1970, 1, 1),
        ["A"],
        0,
        database_path=database,
    )
    event = next(market)
    assert event[0] == "A"
    assert event[1] == pytest.approx(0.000001)
    assert event[2] == trade_row
    assert event[3] is None
    assert event[4] == {"A": trade_row}
    assert event[5] == {"A": quote_row}
    with pytest.raises(StopIteration):
        next(market)


def test_simple_market_defaults_to_150ms_execution_latency(tmp_path: Path) -> None:
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    database = tmp_path / "db"
    date = "1970-01-01"
    trade_timestamp = 1_000_000_000
    execution_timestamp = trade_timestamp + 150_000_000
    trade = module.StockTrade(
        [
            "A",
            "",
            "0",
            "8",
            "1",
            str(trade_timestamp),
            "10",
            "1",
            str(trade_timestamp),
            "1",
            "1",
            "0",
            "0",
        ]
    )

    def quote(timestamp: int, bid: float, ask: float):
        return module.StockQuote(
            [
                "A",
                "8",
                str(ask),
                "1",
                "8",
                str(bid),
                "1",
                "",
                "",
                str(timestamp),
                str(timestamp),
                str(timestamp),
                "1",
                "0",
            ]
        )

    quotes = [
        quote(900_000_000, 9.0, 11.0),
        quote(execution_timestamp, 10.0, 12.0),
    ]
    trade_path = database / "stock_trade" / date / "A"
    quote_path = database / "stock_quote" / date / "A"
    trade_path.parent.mkdir(parents=True)
    quote_path.parent.mkdir(parents=True)
    trade_path.write_bytes(trade.pack())
    quote_path.write_bytes(b"".join(row.pack() for row in quotes))

    market = module.SimpleMarket(date, ["A"], database_path=database)
    event = next(market)
    assert event[2] == trade
    assert event[6].buy(1) is None
    with pytest.raises(RuntimeError, match="iterator is exhausted"):
        market.summary()

    assert list(market) == []
    summary = market.summary()
    assert summary["trade_latency_ns"] == 150_000_000
    assert summary["orders"][0]["execution_timestamp"] == execution_timestamp
    assert summary["orders"][0]["quote_timestamp"] == execution_timestamp
    assert summary["orders"][0]["price"] == 12.0


def test_simple_market_hides_execution_rejection_until_summary(tmp_path: Path) -> None:
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    database = tmp_path / "db"
    date = "1970-01-01"
    trade = module.StockTrade(
        ["A", "", "0", "8", "1", "1000", "10", "1", "1000", "1", "1", "0", "0"]
    )
    quote = module.StockQuote(
        ["A", "8", "11", "1", "8", "9", "1", "", "", "2000", "1", "2000", "1", "0"]
    )
    trade_path = database / "stock_trade" / date / "A"
    quote_path = database / "stock_quote" / date / "A"
    trade_path.parent.mkdir(parents=True)
    quote_path.parent.mkdir(parents=True)
    trade_path.write_bytes(trade.pack())
    quote_path.write_bytes(quote.pack())

    market = module.SimpleMarket(date, ["A"], 0, database_path=database)
    broker = next(market)[6]
    assert broker.buy(1) is None
    with pytest.raises(RuntimeError, match="iterator is exhausted"):
        market.summary()

    assert list(market) == []
    order = market.summary()["orders"][0]
    assert order["status"] == "rejected"
    assert order["reason"] == "no_quote"
    assert "price" not in order


def test_simple_market_fast_reuses_recent_record_dicts(tmp_path: Path) -> None:
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    database = tmp_path / "db"
    date = "1970-01-01"
    trade_rows = [
        module.StockTrade(
            ["A", "", "0", "8", "1", "1000", "10.0", "1", "1000", "100", "1", "0", "0"]
        ),
        module.StockTrade(
            ["A", "", "0", "8", "2", "2000", "11.0", "2", "2000", "100", "1", "0", "0"]
        ),
    ]
    quote_row = module.StockQuote(
        ["A", "8", "11.0", "10", "8", "9.0", "10", "", "", "900", "1", "900", "1", "0"]
    )
    trade_path = database / "stock_trade" / date / "A"
    quote_path = database / "stock_quote" / date / "A"
    trade_path.parent.mkdir(parents=True, exist_ok=True)
    quote_path.parent.mkdir(parents=True, exist_ok=True)
    trade_path.write_bytes(b"".join(row.pack() for row in trade_rows))
    quote_path.write_bytes(quote_row.pack())

    slow_market = module.SimpleMarket(
        date,
        ["A"],
        0,
        database_path=database,
        fast=False,
    )
    slow_first = next(slow_market)
    slow_second = next(slow_market)
    assert slow_first[4] is not slow_second[4]
    assert slow_first[5] is not slow_second[5]

    fast_market = module.SimpleMarket(
        date,
        ["A"],
        0,
        database_path=database,
        fast=True,
    )
    fast_first = next(fast_market)
    fast_second = next(fast_market)
    assert fast_first[4] is fast_second[4]
    assert fast_first[5] is fast_second[5]
    assert fast_first[4]["A"] == trade_rows[1]


def test_option_market_hides_fills_until_session_summary(tmp_path: Path) -> None:
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    database = tmp_path / "db"
    date = "1970-01-01"
    root = "A"
    expiration = "2026-06-18"
    right = "C"
    strike = 115.0
    contract_path = Path(root) / expiration / right / "00115000"
    trade_path = database / "option_trade" / date / contract_path
    quote_path = database / "option_quote" / date / contract_path
    trade_path.parent.mkdir(parents=True, exist_ok=True)
    quote_path.parent.mkdir(parents=True, exist_ok=True)

    trade_rows = [
        module.OptionTrade(
            ["O:A260618C00115000", "209", "0", "320", "10.7", "1000", "2"]
        ),
        module.OptionTrade(
            ["O:A260618C00115000", "227", "0", "308", "11.2", "3000", "1"]
        ),
    ]
    quote_rows = [
        module.OptionQuote(
            ["O:A260618C00115000", "320", "11.0", "1", "320", "10.5", "1", "1", "900"]
        ),
        module.OptionQuote(
            ["O:A260618C00115000", "320", "12.0", "1", "320", "11.0", "1", "2", "2500"]
        ),
    ]
    trade_path.write_bytes(b"".join(row.pack() for row in trade_rows))
    quote_path.write_bytes(b"".join(row.pack() for row in quote_rows))

    market = module.OptionMarket(
        date,
        root,
        expiration,
        right,
        strike,
        0,
        database_path=database,
        quotes=True,
    )

    first = next(market)
    assert first[0] == (root, expiration, right, strike)
    assert first[1] == pytest.approx(0.0000009)
    assert first[2] is None
    assert first[3] == quote_rows[0]
    assert first[5] == {(root, expiration, right, strike): quote_rows[0]}
    assert first[6].contract == (root, expiration, right, strike)
    assert first[6].sip_timestamp == 900

    second = next(market)
    assert second[2] == trade_rows[0]
    assert second[3] is None
    inventory = 0.0
    assert second[6].buy(2) is None
    inventory += 2
    assert second[6].sell(1) is None
    inventory -= 1

    assert inventory == 1.0
    with pytest.raises(RuntimeError, match="iterator is exhausted"):
        market.summary()
    with pytest.raises(TypeError):
        market[(root, expiration, right, strike)]

    remaining = list(market)
    assert [
        (
            timestamp,
            trade.sip_timestamp if trade else None,
            quote.sip_timestamp if quote else None,
        )
        for _, timestamp, trade, quote, _, _, _ in remaining
    ] == [
        (0.0000025, None, 2500),
        (0.000003, 3000, None),
    ]

    summary = market.summary()
    assert summary["session_date"] == date
    assert summary["trade_latency_ns"] == 0
    assert summary["order_count"] == 2
    assert summary["fill_count"] == 2
    assert summary["rejection_count"] == 0
    assert summary["cash_flow"] == pytest.approx(-11.5)
    assert [order["price"] for order in summary["orders"]] == [11.0, 10.5]
    assert {order["instrument"] for order in summary["orders"]} == {
        "A/2026-06-18/C/00115000"
    }


def test_futures_market_hides_fills_until_session_summary(tmp_path: Path) -> None:
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    database = tmp_path / "db"
    date = "2026-05-21"

    def write_records(record_type: str, ticker: str, rows) -> None:
        path = database / record_type / date / ticker
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"".join(row.pack() for row in rows))

    def trade(ticker: str, timestamp: int, price: float):
        return module.FuturesTrade(
            [
                ticker,
                str(timestamp),
                str(timestamp),
                "1",
                str(price),
                "1",
                "0",
                "4",
                date,
            ]
        )

    def quote(ticker: str, timestamp: int, bid: float, ask: float):
        return module.FuturesQuote(
            [
                ticker,
                str(timestamp),
                str(timestamp),
                "1",
                str(timestamp),
                str(ask),
                "1",
                str(timestamp),
                str(bid),
                "1",
                "4",
                date,
            ]
        )

    write_records(
        "future_cme_trade",
        "0BTZ9",
        [trade("0BTZ9", 1000, 10.0), trade("0BTZ9", 3000, 12.0)],
    )
    write_records(
        "future_cme_quote",
        "0BTZ9",
        [quote("0BTZ9", 900, 9.0, 11.0), quote("0BTZ9", 2500, 10.0, 12.0)],
    )
    write_records("future_cme_trade", "1BTZ9", [trade("1BTZ9", 1500, 20.0)])
    write_records(
        "future_cme_quote",
        "1BTZ9",
        [quote("1BTZ9", 1000, 19.0, 21.0), quote("1BTZ9", 2000, 20.0, 22.0)],
    )

    market = module.FuturesMarket(
        date,
        ["0BTZ9", "1BTZ9"],
        1000,
        exchange="cme",
        database_path=database,
        quotes=True,
    )
    (
        first_symbol,
        first_timestamp,
        first_trade,
        first_quote,
        first_trades,
        first_quotes,
        first_broker,
    ) = next(market)
    assert first_symbol == "0BTZ9"
    assert first_timestamp == pytest.approx(0.0000009)
    assert first_trade is None
    assert first_quote.timestamp == 900
    assert first_trades == {}
    assert first_quotes == {"0BTZ9": first_quote}
    assert first_broker.symbol == "0BTZ9"
    assert first_broker.timestamp == 900
    assert first_broker.sip_timestamp == 900

    (
        second_symbol,
        second_timestamp,
        second_trade,
        second_quote,
        second_trades,
        second_quotes,
        second_broker,
    ) = next(market)
    assert second_symbol == "0BTZ9"
    assert second_timestamp == pytest.approx(0.000001)
    assert second_trade.timestamp == 1000
    assert second_quote is None
    assert second_trades == {"0BTZ9": second_trade}
    assert second_quotes == {"0BTZ9": first_quote}
    inventory = {"0BTZ9": 0.0, "1BTZ9": 0.0}
    assert second_broker.buy(2) is None
    inventory["0BTZ9"] += 2
    assert second_broker.sell(1, "1BTZ9") is None
    inventory["1BTZ9"] -= 1

    assert inventory == {"0BTZ9": 2.0, "1BTZ9": -1.0}
    with pytest.raises(RuntimeError, match="iterator is exhausted"):
        market.summary()
    with pytest.raises(TypeError):
        market["0BTZ9"]

    remaining = list(market)
    assert [
        (
            symbol,
            timestamp,
            trade.timestamp if trade else None,
            quote.timestamp if quote else None,
        )
        for symbol, timestamp, trade, quote, _, _, _ in remaining
    ] == [
        ("1BTZ9", 0.000001, None, 1000),
        ("1BTZ9", 0.0000015, 1500, None),
        ("1BTZ9", 0.000002, None, 2000),
        ("0BTZ9", 0.0000025, None, 2500),
        ("0BTZ9", 0.000003, 3000, None),
    ]

    summary = market.summary()
    assert summary["session_date"] == date
    assert summary["trade_latency_ns"] == 1000
    assert summary["order_count"] == 2
    assert summary["fill_count"] == 2
    assert summary["rejection_count"] == 0
    assert summary["cash_flow"] == pytest.approx(-2.0)
    assert [order["instrument"] for order in summary["orders"]] == [
        "0BTZ9",
        "1BTZ9",
    ]
    assert [order["price"] for order in summary["orders"]] == [11.0, 20.0]


def test_futures_market_skips_quote_events_by_default(tmp_path: Path) -> None:
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    database = tmp_path / "db"
    date = "2026-05-21"
    trade_row = module.FuturesTrade(
        ["0BTZ9", "1000", "1", "1", "10.0", "1", "0", "4", date]
    )
    quote_row = module.FuturesQuote(
        [
            "0BTZ9",
            "900",
            "1",
            "1",
            "900",
            "11.0",
            "1",
            "900",
            "9.0",
            "1",
            "4",
            date,
        ]
    )
    trade_path = database / "future_cme_trade" / date / "0BTZ9"
    quote_path = database / "future_cme_quote" / date / "0BTZ9"
    trade_path.parent.mkdir(parents=True, exist_ok=True)
    quote_path.parent.mkdir(parents=True, exist_ok=True)
    trade_path.write_bytes(trade_row.pack())
    quote_path.write_bytes(quote_row.pack())

    market = module.FuturesMarket(
        dt.date(2026, 5, 21),
        ["0BTZ9"],
        0,
        exchange="cme",
        database_path=database,
    )
    event = next(market)
    assert event[0] == "0BTZ9"
    assert event[1] == pytest.approx(0.000001)
    assert event[2] == trade_row
    assert event[3] is None
    assert event[4] == {"0BTZ9": trade_row}
    assert event[5] == {"0BTZ9": quote_row}
    with pytest.raises(StopIteration):
        next(market)


def test_futures_market_fast_reuses_recent_record_dicts(tmp_path: Path) -> None:
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    database = tmp_path / "db"
    date = "2026-05-21"
    trade_rows = [
        module.FuturesTrade(["0BTZ9", "1000", "1", "1", "10.0", "1", "0", "4", date]),
        module.FuturesTrade(["0BTZ9", "2000", "2", "2", "11.0", "1", "0", "4", date]),
    ]
    quote_row = module.FuturesQuote(
        [
            "0BTZ9",
            "900",
            "1",
            "1",
            "900",
            "11.0",
            "1",
            "900",
            "9.0",
            "1",
            "4",
            date,
        ]
    )
    trade_path = database / "future_cme_trade" / date / "0BTZ9"
    quote_path = database / "future_cme_quote" / date / "0BTZ9"
    trade_path.parent.mkdir(parents=True, exist_ok=True)
    quote_path.parent.mkdir(parents=True, exist_ok=True)
    trade_path.write_bytes(b"".join(row.pack() for row in trade_rows))
    quote_path.write_bytes(quote_row.pack())

    slow_market = module.FuturesMarket(
        date,
        ["0BTZ9"],
        0,
        exchange="cme",
        database_path=database,
        fast=False,
    )
    slow_first = next(slow_market)
    slow_second = next(slow_market)
    assert slow_first[4] is not slow_second[4]
    assert slow_first[5] is not slow_second[5]

    fast_market = module.FuturesMarket(
        date,
        ["0BTZ9"],
        0,
        exchange="cme",
        database_path=database,
        fast=True,
    )
    fast_first = next(fast_market)
    fast_second = next(fast_market)
    assert fast_first[4] is fast_second[4]
    assert fast_first[5] is fast_second[5]
    assert fast_first[4]["0BTZ9"] == trade_rows[1]


def test_parsed_rows_use_instance_lifetime_bitset_cache() -> None:
    repo_root = Path(__file__).resolve().parents[1]
    native_source = (repo_root / "src/cpp/native.hpp").read_text(encoding="utf-8")

    assert "class BitsetParseCache" in native_source
    assert "detail::BitsetParseCache<96> bitset_cache_;" in native_source
    assert "row = Implementation::parse_trade_row(line, bitset_cache_);" in native_source
    assert "row = Implementation::parse_quote_row(line, bitset_cache_);" in native_source
    assert "result.conditions = bitset_cache.get_or_parse(" in native_source
    assert "result.indicators = bitset_cache.get_or_parse(" in native_source


def test_download_stops_retrying_permission_errors(monkeypatch) -> None:
    from massive_speedup import download

    class FakeClientError(Exception):
        def __init__(self, code: str) -> None:
            super().__init__(code)
            self.response = {"Error": {"Code": code}}

    class FakeClient:
        def __init__(self) -> None:
            self.attempts = 0

        def download_file(self, bucket: str, key: str, destination: str) -> None:
            assert bucket == download.BUCKET
            assert key == "some/key.csv.gz"
            assert destination == "/tmp/key.csv.gz"
            self.attempts += 1
            raise FakeClientError("403")

    client = FakeClient()
    sleeps: list[int] = []
    messages: list[str] = []

    monkeypatch.setattr(download, "_get_client", lambda: client)
    monkeypatch.setattr(download, "_is_entitled_to_download", lambda key: True)
    monkeypatch.setattr(download.time, "sleep", sleeps.append)
    monkeypatch.setattr(download.tqdm, "write", messages.append)

    assert download._download_one(("/tmp/key.csv.gz", "some/key.csv.gz")) is None
    assert sleeps == [1, 1, 2, 3, 5, 8, 13, 21, 34, 55]
    assert client.attempts == 11
    assert messages[-1] == "Skipping unauthorized flatfile some/key.csv.gz"


def test_download_retries_endpoint_connection_errors(monkeypatch) -> None:
    from massive_speedup import download

    EndpointConnectionError = type("EndpointConnectionError", (Exception,), {})

    class FakeClient:
        def __init__(self) -> None:
            self.attempts = 0

        def download_file(self, bucket: str, key: str, destination: str) -> None:
            self.attempts += 1
            if self.attempts <= 3:
                raise EndpointConnectionError(
                    'Could not connect to the endpoint URL: "https://files.massive.com/flatfiles/key"'
                )

    client = FakeClient()
    sleeps: list[int] = []
    messages: list[str] = []

    monkeypatch.setattr(download, "_get_client", lambda: client)
    monkeypatch.setattr(download, "_is_entitled_to_download", lambda key: True)
    monkeypatch.setattr(download.time, "sleep", sleeps.append)
    monkeypatch.setattr(download.tqdm, "write", messages.append)

    assert download._download_one(("/tmp/key.csv.gz", "some/key.csv.gz")) == "some/key.csv.gz"
    assert sleeps == [1, 1, 2]
    assert client.attempts == 4
    assert messages[-2] == (
        "endpoint connection error downloading some/key.csv.gz; retrying in 2 seconds"
    )
    assert messages[-1] == "Downloaded some/key.csv.gz -> /tmp/key.csv.gz"


def test_download_does_not_retry_non_403_errors(monkeypatch) -> None:
    from massive_speedup import download

    class FakeClient:
        def download_file(self, bucket: str, key: str, destination: str) -> None:
            raise RuntimeError("boom")

    sleeps: list[int] = []

    monkeypatch.setattr(download, "_get_client", lambda: FakeClient())
    monkeypatch.setattr(download, "_is_entitled_to_download", lambda key: True)
    monkeypatch.setattr(download.time, "sleep", sleeps.append)

    with pytest.raises(RuntimeError, match="boom"):
        download._download_one(("/tmp/key.csv.gz", "some/key.csv.gz"))
    assert sleeps == []


def test_download_checks_entitlement_immediately_before_s3_download(monkeypatch) -> None:
    from massive_speedup import download

    messages: list[str] = []

    class FakeClient:
        def download_file(self, bucket: str, key: str, destination: str) -> None:
            raise AssertionError("S3 download should not run for unauthorized files")

    monkeypatch.setattr(download, "_get_client", lambda: FakeClient())
    monkeypatch.setattr(download, "_is_entitled_to_download", lambda key: False)
    monkeypatch.setattr(download.tqdm, "write", messages.append)

    assert download._download_one(("/tmp/blocked.csv.gz", "some/key.csv.gz")) is None


def test_download_main_defers_entitlement_checks_to_download_loop(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    from massive_speedup import download

    day = dt.date.today() - dt.timedelta(days=1)
    stem = f"{day.year}-{day.month:02d}-{day.day:02d}.csv.gz"
    trade_key = f"global_crypto/trades_v1/{day.year}/{day.month:02d}/{stem}"
    downloaded_jobs: list[tuple[str, str]] = []
    titles: list[str] = []

    monkeypatch.setattr(download, "set_process_title", titles.append)
    monkeypatch.setattr(download, "scan_keys", lambda products: {trade_key})
    monkeypatch.setattr(download, "_is_entitled_to_download", lambda key: True)
    monkeypatch.setattr(download, "_download_one", lambda job: downloaded_jobs.append(job))
    monkeypatch.setattr(download.tqdm, "write", lambda message: None)

    result = download.main(
        [
            "--download-path",
            str(tmp_path / "downloads"),
            "--products",
            "cryptos",
            "--aws-access-key-id",
            "id",
            "--aws-secret-access-key",
            "secret",
            "--massive-api-key",
            "api-key",
            "--end-date",
            day.isoformat(),
        ]
    )

    assert result == 0
    assert titles == ["massive-dl"]
    assert downloaded_jobs == [
        (
            str((tmp_path / "downloads" / "crypto_trade" / stem).resolve()),
            trade_key,
        )
    ]


def test_download_entitlement_preflight_uses_massive_api(monkeypatch) -> None:
    from massive_speedup import download

    captured_urls: list[str] = []

    def fake_massive_api_allows(url: str) -> bool:
        captured_urls.append(url)
        return True

    monkeypatch.setattr(download, "_MASSIVE_API_KEY", "api-key")
    monkeypatch.setattr(download, "_massive_api_allows", fake_massive_api_allows)

    assert download._is_entitled_to_download(
        "us_stocks_sip/trades_v1/2026/01/2026-01-23.csv.gz"
    )
    assert captured_urls == [
        "https://api.massive.com/v3/trades/AAPL?"
        "timestamp=2026-01-23&order=asc&sort=timestamp&limit=1&apiKey=api-key"
    ]


def test_download_crypto_entitlement_preflight_uses_massive_api(monkeypatch) -> None:
    from massive_speedup import download

    captured_urls: list[str] = []

    def fake_massive_api_allows(url: str) -> bool:
        captured_urls.append(url)
        return True

    monkeypatch.setattr(download, "_MASSIVE_API_KEY", "api-key")
    monkeypatch.setattr(download, "_massive_api_allows", fake_massive_api_allows)

    assert download._is_entitled_to_download(
        "global_crypto/trades_v1/2026/01/2026-01-23.csv.gz"
    )
    assert captured_urls == [
        "https://api.massive.com/v3/trades/X:BTCUSD?"
        "timestamp=2026-01-23&order=asc&sort=timestamp&limit=1&apiKey=api-key",
    ]


@pytest.mark.parametrize(
    ("key", "expected_url"),
    [
        (
            "us_options_opra/trades_v1/2026/01/2026-01-23.csv.gz",
            "https://api.massive.com/v3/trades/O:SPY260116C00600000?"
            "timestamp=2026-01-23&order=asc&sort=timestamp&limit=1&apiKey=api-key",
        ),
        (
            "us_options_opra/quotes_v1/2026/01/2026-01-23.csv.gz",
            "https://api.massive.com/v3/quotes/O:SPY260116C00600000?"
            "timestamp=2026-01-23&order=asc&sort=timestamp&limit=1&apiKey=api-key",
        ),
    ],
)
def test_download_options_entitlement_preflight_uses_massive_api(
    monkeypatch,
    key: str,
    expected_url: str,
) -> None:
    from massive_speedup import download

    captured_urls: list[str] = []

    def fake_massive_api_allows(url: str) -> bool:
        captured_urls.append(url)
        return True

    monkeypatch.setattr(download, "_MASSIVE_API_KEY", "api-key")
    monkeypatch.setattr(download, "_massive_api_allows", fake_massive_api_allows)

    assert download._is_entitled_to_download(key)
    assert captured_urls == [expected_url]


def test_download_indices_entitlement_preflight_uses_massive_api(monkeypatch) -> None:
    from massive_speedup import download

    captured_urls: list[str] = []
    monkeypatch.setattr(download, "_MASSIVE_API_KEY", "api-key")
    monkeypatch.setattr(
        download,
        "_massive_api_allows",
        lambda url: captured_urls.append(url) or True,
    )

    assert download._is_entitled_to_download(
        "us_indices/values_v1/2026/07/2026-07-17.csv.gz"
    )
    assert captured_urls == [
        "https://api.massive.com/v3/snapshot/indices?"
        "ticker=I%3ASPX&order=asc&sort=ticker&limit=1&apiKey=api-key"
    ]


def test_download_product_selection_and_futures_jobs(tmp_path: Path) -> None:
    from massive_speedup import download

    day = dt.date.today() - dt.timedelta(days=1)
    stem = f"{day.year}-{day.month:02d}-{day.day:02d}.csv.gz"
    keys = {
        f"us_stocks_sip/trades_v1/{day.year}/{day.month:02d}/{stem}",
        f"global_forex/quotes_v1/{day.year}/{day.month:02d}/{stem}",
        f"us_futures_cme/trades_v1/{day.year}/{day.month:02d}/{stem}",
        f"us_futures_cme/quotes_v1/{day.year}/{day.month:02d}/{stem}",
    }

    jobs = download.build_download_jobs(
        keys,
        download_path=tmp_path,
        end_date=day,
        products=("futures",),
    )

    assert jobs == [
        (
            str((tmp_path / "future_cme_trade" / stem).resolve()),
            f"us_futures_cme/trades_v1/{day.year}/{day.month:02d}/{stem}",
        ),
        (
            str((tmp_path / "future_cme_quote" / stem).resolve()),
            f"us_futures_cme/quotes_v1/{day.year}/{day.month:02d}/{stem}",
        ),
    ]


def test_download_product_selection_and_crypto_jobs(tmp_path: Path) -> None:
    from massive_speedup import download

    day = dt.date.today() - dt.timedelta(days=1)
    stem = f"{day.year}-{day.month:02d}-{day.day:02d}.csv.gz"
    keys = {
        f"us_stocks_sip/trades_v1/{day.year}/{day.month:02d}/{stem}",
        f"global_crypto/trades_v1/{day.year}/{day.month:02d}/{stem}",
    }

    jobs = download.build_download_jobs(
        keys,
        download_path=tmp_path,
        end_date=day,
        products=("crypto",),
    )

    assert jobs == [
        (
            str((tmp_path / "crypto_trade" / stem).resolve()),
            f"global_crypto/trades_v1/{day.year}/{day.month:02d}/{stem}",
        )
    ]


def test_download_product_selection_and_options_jobs(tmp_path: Path) -> None:
    from massive_speedup import download

    day = dt.date.today() - dt.timedelta(days=1)
    stem = f"{day.year}-{day.month:02d}-{day.day:02d}.csv.gz"
    keys = {
        f"us_stocks_sip/trades_v1/{day.year}/{day.month:02d}/{stem}",
        f"us_options_opra/trades_v1/{day.year}/{day.month:02d}/{stem}",
        f"us_options_opra/quotes_v1/{day.year}/{day.month:02d}/{stem}",
    }

    jobs = download.build_download_jobs(
        keys,
        download_path=tmp_path,
        end_date=day,
        products=("options",),
    )

    assert jobs == [
        (
            str((tmp_path / "option_trade" / stem).resolve()),
            f"us_options_opra/trades_v1/{day.year}/{day.month:02d}/{stem}",
        ),
        (
            str((tmp_path / "option_quote" / stem).resolve()),
            f"us_options_opra/quotes_v1/{day.year}/{day.month:02d}/{stem}",
        ),
    ]


def test_download_product_selection_uses_configured_download_root(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    from massive_speedup import download

    day = dt.date.today() - dt.timedelta(days=1)
    stem = f"{day.year}-{day.month:02d}-{day.day:02d}.csv.gz"
    key = f"us_indices/values_v1/{day.year}/{day.month:02d}/{stem}"
    monkeypatch.setenv("MASSIVE_SPEEDUP_DOWNLOAD_PATH", str(tmp_path))

    jobs = download.build_download_jobs(
        {key},
        end_date=day,
        products=("indices",),
    )

    assert jobs == [
        (str((tmp_path / "index_value" / stem).resolve()), key),
    ]


def test_download_parser_accepts_crypto_product_aliases() -> None:
    from massive_speedup import download

    parser = download.build_parser()
    args = parser.parse_args(
        [
            "--download-path",
            "downloads",
            "--products",
            "cryptos,stocks",
            "--aws-access-key-id",
            "id",
            "--aws-secret-access-key",
            "secret",
            "--massive-api-key",
            "api-key",
        ]
    )

    assert download._normalize_products(args.products) == ("crypto", "stocks")


def test_download_parser_accepts_options_product_aliases() -> None:
    from massive_speedup import download

    parser = download.build_parser()
    args = parser.parse_args(
        [
            "--download-path",
            "downloads",
            "--products",
            "options,option",
            "--aws-access-key-id",
            "id",
            "--aws-secret-access-key",
            "secret",
            "--massive-api-key",
            "api-key",
        ]
    )

    assert download._normalize_products(args.products) == ("options",)


def test_download_parser_accepts_indices_product_aliases() -> None:
    from massive_speedup import download

    parser = download.build_parser()
    args = parser.parse_args(
        [
            "--download-path",
            "downloads",
            "--products",
            "index,indices",
            "--aws-access-key-id",
            "id",
            "--aws-secret-access-key",
            "secret",
            "--massive-api-key",
            "api-key",
        ]
    )

    assert download._normalize_products(args.products) == ("indices",)


def test_download_parser_accepts_iso_end_date() -> None:
    from massive_speedup import download

    parser = download.build_parser()
    args = parser.parse_args(
        [
            "--download-path",
            "downloads",
            "--aws-access-key-id",
            "id",
            "--aws-secret-access-key",
            "secret",
            "--massive-api-key",
            "api-key",
            "--end-date",
            "2026-06-17",
        ]
    )

    assert args.end_date == dt.date(2026, 6, 17)


def test_download_parser_accepts_english_end_date(monkeypatch: pytest.MonkeyPatch) -> None:
    from massive_speedup import download

    calls: list[tuple[str, dict[str, object]]] = []

    def parse(value: str, *, settings: dict[str, object]) -> dt.datetime:
        calls.append((value, settings))
        return dt.datetime(2026, 6, 24, 14, 30)

    monkeypatch.setitem(sys.modules, "dateparser", types.SimpleNamespace(parse=parse))

    parser = download.build_parser()
    args = parser.parse_args(
        [
            "--download-path",
            "downloads",
            "--aws-access-key-id",
            "id",
            "--aws-secret-access-key",
            "secret",
            "--massive-api-key",
            "api-key",
            "--end-date",
            "three days ago",
        ]
    )

    assert args.end_date == dt.date(2026, 6, 24)
    assert calls == [
        (
            "three days ago",
            {
                "PREFER_DATES_FROM": "past",
                "RETURN_AS_TIMEZONE_AWARE": False,
            },
        )
    ]


def test_futures_trade_record_packs_without_ticker() -> None:
    import massive_speedup

    trade = massive_speedup.FuturesTrade(
        [
            "0BTZ9",
            "1779368715688046681",
            "104906685",
            "259",
            "100.000000000",
            "1",
            "0",
            "4",
            "2026-05-21",
        ]
    )

    assert trade.ticker == "0BTZ9"
    assert trade.timestamp == 1779368715688046681
    assert trade.sequence_number == 104906685
    assert trade.report_sequence == 259
    assert trade.price == 100.0
    assert trade.size == 1
    assert trade.correction == 0
    assert trade.exchange == 4
    assert trade.session_end_date == "2026-05-21"

    packed = trade.pack()
    assert len(packed) == massive_speedup.FuturesTrade.packed_size == 42
    assert b"0BTZ9" not in packed

    restored = massive_speedup.FuturesTrade.from_packed(packed, "0BTZ9")
    assert restored == trade
    assert massive_speedup.FuturesTrade.timestamp_from_packed(packed) == trade.timestamp


def test_futures_quote_record_packs_without_ticker_and_handles_empty_price() -> None:
    import massive_speedup

    quote = massive_speedup.FuturesQuote(
        [
            "0BTZ9",
            "1779368715560670607",
            "104906684",
            "257",
            "1779368715560670607",
            "100.000000000",
            "1",
            "1779047085970577269",
            "",
            "0",
            "4",
            "2026-05-21",
        ]
    )

    assert quote.ticker == "0BTZ9"
    assert quote.timestamp == 1779368715560670607
    assert quote.sequence_number == 104906684
    assert quote.report_sequence == 257
    assert quote.ask_timestamp == 1779368715560670607
    assert quote.ask_price == 100.0
    assert quote.ask_size == 1
    assert quote.bid_timestamp == 1779047085970577269
    assert math.isnan(quote.bid_price)
    assert quote.bid_size == 0
    assert quote.exchange == 4
    assert quote.session_end_date == "2026-05-21"

    packed = quote.pack()
    assert len(packed) == massive_speedup.FuturesQuote.packed_size == 66
    assert b"0BTZ9" not in packed

    restored = massive_speedup.FuturesQuote.from_packed(packed, "0BTZ9")
    assert restored == quote
    assert massive_speedup.FuturesQuote.timestamp_from_packed(packed) == quote.timestamp


def test_flatfiles_futures_parse_trade_and_quote_rows(tmp_path: Path) -> None:
    import massive_speedup

    trade_path = tmp_path / "futures_trades.csv.gz"
    with gzip.open(trade_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,timestamp,sequence_number,report_sequence,price,size,"
            "correction,exchange,session_end_date\n"
        )
        handle.write(
            "0BTZ9,1779368715688046681,104906685,259,100.000000000,1,0,4,2026-05-21\n"
        )

    quote_path = tmp_path / "futures_quotes.csv.gz"
    with gzip.open(quote_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,timestamp,sequence_number,report_sequence,ask_timestamp,"
            "ask_price,ask_size,bid_timestamp,bid_price,bid_size,exchange,"
            "session_end_date\n"
        )
        handle.write(
            "0BTZ9,1779368715560670607,104906684,257,"
            "1779368715560670607,100.000000000,1,"
            "1779047085970577269,,0,4,2026-05-21\n"
        )

    trade = next(iter(massive_speedup.FlatFiles.Futures.Trade.parse(trade_path)))
    quote = next(iter(massive_speedup.FlatFiles.Futures.Quote.parse(quote_path)))

    assert isinstance(trade, massive_speedup.FuturesTrade)
    assert trade.ticker == "0BTZ9"
    assert trade.timestamp == 1779368715688046681
    assert trade.session_end_date == "2026-05-21"
    assert isinstance(quote, massive_speedup.FuturesQuote)
    assert quote.ticker == "0BTZ9"
    assert quote.session_end_date == "2026-05-21"
    assert math.isnan(quote.bid_price)


def test_flatfiles_options_parse_trade_rows(tmp_path: Path) -> None:
    path = tmp_path / "options_trades.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write("ticker,conditions,correction,exchange,price,sip_timestamp,size\n")
        handle.write("O:A260618C00100000,227,0,308,26.57,1781794876313000000,1\n")
        handle.write("O:A260618C00100000,209,0,325,26.68,1781794876777000000,1\n")
        handle.write(
            'O:A260618C00115000,"209,227",0,320,10.7,1781791123025000000,2\n'
        )
        handle.write(
            "O:MSFT260618P00250000,227,0,308,12.25,1781790000000000000,3\n"
        )

    trades = list(FlatFiles.Options.Trade.parse(path))
    raw = list(FlatFiles.Options.Trade.parse_raw(path))
    sorted_trades = list(
        FlatFiles.Options.Trade.parse(
            path,
            sort_by_sip_timestamp=True,
        )
    )

    assert len(trades) == 4
    assert isinstance(trades[0], OptionTrade)
    assert trades[0].root == "A"
    assert trades[0].expiration == "2026-06-18"
    assert trades[0].right == "C"
    assert trades[0].strike == 115.0
    assert trades[0].conditions == frozenset({209, 227})
    assert trades[0].correction == 0
    assert trades[0].exchange == 320
    assert trades[0].price == 10.7
    assert trades[0].sip_timestamp == 1781791123025000000
    assert trades[0].size == 2
    assert trades[1].conditions == frozenset({227})
    assert trades[3].root == "MSFT"
    assert trades[3].right == "P"
    assert trades[3].strike == 250.0
    assert list(trades[0]) == [
        "A",
        "2026-06-18",
        "C",
        115.0,
        frozenset({209, 227}),
        0,
        320,
        10.7,
        1781791123025000000,
        2,
    ]
    assert raw[2] == (
        b"O:A260618C00115000",
        b"209,227",
        b"0",
        b"320",
        b"10.7",
        b"1781791123025000000",
        b"2",
    )
    assert [trade.root for trade in trades] == ["A", "A", "A", "MSFT"]
    assert [trade.sip_timestamp for trade in sorted_trades] == [
        1781791123025000000,
        1781794876313000000,
        1781794876777000000,
        1781790000000000000,
    ]
    assert "OptionTrade(" in repr(trades[0])


def test_flatfiles_options_parse_quote_rows(tmp_path: Path) -> None:
    path = tmp_path / "options_quotes.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,ask_exchange,ask_price,ask_size,bid_exchange,bid_price,"
            "bid_size,sequence_number,sip_timestamp\n"
        )
        handle.write(
            "O:A260717C00060000,0,0,0,320,57.7,1,1149543,1781789401689374260\n"
        )
        handle.write(
            "O:A260717C00060000,320,72.7,1,320,57.7,1,1149544,1781789401689374260\n"
        )
        handle.write(
            "O:A260717C00065000,300,67.7,2,300,62.7,2,1152168,1781789401696866913\n"
        )
        handle.write(
            "O:MSFT260717P00250000,323,12.1,1,323,11.9,1,1375552,1781780000000000000\n"
        )

    quotes = list(FlatFiles.Options.Quote.parse(path))
    raw = list(FlatFiles.Options.Quote.parse_raw(path))
    sorted_quotes = list(
        FlatFiles.Options.Quote.parse(
            path,
            sort_by_sip_timestamp=True,
        )
    )

    assert len(quotes) == 4
    assert isinstance(quotes[0], OptionQuote)
    assert quotes[0].root == "A"
    assert quotes[0].expiration == "2026-07-17"
    assert quotes[0].right == "C"
    assert quotes[0].strike == 60.0
    assert quotes[0].ask_exchange == 0
    assert quotes[0].ask_price == 0.0
    assert quotes[0].ask_size == 0
    assert quotes[0].bid_exchange == 320
    assert quotes[0].bid_price == 57.7
    assert quotes[0].bid_size == 1
    assert quotes[0].sequence_number == 1149543
    assert quotes[0].sip_timestamp == 1781789401689374260
    assert quotes[3].root == "MSFT"
    assert quotes[3].right == "P"
    assert quotes[3].strike == 250.0
    assert list(quotes[0]) == [
        "A",
        "2026-07-17",
        "C",
        60.0,
        0,
        0.0,
        0,
        320,
        57.7,
        1,
        1149543,
        1781789401689374260,
    ]
    assert raw[0] == (
        b"O:A260717C00060000",
        b"0",
        b"0",
        b"0",
        b"320",
        b"57.7",
        b"1",
        b"1149543",
        b"1781789401689374260",
    )
    assert [quote.root for quote in quotes] == ["A", "A", "A", "MSFT"]
    assert [quote.sip_timestamp for quote in sorted_quotes] == [
        1781789401689374260,
        1781789401689374260,
        1781789401696866913,
        1781780000000000000,
    ]
    assert "OptionQuote(" in repr(quotes[0])


def test_options_records_pack_without_ticker_identity() -> None:
    import massive_speedup

    trade = massive_speedup.OptionTrade(
        [
            "O:A260618C00115000",
            "209,227",
            "0",
            "320",
            "10.7",
            "1781791123025000000",
            "2",
        ]
    )
    quote = massive_speedup.OptionQuote(
        [
            "O:A260717C00060000",
            "0",
            "0",
            "0",
            "320",
            "57.7",
            "1",
            "1149543",
            "1781789401689374260",
        ]
    )

    packed_trade = trade.pack()
    packed_quote = quote.pack()

    assert len(packed_trade) == massive_speedup.OptionTrade.packed_size == 32
    assert b"O:A" not in packed_trade
    assert massive_speedup.OptionTrade.sip_timestamp_from_packed(packed_trade) == (
        trade.sip_timestamp
    )
    assert (
        massive_speedup.OptionTrade.from_packed(
            packed_trade,
            "A",
            "2026-06-18",
            "C",
            115.0,
        )
        == trade
    )

    assert len(packed_quote) == massive_speedup.OptionQuote.packed_size == 44
    assert b"O:A" not in packed_quote
    assert massive_speedup.OptionQuote.sip_timestamp_from_packed(packed_quote) == (
        quote.sip_timestamp
    )
    assert (
        massive_speedup.OptionQuote.from_packed(
            packed_quote,
            "A",
            "2026-07-17",
            "C",
            60.0,
        )
        == quote
    )


def test_flatfiles_crypto_parse_trade_rows(tmp_path: Path) -> None:
    path = tmp_path / "crypto_trades.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(
            "ticker,conditions,exchange,id,participant_timestamp,price,size\n"
        )
        handle.write(
            "X:00-USD,2,1,3641496,1781656423480597000,0.0036,7575.0\n"
        )
        handle.write(
            "X:00-USD,2,1,3641495,1781654446565723000,0.0036,30000.0\n"
        )

    trades = list(FlatFiles.Crypto.Trade.parse(path))
    raw = next(FlatFiles.Crypto.Trade.parse_raw(path))
    sorted_trades = list(
        FlatFiles.Crypto.Trade.parse(
            path,
            sort_by_participant_timestamp=True,
        )
    )

    assert len(trades) == 2
    assert isinstance(trades[0], CryptoTrade)
    assert trades[0].ticker == "X:00-USD"
    assert trades[0].conditions == frozenset({2})
    assert trades[0].exchange == 1
    assert trades[0].id == 3641496
    assert trades[0].participant_timestamp == 1781656423480597000
    assert trades[0].price == 0.0036
    assert trades[0].size == 7575.0
    assert list(trades[0]) == [
        "X:00-USD",
        frozenset({2}),
        1,
        3641496,
        1781656423480597000,
        0.0036,
        7575.0,
    ]
    assert raw == (
        b"X:00-USD",
        b"2",
        b"1",
        b"3641496",
        b"1781656423480597000",
        b"0.0036",
        b"7575.0",
    )
    assert [trade.id for trade in sorted_trades] == [3641495, 3641496]
    packed = trades[0].pack()
    assert len(packed) == CryptoTrade.packed_size
    assert CryptoTrade.participant_timestamp_from_packed(packed) == (
        trades[0].participant_timestamp
    )
    assert CryptoTrade.from_packed(packed, "X:00-USD") == trades[0]
    assert "CryptoTrade(" in repr(trades[0])
