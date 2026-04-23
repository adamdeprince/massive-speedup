# massive-speedup

`massive-speedup` is a Poetry-managed Python package with nanobind/C++ parser
modules selected at Python import time. The native core is centered on:

- C++23-native parser and utility code
- A serializable `Parser` base class implemented in C++
- Two common parser modules: `FlatFiles` and `WebSocket`
- Backend-specialized native extensions for `generic`, `sse`, `avx`,
  `avx512`, `neon`, `sve`, `sve2`, `lsx`, and `lasx`
- Native C++ access to `simdjson` and `rapidgzip` so decompression and JSON
  parsing can stay out of Python bytecode paths

## Layout

```text
.
|-- CMakeLists.txt
|-- include/massive_speedup/
|-- pyproject.toml
|-- src/
|   |-- cpp/
|   |   |-- backends/
|   |   |   |-- generic.cpp
|   |   |   |-- x86_sse.cpp
|   |   |   |-- x86_avx.cpp
|   |   |   `-- ...
|   |   |-- cpu.cpp
|   |   |-- flatfiles.cpp
|   |   |-- module_bindings.hpp
|   |   |-- parser_common.cpp
|   |   `-- websocket.cpp
|   `-- massive_speedup/
`-- tests/
```

## Development

```bash
poetry install
poetry run pip install -e .
poetry run pytest
```

`poetry install` creates the development environment and installs Python-side
dependencies. `poetry run pip install -e .` builds the editable nanobind/C++
extension inside that environment. Wheel builds are driven by `scikit-build-core`
and `CMake`, while Poetry manages the project environment and lockfile.

The C++ build now vendors `simdjson` and `rapidgzip` by default using
`FetchContent`. If you already have them locally, you can switch to local/system
resolution with:

```bash
cmake -S . -B build \
  -DMASSIVE_SPEEDUP_USE_SYSTEM_SIMDJSON=ON \
  -DMASSIVE_SPEEDUP_USE_SYSTEM_RAPIDGZIP=ON \
  -DMASSIVE_SPEEDUP_RAPIDGZIP_SOURCE_DIR=/path/to/rapidgzip
```

This is intended for direct native use from your parser implementations, so
gzip decompression and JSON parsing can happen entirely in C++.

The shared native layer also exposes a C++23 line-streaming helper,
`read_gzip_lines(...)`, which is intended to open a gzip file with rapidgzip
and yield one decoded line at a time through `std::generator<std::string>`.

## API Direction

`massive_speedup.FlatFiles` and `massive_speedup.WebSocket` choose a backend
module at import time using `detect_best_backend()`, following the same pattern
as `negcycle`: the top-level `__init__.py` imports one backend module such as
`massive_speedup._generic`, `massive_speedup._sse`, `massive_speedup._avx`, or
`massive_speedup._avx512`. The package also exposes `available_backends()` and
`backend_is_available(...)` for explicit inspection. The common C++ subclasses
own the stub parse methods, and the backend-specialized modules currently only
override the virtual `split_on_commas(std::string, std::vector<std::string>&)`.
