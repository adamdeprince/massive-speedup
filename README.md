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
`interval_seconds` selects any positive whole-second interval. The optional
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

Build fixed-length binary database files from one or more input `.csv.gz` files:

```bash
massive-speedup-build-database 2026-01-23.csv.gz
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
massive-speedup-build-database --force 2026-01-23.csv.gz
```

Each ticker is written to `{ticker}.incomplete` and atomically renamed to
`{ticker}` only after the complete file has been flushed and closed. A failed
build therefore cannot publish a partial database file; rerunning the builder
can safely replace the leftover incomplete file.

Use `--benchmark` to print throughput:

```bash
massive-speedup-build-database --benchmark *.csv.gz
```

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

## Real-Time WebSocket Messages

The native WebSocket parser consumes one Massive message frame as `str` or
`bytes`. A frame may contain one JSON object or an array of objects:

```python
message = massive_speedup.WebSocket.Stocks.parse(frame)

for event in message:
    if isinstance(event, massive_speedup.WebSocket.Stocks.Trade):
        print(event.ticker, event.sip_timestamp, event.price, event.size)
    elif isinstance(event, massive_speedup.WebSocket.Stocks.Quote):
        print(event.ticker, event.bid_price, event.ask_price)
```

`WebSocketMessage` owns one padded input buffer shared by every event in the
frame. The event type is classified up front; other fields are extracted with
simdjson only when their read-only property or mapping key is first requested,
then cached. `cached_fields`, `cached_properties`, `is_cached(name)`, and
`is_property_cached(name)` make that behavior observable. Raw protocol fields
remain available through `event["field"]`, `event.get(...)`, and
`event.raw_json`.

Typed events use the same property names and nanosecond timestamp units as the
database rows:

- `WebSocket.Stocks.Trade` and `.Quote`
- `WebSocket.Options.Trade` and `.Quote`
- `WebSocket.Futures.Trade` and `.Quote`
- `WebSocket.Crypto.Trade` and `.Quote`
- `WebSocket.Forex.Quote`
- `WebSocket.Indices.Value`
- `WebSocket.Stocks.LimitUpLimitDown` and `.NetOrderImbalance`
- `WebSocket.{Stocks,Options,Crypto,Forex}.FairMarketValue`
- `WebSocket.Status`

Other control messages and Massive-provided aggregate events remain lossless
generic `WebSocketEvent` objects. Vendor aggregates are intentionally not
exposed as typed rows; use the database row aggregators for custom window sizes
and anchors. Massive's FMV and LULD documentation contains timestamp-unit
contradictions between its field descriptions and examples, so those typed
events accept either millisecond or nanosecond epoch values and expose
normalized nanoseconds.

No network client is included yet. A synchronous iterable of frames can be
adapted to the same row-at-a-time 7-tuple used by the database markets:

```python
class LiveExecution:
    def buy(self, quantity, symbol=None):
        send_order("buy", quantity, symbol)

    def sell(self, quantity, symbol=None):
        send_order("sell", quantity, symbol)


execution = LiveExecution()
market = massive_speedup.WebSocket.Stocks.market(
    websocket_frames,
    execution,
    quotes=True,
)

for symbol, timestamp, trade, quote, trades, quotes, broker in market:
    if trade is not None and should_buy(trade, quotes.get(symbol)):
        broker.buy(100, symbol)
```

The seventh value is the exact object supplied as `broker`; construction only
checks that its `buy` and `sell` attributes are callable. The library does not
invoke it, infer fills, track inventory, or expose a session summary. Quote
events always update the latest-quote dictionary and are emitted only with
`quotes=True`. As with database markets, `fast=True` reuses the two state dicts.
Option keys are `(root, expiration, right, strike)` tuples. Index values occupy
the data/trade slot, while Forex events occupy the quote slot and therefore
require `quotes=True` to be emitted. Status, FMV, LULD, imbalance, control, and
aggregate events have no database-market tuple slot and are skipped by
`market(...)`; they remain available when iterating parsed messages directly.

## Simple Market Simulation

`SimpleMarket` opens stock trade and quote database files for a date and a
sequence of symbols, then merges all events in SIP timestamp order inside C++.
It is intended for row-at-a-time strategy code that needs current trade/quote
state without bouncing through Python for every lookup.

```python
import massive_speedup

market = massive_speedup.SimpleMarket(
    "2026-01-23",
    ["AAPL", "MSFT"],
    quotes=True,
)
inventory = {"AAPL": 0, "MSFT": 0}

for symbol, timestamp, trade, quote, trades, quotes, broker in market:
    if trade is not None:
        last_quote = quotes.get(symbol)
        if last_quote is not None and trade.price < last_quote.bid_price:
            broker.buy(100)
            inventory[symbol] += 100
    else:
        print("quote update", symbol, timestamp, quote.bid_price, quote.ask_price)

summary = market.summary()
print(inventory)
print(summary["cash_flow"], summary["orders"])
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
sells use the bid. The default simulated latency is 150,000,000 nanoseconds
(150 ms).

Broker calls return `None`; fill prices, fill status, and rejections are hidden
while iteration is in progress. `market.summary()` raises until the iterator is
exhausted, then returns the execution ledger and net cash flow. Strategy code is
responsible for its own intended inventory rather than reading positions from
the market simulator.

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
