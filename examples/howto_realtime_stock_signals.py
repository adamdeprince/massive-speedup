#!/usr/bin/env python3
"""Print paper buy/sell signals from Massive's real-time stock feed.

This is the live counterpart to the row-at-a-time SimpleMarket example. It is
deliberately small: a trade at or below the latest bid opens one unit of caller-
managed inventory, and a trade at or above the latest ask closes it. The
execution endpoint only prints the action; it doesn't place an order.

Set MASSIVE_API_KEY before running:

    python examples/howto_realtime_stock_signals.py TICKER
"""

from __future__ import annotations

import argparse
import sys
from collections.abc import Iterable

import massive_speedup


class PrintSignals:
    def buy(self, quantity: int, symbol: str | None = None) -> None:
        del quantity, symbol
        print("buy", flush=True)

    def sell(self, quantity: int, symbol: str | None = None) -> None:
        del quantity, symbol
        print("sell", flush=True)


def run_signals(market: Iterable[tuple[object, ...]]) -> dict[str, int]:
    inventory: dict[str, int] = {}

    for symbol, _, trade, _, _, quotes, endpoint in market:
        if trade is None:
            continue
        quote = quotes.get(symbol)
        if quote is None:
            continue
        if quote.bid_price <= 0 or quote.ask_price <= 0:
            continue
        if quote.bid_price > quote.ask_price:
            continue

        position = inventory.get(symbol, 0)
        if position == 0 and trade.price <= quote.bid_price:
            endpoint.buy(1, symbol)
            inventory[symbol] = 1
        elif position == 1 and trade.price >= quote.ask_price:
            endpoint.sell(1, symbol)
            inventory[symbol] = 0

    return inventory


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Print paper buy/sell signals from Massive real-time stock ticks."
    )
    parser.add_argument("ticker", help="one stock ticker, for example AAPL")
    parser.add_argument(
        "--url",
        help=("WebSocket endpoint override; the default is wss://socket.massive.com/stocks"),
    )
    parser.add_argument("--timeout", type=float, default=10.0)
    parser.add_argument("--queue-capacity", type=int, default=1024)
    parser.add_argument("--no-reconnect", action="store_true")
    return parser.parse_args(argv[1:])


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    ticker = args.ticker.strip().upper()
    if not ticker or "," in ticker or ticker == "*":
        raise SystemExit("TICKER must name exactly one stock")

    subscriptions = f"T.{ticker},Q.{ticker}"
    signals = PrintSignals()

    try:
        with massive_speedup.WebSocket.Stocks.connect(
            subscriptions,
            url=args.url,
            timeout=args.timeout,
            queue_capacity=args.queue_capacity,
            reconnect=not args.no_reconnect,
        ) as feed:
            market = massive_speedup.WebSocket.Stocks.market(
                feed,
                signals,
                fast=True,
            )
            run_signals(market)
    except KeyboardInterrupt:
        return 130
    except RuntimeError as error:
        raise SystemExit(str(error)) from error

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
