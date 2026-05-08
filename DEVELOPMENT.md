# Development And Publishing

This project uses scikit-build-core, CMake, C++23, and nanobind.

Useful references:

- PyPA build: https://build.pypa.io/en/latest/
- scikit-build-core build docs: https://scikit-build-core.readthedocs.io/en/stable/guide/build.html
- Twine: https://twine.readthedocs.io/en/stable/
- PyPI trusted publishing: https://docs.pypi.org/trusted-publishers/
- cibuildwheel: https://cibuildwheel.pypa.io/

## Local Development

Install build and test tooling:

```bash
python -m pip install -U pip
python -m pip install "scikit-build-core>=0.12.2,<0.13.0" "nanobind>=2.12.0,<3.0.0"
python -m pip install pytest build twine cibuildwheel
```

Install editable:

```bash
python -m pip install -e . --no-build-isolation
```

Run tests:

```bash
poetry run pytest
```

If Poetry is not managing the active environment:

```bash
python -m pytest
```

## Release Checklist

1. Update `version` in `pyproject.toml`.
2. Verify `README.md`, `INSTALL.md`, and this file describe the release.
3. Run the test suite.
4. Build an sdist and at least one wheel.
5. Install and smoke-test the built wheel.
6. Upload to TestPyPI first.
7. Upload to PyPI after TestPyPI verification.

## Build Source And Local Wheel

From a clean checkout:

```bash
python -m build
```

By default, `python -m build` creates an sdist and then builds a wheel from that
sdist into `dist/`. That is the preferred local release check because it proves
the sdist contains enough files to build the binary extension.

Build only the sdist:

```bash
python -m build --sdist
```

Build only a local binary wheel:

```bash
python -m build --wheel
```

Pass CMake options through scikit-build-core config settings when needed:

```bash
python -m build --wheel -Ccmake.define.MASSIVE_SPEEDUP_OFFLINE=ON
```

## Verify Artifacts

Check metadata:

```bash
python -m twine check dist/*
```

Inspect contents:

```bash
tar -tf dist/*.tar.gz | head
for wheel in dist/*.whl; do python -m zipfile --list "$wheel" | head; done
```

Smoke-test the wheel in a clean virtual environment:

```bash
python -m venv /tmp/massive-speedup-wheel-test
/tmp/massive-speedup-wheel-test/bin/python -m pip install dist/*.whl
/tmp/massive-speedup-wheel-test/bin/python - <<'PY'
import massive_speedup

print(massive_speedup.StockTrade)
print(massive_speedup.FlatFiles.Stock.Trade.Aggregator)
PY
```

## Build Redistributable Binary Wheels

Local Linux wheels built directly with `python -m build --wheel` are usually
tagged for the local machine. For PyPI binary distribution, build repaired
platform wheels with `cibuildwheel`.

Do not upload local wheels named like:

```text
massive_speedup-<version>-cp313-cp313-linux_x86_64.whl
```

PyPI rejects the `linux_x86_64` platform tag. Upload only the sdist and wheels
with valid platform tags such as `manylinux_2_28_x86_64` or
`musllinux_1_2_x86_64`.

Build wheels for the current platform:

```bash
python -m cibuildwheel --platform linux --output-dir dist-pypi-<version>
```

Limit Python versions if needed:

```bash
CIBW_BUILD="cp311-* cp312-* cp313-* cp314-*" python -m cibuildwheel --platform linux --output-dir dist-pypi-<version>
```

Linux builds normally require Docker because cibuildwheel builds manylinux or
musllinux-compatible wheels in containers.

The default project configuration builds Linux x86_64 wheels for CPython
3.9-3.14 on both manylinux and musllinux. It also includes the free-threaded
`cp313t` and `cp314t` variants.

## Publish To TestPyPI

Manual token-based upload:

```bash
python -m twine upload -r testpypi dist/*
```

Use a TestPyPI API token. For token auth, the username is `__token__` and the
password is the token value.

Install from TestPyPI in a clean environment:

```bash
python -m pip install \
  --index-url https://test.pypi.org/simple/ \
  --extra-index-url https://pypi.org/simple/ \
  massive-speedup
```

## Publish To PyPI

Preferred deployment for CI is PyPI trusted publishing so long-lived upload
tokens are not stored in the repository or CI secrets.

Manual token-based upload:

```bash
python -m twine upload dist/massive_speedup-<version>.tar.gz
python -m twine upload dist-pypi-<version>/massive_speedup-<version>-*.whl
```

Upload both source and binary artifacts together when possible:

```text
dist/massive_speedup-<version>.tar.gz
dist-pypi-<version>/massive_speedup-<version>-<python>-<abi>-<platform>.whl
```

Do not reuse a version number after uploading to PyPI; PyPI files are immutable.

## Release Command Pattern

This is the exact pattern to use for a Linux release:

```bash
VERSION=0.1.4
rm -rf dist "dist-pypi-$VERSION"
python -m build --sdist
python -m cibuildwheel --platform linux --output-dir "dist-pypi-$VERSION"
python -m twine check "dist/massive_speedup-$VERSION.tar.gz" "dist-pypi-$VERSION"/*.whl
python -m twine upload "dist/massive_speedup-$VERSION.tar.gz"
python -m twine upload "dist-pypi-$VERSION/massive_speedup-$VERSION-"*.whl
```

If `python -m build --wheel` is run locally as a smoke test, leave that wheel
out of the upload command unless it has been repaired and retagged by
`cibuildwheel`.

The `0.1.3` sdist was uploaded before the wheel-test fallback export issue was
fixed. Do not try to replace it; publish the next version instead.
