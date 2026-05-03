#!/usr/bin/env python3
"""Compute daily VWAP for one ticker from a massive-speedup database file."""

import sys
import time

import massive_speedup

if len(sys.argv) != 4:
    raise SystemExit(f"usage: {sys.argv[0]} DATABASE_PATH YYYY-MM-DD TICKER")

database_path = sys.argv[1]
date = sys.argv[2]
ticker = sys.argv[3]

started_at = time.perf_counter()

records = massive_speedup.StockTradeDatabase(database_path, date, ticker)
aggregates = massive_speedup.StockTradeAggregator(records, interval_seconds=86_400)
aggregate = next(aggregates, None)

elapsed_seconds = time.perf_counter() - started_at

if aggregate is None or aggregate.volume == 0:
    raise SystemExit(f"no volume found for ticker {ticker!r} on {date} in {database_path}")

print(f"ticker={ticker}")
print(f"date={date}")
print(f"vwap={aggregate.volume_weighted_avg:.10f}")
print(f"volume={aggregate.volume}")
print(f"elapsed_seconds={elapsed_seconds:.6f}")
