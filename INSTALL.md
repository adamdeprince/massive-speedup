# Installing massive-speedup

`massive-speedup` is a Python package with a C++23 nanobind extension built by
scikit-build-core and CMake.

## Requirements

- Python 3.9 or newer
- A C++23-capable compiler
- CMake 3.24 or newer
- `pip`
- `git` when third-party C++ dependency sources are not already cached

The build uses `simdjson` and `rapidgzip` at the C++ level. If their source
trees are already present under the build directory, CMake reuses them. Missing
sources are downloaded only when offline mode is not enabled.

## Install From Source

From the repository root:

```bash
python -m pip install .
```

For editable development installs:

```bash
python -m pip install -e .
```

If build dependencies are already installed and you want to avoid build
isolation network access:

```bash
python -m pip install -e . --no-build-isolation
```

## Install Build Dependencies First

This is useful on metered or offline-prone connections:

```bash
python -m pip install "scikit-build-core>=0.12.2,<0.13.0" "nanobind>=2.12.0,<3.0.0"
python -m pip install -e . --no-build-isolation
```

## Offline Or Cached Builds

After a successful build has populated `build/{wheel_tag}/_deps`, rebuilds reuse
the cached dependency source trees. To fail instead of downloading missing C++
dependency sources:

```bash
CMAKE_ARGS="-DMASSIVE_SPEEDUP_OFFLINE=ON" python -m pip install -e . --no-build-isolation
```

To use a local rapidgzip checkout:

```bash
CMAKE_ARGS="-DMASSIVE_SPEEDUP_USE_SYSTEM_RAPIDGZIP=ON -DMASSIVE_SPEEDUP_RAPIDGZIP_SOURCE_DIR=/path/to/rapidgzip" \
  python -m pip install -e . --no-build-isolation
```

## Verify

```bash
python - <<'PY'
import massive_speedup

print(massive_speedup.StockTrade)
print(massive_speedup.FlatFiles.Stock.Trade.Aggregator)
PY
```

The console script should also be installed:

```bash
massive-speedup-build-database --help
```
