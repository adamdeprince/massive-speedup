#!/usr/bin/env python3
"""Compute daily VWAP for one ticker from a stock trades .csv.gz file."""

import csv
import gzip
import sys
import time

if len(sys.argv) != 3:
    raise SystemExit(f"usage: {sys.argv[0]} trades.csv.gz TICKER")

csv_gzip_path = sys.argv[1]
ticker = sys.argv[2]

weighted_price_sum = 0.0
volume = 0
seen_ticker = False

started_at = time.perf_counter()

with gzip.open(csv_gzip_path, "rt", encoding="utf-8", newline="") as handle:
    for row in csv.DictReader(handle):
        row_ticker = row["ticker"]

        if row_ticker != ticker:
            if seen_ticker or row_ticker > ticker:
                break
            continue

        seen_ticker = True
        size = int(row["size"])
        weighted_price_sum += float(row["price"]) * size
        volume += size

elapsed_seconds = time.perf_counter() - started_at

if volume == 0:
    raise SystemExit(f"no volume found for ticker {ticker!r} in {csv_gzip_path}")

print(f"ticker={ticker}")
print(f"vwap={weighted_price_sum / volume:.10f}")
print(f"volume={volume}")
print(f"elapsed_seconds={elapsed_seconds:.6f}")
