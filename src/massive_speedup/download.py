"""Download massive flat-files for selected trades/quotes products."""

from __future__ import annotations

import argparse
import datetime as dt
import os
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode
from urllib.request import urlopen

from tqdm import tqdm

from ._process_title import set_process_title

BUCKET = "flatfiles"
ENDPOINT_URL = "https://files.massive.com"
MASSIVE_API_URL = "https://api.massive.com"
ALL_PRODUCTS = ("stocks", "currencies", "futures", "crypto", "options", "indices")
PRODUCT_ALIASES = {
    "stock": "stocks",
    "stocks": "stocks",
    "currency": "currencies",
    "currencies": "currencies",
    "future": "futures",
    "futures": "futures",
    "crypto": "crypto",
    "cryptos": "crypto",
    "option": "options",
    "options": "options",
    "index": "indices",
    "indices": "indices",
}
FUTURES_EXCHANGES = ("cbot", "cme", "comex", "nymex")
FUTURES_PROBE_SYMBOLS = {
    "cbot": "ZC",
    "cme": "ES",
    "comex": "GC",
    "nymex": "CL",
}
FUTURES_PROBE_MONTHS = {
    "cbot": (3, 5, 7, 9, 12),
    "cme": (3, 6, 9, 12),
    "comex": tuple(range(1, 13)),
    "nymex": tuple(range(1, 13)),
}
FUTURES_MONTH_CODES = {
    1: "F",
    2: "G",
    3: "H",
    4: "J",
    5: "K",
    6: "M",
    7: "N",
    8: "Q",
    9: "U",
    10: "V",
    11: "X",
    12: "Z",
}

_CLIENT: object | None = None
_AWS_ACCESS_KEY_ID: str | None = None
_AWS_SECRET_ACCESS_KEY: str | None = None
_MASSIVE_API_KEY: str | None = None
_INITIAL_DOWNLOAD_BACKOFF_SECONDS = 1
_MAX_DOWNLOAD_BACKOFF_SECONDS = 89


@dataclass(frozen=True)
class Dataset:
    product: str
    category: str
    prefix: str


DATASETS: tuple[Dataset, ...] = (
    Dataset("stocks", "stock_trade", "us_stocks_sip/trades_v1"),
    Dataset("stocks", "stock_quote", "us_stocks_sip/quotes_v1"),
    Dataset("currencies", "currency_quote", "global_forex/quotes_v1"),
    Dataset("crypto", "crypto_trade", "global_crypto/trades_v1"),
    Dataset("options", "option_trade", "us_options_opra/trades_v1"),
    Dataset("options", "option_quote", "us_options_opra/quotes_v1"),
    Dataset("indices", "index_value", "us_indices/values_v1"),
    *(
        Dataset(
            "futures",
            f"future_{exchange}_trade",
            f"us_futures_{exchange}/trades_v1",
        )
        for exchange in FUTURES_EXCHANGES
    ),
    *(
        Dataset(
            "futures",
            f"future_{exchange}_quote",
            f"us_futures_{exchange}/quotes_v1",
        )
        for exchange in FUTURES_EXCHANGES
    ),
)


class HelpOnErrorArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        self.print_help(sys.stderr)
        self.exit(2, f"{self.prog}: error: {message}\n")


def build_parser() -> argparse.ArgumentParser:
    parser = HelpOnErrorArgumentParser(
        prog="massive-speedup-download",
        description=(
            "Download selected massive trades/quotes flatfiles into a "
            "structured directory."
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
            "dataset-specific directories such as stock_trade, stock_quote, "
            "currency_quote, crypto_trade, option_trade, option_quote, "
            "future_cme_trade, and index_value."
        ),
    )
    parser.add_argument(
        "--products",
        nargs="+",
        default=list(ALL_PRODUCTS),
        help=(
            "Products to download: stocks, currencies, futures, crypto, options, "
            "indices. "
            "Singular aliases are accepted. Values may be repeated or "
            "comma-delimited. Defaults to all products."
        ),
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
        "--massive-api-key",
        "--massive_api_key",
        dest="massive_api_key",
        default=os.environ.get("MASSIVE_API_KEY") or os.environ.get("POLYGON_API_KEY"),
        help=(
            "Massive REST API key used for entitlement preflight. Defaults to "
            "MASSIVE_API_KEY, then POLYGON_API_KEY, when set."
        ),
    )
    parser.add_argument(
        "--end-date",
        type=_parse_end_date,
        default=_default_end_date(),
        help=(
            "Oldest date to include (inclusive). Accepts YYYY-MM-DD or "
            "English phrases such as 'three days ago' and 'a week ago'. "
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


def _parse_end_date(value: str) -> dt.date:
    try:
        return dt.date.fromisoformat(value)
    except ValueError:
        pass

    try:
        import dateparser
    except ImportError as error:
        raise argparse.ArgumentTypeError(
            "dateparser is required to parse non-YYYY-MM-DD --end-date values"
        ) from error

    parsed = dateparser.parse(
        value,
        settings={
            "PREFER_DATES_FROM": "past",
            "RETURN_AS_TIMEZONE_AWARE": False,
        },
    )
    if parsed is None:
        raise argparse.ArgumentTypeError(
            f"invalid date '{value}', expected YYYY-MM-DD or an English date phrase"
        )
    return parsed.date()


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


def _get_client():
    global _CLIENT
    client = _CLIENT
    if client is None:
        client = _build_s3_client()
        _CLIENT = client
    return client


def _normalize_products(products: list[str]) -> tuple[str, ...]:
    normalized: list[str] = []
    for value in products:
        for product in value.split(","):
            product = product.strip().lower()
            if not product:
                continue
            try:
                normalized.append(PRODUCT_ALIASES[product])
            except KeyError as error:
                choices = ", ".join(sorted(PRODUCT_ALIASES))
                raise ValueError(
                    f"unknown product '{product}', expected one of: {choices}"
                ) from error
    if not normalized:
        raise ValueError("at least one product must be provided")
    return tuple(dict.fromkeys(normalized))


def _selected_datasets(products: tuple[str, ...]) -> tuple[Dataset, ...]:
    selected = set(products)
    return tuple(dataset for dataset in DATASETS if dataset.product in selected)


def _dataset_for_key(key: str) -> Dataset | None:
    for dataset in DATASETS:
        if key.startswith(f"{dataset.prefix}/"):
            return dataset
    return None


def _date_from_key(key: str) -> dt.date:
    filename = key.rsplit("/", 1)[-1]
    if not filename.endswith(".csv.gz"):
        raise ValueError(f"cannot infer date from flatfile key: {key}")
    return dt.date.fromisoformat(filename[:10])


def scan_keys(products: tuple[str, ...] = ALL_PRODUCTS) -> set[str]:
    client = _build_s3_client()
    paginator = client.get_paginator("list_objects_v2")
    prefixes = tuple(f"{dataset.prefix}/" for dataset in _selected_datasets(products))

    keys: set[str] = set()
    for page in paginator.paginate(Bucket=BUCKET):
        for obj in page.get("Contents", ()):
            key = obj["Key"]
            if key.startswith(prefixes):
                keys.add(key)
    return keys


def _date_key_candidates(
    current: dt.date,
    products: tuple[str, ...] = ALL_PRODUCTS,
) -> tuple[tuple[str, str], ...]:
    stem = f"{current.year}-{current.month:02d}-{current.day:02d}.csv.gz"
    month = f"{current.month:02d}"
    return tuple(
        (
            dataset.category,
            f"{dataset.prefix}/{current.year}/{month}/{stem}",
        )
        for dataset in _selected_datasets(products)
    )


def build_download_jobs(
    download_path: Path,
    keys: set[str],
    *,
    end_date: dt.date,
    products: tuple[str, ...] = ALL_PRODUCTS,
) -> list[tuple[str, str]]:
    download_path = download_path.expanduser().resolve()
    targets = {
        dataset.category: download_path / dataset.category
        for dataset in _selected_datasets(products)
    }
    for directory in targets.values():
        directory.mkdir(parents=True, exist_ok=True)

    jobs: list[tuple[str, str]] = []
    day = dt.date.today() - dt.timedelta(days=1)
    while day >= end_date:
        for category, key in _date_key_candidates(day, products):
            if key not in keys:
                continue
            output_file = targets[category] / key.rsplit("/", 1)[-1]
            if output_file.exists():
                continue
            jobs.append((str(output_file), key))
        day -= dt.timedelta(days=1)
    return jobs


def _client_error_code(error: BaseException) -> str | None:
    response = getattr(error, "response", None)
    if not isinstance(response, dict):
        return None
    error_data = response.get("Error")
    if not isinstance(error_data, dict):
        return None
    code = error_data.get("Code")
    return str(code) if code is not None else None


def _is_forbidden_client_error(error: BaseException) -> bool:
    code = _client_error_code(error)
    if code is None:
        return False
    return code in {"403", "Forbidden", "AccessDenied"}


def _is_endpoint_connection_error(error: BaseException) -> bool:
    return error.__class__.__name__ == "EndpointConnectionError"


def _should_retry_download_error(error: BaseException) -> bool:
    return _is_forbidden_client_error(error) or _is_endpoint_connection_error(error)


def _retry_reason(error: BaseException) -> str:
    if _is_forbidden_client_error(error):
        return "403 Forbidden"
    if _is_endpoint_connection_error(error):
        return "endpoint connection error"
    return error.__class__.__name__


@dataclass
class FibonacciBackoff:
    previous: int = 0
    current: int = _INITIAL_DOWNLOAD_BACKOFF_SECONDS

    def delay(self) -> int:
        return self.current

    def advance(self) -> None:
        self.previous, self.current = (
            self.current,
            min(self.previous + self.current, _MAX_DOWNLOAD_BACKOFF_SECONDS),
        )

    def reached_cap(self) -> bool:
        return self.current >= _MAX_DOWNLOAD_BACKOFF_SECONDS


def _next_futures_probe_ticker(exchange: str, current: dt.date) -> str:
    months = FUTURES_PROBE_MONTHS[exchange]
    year = current.year
    for month in months:
        if month >= current.month:
            break
    else:
        month = months[0]
        year += 1
    return f"{FUTURES_PROBE_SYMBOLS[exchange]}{FUTURES_MONTH_CODES[month]}{year % 10}"


def _api_probe_request(dataset: Dataset, current: dt.date) -> tuple[str, dict[str, str | int]]:
    date_text = current.isoformat()
    category = dataset.category
    if category == "stock_trade":
        return "/v3/trades/AAPL", {
            "timestamp": date_text,
            "order": "asc",
            "sort": "timestamp",
            "limit": 1,
        }
    if category == "stock_quote":
        return "/v3/quotes/AAPL", {
            "timestamp": date_text,
            "order": "asc",
            "sort": "timestamp",
            "limit": 1,
        }
    if category == "currency_quote":
        return "/v3/quotes/C:EURUSD", {
            "timestamp": date_text,
            "order": "asc",
            "sort": "timestamp",
            "limit": 1,
        }
    if category == "crypto_trade":
        return "/v3/trades/X:BTCUSD", {
            "timestamp": date_text,
            "order": "asc",
            "sort": "timestamp",
            "limit": 1,
        }
    if category == "option_trade":
        return "/v3/trades/O:SPY260116C00600000", {
            "timestamp": date_text,
            "order": "asc",
            "sort": "timestamp",
            "limit": 1,
        }
    if category == "option_quote":
        return "/v3/quotes/O:SPY260116C00600000", {
            "timestamp": date_text,
            "order": "asc",
            "sort": "timestamp",
            "limit": 1,
        }
    if category == "index_value":
        return "/v3/snapshot/indices", {
            "ticker": "I:SPX",
            "order": "asc",
            "sort": "ticker",
            "limit": 1,
        }
    if dataset.product == "futures":
        parts = dataset.category.split("_")
        exchange = parts[1]
        ticker = _next_futures_probe_ticker(exchange, current)
        endpoint = "trades" if dataset.category.endswith("_trade") else "quotes"
        return f"/futures/v1/{endpoint}/{ticker}", {
            "timestamp": date_text,
            "session_end_date": date_text,
            "sort": "timestamp.asc",
            "limit": 1,
        }
    raise ValueError(f"unsupported flatfile dataset: {dataset.category}")


def _massive_api_url(path: str, params: dict[str, str | int]) -> str:
    if _MASSIVE_API_KEY is None:
        raise RuntimeError("Massive REST API key is not configured")
    request_params = dict(params)
    request_params["apiKey"] = _MASSIVE_API_KEY
    return f"{MASSIVE_API_URL}{path}?{urlencode(request_params)}"


def _massive_api_allows(url: str) -> bool:
    backoff = FibonacciBackoff()
    while True:
        try:
            with urlopen(url, timeout=30) as response:
                response.read(1)
            return True
        except HTTPError as exc:
            if exc.code in {401, 403}:
                return False
            if exc.code not in {429, 500, 502, 503, 504}:
                raise
            if backoff.reached_cap():
                raise
            tqdm.write(
                f"Massive API returned HTTP {exc.code}; retrying in "
                f"{backoff.delay()} seconds"
            )
            time.sleep(backoff.delay())
            backoff.advance()
        except URLError:
            if backoff.reached_cap():
                raise
            tqdm.write(
                f"Massive API connection error; retrying in "
                f"{backoff.delay()} seconds"
            )
            time.sleep(backoff.delay())
            backoff.advance()


def _is_entitled_to_download(key: str) -> bool:
    dataset = _dataset_for_key(key)
    if dataset is None:
        raise ValueError(f"unsupported flatfile key: {key}")
    path, params = _api_probe_request(dataset, _date_from_key(key))
    if _massive_api_allows(_massive_api_url(path, params)):
        return True
    tqdm.write(f"Skipping unauthorized flatfile {key}")
    return False


def _download_one(job: tuple[str, str]) -> str | None:
    destination, key = job
    if not _is_entitled_to_download(key):
        return None

    backoff = FibonacciBackoff()
    while True:
        try:
            _get_client().download_file(BUCKET, key, destination)
            tqdm.write(f"Downloaded {key} -> {destination}")
            break
        except Exception as exc:
            if _is_forbidden_client_error(exc):
                if backoff.reached_cap():
                    tqdm.write(f"Skipping unauthorized flatfile {key}")
                    return None
                tqdm.write(
                    f"403 Forbidden downloading {key}; retrying in "
                    f"{backoff.delay()} seconds"
                )
                time.sleep(backoff.delay())
                backoff.advance()
                continue
            if not _should_retry_download_error(exc):
                raise
            tqdm.write(
                f"{_retry_reason(exc)} downloading {key}; retrying in "
                f"{backoff.delay()} seconds"
            )
            time.sleep(backoff.delay())
            backoff.advance()
    return key


def run_downloads(jobs: list[tuple[str, str]]) -> None:
    if not jobs:
        return

    for _ in tqdm((_download_one(job) for job in jobs), total=len(jobs), unit="file"):
        pass


def _write_job_summary(jobs: list[tuple[str, str]], products: tuple[str, ...]) -> None:
    counts = {dataset.category: 0 for dataset in _selected_datasets(products)}
    for _, key in jobs:
        dataset = _dataset_for_key(key)
        if dataset is not None and dataset.category in counts:
            counts[dataset.category] += 1
    for dataset in _selected_datasets(products):
        tqdm.write(
            f"Queued {counts[dataset.category]} {dataset.category} files "
            f"from {dataset.prefix}"
        )


def main(argv: list[str] | None = None) -> int:
    set_process_title("massive-dl")
    parser = build_parser()
    args = parser.parse_args(argv)

    if not args.aws_access_key_id or not args.aws_secret_access_key:
        parser.error(
            "missing credentials: provide --aws-access-key-id and "
            "--aws-secret-access-key, or set AWS_ACCESS_KEY_ID and "
            "AWS_SECRET_ACCESS_KEY"
        )
    if not args.massive_api_key:
        parser.error(
            "missing Massive REST API key: provide --massive-api-key, or set "
            "MASSIVE_API_KEY"
        )

    global _AWS_ACCESS_KEY_ID, _AWS_SECRET_ACCESS_KEY, _MASSIVE_API_KEY
    _AWS_ACCESS_KEY_ID = args.aws_access_key_id
    _AWS_SECRET_ACCESS_KEY = args.aws_secret_access_key
    _MASSIVE_API_KEY = args.massive_api_key

    end_date = args.end_date
    if end_date > dt.date.today() - dt.timedelta(days=1):
        parser.error("--end-date cannot be in the future")

    try:
        products = _normalize_products(args.products)
    except ValueError as error:
        parser.error(str(error))
    keys = scan_keys(products)
    jobs = build_download_jobs(
        args.download_path,
        keys,
        end_date=end_date,
        products=products,
    )
    _write_job_summary(jobs, products)
    run_downloads(jobs)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
