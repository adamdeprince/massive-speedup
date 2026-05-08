"""Download massive flat-files for stock trades/quotes and currency quotes."""

from __future__ import annotations

import argparse
import datetime as dt
import multiprocessing.pool
import os
import sys
from pathlib import Path
from threading import local

from tqdm import tqdm

BUCKET = "flatfiles"
ENDPOINT_URL = "https://files.massive.com"

_CLIENT_LOCAL = local()
_AWS_ACCESS_KEY_ID: str | None = None
_AWS_SECRET_ACCESS_KEY: str | None = None


class HelpOnErrorArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        self.print_help(sys.stderr)
        self.exit(2, f"{self.prog}: error: {message}\n")


def build_parser() -> argparse.ArgumentParser:
    parser = HelpOnErrorArgumentParser(
        prog="massive-speedup-download",
        description=(
            "Download stock trades/quotes and currency quotes from massive "
            "flatfiles into a structured directory."
        ),
    )
    parser.add_argument(
        "--download-path",
        "--download_path",
        dest="download_path",
        required=True,
        type=Path,
        help=(
            "Root directory for downloads. Files are placed under "
            "{download-path}/stock_trade, {download-path}/stock_quote, "
            "and {download-path}/currency_quote."
        ),
    )
    parser.add_argument(
        "--threads",
        type=int,
        default=10,
        help="Number of downloader worker threads (default: 10).",
    )
    parser.add_argument(
        "--aws-access-key-id",
        "--aws_access_key_id",
        dest="aws_access_key_id",
        default=os.environ.get("AWS_ACCESS_KEY_ID"),
        help=(
            "Massive/AWS access key ID. Defaults to environment variable "
            "AWS_ACCESS_KEY_ID when set."
        ),
    )
    parser.add_argument(
        "--aws-secret-access-key",
        "--aws_secret_access_key",
        dest="aws_secret_access_key",
        default=os.environ.get("AWS_SECRET_ACCESS_KEY"),
        help=(
            "Massive/AWS secret access key. Defaults to environment variable "
            "AWS_SECRET_ACCESS_KEY when set."
        ),
    )
    parser.add_argument(
        "--end-date",
        type=_parse_iso_date,
        default=_default_end_date(),
        help=(
            "Oldest date to include (inclusive), in YYYY-MM-DD format. "
            "Defaults to one month ago."
        ),
    )
    return parser


def _default_end_date() -> dt.date:
    today = dt.date.today()
    month = today.month - 1
    year = today.year
    if month == 0:
        month = 12
        year -= 1
    day = min(today.day, _days_in_month(year, month))
    return dt.date(year, month, day)


def _days_in_month(year: int, month: int) -> int:
    if month == 12:
        next_month = dt.date(year + 1, 1, 1)
    else:
        next_month = dt.date(year, month + 1, 1)
    return (next_month - dt.timedelta(days=1)).day


def _parse_iso_date(value: str) -> dt.date:
    try:
        return dt.date.fromisoformat(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError(
            f"invalid date '{value}', expected YYYY-MM-DD"
        ) from error


def _build_s3_client():
    import boto3
    from botocore.config import Config

    session = boto3.Session()
    if _AWS_ACCESS_KEY_ID is None or _AWS_SECRET_ACCESS_KEY is None:
        raise RuntimeError("AWS credentials are not configured")
    return session.client(
        "s3",
        endpoint_url=ENDPOINT_URL,
        config=Config(signature_version="s3v4"),
        aws_access_key_id=_AWS_ACCESS_KEY_ID,
        aws_secret_access_key=_AWS_SECRET_ACCESS_KEY,
    )


def _get_thread_client():
    client = getattr(_CLIENT_LOCAL, "s3", None)
    if client is None:
        client = _build_s3_client()
        _CLIENT_LOCAL.s3 = client
    return client


def scan_keys() -> set[str]:
    client = _build_s3_client()
    paginator = client.get_paginator("list_objects_v2")

    keys: set[str] = set()
    for page in paginator.paginate(Bucket=BUCKET):
        for obj in page.get("Contents", ()):
            key = obj["Key"]
            if key.startswith("us_stocks_sip/trades_v1/"):
                keys.add(key)
            elif key.startswith("us_stocks_sip/quotes_v1/"):
                keys.add(key)
            elif key.startswith("global_forex/quotes_v1/"):
                keys.add(key)
    return keys


def _date_key_candidates(current: dt.date) -> tuple[tuple[str, str], tuple[str, str], tuple[str, str]]:
    stem = f"{current.year}-{current.month:02d}-{current.day:02d}.csv.gz"
    month = f"{current.month:02d}"
    return (
        ("stock_trade", f"us_stocks_sip/trades_v1/{current.year}/{month}/{stem}"),
        ("stock_quote", f"us_stocks_sip/quotes_v1/{current.year}/{month}/{stem}"),
        ("currency_quote", f"global_forex/quotes_v1/{current.year}/{month}/{stem}"),
    )


def build_download_jobs(
    download_path: Path,
    keys: set[str],
    *,
    end_date: dt.date,
) -> list[tuple[str, str]]:
    download_path = download_path.expanduser().resolve()
    targets = {
        "stock_trade": download_path / "stock_trade",
        "stock_quote": download_path / "stock_quote",
        "currency_quote": download_path / "currency_quote",
    }
    for directory in targets.values():
        directory.mkdir(parents=True, exist_ok=True)

    jobs: list[tuple[str, str]] = []
    day = dt.date.today() - dt.timedelta(days=1)
    while day >= end_date:
        for category, key in _date_key_candidates(day):
            if key not in keys:
                continue
            output_file = targets[category] / key.rsplit("/", 1)[-1]
            if output_file.exists():
                continue
            jobs.append((str(output_file), key))
        day -= dt.timedelta(days=1)
    return jobs


def _download_one(job: tuple[str, str]) -> str:
    destination, key = job
    _get_thread_client().download_file(BUCKET, key, destination)
    return key


def run_downloads(jobs: list[tuple[str, str]], threads: int) -> None:
    if threads < 1:
        raise ValueError("--threads must be >= 1")

    if not jobs:
        return

    with multiprocessing.pool.ThreadPool(processes=threads) as pool:
        for _ in tqdm(pool.imap(_download_one, jobs), total=len(jobs), unit="file"):
            pass


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if not args.aws_access_key_id or not args.aws_secret_access_key:
        parser.error(
            "missing credentials: provide --aws-access-key-id and "
            "--aws-secret-access-key, or set AWS_ACCESS_KEY_ID and "
            "AWS_SECRET_ACCESS_KEY"
        )

    global _AWS_ACCESS_KEY_ID, _AWS_SECRET_ACCESS_KEY
    _AWS_ACCESS_KEY_ID = args.aws_access_key_id
    _AWS_SECRET_ACCESS_KEY = args.aws_secret_access_key

    end_date = args.end_date
    if end_date > dt.date.today() - dt.timedelta(days=1):
        parser.error("--end-date cannot be in the future")

    keys = scan_keys()
    jobs = build_download_jobs(args.download_path, keys, end_date=end_date)
    run_downloads(jobs, args.threads)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
