from pathlib import Path
from importlib import import_module
import datetime as dt
import gzip
import sys
import types

import pytest
from massive_speedup import (
    CurrencyAggregate,
    CurrencyQuote,
    FlatFiles,
    StockAggregate,
    StockQuote,
    StockTrade,
    WebSocket,
    gzip_lines,
    read_gzip_lines,
)


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


def test_flatfiles_currencies_aggregate_parse_and_parse_raw(tmp_path: Path) -> None:
    path = tmp_path / "currency_aggregates.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write("ticker,volume,open,close,high,low,window_start,transactions\n")
        handle.write(
            "C:AED-AUD,1,0.397700393851942,0.397700393851942,0.397700393851942,"
            "0.397700393851942,1769133600000000000,1\n"
        )

    aggregate = next(FlatFiles.currency.Aggregate.parse(path))
    raw_aggregate = next(FlatFiles.currency.Aggregate.parse_raw(path))

    assert isinstance(aggregate, CurrencyAggregate)
    assert aggregate.ticker == "C:AED-AUD"
    assert aggregate.volume == 1
    assert aggregate.window_start == 1769133600000000000
    assert aggregate.transactions == 1
    assert raw_aggregate == (
        b"C:AED-AUD",
        b"1",
        b"0.397700393851942",
        b"0.397700393851942",
        b"0.397700393851942",
        b"0.397700393851942",
        b"1769133600000000000",
        b"1",
    )
    assert aggregate.tickers == ("AED", "AUD")


def test_flatfiles_currencies_aggregate_can_sort_by_window_start(tmp_path: Path) -> None:
    path = tmp_path / "currency_aggregates.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write("ticker,volume,open,close,high,low,window_start,transactions\n")
        handle.write("C:GBP-USD,2,1.2,1.2,1.2,1.2,200,2\n")
        handle.write("C:AED-AUD,1,0.4,0.4,0.4,0.4,100,1\n")

    aggregates = list(FlatFiles.currency.Aggregate.parse(path, sort_by_window_start=True))

    assert [aggregate.window_start for aggregate in aggregates] == [100, 200]
    assert [aggregate.ticker for aggregate in aggregates] == ["C:AED-AUD", "C:GBP-USD"]


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


def test_flatfiles_stock_aggregate_parse_and_parse_raw(tmp_path: Path) -> None:
    path = tmp_path / "stock_aggregates.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write("ticker,volume,open,close,high,low,window_start,transactions\n")
        handle.write("A,18218,138.0,137.93,138.36,137.75,1769178600000000000,125\n")

    aggregate = next(FlatFiles.Stock.Aggregate.parse(path))
    raw_aggregate = next(FlatFiles.Stock.Aggregate.parse_raw(path))

    assert isinstance(aggregate, StockAggregate)
    assert aggregate.ticker == "A"
    assert aggregate.volume == 18218
    assert aggregate.close == 137.93
    assert aggregate.window_start == 1769178600000000000
    assert raw_aggregate == (
        b"A",
        b"18218",
        b"138.0",
        b"137.93",
        b"138.36",
        b"137.75",
        b"1769178600000000000",
        b"125",
    )


def test_flatfiles_stock_aggregate_can_sort_by_window_start(tmp_path: Path) -> None:
    path = tmp_path / "stock_aggregates.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write("ticker,volume,open,close,high,low,window_start,transactions\n")
        handle.write("B,10,1.0,1.1,1.2,0.9,200,2\n")
        handle.write("A,12,2.0,2.1,2.2,1.9,100,3\n")

    aggregates = list(FlatFiles.Stock.Aggregate.parse(path, sort_by_window_start=True))

    assert [aggregate.window_start for aggregate in aggregates] == [100, 200]
    assert [aggregate.ticker for aggregate in aggregates] == ["A", "B"]


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


def test_flatfiles_forex_unimplemented_methods_raise() -> None:
    with pytest.raises(Exception):
        FlatFiles.Forex.parse_daily_aggregates("pair,close\nEURUSD,1.10\n")


def test_websocket_messages_parse_message_is_class_based() -> None:
    summary = WebSocket.Messages.parse_message('{"ev":"status"},{"ev":"trade"}')
    assert summary["parser_group"] == "websocket"
    assert summary["asset_class"] == "messages"
    assert summary["operation"] == "parse_message"
    assert summary["message_frames"] == 2


def test_websocket_crypto_parse_message_uses_selected_processor_module() -> None:
    summary = WebSocket.Crypto.parse_message('{"ev":"XT"}')
    assert summary["asset_class"] == "crypto"
    assert summary["processor"] in {
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


def test_currency_aggregate_row_model_is_iterable_hashable_and_ordered() -> None:
    newer = CurrencyAggregate(
        [
            "C:AED-AUD",
            "2",
            "0.4",
            "0.5",
            "0.6",
            "0.3",
            "1769133600000000001",
            "7",
        ]
    )
    older = CurrencyAggregate(
        [
            "C:AED-AUD",
            "2",
            "0.4",
            "0.5",
            "0.6",
            "0.3",
            "1769133600000000000",
            "7",
        ]
    )

    assert newer.volume == 2
    assert list(newer)[6] == 1769133600000000001
    assert newer > older
    assert hash(newer) == hash(
        CurrencyAggregate(
            [
                "C:AED-AUD",
                "2",
                "0.4",
                "0.5",
                "0.6",
                "0.3",
                "1769133600000000001",
                "7",
            ]
        )
    )
    assert newer.tickers == ("AED", "AUD")
    assert "CurrencyAggregate(" in repr(newer)


def test_stock_aggregate_row_model_is_iterable_hashable_and_ordered() -> None:
    newer = StockAggregate(
        [
            "A",
            "18218",
            "138.0",
            "137.93",
            "138.36",
            "137.75",
            "1769178600000000001",
            "125",
        ]
    )
    older = StockAggregate(
        [
            "A",
            "18218",
            "138.0",
            "137.93",
            "138.36",
            "137.75",
            "1769178600000000000",
            "125",
        ]
    )

    assert newer.volume == 18218
    assert list(newer)[6] == 1769178600000000001
    assert newer > older
    assert hash(newer) == hash(
        StockAggregate(
            [
                "A",
                "18218",
                "138.0",
                "137.93",
                "138.36",
                "137.75",
                "1769178600000000001",
                "125",
            ]
        )
    )
    assert "StockAggregate(" in repr(newer)


def test_condition_frozensets_are_globally_interned() -> None:
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

    assert trade.conditions is quote.conditions


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
    aggregate = CurrencyAggregate(
        [
            "C:AED-AUD",
            "1",
            "0.397700393851942",
            "0.397700393851942",
            "0.397700393851942",
            "0.397700393851942",
            "1769133600000000000",
            "1",
        ]
    )

    assert quote.tickers is aggregate.tickers


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
    assert args.database == Path("db")
    assert args.benchmark is False
    assert not hasattr(args, "record_type")
    assert not hasattr(args, "input_stdin")
    assert not hasattr(args, "input_file")


def test_build_database_infers_record_type_from_header(tmp_path: Path) -> None:
    from massive_speedup import build_database

    cases = {
        build_database.STOCK_TRADE_HEADER: "stock_trade",
        build_database.STOCK_QUOTE_HEADER: "stock_quote",
        build_database.CURRENCY_QUOTE_HEADER: "currency_quote",
    }

    for index, (header, expected_type) in enumerate(cases.items()):
        path = tmp_path / f"{index}.csv.gz"
        with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
            handle.write(header + "\n")

        assert build_database.infer_record_type(path) == expected_type


def test_build_database_main_processes_mixed_positional_files(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    from massive_speedup import build_database

    stock_path = tmp_path / "stock_quotes.csv.gz"
    currency_path = tmp_path / "currency_quotes.csv.gz"
    with gzip.open(stock_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.STOCK_QUOTE_HEADER + "\n")
    with gzip.open(currency_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.CURRENCY_QUOTE_HEADER + "\n")

    calls = []

    def fake_write_database_file(input_path: Path, database: Path, record_type: str) -> int:
        calls.append(("write", input_path, database, record_type))
        return 1

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
    assert calls == [
        ("write", stock_path, tmp_path / "db", "stock_quote"),
        ("write", currency_path, tmp_path / "db", "currency_quote"),
    ]


def test_build_database_main_benchmark_prints_metrics(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
) -> None:
    from massive_speedup import build_database

    stock_path = tmp_path / "stock_quotes.csv.gz"
    with gzip.open(stock_path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.STOCK_QUOTE_HEADER + "\n")

    timer_values = iter([10.0, 12.0])

    monkeypatch.setattr(
        build_database,
        "write_database_file",
        lambda input_path, database, record_type: 2,
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
    stderr = capsys.readouterr().err
    assert f"file={stock_path}" in stderr
    assert "type=stock_quote" in stderr
    assert "lines=2 lines" in stderr
    assert "seconds=2.000000 s" in stderr
    assert "throughput=0.000001 Mlines/s" in stderr


def test_build_database_file_native_groups_records_by_ticker_using_first_record_date(
    tmp_path: Path,
) -> None:
    from massive_speedup import build_database
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    one_day_ns = 86_400_000_000_000
    path = tmp_path / "trades.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.STOCK_TRADE_HEADER + "\n")
        handle.write("A,12,0,8,52983525035849,0,129.79,6876,0,100,1,0,0\n")
        handle.write(
            f"B,37,0,8,52983525035850,{one_day_ns},129.80,6877,{one_day_ns},100,1,0,0\n"
        )

    rows_written = build_database.write_database_file(path, tmp_path / "db", "stock_trade")

    root = tmp_path / "db" / "stock_trade" / "1970-01-01"
    assert rows_written == 2
    assert len((root / "A").read_bytes()) == module.StockTrade.packed_size
    assert len((root / "B").read_bytes()) == module.StockTrade.packed_size
    assert module.StockTrade.from_packed((root / "A").read_bytes(), "A").ticker == "A"
    assert module.StockTrade.from_packed((root / "B").read_bytes(), "B").ticker == "B"
    assert not (tmp_path / "db" / "stock_trade" / "1970-01-02").exists()


def test_build_database_file_native_creates_database_root_for_empty_input(tmp_path: Path) -> None:
    from massive_speedup import build_database
    try:
        import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    database = tmp_path / "db"
    path = tmp_path / "trades.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.STOCK_TRADE_HEADER + "\n")

    rows_written = build_database.write_database_file(path, database, "stock_trade")

    assert rows_written == 0
    assert database.is_dir()


def test_build_database_file_native_uses_participant_timestamp_for_currency_quote_date(
    tmp_path: Path,
) -> None:
    from massive_speedup import build_database
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    path = tmp_path / "currency_quotes.csv.gz"
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        handle.write(build_database.CURRENCY_QUOTE_HEADER + "\n")
        handle.write("C:AED-AUD,48,0.412060465749694,48,0.411836123587859,0\n")

    rows_written = build_database.write_database_file(path, tmp_path / "db", "currency_quote")

    assert rows_written == 1
    output = tmp_path / "db" / "currency_quote" / "1970-01-01" / "C:AED-AUD"
    assert len(output.read_bytes()) == module.CurrencyQuote.packed_size


def test_direct_native_module_exports_api() -> None:
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    assert hasattr(module, "FlatFiles")
    assert hasattr(module, "WebSocket")
    assert hasattr(module, "StockTrade")
    assert hasattr(module, "StockQuote")
    assert hasattr(module, "StockAggregate")
    assert hasattr(module, "StockTradeDatabase")
    assert hasattr(module, "StockQuoteDatabase")
    assert hasattr(module, "CurrencyQuote")
    assert hasattr(module, "CurrencyAggregate")
    assert hasattr(module, "CurrencyQuoteDatabase")
    assert hasattr(module, "gzip_lines")
    assert hasattr(module, "build_database_file")


def test_direct_native_module_classes_are_callable_when_built() -> None:
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    path = Path(__file__).resolve().parent / "data" / "hello_world.txt.gz"
    message_summary = module.WebSocket.Messages.parse_message(b'{"ev":"status"}')

    assert hasattr(module.FlatFiles.Stock, "parse_quotes")
    assert hasattr(module.FlatFiles.Stock, "parse_trades")
    assert hasattr(module.FlatFiles.Stock, "parse_raw_quotes")
    assert hasattr(module.FlatFiles.Stock, "parse_raw_trades")
    assert hasattr(module.FlatFiles.Stock, "parse_minute_aggregates")
    assert hasattr(module.FlatFiles.Stock, "parse_daily_aggregates")
    assert hasattr(module.FlatFiles.Stock, "parse_raw_minute_aggregates")
    assert hasattr(module.FlatFiles.Stock, "parse_raw_daily_aggregates")
    assert hasattr(module.FlatFiles.Stock, "Aggregate")
    assert hasattr(module.FlatFiles.Stock, "raw_lines")
    assert not hasattr(module.FlatFiles, "Stocks")
    assert hasattr(module.FlatFiles.currency, "parse_quotes")
    assert hasattr(module.FlatFiles.currency, "parse_raw_quotes")
    assert hasattr(module.FlatFiles.currency, "parse_minute_aggregates")
    assert hasattr(module.FlatFiles.currency, "parse_daily_aggregates")
    assert hasattr(module.FlatFiles.currency, "parse_raw_minute_aggregates")
    assert hasattr(module.FlatFiles.currency, "parse_raw_daily_aggregates")
    assert hasattr(module.FlatFiles.currency, "Aggregate")
    assert hasattr(module.FlatFiles.currency, "raw_lines")
    assert hasattr(module, "read_gzip_lines_bytes")
    assert list(module.read_gzip_lines_bytes(path)) == [b"Hello", b"World!"]
    assert message_summary["asset_class"] == "messages"


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
            "15",
            "1",
            "0",
            "0",
        ],
    )
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
    assert_roundtrip(
        module.StockAggregate,
        [
            "Brk.bBb",
            "18218",
            "138.0",
            "137.93",
            "138.36",
            "137.75",
            "1769178600000000000",
            "125",
        ],
    )
    assert_roundtrip(
        module.CurrencyAggregate,
        [
            "C:AED-AUD",
            "1",
            "0.397700393851942",
            "0.397700393851942",
            "0.397700393851942",
            "0.397700393851942",
            "1769133600000000000",
            "1",
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

    aggregate = module.StockAggregate(
        ["A", "18218", "138.0", "137.93", "138.36", "137.75", "1", "125"]
    )
    assert aggregate.volume is aggregate.volume
    assert aggregate.open is aggregate.open

    currency_aggregate = module.CurrencyAggregate(
        ["C:AED-AUD", "1", "0.3977", "0.3977", "0.3977", "0.3977", "1", "1"]
    )
    assert currency_aggregate.tickers is currency_aggregate.tickers
    assert currency_aggregate.volume is currency_aggregate.volume


def test_native_quote_and_trade_aggregators_yield_native_result_objects() -> None:
    try:
        module = import_module("massive_speedup._native")
    except ImportError:
        pytest.skip("massive_speedup._native is not built in this environment")

    assert module.FlatFiles.Stock.Trade.Aggregator is module.StockTradeAggregator
    assert module.FlatFiles.Stock.Quote.Aggregator is module.StockQuoteAggregator
    assert module.FlatFiles.currency.Quote.Aggregator is module.CurrencyQuoteAggregator

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
                "300",
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
    assert first_trade.volume_weighted_avg == 13.0
    assert first_trade.volume == 400
    assert first_trade.window_start == 1_000_000_000
    assert first_trade.transactions == 2
    assert first_trade.stddev == 2.0
    assert first_trade.dollar_volume == 5200.0
    assert first_trade.avg_trade_size == 200.0
    assert first_trade.min_trade_size == 100
    assert first_trade.max_trade_size == 300
    assert first_trade.price_change == 4.0
    assert first_trade.return_bps == pytest.approx(4000.0)
    assert first_trade.price_range == 4.0
    assert first_trade.range_bps == pytest.approx(4000.0)
    assert first_trade.first_timestamp == 1_000_000_000
    assert first_trade.last_timestamp == 1_500_000_000
    assert first_trade.duration_ns == 500_000_000
    assert trade_aggregates[1].window_start == 2_000_000_000

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

    trade_records = module.StockTradeDatabase(database, date_object, "A")
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
    assert trade_records.index_before_timestamp(2000, galloping=True) == 1
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
        trade_records.find_after_participant_timestamp(1500, 1000, galloping=True)
        == trade_rows[1]
    )
    trade_bar = next(module.StockTradeAggregator(trade_records, interval_seconds=1))
    assert trade_bar.volume == 200
    assert trade_bar.transactions == 2
    assert trade_bar.volume_weighted_avg == pytest.approx(129.795)

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
    quote_records = module.StockQuoteDatabase(database, date, "A")
    assert quote_records[1] == quote_rows[1]
    assert quote_records.index_before_timestamp(1999) == 0
    assert quote_records.index_before_timestamp(dt.time(0, 0, 0, 2), galloping=True) == 1
    assert list(quote_records.iterate_bounded(1000, 1999)) == [quote_rows[0]]
    assert list(quote_records.iterate_bounded(dt.time(0, 0, 0, 2))) == [quote_rows[1]]
    assert quote_records.find_after_participant_timestamp(1500, 1000) == quote_rows[1]
    assert quote_records.find_before_participant_timestamp(2000, on=False) == quote_rows[0]
    quote_bar = next(module.StockQuoteAggregator(quote_records, interval_seconds=1))
    assert quote_bar.transactions == 2
    assert quote_bar.ask_volume == 0
    assert quote_bar.bid_volume == 0

    currency_rows = [
        module.CurrencyQuote(["C:AED-AUD", "48", "0.412", "48", "0.411", "1000"]),
        module.CurrencyQuote(["C:AED-AUD", "48", "0.413", "48", "0.412", "2000"]),
    ]
    write_records("currency_quote", "C:AED-AUD", currency_rows)
    currency_records = module.CurrencyQuoteDatabase(database, date, "C:AED-AUD")
    assert currency_records[0] == currency_rows[0]
    assert currency_records.index_before_timestamp(1999) == 0
    assert currency_records.index_before_timestamp(dt.time(0, 0, 0, 2), galloping=True) == 1
    assert list(currency_records.iterate_bounded(1000, 1999)) == [currency_rows[0]]
    assert list(currency_records.iterate_bounded(dt.time(0, 0, 0, 2))) == [currency_rows[1]]
    assert currency_records.find_before_participant_timestamp(1500, 1000) == currency_rows[0]
    assert currency_records.find_after_participant_timestamp(1500, galloping=True) == currency_rows[1]
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


def test_parsed_rows_use_instance_lifetime_bitset_cache() -> None:
    repo_root = Path(__file__).resolve().parents[1]
    native_source = (repo_root / "src/cpp/native.hpp").read_text(encoding="utf-8")

    assert "class BitsetParseCache" in native_source
    assert "detail::BitsetParseCache<96> bitset_cache_;" in native_source
    assert "row = Implementation::parse_trade_row(line, bitset_cache_);" in native_source
    assert "row = Implementation::parse_quote_row(line, bitset_cache_);" in native_source
    assert "result.conditions = bitset_cache.get_or_parse(" in native_source
    assert "result.indicators = bitset_cache.get_or_parse(" in native_source
