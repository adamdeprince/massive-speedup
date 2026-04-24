from pathlib import Path
from importlib import import_module
import gzip

import pytest
from massive_speedup import (
    BackendKind,
    CurrencyAggregate,
    CurrencyQuote,
    FlatFiles,
    StockAggregate,
    StockQuote,
    StockTrade,
    WebSocket,
    available_backends,
    backend_is_available,
    detect_best_backend,
    detect_processor_type,
    gzip_lines,
    read_gzip_lines,
)


def test_detect_best_backend_returns_enum() -> None:
    backend = detect_best_backend()
    assert isinstance(backend, BackendKind)


def test_available_backends_includes_generic() -> None:
    backends = available_backends()
    assert any(record.name == "generic" and record.available for record in backends)


def test_backend_is_available_for_detected_backend() -> None:
    assert backend_is_available(detect_best_backend())


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


@pytest.mark.parametrize(
    ("module_name", "flatfile_attr", "websocket_attr"),
    [
        ("massive_speedup._generic", "_GenericFlatFileStocksParser", "_GenericWebSocketMessagesParser"),
        ("massive_speedup._sse", "_SseFlatFileStocksParser", "_SseWebSocketMessagesParser"),
        ("massive_speedup._avx", "_AvxFlatFileStocksParser", "_AvxWebSocketMessagesParser"),
        ("massive_speedup._avx512", "_Avx512FlatFileStocksParser", "_Avx512WebSocketMessagesParser"),
        ("massive_speedup._neon", "_NeonFlatFileStocksParser", "_NeonWebSocketMessagesParser"),
        ("massive_speedup._sve", "_SveFlatFileStocksParser", "_SveWebSocketMessagesParser"),
        ("massive_speedup._sve2", "_Sve2FlatFileStocksParser", "_Sve2WebSocketMessagesParser"),
        ("massive_speedup._lsx", "_LsxFlatFileStocksParser", "_LsxWebSocketMessagesParser"),
        ("massive_speedup._lasx", "_LasxFlatFileStocksParser", "_LasxWebSocketMessagesParser"),
    ],
)
def test_direct_backend_module_imports_expose_backend_specific_aliases(
    module_name: str,
    flatfile_attr: str,
    websocket_attr: str,
) -> None:
    try:
        module = import_module(module_name)
    except ImportError:
        pytest.skip(f"{module_name} is not built in this environment")

    assert hasattr(module, "FlatFiles")
    assert hasattr(module, "WebSocket")
    assert hasattr(module, "BACKEND_KIND")
    assert hasattr(module, "BACKEND_NAME")
    assert hasattr(module, "StockTrade")
    assert hasattr(module, "StockQuote")
    assert hasattr(module, "StockAggregate")
    assert hasattr(module, "CurrencyQuote")
    assert hasattr(module, "CurrencyAggregate")
    assert hasattr(module, "gzip_lines")
    assert hasattr(module, flatfile_attr)
    assert hasattr(module, websocket_attr)


@pytest.mark.parametrize(
    "module_name",
    [
        "massive_speedup._generic",
        "massive_speedup._sse",
        "massive_speedup._avx",
        "massive_speedup._avx512",
        "massive_speedup._neon",
        "massive_speedup._sve",
        "massive_speedup._sve2",
        "massive_speedup._lsx",
        "massive_speedup._lasx",
    ],
)
def test_direct_backend_module_classes_are_callable_when_built(module_name: str) -> None:
    try:
        module = import_module(module_name)
    except ImportError:
        pytest.skip(f"{module_name} is not built in this environment")

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


def test_sse_parse_raw_uses_backend_specialization_hooks() -> None:
    sse_source = Path("src/cpp/backends/x86_sse.cpp").read_text(encoding="utf-8")
    generic_source = Path("src/cpp/backends/generic.hpp").read_text(encoding="utf-8")

    assert "using Implementation = massive_speedup::backend_generic::Implementation<Base, Specialization>" in sse_source
    assert "parse_raw_trade_tuple" in generic_source
    assert "parse_raw_quote_tuple" in generic_source
    assert "next_raw_condition_field<true>" in generic_source
    assert "Specialization::template parse_unquoted_field<ExpectMore>" in generic_source
    assert "static inline std::string_view parse_unquoted_field" in sse_source
    assert "static inline std::string_view parse_quoted_field" in sse_source


def test_parsed_rows_use_instance_lifetime_bitset_cache() -> None:
    generic_source = Path("src/cpp/backends/generic.hpp").read_text(encoding="utf-8")

    assert "class BitsetParseCache" in generic_source
    assert "detail::BitsetParseCache<96> bitset_cache_;" in generic_source
    assert "row = Implementation::parse_trade_row(line, bitset_cache_);" in generic_source
    assert "row = Implementation::parse_quote_row(line, bitset_cache_);" in generic_source
    assert "result.conditions = bitset_cache.get_or_parse(" in generic_source
    assert "result.indicators = bitset_cache.get_or_parse(" in generic_source
