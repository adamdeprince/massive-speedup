# massive-speedup

Native C++/nanobind readers for Polygon/Massive flat-file market data.

See [INSTALL.md](INSTALL.md) for installation details and [DEVELOPMENT.md](DEVELOPMENT.md)
for release and PyPI publishing notes.

## Storage Configuration

Configure the binary database and downloaded `.csv.gz` roots once in the
environment:

```bash
export MASSIVE_SPEEDUP_DB_PATH="$HOME/massive-db"
export MASSIVE_SPEEDUP_DOWNLOAD_PATH="$HOME/massive-download"
```

Database loaders and both command-line tools use these defaults. Python APIs
accept `database_path=...` or `download_path=...` as optional keyword-only
overrides when a different root is needed.

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

The native aggregators consume records in timestamp order and yield one result at
a time. Result attributes are read-only and lazily converted to Python objects.
`interval_seconds` selects any positive interval in seconds (a `float`, so
sub-second bars such as `0.25` or `0.5` are allowed). The optional
`start_timestamp` both excludes earlier rows and anchors the window boundaries;
`window_start` is always nanoseconds since the epoch.

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
- `massive_speedup.CryptoTradeAggregator` / `FlatFiles.Crypto.Trade.Aggregator`
- `massive_speedup.CurrencyQuoteAggregator` / `FlatFiles.currency.Quote.Aggregator`
- `massive_speedup.IndexValueAggregator` / `FlatFiles.Indices.Value.Aggregator`
- `massive_speedup.FuturesTradeAggregator` / `FlatFiles.Futures.Trade.Aggregator`
- `massive_speedup.FuturesQuoteAggregator` / `FlatFiles.Futures.Quote.Aggregator`
- `massive_speedup.OptionTradeAggregator` / `FlatFiles.Options.Trade.Aggregator`
- `massive_speedup.OptionQuoteAggregator` / `FlatFiles.Options.Quote.Aggregator`

Stock trades aggregate `price` and use `size` for `volume` and
`volume_weighted_avg`. Stock quotes aggregate ask and bid prices separately and
use ask/bid sizes for ask/bid volume-weighted averages. Currency quotes aggregate
ask and bid prices separately and omit volume and volume-weighted averages
because the source rows have no size field.

```python
import datetime
import massive_speedup

quotes = massive_speedup.StockQuoteDatabase("2026-01-23", "A")

for quote_bar in massive_speedup.StockQuoteAggregator(
    quotes,
    interval_seconds=1,
    start_timestamp=datetime.time(9, 30),
):
    print(quote_bar.ask_open, quote_bar.ask_close, quote_bar.bid_avg)
```

For a native daily database, `start_timestamp` may be a `datetime.time`; the
database date supplies the date component. It may also be an absolute numeric
nanosecond timestamp. Iterable inputs without a database date require the
absolute numeric form. Futures time-of-day starts use the prior evening when the
time falls in the evening portion of the session.

Aggregators stream consecutive `(ticker, window_start)` groups. Use input ordered
by ticker and timestamp, such as the native database iterators or default
Massive/Polygon flat-file order. `stddev` is population standard deviation.
Massive-provided minute and daily aggregate flatfiles are intentionally not
parsed; bars are computed locally from the same row-at-a-time data used in live
strategy execution.

## Build Database Files

Build fixed-length binary database files from every `.csv.gz` file under
`MASSIVE_SPEEDUP_DOWNLOAD_PATH` into `MASSIVE_SPEEDUP_DB_PATH`:

```bash
massive-speedup-build-database
```

To build one or more specific files or directories instead, pass them explicitly:

```bash
massive-speedup-build-database 2026-01-23.csv.gz
```

The input type is inferred from the CSV header. Output layout is:

```text
{database}/{stock_trade|stock_quote|currency_quote}/{YYYY-MM-DD}/{ticker}
```

Existing ticker files are not overwritten by default. The builder keeps reading
the input until the next ticker and only writes missing ticker files. Before
opening an input gzip, the CLI derives its dataset and date from the download
path and scans the corresponding target directory. A nonempty directory with
no `.incomplete` entries is skipped without opening or decompressing the source
file. Use `--force` to rebuild existing ticker files, which is useful after a
binary record format change:

```bash
massive-speedup-build-database --force 2026-01-23.csv.gz
```

Each final ticker file is first written as `{ticker}.incomplete` and atomically
renamed to `{ticker}` only after the complete file has been flushed and closed.
A date build also keeps `.massive-speedup.incomplete` in its target directory
for the entire operation and atomically renames it to
`.massive-speedup.complete` at the end. This prevents an interruption between
ticker writes from making a partially populated directory look complete. A
failed or interrupted build therefore cannot publish a partial database file
or a false directory-completion state. The `.incomplete` suffix is an explicit
rewrite marker and database readers never open it. On Ctrl-C the worker pool is
terminated immediately, serial native reads poll for pending Python signals,
and the current local external-sort run is removed; destination-side markers
are safely discarded and rebuilt on the next run. If a previous complete file
exists, it remains untouched until its replacement is ready.

Use `--benchmark` to print throughput:

```bash
massive-speedup-build-database --benchmark *.csv.gz
```

Benchmark `lines` and throughput count non-empty source records processed, even
when an existing final database file causes those records to be skipped during
publication.

Files are built in separate worker processes. The default is one worker process;
opt into parallel file processing explicitly:

```bash
massive-speedup-build-database --processes 8
```

Limit discovery to one data family with `--only`. For example, `stock` selects
both `stock_trade` and `stock_quote` files:

```bash
massive-speedup-build-database --only stock --processes 16
```

The accepted families are `stock`, `currency`, `crypto`, `option`, `future`, and
`index`; singular and plural names are accepted. Filtering uses the
dataset-specific directories created by `massive-speedup-download`, and skips
uncategorized inputs.

Use `--not-before` to process only files whose `YYYY-MM-DD` filename date is
equal to or later than the requested date:

```bash
massive-speedup-build-database --not-before 2026-01-01
```

The boundary is inclusive. Database discovery only accepts files named exactly
`YYYY-MM-DD.csv.gz`; unrelated gzip files such as `foobar.csv.gz` are always
skipped without being opened.

Discovered inputs are queued globally from newest date to oldest date, across
all selected data families.

Parallelism is across input files. A single gzip stream is handled by one
worker, so the effective worker count is capped at the number of input files.
Each worker reads its file's header immediately before building it, allowing
header I/O to overlap database computation in the other workers. Header
inference and row decoding share one native rapidgzip reader, so the file is
not reopened between those steps.

The default mode performs a stable in-memory timestamp sort one ticker at a
time before writing the final database file. Option data is buffered one root
at a time and sorted per contract.

For symbols too large to sort comfortably in memory, install
[adamdeprince/bsort](https://github.com/adamdeprince/bsort) as a separate
command-line program:

```bash
git clone https://github.com/adamdeprince/bsort.git
cmake -S bsort -B bsort/build -DCMAKE_BUILD_TYPE=Release
cmake --build bsort/build --parallel
cmake --install bsort/build --prefix ~/.local
```

Then opt into external sorting:

```bash
massive-speedup-build-database --bsort
```

`--bsort` streams unsorted packed records into private temporary files, invokes
`bsort -r RECORD_SIZE -k 8 FILE` on each file in place, and then publishes the
sorted data to the database. The executable is neither vendored nor linked
into massive-speedup. It must be on `PATH` or installed beside the active
Python executable.

External files default to `/var/tmp`, keeping random sort I/O away from a slow
NAS. Select another local SSD or scratch filesystem with:

```bash
massive-speedup-build-database \
  --bsort \
  --external-sort-path /mnt/local-scratch
```

Each worker gets a private staging tree, so unsorted staging writes and bsort
jobs remain parallel across input files. The run tree is removed after success,
failure, or Ctrl-C.

For a NAS or HDD that handles concurrent reads better than concurrent writes,
enable lockstep output:

```bash
massive-speedup-build-database \
  --processes 64 \
  --lockstep-writer \
  --block-size 1MiB
```

Workers continue reading, decompressing, parsing, and—under `--bsort`—writing
local staging files independently. All workers share one process-safe lock only
for final database output. The lock is held while one final output block is
written and while a final file is opened, closed, or atomically published.
This applies to both internal and external sorting; local staging writes and
bsort never take the NAS write lock.

`--block-size` controls both read buffers and the final output block; it accepts
byte counts or `K`, `M`, and `G` suffixes and defaults to 1 MiB. It does not
enlarge rapidgzip's internal decode chunks. The CLI divides a machine-wide
rapidgzip decoder budget across the active worker processes, so each process no
longer creates its own machine-wide decoder pool. Forward-only database reads
also disable rapidgzip's seek index, preventing retained window state from
growing with very large decompressed files. Builder inputs and completed output
files are marked as non-reusable streaming I/O and evicted from the Linux page
cache after use, preventing a multi-terabyte run from charging its entire
working set to the invoking user session.

Pool work uses one-file chunks and unordered completion, so a large input does
not prevent smaller completed inputs from advancing the progress display or a
free worker from taking another file. Workers are spawned into clean process
spaces instead of forking the progress display's monitor thread, and each
worker exits after one file so native allocator state is returned to the
operating system.

## Database Files

Open a fixed-length binary file through mmap and iterate records:

```python
records = massive_speedup.StockTradeDatabase(
    "2026-01-23",
    "A",
)

for trade in records:
    print(trade.sip_timestamp, trade.price)
```

Constructors first ask **`pandas_market_calendars` (NYSE)** whether the date is
a session (result cached process-wide so multi-day loops do not thrash NFS).
Non-sessions open as an **empty** database without checking the filesystem
(on-disk files are only created when a tick was seen in the flatfile, so
weekends/holidays never have files).

On a valid session, open the **on-disk ticker flat file only**. If it is
missing, raise a clear **missing market data** error so the backtest can skip
the day or fail. There is **no REST / API backfill** on database open —
`MASSIVE_API_KEY` is irrelevant to `StockTradeDatabase` /
`StockQuoteDatabase` (and siblings). Build files offline with
`massive-speedup-build-database` / the download tooling.

Aggregators scan mmap themselves (`packed_data_at` + `add_packed`) so bars
are built without allocating a record object per trade/quote.
`__len__` and timestamp search (binary/galloping) require the on-disk file.

Database files are headerless sequences of fixed-size records. Every record
starts with its primary chronological sort key as an unsigned 64-bit,
big-endian integer:

| Record | Sort key at bytes 0–7 |
| --- | --- |
| `StockTrade` | `sip_timestamp` |
| `StockQuote` | `sip_timestamp` |
| `CryptoTrade` | `participant_timestamp` |
| `CurrencyQuote` | `participant_timestamp` |
| `IndexValue` | `timestamp` |
| `FuturesTrade` | `timestamp` |
| `FuturesQuote` | `timestamp` |
| `OptionTrade` | `sip_timestamp` |
| `OptionQuote` | `sip_timestamp` |

This makes unsigned timestamp order match lexicographic byte order across
records. Remaining numeric fields retain their existing little-endian
encoding, and the fixed record sizes are unchanged. Databases built with the
earlier layout are not compatible; rebuild them with
`massive-speedup-build-database --force`.

Use `MultiDayDatabase` when a strategy should see date-partitioned files as one
row sequence. It discovers available days, opens daily mmap files on demand,
and keeps only the most recently used `max_open_days` files open:

```python
import datetime
import massive_speedup

with massive_speedup.MultiDayDatabase(
    "stock_trade",
    "AAPL",
    start_date="2026-01-20",
    end_date="2026-01-23",
    max_open_days=2,
) as trades:
    for trade in trades:
        consume(trade)

    next_trade = trades.find_after_timestamp(
        datetime.datetime(2026, 1, 22, 15, 0, tzinfo=datetime.UTC)
    )
```

The same wrapper supports stock, crypto, currency, index, futures, and option
record databases. `locate_before_timestamp` and `locate_after_timestamp` also
return the daily database date and local row index for reusable galloping-search
hints.

Merge stock trades and quotes for one date and ticker in SIP timestamp order:

```python
for trade, quote in massive_speedup.stock_trade_quote_timeline(
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

`SimpleMarket` is the stock **market simulator**: it merges trade/quote tapes in
SIP order and runs a built-in `TradeEmulator` for fills, cash, and positions.
Strategy code sets a **desired unit position** each event; the simulator decides
whether/when to fill.

```python
import massive_speedup

market = massive_speedup.SimpleMarket(
    "2026-01-23",
    ["AAPL", "MSFT"],
    trade_latency_ns=150_000_000,
    quotes=True,
    execution="market",   # or "middle"
    passivity=0.0,        # middle: 0=touch, 1=far side, >1=beyond far side
    unit_shares=1.0,
)

for symbol, timestamp, trade, quote, trades, quotes, broker in market:
    desired = 1 if should_be_long(symbol, trade, quote) else 0
    broker.set_desired(desired)   # +1 long / 0 flat / -1 short
    # market[symbol] / market[None] → position / cash

print(market.summary())  # fills, cancels, cash, positions, execution knobs
```

### Execution modes (part of the market)

| `execution` | Behavior |
|-------------|----------|
| `"market"` (default) | When desired ≠ current unit position, trade the delta at **ask** (buy) / **bid** (sell). |
| `"middle"` | Rest a limit from the NBBO. `passivity>=0` (no upper cap): `0` touch (marketable); `1` far side (buy bid / sell ask); `>1` beyond far side. Fills when the quote becomes marketable vs the limit. |

Limit prices (middle):

```text
buy  limit = ask − passivity × (ask − bid)
sell limit = bid + passivity × (ask − bid)
```

Fills use the **touch** when marketable (not free mid). Resting orders are
**polled automatically** on every trade/quote event so a passive order can fill
as the book moves without a new `set_desired`. Changing intent cancels the
working order and places a new one (so long→flat→long before a sell fills can
leave you still long).

Properties: `market.execution`, `market.passivity`, `market.unit_shares`,
`market.trade_latency_ns`, `market.trade_emulator`.

Each iteration yields a 7-tuple:

- `symbol`: the current event's symbol.
- `timestamp`: the current event's SIP timestamp as floating-point seconds since the epoch.
- `trade`: the current `StockTrade`, or `None` when the event is a quote.
- `quote`: the current `StockQuote`, or `None` when the event is a trade.
- `trades`: a dict mapping symbols to the most recent trade seen for each symbol.
- `quotes`: a dict mapping symbols to the most recent quote seen for each symbol.
- `broker`: a `SimpleMarketBroker` bound to the current symbol and SIP timestamp.

By default, the iterator emits only trade events, but quote files are still read
so `quotes` contains the latest quote state and resting limits still poll. Set
`quotes=True` to also emit quote events. The `date` argument may be a
`YYYY-MM-DD` string or a `datetime.date`.

Legacy `broker.buy(shares)` / `broker.sell(shares)` still always cross the
spread. Prefer `broker.set_desired(...)`.

`market.summary()` raises until the iterator is exhausted, then returns the
execution ledger, cash, positions, and fill/cancel counts.

### Multi-day `TradeEmulator`

Pass a shared `TradeEmulator` to carry cash, positions, and the order ledger
across calendar days. Execution policy lives on that emulator:

```python
import datetime as dt
import massive_speedup

emulator = massive_speedup.TradeEmulator(
    trade_latency_ns=150_000_000,
    execution="middle",
    passivity=0.5,
    unit_shares=1.0,
)

for day in (dt.date(2026, 1, 23), dt.date(2026, 1, 24)):
    market = massive_speedup.SimpleMarket(
        day,
        ["AAPL"],
        trade_emulator=emulator,  # execution knobs taken from emulator
    )
    for symbol, timestamp, trade, quote, trades, quotes, broker in market:
        if trade is None:
            continue
        broker.set_desired(1 if should_enter(trade) else 0)

print(emulator.cash, emulator.position("AAPL"))
print(emulator.summary())
```

### Bar aggregator broker

`StockTradeAggregator` accepts the same shared emulator when you also pass a
`StockQuoteDatabase` for fill pricing. The bar decision timestamp is
`window_start + interval_seconds` (bar close). Access the broker via
`aggregator.broker` after each bar:

```python
emulator = massive_speedup.TradeEmulator()
for day in days:
    trades = massive_speedup.StockTradeDatabase(day, "AAPL")
    quotes = massive_speedup.StockQuoteDatabase(day, "AAPL")
    bars = massive_speedup.StockTradeAggregator(
        trades,
        interval_seconds=60,
        start_timestamp=trades.market_open,
        quotes=quotes,
        trade_emulator=emulator,
    )
    for bar in bars:
        if emulator.position(bar.ticker) > 0 and exit_signal(bar):
            bars.broker.sell(emulator.position(bar.ticker))
        elif emulator.position(bar.ticker) == 0 and entry_signal(bar):
            bars.broker.buy(100)
```

`quotes` and `trade_emulator` must be provided together. Without them the
aggregator behaves as before and only yields bars.

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
