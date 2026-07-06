# massive-speedup

Native C++/nanobind readers for Polygon/Massive flat-file market data.

See [INSTALL.md](INSTALL.md) for installation details and [DEVELOPMENT.md](DEVELOPMENT.md)
for release and PyPI publishing notes.

## CSV Gzip Files

Install/build the native extension:

```bash
pip3 install -e .
```

Iterate parsed records directly from a `.csv.gz` file:

```python
import massive_speedup

for trade in massive_speedup.FlatFiles.Stock.Trade.parse("trades.csv.gz"):
    print(trade.ticker, trade.sip_timestamp, trade.price)

for quote in massive_speedup.FlatFiles.Stock.Quote.parse("quotes.csv.gz"):
    print(quote.ticker, quote.bid_price, quote.ask_price)

for quote in massive_speedup.FlatFiles.currency.Quote.parse("currency_quotes.csv.gz"):
    print(quote.ticker, quote.participant_timestamp)
```

You can also iterate raw CSV fields as `bytes` tuples:

```python
for row in massive_speedup.FlatFiles.Stock.Trade.parse_raw("trades.csv.gz"):
    print(row[0], row[8])
```

Example scripts:

- [examples/howto_csv_gzip_daily_vwap.py](examples/howto_csv_gzip_daily_vwap.py) computes daily stock-trade VWAP using `gzip` and `csv.DictReader`.
- [examples/howto_database_daily_vwap.py](examples/howto_database_daily_vwap.py) computes the same value from a `massive-speedup` binary database file using mmap and the native C++ aggregator.

## Record Access

Parsed records expose read-only attributes and are iterable in CSV field order:

```python
trade = next(massive_speedup.FlatFiles.Stock.Trade.parse("trades.csv.gz"))

print(trade.ticker)
print(trade.conditions)
print(trade.sip_timestamp)
print(trade.pack())
print(list(trade))
```

Packed records do not include the ticker. Reconstruct with the ticker from the file name:

```python
packed = trade.pack()
trade2 = massive_speedup.StockTrade.from_packed(packed, trade.ticker)
```

## Window Aggregation

The native aggregators consume iterables of parsed records and yield C++ result
objects exposed through nanobind. Result attributes are read-only and lazily
converted to Python objects on first access. The aggregation interval and offset
are expressed in seconds; the returned `window_start` is still nanoseconds since
epoch.

```python
import massive_speedup

trades = massive_speedup.FlatFiles.Stock.Trade.parse("trades.csv.gz")

for bar in massive_speedup.FlatFiles.Stock.Trade.Aggregator(
    trades,
    interval_seconds=60,
):
    print(
        bar.ticker,
        bar.window_start,
        bar.open,
        bar.close,
        bar.high,
        bar.low,
        bar.avg,
        bar.volume_weighted_avg,
        bar.volume,
        bar.transactions,
        bar.stddev,
    )
```

Available aggregators:

- `massive_speedup.StockTradeAggregator` / `FlatFiles.Stock.Trade.Aggregator`
- `massive_speedup.StockQuoteAggregator` / `FlatFiles.Stock.Quote.Aggregator`
- `massive_speedup.CurrencyQuoteAggregator` / `FlatFiles.currency.Quote.Aggregator`

Stock trades aggregate `price` and use `size` for `volume` and
`volume_weighted_avg`. Stock quotes aggregate ask and bid prices separately and
use ask/bid sizes for ask/bid volume-weighted averages. Currency quotes aggregate
ask and bid prices separately and omit volume and volume-weighted averages
because the source rows have no size field.

```python
quotes = massive_speedup.StockQuoteDatabase("/data/massive-db", "2026-01-23", "A")

for quote_bar in massive_speedup.StockQuoteAggregator(
    quotes,
    interval_seconds=1,
    offset_seconds=0,
):
    print(quote_bar.ask_open, quote_bar.ask_close, quote_bar.bid_avg)
```

Aggregators stream consecutive `(ticker, window_start)` groups. Use input ordered
by ticker and timestamp, such as the native database iterators or default
Massive/Polygon flat-file order. `stddev` is population standard deviation.

## Build Database Files

Build fixed-length binary database files from one or more input `.csv.gz` files:

```bash
massive-speedup-build-database --database /data/massive-db 2026-01-23.csv.gz
```

The input type is inferred from the CSV header. Output layout is:

```text
{database}/{stock_trade|stock_quote|currency_quote}/{YYYY-MM-DD}/{ticker}
```

Existing ticker files are not overwritten by default. The builder keeps reading
the input until the next ticker and only writes missing ticker files. Use
`--force` to rebuild existing ticker files, which is useful after a binary
record format change:

```bash
massive-speedup-build-database --force --database /data/massive-db 2026-01-23.csv.gz
```

Date-level idempotency uses an `.incomplete` marker in
`{database}/{type}/{YYYY-MM-DD}`. If the date directory exists without
`.incomplete`, the input file is skipped. If the directory is new, `.incomplete`
is created before processing and removed only after successful completion. Use
`--force` to process a date even when `.incomplete` is absent.

Use `--benchmark` to print throughput:

```bash
massive-speedup-build-database --benchmark --database /data/massive-db *.csv.gz
```

## Database Files

Open a fixed-length binary file through mmap and iterate records:

```python
records = massive_speedup.StockTradeDatabase(
    "/data/massive-db",
    "2026-01-23",
    "A",
)

for trade in records:
    print(trade.sip_timestamp, trade.price)
```

Merge stock trades and quotes for one date and ticker in SIP timestamp order:

```python
for trade, quote in massive_speedup.stock_trade_quote_timeline(
    "/data/massive-db",
    "2026-01-23",
    "A",
):
    if trade:
        print("trade", trade.sip_timestamp, trade.price, quote)
    else:
        print("quote", quote.sip_timestamp, quote.bid_price, quote.ask_price)
```

Quote rows yield `(None, current_quote)`. Trade rows yield
`(trade, last_quote)`, where `last_quote` is `None` until the first quote has
appeared. When a trade and quote have the same SIP timestamp, the quote is
yielded first.

## Simple Market Simulation

`SimpleMarket` opens stock trade and quote database files for a date and a
sequence of symbols, then merges all events in SIP timestamp order inside C++.
It is intended for row-at-a-time strategy code that needs current trade/quote
state without bouncing through Python for every lookup.

```python
import massive_speedup

market = massive_speedup.SimpleMarket(
    "/data/massive-db",
    "2026-01-23",
    ["AAPL", "MSFT"],
    1_000_000,  # simulated trade latency in nanoseconds
    quotes=True,
)

for symbol, timestamp, trade, quote, trades, quotes, broker in market:
    if trade is not None:
        last_quote = quotes.get(symbol)
        if last_quote is not None and trade.price < last_quote.bid_price:
            broker.buy(100)
    else:
        print("quote update", symbol, timestamp, quote.bid_price, quote.ask_price)

print(market["AAPL"])
print(market[None])  # cash delta
```

Each iteration yields a 7-tuple:

- `symbol`: the current event's symbol.
- `timestamp`: the current event's SIP timestamp as floating-point seconds since the epoch.
- `trade`: the current `StockTrade`, or `None` when the event is a quote.
- `quote`: the current `StockQuote`, or `None` when the event is a trade.
- `trades`: a dict mapping symbols to the most recent trade seen for each symbol.
- `quotes`: a dict mapping symbols to the most recent quote seen for each symbol.
- `broker`: a `SimpleMarketBroker` bound to the current symbol and SIP timestamp.

By default, the iterator emits only trade events, but quote files are still read
so `quotes` contains the latest quote state for trade handling. Set
`quotes=True` to also emit quote events. The `date` argument may be a
`YYYY-MM-DD` string or a `datetime.date`.

The broker supports `buy(shares, symbol=None)` and `sell(shares, symbol=None)`.
If `symbol` is omitted, the current event symbol is used. Fills are priced using
the quote at `current_sip_timestamp + trade_latency_ns`; buys use the ask and
sells use the bid. Holdings are exposed through a small dict-like interface:
symbol keys return share positions, and `None` returns the cash delta.

```python
broker.sell(50, "MSFT")

print("cash", market[None])
print(dict(market.items()))
```

`fast=False` is the default and returns fresh `trades` and `quotes` dicts for
each event. Set `fast=True` to reuse those dict objects across iterations and
reduce Python allocation churn; do not store those dicts when using fast mode
because later iterations mutate them in place.

Database files support indexing and timestamp search:

```python
first = records[0]
last = records[-1]

index = records.index_before_timestamp(1769161728012983416)
near_open = records.index_before_timestamp(1769161728012983416, galloping=0)
next_index = records.index_after_timestamp(1769161728012983416, galloping=index + 1)
```

Timestamp arguments are nanoseconds since epoch. Database readers also accept
`datetime.time` values, which are resolved using the reader's date:

```python
import datetime as dt

index = records.index_before_timestamp(dt.time(9, 30))
```

Find the closest record before or after a participant timestamp:

```python
before = records.find_before_participant_timestamp(
    1769161728012624580,
)
after = records.find_after_participant_timestamp(
    1769161728012624580,
    fuzz=250_000_000,
    galloping=True,
)
strict_before = records.find_before_participant_timestamp(
    1769161728012624580,
    on=False,
)
```

`find_before_participant_timestamp` returns the record with the highest
participant timestamp less than or equal to the target. `find_after_participant_timestamp`
returns the record with the lowest participant timestamp greater than or equal
to the target. Set `on=False` for strict `<` or `>` comparisons. `fuzz` is a
nanosecond scan window around the searched timestamp and defaults to one second
(`1_000_000_000`). Both methods return records, not indexes.

Stock database readers also expose NYSE market session timestamps in nanoseconds:

```python
print(records.market_open)
print(records.market_close)
```
