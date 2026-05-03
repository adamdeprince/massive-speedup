"""Build packed binary ticker-partitioned flat-file databases."""

from __future__ import annotations

import argparse
import gzip
import sys
import time
from pathlib import Path


class HelpOnErrorArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        self.print_help(sys.stderr)
        self.exit(2, f"{self.prog}: error: {message}\n")


STOCK_QUOTE_HEADER = (
    "ticker,ask_exchange,ask_price,ask_size,bid_exchange,bid_price,bid_size,"
    "conditions,indicators,participant_timestamp,sequence_number,sip_timestamp,"
    "tape,trf_timestamp"
)
STOCK_TRADE_HEADER = (
    "ticker,conditions,correction,exchange,id,participant_timestamp,price,"
    "sequence_number,sip_timestamp,size,tape,trf_id,trf_timestamp"
)
CURRENCY_QUOTE_HEADER = (
    "ticker,ask_exchange,ask_price,bid_exchange,bid_price,participant_timestamp"
)
HEADER_RECORD_TYPES = {
    STOCK_TRADE_HEADER: "stock_trade",
    STOCK_QUOTE_HEADER: "stock_quote",
    CURRENCY_QUOTE_HEADER: "currency_quote",
}


def build_parser() -> argparse.ArgumentParser:
    parser = HelpOnErrorArgumentParser(
        prog="massive-speedup-build-database",
        description="Build a packed binary massive-speedup database from a flat-file CSV gzip.",
    )
    parser.add_argument(
        "input_files",
        nargs="+",
        type=Path,
        help="Input flat-file CSV gzip path(s).",
    )
    parser.add_argument(
        "--database",
        type=Path,
        required=True,
        help="Path to the database root directory.",
    )
    parser.add_argument(
        "--benchmark",
        action="store_true",
        help="Print per-file build throughput metrics to stderr.",
    )
    return parser


def infer_record_type(input_path: Path) -> str:
    with gzip.open(input_path, "rb") as handle:
        header = handle.readline().rstrip(b"\r\n").decode("utf-8")

    if not header:
        raise ValueError(f"input file has no header: {input_path}")

    try:
        return HEADER_RECORD_TYPES[header]
    except KeyError as error:
        raise ValueError(f"unsupported input header in {input_path}: {header}") from error


def write_database_file(input_path: Path, database: Path, record_type: str) -> int:
    import massive_speedup

    try:
        build_database_file = massive_speedup.build_database_file
    except AttributeError as error:
        raise RuntimeError(
            "massive-speedup-build-database requires the native massive_speedup extension"
        ) from error

    return int(build_database_file(input_path, database, record_type))


def _print_benchmark(
    input_file: Path,
    record_type: str,
    lines: int,
    seconds: float,
) -> None:
    megalines_per_second = (lines / 1_000_000) / seconds if seconds > 0 else float("inf")
    print(
        f"file={input_file} | type={record_type} | lines={lines} lines | "
        f"seconds={seconds:.6f} s | throughput={megalines_per_second:.6f} Mlines/s",
        file=sys.stderr,
    )


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    for input_file in args.input_files:
        record_type = infer_record_type(input_file)
        start = time.perf_counter()
        lines = write_database_file(input_file, args.database, record_type)
        seconds = time.perf_counter() - start
        if args.benchmark:
            _print_benchmark(input_file, record_type, lines, seconds)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
