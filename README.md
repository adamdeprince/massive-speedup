# massive-speedup

Native C++/nanobind readers for Polygon/Massive flat-file market data.

See [INSTALL.md](INSTALL.md) for installation details and [DEVELOPMENT.md](DEVELOPMENT.md)
for release and PyPI publishing notes.

## Massive CSV Gzip Files

Install/build the native extension:

```bash
pip3 install -e .
```

### Parsed Records

Use `parse()` when you want typed C++ row objects with read-only attributes.
The default path streams the gzip file row by row, skips the CSV header, and
yields one Python object at a time:

```python
import massive_speedup

for trade in massive_speedup.FlatFiles.Stock.Trade.parse("trades.csv.gz"):
    print(trade.ticker, trade.sip_timestamp, trade.price)

for quote in massive_speedup.FlatFiles.Stock.Quote.parse("quotes.csv.gz"):
    print(quote.ticker, quote.bid_price, quote.ask_price)

for quote in massive_speedup.FlatFiles.currency.Quote.parse("currency_quotes.csv.gz"):
    print(quote.ticker, quote.participant_timestamp)
```

Available parsed endpoints:

| Endpoint | Yields |
| --- | --- |
| `FlatFiles.Stock.Trade.parse(path)` | `StockTrade` |
| `FlatFiles.Stock.Quote.parse(path)` | `StockQuote` |
| `FlatFiles.Stock.Aggregate.parse(path)` | `StockAggregate` |
| `FlatFiles.currency.Quote.parse(path)` | `CurrencyQuote` |
| `FlatFiles.currency.Aggregate.parse(path)` | `CurrencyAggregate` |

### Raw Field Tuples

Use `parse_raw()` when you want CSV fields without numeric conversion or record
construction. It yields one tuple of `bytes` per input row:

```python
for row in massive_speedup.FlatFiles.Stock.Trade.parse_raw("trades.csv.gz"):
    ticker = row[0]
    sip_timestamp = row[8]
    print(ticker, sip_timestamp)

for row in massive_speedup.FlatFiles.Stock.Quote.parse_raw("quotes.csv.gz"):
    ticker, ask_exchange, ask_price = row[:3]
    print(ticker, ask_exchange, ask_price)
```

Available raw endpoints:

| Endpoint | Yields |
| --- | --- |
| `FlatFiles.Stock.Trade.parse_raw(path)` | `tuple[bytes, ...]` with trade fields |
| `FlatFiles.Stock.Quote.parse_raw(path)` | `tuple[bytes, ...]` with quote fields |
| `FlatFiles.Stock.Aggregate.parse_raw(path)` | `tuple[bytes, ...]` with aggregate fields |
| `FlatFiles.currency.Quote.parse_raw(path)` | `tuple[bytes, ...]` with currency quote fields |
| `FlatFiles.currency.Aggregate.parse_raw(path)` | `tuple[bytes, ...]` with currency aggregate fields |

### Raw Lines

Use `gzip_lines()` to benchmark decompression and line delivery without CSV
field splitting. It yields decompressed lines as `bytes`:

```python
for line in massive_speedup.gzip_lines("trades.csv.gz"):
    print(line)
```

Use `raw_lines()` from a row class when you want data rows after the CSV header:

```python
for line in massive_speedup.FlatFiles.Stock.Trade.raw_lines("trades.csv.gz"):
    print(line)
```

### Sorting

Sorting flags are keyword-only. Leaving them unset preserves the file order and
streams rows without collecting the file in memory. Enabling a sort flag
materializes rows, sorts them, and then yields one row at a time:

```python
for trade in massive_speedup.FlatFiles.Stock.Trade.parse(
    "trades.csv.gz",
    sort_by_sip_timestamp=True,
):
    print(trade.sip_timestamp, trade.ticker)

for trade in massive_speedup.FlatFiles.Stock.Trade.parse_raw(
    "trades.csv.gz",
    sort_by_participant_timestamp=True,
):
    print(trade[5])

for bar in massive_speedup.FlatFiles.Stock.Aggregate.parse(
    "stock_aggregates.csv.gz",
    sort_by_window_start=True,
):
    print(bar.window_start)
```

`Stock.Trade` and `Stock.Quote` support `sort_by_participant_timestamp` and
`sort_by_sip_timestamp`. `Stock.Aggregate` and `currency.Aggregate` support
`sort_by_window_start`. `currency.Quote` supports
`sort_by_participant_timestamp`.

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

Database files support indexing and timestamp search:

```python
first = records[0]
last = records[-1]

index = records.index_before_timestamp(1769161728012983416)
near_open = records.index_before_timestamp(1769161728012983416, galloping=True)
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
