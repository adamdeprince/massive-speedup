"""Build packed binary ticker-partitioned flat-file databases."""

from __future__ import annotations

import argparse
import datetime as dt
import gzip
import sys
import time
import zlib
from pathlib import Path

from tqdm import tqdm

from ._process_title import set_process_title


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
CRYPTO_TRADE_HEADER = (
    "ticker,conditions,exchange,id,participant_timestamp,price,size"
)
OPTION_TRADE_HEADER = (
    "ticker,conditions,correction,exchange,price,sip_timestamp,size"
)
OPTION_QUOTE_HEADER = (
    "ticker,ask_exchange,ask_price,ask_size,bid_exchange,bid_price,bid_size,"
    "sequence_number,sip_timestamp"
)
FUTURES_TRADE_HEADER = (
    "ticker,timestamp,sequence_number,report_sequence,price,size,correction,"
    "exchange,session_end_date"
)
FUTURES_QUOTE_HEADER = (
    "ticker,timestamp,sequence_number,report_sequence,ask_timestamp,ask_price,"
    "ask_size,bid_timestamp,bid_price,bid_size,exchange,session_end_date"
)
HEADER_RECORD_TYPES = {
    STOCK_TRADE_HEADER: "stock_trade",
    STOCK_QUOTE_HEADER: "stock_quote",
    CURRENCY_QUOTE_HEADER: "currency_quote",
    CRYPTO_TRADE_HEADER: "crypto_trade",
    OPTION_TRADE_HEADER: "option_trade",
    OPTION_QUOTE_HEADER: "option_quote",
}
FUTURES_EXCHANGES = ("cbot", "cme", "comex", "nymex")


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
        help="Print per-file build throughput metrics.",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Overwrite existing per-symbol database files.",
    )
    return parser


def _infer_futures_exchange(input_path: Path) -> str | None:
    for part in reversed(input_path.parts):
        lowered = part.lower()
        for exchange in FUTURES_EXCHANGES:
            if lowered in {
                f"future_{exchange}",
                f"future_{exchange}_trade",
                f"future_{exchange}_quote",
                f"us_futures_{exchange}",
            }:
                return exchange
    return None


def _futures_record_type(input_path: Path, kind: str) -> str:
    exchange = _infer_futures_exchange(input_path)
    if exchange is None:
        return f"future_{kind}"
    return f"future_{exchange}_{kind}"


def infer_record_type(input_path: Path) -> str:
    with gzip.open(input_path, "rb") as handle:
        header = handle.readline().rstrip(b"\r\n").decode("utf-8")

    if not header:
        raise ValueError(f"input file has no header: {input_path}")

    if header == FUTURES_TRADE_HEADER:
        return _futures_record_type(input_path, "trade")
    if header == FUTURES_QUOTE_HEADER:
        return _futures_record_type(input_path, "quote")

    try:
        return HEADER_RECORD_TYPES[header]
    except KeyError as error:
        raise ValueError(f"unsupported input header in {input_path}: {header}") from error


def infer_record_date(input_path: Path, record_type: str) -> str | None:
    del record_type
    filename = input_path.name
    try:
        return dt.date.fromisoformat(filename[:10]).isoformat()
    except ValueError as error:
        raise ValueError(
            f"database input filename must begin with YYYY-MM-DD: {input_path}"
        ) from error


def prepare_incomplete_marker(
    input_path: Path,
    database: Path,
    record_type: str,
    *,
    force: bool,
) -> tuple[Path | None, bool]:
    record_date = infer_record_date(input_path, record_type)
    if record_date is None:
        return None, False

    target_date_dir = database / record_type / record_date
    marker = target_date_dir / ".incomplete"

    if not target_date_dir.exists():
        target_date_dir.mkdir(parents=True, exist_ok=True)
        marker.touch(exist_ok=True)
        return marker, False

    if force:
        marker.touch(exist_ok=True)
        return marker, False

    if marker.exists():
        return marker, False

    return None, True


def write_database_file(
    input_path: Path,
    database: Path,
    record_type: str,
    *,
    force: bool = False,
) -> int:
    import massive_speedup

    try:
        build_database_file = massive_speedup.build_database_file
    except AttributeError as error:
        raise RuntimeError(
            "massive-speedup-build-database requires the native massive_speedup extension"
        ) from error

    return int(build_database_file(input_path, database, record_type, force=force))


def expand_input_files(input_paths: list[Path]) -> list[Path]:
    expanded: list[Path] = []
    seen: set[Path] = set()
    for path in input_paths:
        resolved = path.expanduser().resolve()
        if resolved.is_dir():
            for nested in sorted(
                candidate
                for candidate in resolved.rglob("*")
                if candidate.is_file() and candidate.name.endswith("csv.gz")
            ):
                if nested in seen:
                    continue
                seen.add(nested)
                expanded.append(nested)
            continue
        if resolved.is_file():
            if resolved not in seen:
                seen.add(resolved)
                expanded.append(resolved)
            continue
        tqdm.write(f"Skipping missing path: {path}")
    return expanded


def is_malformed_gzip_error(error: BaseException) -> bool:
    return isinstance(error, (gzip.BadGzipFile, EOFError, zlib.error))


def _print_benchmark(
    input_file: Path,
    record_type: str,
    lines: int,
    seconds: float,
) -> None:
    megalines_per_second = (lines / 1_000_000) / seconds if seconds > 0 else float("inf")
    tqdm.write(
        f"file={input_file} | type={record_type} | lines={lines} lines | "
        f"seconds={seconds:.6f} s | throughput={megalines_per_second:.6f} Mlines/s"
    )


def _process_input_file(
    task: tuple[Path, Path, str, bool, Path | None],
) -> tuple[Path, str, int, float, str | None]:
    input_file, database, record_type, force, marker = task
    start = time.perf_counter()
    try:
        lines = write_database_file(
            input_file,
            database,
            record_type,
            force=force,
        )
    except Exception as error:
        if is_malformed_gzip_error(error):
            seconds = time.perf_counter() - start
            return input_file, record_type, -1, seconds, str(error)
        raise
    else:
        if marker is not None and marker.exists():
            marker.unlink()
    seconds = time.perf_counter() - start
    return input_file, record_type, lines, seconds, None


def main(argv: list[str] | None = None) -> int:
    set_process_title("massive-builddb")
    parser = build_parser()
    args = parser.parse_args(argv)
    tasks: list[tuple[Path, Path, str, bool, Path | None]] = []
    for input_file in expand_input_files(args.input_files):
        try:
            record_type = infer_record_type(input_file)
        except ValueError as error:
            tqdm.write(f"Skipping {input_file}: {error}")
            continue
        except Exception as error:
            if is_malformed_gzip_error(error):
                tqdm.write(f"Skipping malformed gzip {input_file}: {error}")
                continue
            raise

        try:
            marker, skip = prepare_incomplete_marker(
                input_file,
                args.database,
                record_type,
                force=args.force,
            )
        except Exception as error:
            if is_malformed_gzip_error(error):
                tqdm.write(f"Skipping malformed gzip {input_file}: {error}")
                continue
            raise

        if skip:
            continue
        tasks.append((input_file, args.database, record_type, args.force, marker))

    if not tasks:
        return 0

    for result in tqdm(
        (_process_input_file(task) for task in tasks),
        total=len(tasks),
        unit="file",
    ):
        input_file, record_type, lines, seconds, error_message = result
        if lines < 0:
            tqdm.write(
                f"Skipping malformed gzip {input_file}: "
                f"{error_message or 'failure while decoding compressed rows'}"
            )
            continue
        if args.benchmark:
            _print_benchmark(input_file, record_type, lines, seconds)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
