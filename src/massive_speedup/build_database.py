"""Build packed binary ticker-partitioned flat-file databases."""

from __future__ import annotations

import argparse
import datetime as dt
import gzip
import multiprocessing
import os
import shutil
import subprocess
import sys
import tempfile
import time
import zlib
from collections.abc import Iterator
from contextlib import contextmanager
from pathlib import Path

from tqdm import tqdm

from ._paths import DATABASE_PATH_ENV, DOWNLOAD_PATH_ENV
from ._paths import database_path as configured_database_path
from ._paths import download_path as configured_download_path
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
INDEX_VALUE_HEADER = "ticker,value,timestamp"
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
    INDEX_VALUE_HEADER: "index_value",
    OPTION_TRADE_HEADER: "option_trade",
    OPTION_QUOTE_HEADER: "option_quote",
}
FUTURES_EXCHANGES = ("cbot", "cme", "comex", "nymex")
FAMILY_ALIASES = {
    "stock": "stock",
    "stocks": "stock",
    "currency": "currency",
    "currencies": "currency",
    "forex": "currency",
    "crypto": "crypto",
    "cryptos": "crypto",
    "option": "option",
    "options": "option",
    "future": "future",
    "futures": "future",
    "index": "index",
    "indices": "index",
}
CATEGORY_FAMILIES = {
    "stock_trade": "stock",
    "stock_quote": "stock",
    "currency_quote": "currency",
    "crypto_trade": "crypto",
    "option_trade": "option",
    "option_quote": "option",
    "index_value": "index",
    "future_trade": "future",
    "future_quote": "future",
    **{
        f"future_{exchange}_{kind}": "future"
        for exchange in FUTURES_EXCHANGES
        for kind in ("trade", "quote")
    },
}
DEFAULT_BLOCK_SIZE = 1 << 20
DEFAULT_EXTERNAL_SORT_PATH = Path("/var/tmp")
DIRECTORY_COMPLETE_MARKER = ".massive-speedup.complete"
DIRECTORY_INCOMPLETE_MARKER = ".massive-speedup.incomplete"
BuildTask = tuple[Path, Path, bool]
BuildResult = tuple[Path, str | None, int, float, str | None]
_WRITE_BLOCK_LOCK: object | None = None
_IO_BLOCK_SIZE = DEFAULT_BLOCK_SIZE
_READER_PARALLELIZATION = 1
_BSORT_EXECUTABLE: Path | None = None
_EXTERNAL_SORT_PATH = DEFAULT_EXTERNAL_SORT_PATH


def _positive_int(value: str) -> int:
    parsed = int(value)
    if parsed < 1:
        raise argparse.ArgumentTypeError("must be at least 1")
    return parsed


def _block_size(value: str) -> int:
    normalized = value.strip().lower()
    suffixes = {
        "gib": 1 << 30,
        "gb": 1 << 30,
        "g": 1 << 30,
        "mib": 1 << 20,
        "mb": 1 << 20,
        "m": 1 << 20,
        "kib": 1 << 10,
        "kb": 1 << 10,
        "k": 1 << 10,
        "b": 1,
    }
    multiplier = 1
    for suffix in suffixes:
        if normalized.endswith(suffix):
            normalized = normalized[: -len(suffix)]
            multiplier = suffixes[suffix]
            break
    try:
        parsed = int(normalized) * multiplier
    except ValueError as error:
        raise argparse.ArgumentTypeError(
            "must be a byte count or a size such as 256KiB, 1MiB, or 8MiB"
        ) from error
    if parsed < 8 << 10:
        raise argparse.ArgumentTypeError("must be at least 8 KiB")
    return parsed


def _format_block_size(size: int) -> str:
    for suffix, multiplier in (("GiB", 1 << 30), ("MiB", 1 << 20), ("KiB", 1 << 10)):
        if size % multiplier == 0:
            return f"{size // multiplier} {suffix}"
    return f"{size} bytes"


def _family(value: str) -> str:
    normalized = value.strip().lower()
    try:
        return FAMILY_ALIASES[normalized]
    except KeyError as error:
        choices = ", ".join(("stock", "currency", "crypto", "option", "future", "index"))
        raise argparse.ArgumentTypeError(
            f"unknown family '{value}', expected one of: {choices}"
        ) from error


def _iso_date(value: str) -> dt.date:
    try:
        parsed = dt.date.fromisoformat(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError(
            "must be a valid date in YYYY-MM-DD format"
        ) from error
    if parsed.isoformat() != value:
        raise argparse.ArgumentTypeError(
            "must be a valid date in YYYY-MM-DD format"
        )
    return parsed


def build_parser() -> argparse.ArgumentParser:
    parser = HelpOnErrorArgumentParser(
        prog="massive-speedup-build-database",
        description="Build a packed binary massive-speedup database from a flat-file CSV gzip.",
    )
    parser.add_argument(
        "input_files",
        nargs="*",
        type=Path,
        help=(
            "Input flat-file CSV gzip path(s) or directories. Defaults to "
            f"{DOWNLOAD_PATH_ENV}."
        ),
    )
    parser.add_argument(
        "--database-path",
        "--database",
        dest="database_path",
        type=Path,
        default=None,
        help=(
            "Path to the database root directory. Defaults to "
            f"{DATABASE_PATH_ENV}."
        ),
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
    parser.add_argument(
        "--processes",
        type=_positive_int,
        default=1,
        metavar="N",
        help="Maximum worker processes. Defaults to %(default)s.",
    )
    parser.add_argument(
        "--only",
        type=_family,
        metavar="FAMILY",
        help=(
            "Build only one family: stock, currency, crypto, option, future, or "
            "index. Singular and plural names are accepted."
        ),
    )
    parser.add_argument(
        "--not-before",
        type=_iso_date,
        metavar="YYYY-MM-DD",
        help=(
            "Process only input files dated on or after this date. "
            "The date is read from the input filename."
        ),
    )
    parser.add_argument(
        "--lockstep-writer",
        action="store_true",
        help=(
            "Allow only one worker process at a time to flush an output "
            "block, while reads and decompression remain parallel."
        ),
    )
    parser.add_argument(
        "--block-size",
        type=_block_size,
        default=DEFAULT_BLOCK_SIZE,
        metavar="SIZE",
        help=(
            "Size of each double-buffered read block and lockstep write block. "
            "Accepts bytes or K/M/G suffixes. Defaults to 1MiB."
        ),
    )
    parser.add_argument(
        "--bsort",
        action="store_true",
        help=(
            "Stream unsorted records to local temporary files, sort them in "
            "place with the external bsort executable, then publish them to "
            "the database. The default is an internal stable sort."
        ),
    )
    parser.add_argument(
        "--external-sort-path",
        type=Path,
        default=DEFAULT_EXTERNAL_SORT_PATH,
        metavar="PATH",
        help=(
            "Local staging directory used with --bsort. "
            "Defaults to /var/tmp."
        ),
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


def write_database_file(
    input_path: Path,
    record_type: str,
    *,
    database_path: str | os.PathLike[str] | None = None,
    force: bool = False,
    write_lock: object | None = None,
    block_size: int = DEFAULT_BLOCK_SIZE,
    reader_parallelization: int = 1,
    sort_records: bool = True,
) -> int:
    import massive_speedup

    try:
        build_database_file = massive_speedup.build_database_file
    except AttributeError as error:
        raise RuntimeError(
            "massive-speedup-build-database requires the native massive_speedup extension"
        ) from error

    arguments = {
        "database_path": configured_database_path(database_path),
        "force": force,
    }
    if write_lock is not None:
        arguments["write_lock"] = write_lock
    if block_size != DEFAULT_BLOCK_SIZE:
        arguments["block_size"] = block_size
    if reader_parallelization != 1:
        arguments["reader_parallelization"] = reader_parallelization
    if not sort_records:
        arguments["sort_records"] = False
    return int(build_database_file(input_path, record_type, **arguments))


def write_database_file_inferred(
    input_path: Path,
    *,
    database_path: str | os.PathLike[str] | None = None,
    force: bool = False,
    write_lock: object | None = None,
    block_size: int = DEFAULT_BLOCK_SIZE,
    reader_parallelization: int = 1,
    sort_records: bool = True,
) -> tuple[str, int]:
    import massive_speedup

    try:
        build_database_file_inferred = massive_speedup.build_database_file_inferred
    except AttributeError as error:
        raise RuntimeError(
            "massive-speedup-build-database requires the native massive_speedup extension"
        ) from error

    arguments = {
        "database_path": configured_database_path(database_path),
        "force": force,
    }
    if write_lock is not None:
        arguments["write_lock"] = write_lock
    if block_size != DEFAULT_BLOCK_SIZE:
        arguments["block_size"] = block_size
    if reader_parallelization != 1:
        arguments["reader_parallelization"] = reader_parallelization
    if not sort_records:
        arguments["sort_records"] = False
    record_type, rows_written = build_database_file_inferred(input_path, **arguments)
    return str(record_type), int(rows_written)


def write_database_file_inferred_with_stats(
    input_path: Path,
    *,
    database_path: str | os.PathLike[str] | None = None,
    force: bool = False,
    write_lock: object | None = None,
    block_size: int = DEFAULT_BLOCK_SIZE,
    reader_parallelization: int = 1,
    sort_records: bool = True,
) -> tuple[str, int, int]:
    import massive_speedup

    try:
        build_database_file_inferred_with_stats = (
            massive_speedup.build_database_file_inferred_with_stats
        )
    except AttributeError as error:
        raise RuntimeError(
            "massive-speedup-build-database requires the native massive_speedup extension"
        ) from error

    arguments = {
        "database_path": configured_database_path(database_path),
        "force": force,
    }
    if write_lock is not None:
        arguments["write_lock"] = write_lock
    if block_size != DEFAULT_BLOCK_SIZE:
        arguments["block_size"] = block_size
    if reader_parallelization != 1:
        arguments["reader_parallelization"] = reader_parallelization
    if not sort_records:
        arguments["sort_records"] = False
    record_type, rows_written, rows_processed = (
        build_database_file_inferred_with_stats(input_path, **arguments)
    )
    return str(record_type), int(rows_written), int(rows_processed)


def _input_family(input_path: Path) -> str | None:
    record_type = _input_record_type(input_path)
    if record_type is None:
        return None
    return CATEGORY_FAMILIES[record_type]


def _input_record_type(input_path: Path) -> str | None:
    for part in reversed(input_path.parts[:-1]):
        record_type = part.lower()
        if record_type in CATEGORY_FAMILIES:
            return record_type
    return None


def _input_date_directory_name(input_path: Path) -> str | None:
    if len(input_path.name) != len("YYYY-MM-DD.csv.gz"):
        return None
    if not input_path.name.endswith(".csv.gz"):
        return None
    candidate = input_path.name[:10]
    try:
        parsed = dt.date.fromisoformat(candidate)
    except ValueError:
        return None
    if parsed.isoformat() != candidate:
        return None
    return candidate


def _input_processing_order(input_path: Path) -> tuple[int, int]:
    date_directory = _input_date_directory_name(input_path)
    if date_directory is None:
        return 1, 0
    date_ordinal = dt.date.fromisoformat(date_directory).toordinal()
    return 0, -date_ordinal


def _target_directory(
    input_path: Path,
    database_path: Path,
) -> tuple[str, Path] | None:
    record_type = _input_record_type(input_path)
    date_directory = _input_date_directory_name(input_path)
    if record_type is None or date_directory is None:
        return None
    return record_type, database_path / record_type / date_directory


def _directory_has_complete_outputs(target_directory: Path) -> bool:
    if not target_directory.is_dir():
        return False

    def raise_walk_error(error: OSError) -> None:
        raise error

    has_output = False
    try:
        for _root, directories, filenames in os.walk(
            target_directory,
            onerror=raise_walk_error,
        ):
            if any(name.endswith(".incomplete") for name in directories):
                return False
            for filename in filenames:
                if filename.endswith(".incomplete"):
                    return False
                has_output = True
    except OSError:
        return False
    return has_output


def _complete_target_directory(
    task: BuildTask,
) -> tuple[str, Path] | None:
    input_path, database_path, force = task
    if force:
        return None
    target = _target_directory(input_path, database_path)
    if target is None:
        return None
    if not _directory_has_complete_outputs(target[1]):
        return None
    return target


def expand_input_files(
    input_paths: list[Path],
    *,
    only_family: str | None = None,
    not_before: dt.date | None = None,
) -> list[Path]:
    expanded: list[Path] = []
    seen: set[Path] = set()

    def append(candidate: Path) -> None:
        if candidate in seen:
            return
        if only_family is not None:
            family = _input_family(candidate)
            if family is None:
                tqdm.write(
                    f"Skipping uncategorized input for --only {only_family}: {candidate}"
                )
                return
            if family != only_family:
                return
        date_directory = _input_date_directory_name(candidate)
        if date_directory is None:
            tqdm.write(
                "Skipping input without a valid YYYY-MM-DD.csv.gz filename: "
                f"{candidate}"
            )
            return
        if not_before is not None:
            if dt.date.fromisoformat(date_directory) < not_before:
                return
        seen.add(candidate)
        expanded.append(candidate)

    for path in input_paths:
        resolved = path.expanduser().resolve()
        if resolved.is_dir():
            for nested in sorted(
                candidate
                for candidate in resolved.rglob("*")
                if candidate.is_file() and candidate.name.endswith("csv.gz")
            ):
                append(nested)
            continue
        if resolved.is_file():
            append(resolved)
            continue
        tqdm.write(f"Skipping missing path: {path}")
    expanded.sort(key=_input_processing_order)
    return expanded


def is_malformed_gzip_error(error: BaseException) -> bool:
    return isinstance(error, (gzip.BadGzipFile, EOFError, zlib.error)) or str(error) == (
        "Failed to detect a valid file format."
    )


def is_input_header_error(error: BaseException) -> bool:
    message = str(error)
    return message.startswith("input file has no header: ") or message.startswith(
        "unsupported input header in "
    )


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


def _resolve_bsort_executable() -> Path:
    discovered = shutil.which("bsort")
    if discovered is not None:
        return Path(discovered).resolve()

    beside_python = Path(sys.executable).with_name("bsort")
    if beside_python.is_file() and os.access(beside_python, os.X_OK):
        return beside_python.resolve()

    raise FileNotFoundError(
        "bsort was not found on PATH or beside the active Python executable; "
        "install https://github.com/adamdeprince/bsort before using --bsort"
    )


def _packed_record_size(record_type: str) -> int:
    import massive_speedup

    class_names = {
        "stock_trade": "StockTrade",
        "stock_quote": "StockQuote",
        "crypto_trade": "CryptoTrade",
        "currency_quote": "CurrencyQuote",
        "index_value": "IndexValue",
        "option_trade": "OptionTrade",
        "option_quote": "OptionQuote",
    }
    class_name = class_names.get(record_type)
    if class_name is None and record_type.startswith("future_"):
        if record_type.endswith("_trade"):
            class_name = "FuturesTrade"
        elif record_type.endswith("_quote"):
            class_name = "FuturesQuote"
    if class_name is None:
        raise ValueError(f"unsupported database record type: {record_type}")

    row_type = getattr(massive_speedup, class_name)
    return int(row_type.packed_size)


def _run_bsort(
    executable: Path,
    staged_file: Path,
    record_size: int,
) -> None:
    command = [
        str(executable),
        "-r",
        str(record_size),
        "-k",
        "8",
        str(staged_file),
    ]
    try:
        completed = subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
        )
    except OSError as error:
        raise RuntimeError(f"unable to execute bsort: {error}") from error

    if completed.returncode == 0:
        return

    detail = completed.stderr.strip() or completed.stdout.strip()
    if detail:
        raise RuntimeError(
            f"bsort failed for {staged_file} with exit status "
            f"{completed.returncode}: {detail}"
        )
    raise RuntimeError(
        f"bsort failed for {staged_file} with exit status {completed.returncode}"
    )


@contextmanager
def _held_write_lock(lock: object | None) -> Iterator[None]:
    if lock is None:
        yield
        return

    lock.acquire()
    try:
        yield
    finally:
        lock.release()


def _write_all(output, data: bytes) -> None:
    remaining = memoryview(data)
    while remaining:
        written = output.write(remaining)
        if written is None or written <= 0:
            raise OSError("failed to write sorted database block")
        remaining = remaining[written:]


def _drop_file_cache(path: Path) -> None:
    posix_fadvise = getattr(os, "posix_fadvise", None)
    dontneed = getattr(os, "POSIX_FADV_DONTNEED", None)
    if posix_fadvise is None or dontneed is None:
        return

    try:
        descriptor = os.open(path, os.O_RDONLY)
    except OSError:
        return
    try:
        posix_fadvise(descriptor, 0, 0, dontneed)
    except OSError:
        pass
    finally:
        os.close(descriptor)


def _remove_incomplete_outputs(
    target_directory: Path,
    *,
    keep: Path,
    write_lock: object | None,
) -> None:
    try:
        incomplete_files = [
            candidate
            for candidate in target_directory.rglob("*.incomplete")
            if candidate != keep and (candidate.is_file() or candidate.is_symlink())
        ]
    except OSError:
        return

    for candidate in incomplete_files:
        with _held_write_lock(write_lock):
            candidate.unlink(missing_ok=True)


@contextmanager
def _target_directory_build(
    input_file: Path,
    database: Path,
    *,
    write_lock: object | None,
) -> Iterator[None]:
    target = _target_directory(input_file, database)
    if target is None:
        yield
        return

    _, target_directory = target
    complete = target_directory / DIRECTORY_COMPLETE_MARKER
    incomplete = target_directory / DIRECTORY_INCOMPLETE_MARKER
    with _held_write_lock(write_lock):
        target_directory.mkdir(parents=True, exist_ok=True)
        if complete.exists():
            os.replace(complete, incomplete)
        else:
            incomplete.touch()

    _remove_incomplete_outputs(
        target_directory,
        keep=incomplete,
        write_lock=write_lock,
    )

    try:
        yield
    except BaseException:
        raise
    else:
        with _held_write_lock(write_lock):
            os.replace(incomplete, complete)


def _publish_sorted_file(
    staged_file: Path,
    destination: Path,
    *,
    force: bool,
    write_lock: object | None,
    block_size: int,
) -> bool:
    incomplete = destination.with_name(destination.name + ".incomplete")
    with _held_write_lock(write_lock):
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not force and destination.exists():
            return False
        output = incomplete.open("wb", buffering=0)

    try:
        with staged_file.open("rb", buffering=0) as source:
            while block := source.read(block_size):
                with _held_write_lock(write_lock):
                    _write_all(output, block)
        _drop_file_cache(staged_file)
    except BaseException:
        _drop_file_cache(staged_file)
        with _held_write_lock(write_lock):
            output.close()
            _drop_file_cache(incomplete)
        raise

    with _held_write_lock(write_lock):
        output.close()
        _drop_file_cache(incomplete)
        if not force and destination.exists():
            incomplete.unlink(missing_ok=True)
            return False
        os.replace(incomplete, destination)
    return True


def _build_database_with_bsort(
    input_file: Path,
    database: Path,
    *,
    force: bool,
    executable: Path,
    external_sort_path: Path,
    write_lock: object | None,
    block_size: int,
    reader_parallelization: int,
) -> tuple[str, int]:
    with tempfile.TemporaryDirectory(
        prefix="input-",
        dir=external_sort_path,
    ) as temporary_directory:
        staging_database = Path(temporary_directory) / "database"
        record_type, _, rows_processed = write_database_file_inferred_with_stats(
            input_file,
            database_path=staging_database,
            force=True,
            block_size=block_size,
            reader_parallelization=reader_parallelization,
            sort_records=False,
        )
        record_size = _packed_record_size(record_type)
        staged_root = staging_database / record_type
        if not staged_root.exists():
            return record_type, rows_processed

        staged_files = sorted(
            path
            for path in staged_root.rglob("*")
            if path.is_file() and not path.name.endswith(".incomplete")
        )
        for staged_file in staged_files:
            destination = database / staged_file.relative_to(staging_database)
            if not force and destination.exists():
                continue

            byte_size = staged_file.stat().st_size
            if byte_size % record_size != 0:
                raise RuntimeError(
                    f"staged database file {staged_file} has {byte_size} bytes, "
                    f"which is not a multiple of its {record_size}-byte record size"
                )

            _run_bsort(executable, staged_file, record_size)
            _publish_sorted_file(
                staged_file,
                destination,
                force=force,
                write_lock=write_lock,
                block_size=block_size,
            )

        return record_type, rows_processed


def _process_input_file(
    task: BuildTask,
) -> BuildResult:
    input_file, database, force = task
    start = time.perf_counter()
    try:
        with _target_directory_build(
            input_file,
            database,
            write_lock=_WRITE_BLOCK_LOCK,
        ):
            if _BSORT_EXECUTABLE is None:
                arguments = {"database_path": database, "force": force}
                if _WRITE_BLOCK_LOCK is not None:
                    arguments["write_lock"] = _WRITE_BLOCK_LOCK
                if _IO_BLOCK_SIZE != DEFAULT_BLOCK_SIZE:
                    arguments["block_size"] = _IO_BLOCK_SIZE
                if _READER_PARALLELIZATION != 1:
                    arguments["reader_parallelization"] = _READER_PARALLELIZATION
                record_type, _, lines = write_database_file_inferred_with_stats(
                    input_file,
                    **arguments,
                )
            else:
                record_type, lines = _build_database_with_bsort(
                    input_file,
                    database,
                    force=force,
                    executable=_BSORT_EXECUTABLE,
                    external_sort_path=_EXTERNAL_SORT_PATH,
                    write_lock=_WRITE_BLOCK_LOCK,
                    block_size=_IO_BLOCK_SIZE,
                    reader_parallelization=_READER_PARALLELIZATION,
                )
    except ValueError as error:
        seconds = time.perf_counter() - start
        if is_input_header_error(error):
            return input_file, None, 0, seconds, f"Skipping {input_file}: {error}"
        if is_malformed_gzip_error(error):
            return (
                input_file,
                None,
                0,
                seconds,
                f"Skipping malformed gzip {input_file}: {error}",
            )
        raise
    except Exception as error:
        if is_malformed_gzip_error(error):
            seconds = time.perf_counter() - start
            return (
                input_file,
                None,
                0,
                seconds,
                f"Skipping malformed gzip {input_file}: {error}",
            )
        raise
    seconds = time.perf_counter() - start
    return input_file, record_type, lines, seconds, None


def _initialize_worker(
    write_block_lock: object | None = None,
    block_size: int = DEFAULT_BLOCK_SIZE,
    reader_parallelization: int = 1,
    bsort_executable: Path | None = None,
    external_sort_path: Path = DEFAULT_EXTERNAL_SORT_PATH,
) -> None:
    global _BSORT_EXECUTABLE
    global _EXTERNAL_SORT_PATH
    global _IO_BLOCK_SIZE
    global _READER_PARALLELIZATION
    global _WRITE_BLOCK_LOCK
    _WRITE_BLOCK_LOCK = write_block_lock
    _IO_BLOCK_SIZE = block_size
    _READER_PARALLELIZATION = reader_parallelization
    _BSORT_EXECUTABLE = bsort_executable
    _EXTERNAL_SORT_PATH = external_sort_path
    set_process_title("massive-builddb-worker")


def _process_tasks(
    tasks: list[BuildTask],
    processes: int,
    *,
    lockstep_writer: bool = False,
    block_size: int = DEFAULT_BLOCK_SIZE,
    bsort_executable: Path | None = None,
    external_sort_path: Path = DEFAULT_EXTERNAL_SORT_PATH,
) -> Iterator[BuildResult]:
    global _BSORT_EXECUTABLE
    global _EXTERNAL_SORT_PATH
    global _IO_BLOCK_SIZE
    global _READER_PARALLELIZATION
    global _WRITE_BLOCK_LOCK
    if not tasks:
        return

    worker_count = min(processes, len(tasks))
    total_reader_parallelization = min(processes, os.cpu_count() or 1)
    reader_parallelization = max(1, total_reader_parallelization // worker_count)
    if processes == 1:
        previous_lock = _WRITE_BLOCK_LOCK
        previous_block_size = _IO_BLOCK_SIZE
        previous_reader_parallelization = _READER_PARALLELIZATION
        previous_bsort_executable = _BSORT_EXECUTABLE
        previous_external_sort_path = _EXTERNAL_SORT_PATH
        _WRITE_BLOCK_LOCK = None
        _IO_BLOCK_SIZE = block_size
        _READER_PARALLELIZATION = reader_parallelization
        _BSORT_EXECUTABLE = bsort_executable
        _EXTERNAL_SORT_PATH = external_sort_path
        try:
            yield from map(_process_input_file, tasks)
        finally:
            _WRITE_BLOCK_LOCK = previous_lock
            _IO_BLOCK_SIZE = previous_block_size
            _READER_PARALLELIZATION = previous_reader_parallelization
            _BSORT_EXECUTABLE = previous_bsort_executable
            _EXTERNAL_SORT_PATH = previous_external_sort_path
        return

    process_context = multiprocessing.get_context("spawn")
    write_block_lock = process_context.Lock() if lockstep_writer else None
    pool = process_context.Pool(
        processes=worker_count,
        initializer=_initialize_worker,
        initargs=(
            write_block_lock,
            block_size,
            reader_parallelization,
            bsort_executable,
            external_sort_path,
        ),
        maxtasksperchild=1,
    )
    try:
        yield from pool.imap_unordered(_process_input_file, tasks, chunksize=1)
    except BaseException:
        pool.terminate()
        pool.join()
        raise
    else:
        pool.close()
        pool.join()


def _write_interrupted_message() -> None:
    tqdm.write(
        "Interrupted: complete database files were left intact. "
        "Any .incomplete output remains a rewrite marker and is never "
        "opened as a database file."
    )


def _run(args: argparse.Namespace, parser: argparse.ArgumentParser) -> int:
    try:
        database_path = configured_database_path(args.database_path)
        input_paths = args.input_files or [configured_download_path()]
    except RuntimeError as error:
        parser.error(str(error))
    discovered_tasks = [
        (input_file, database_path, args.force)
        for input_file in expand_input_files(
            input_paths,
            only_family=args.only,
            not_before=args.not_before,
        )
    ]
    tasks: list[BuildTask] = []
    complete_targets: list[tuple[Path, str, Path]] = []
    for task in discovered_tasks:
        complete_target = _complete_target_directory(task)
        if complete_target is None:
            tasks.append(task)
            continue
        record_type, target_directory = complete_target
        complete_targets.append((task[0], record_type, target_directory))

    if complete_targets:
        tqdm.write(
            f"Skipping {len(complete_targets)} complete target "
            f"director{'y' if len(complete_targets) == 1 else 'ies'} "
            "without opening the source gzip file(s)"
        )

    if not tasks:
        return 0

    if args.processes == 1:
        family = f" {args.only}" if args.only else ""
        tqdm.write(f"Building {len(tasks)}{family} file(s) serially")
    else:
        worker_count = min(args.processes, len(tasks))
        family = f" {args.only}" if args.only else ""
        tqdm.write(
            f"Building {len(tasks)}{family} file(s) with "
            f"{worker_count} worker process(es)"
        )
    if args.lockstep_writer:
        tqdm.write(
            "Lockstep writer enabled: one worker flushes a "
            f"{_format_block_size(args.block_size)} block at a time"
        )

    bsort_executable: Path | None = None
    external_sort_run_path: Path | None = None
    if args.bsort:
        try:
            bsort_executable = _resolve_bsort_executable()
            external_sort_root = args.external_sort_path.expanduser().resolve()
            external_sort_root.mkdir(parents=True, exist_ok=True)
            external_sort_run_path = Path(
                tempfile.mkdtemp(
                    prefix="massive-speedup-sort-run-",
                    dir=external_sort_root,
                )
            )
        except OSError as error:
            parser.error(str(error))
        tqdm.write(
            f"External bsort enabled: staging under {external_sort_run_path}"
        )
    else:
        tqdm.write("Internal stable sort enabled")

    results = _process_tasks(
        tasks,
        args.processes,
        lockstep_writer=args.lockstep_writer,
        block_size=args.block_size,
        bsort_executable=bsort_executable,
        external_sort_path=external_sort_run_path or DEFAULT_EXTERNAL_SORT_PATH,
    )
    progress = tqdm(results, total=len(tasks), unit="file")
    try:
        for result in progress:
            input_file, record_type, lines, seconds, error_message = result
            if record_type is None:
                tqdm.write(error_message or f"Skipping {input_file}")
                continue
            if lines < 0:
                tqdm.write(
                    f"Skipping malformed gzip {input_file}: "
                    f"{error_message or 'failure while decoding compressed rows'}"
                )
                continue
            if args.benchmark:
                _print_benchmark(input_file, record_type, lines, seconds)
    except KeyboardInterrupt:
        _write_interrupted_message()
        return 130
    finally:
        progress.close()
        close_results = getattr(results, "close", None)
        if close_results is not None:
            close_results()
        if external_sort_run_path is not None:
            shutil.rmtree(external_sort_run_path, ignore_errors=True)

    return 0


def main(argv: list[str] | None = None) -> int:
    set_process_title("massive-builddb")
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return _run(args, parser)
    except KeyboardInterrupt:
        _write_interrupted_message()
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
