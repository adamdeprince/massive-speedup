# massive-speedup

`massive-speedup` is a Poetry-managed Python package with a nanobind/C++ parser
module. The native core is centered on:

- C++23-native parser and utility code
- A serializable `Parser` base class implemented in C++
- Two common parser modules: `FlatFiles` and `WebSocket`
- A single native extension module, `massive_speedup._native`
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
|   |   |-- flatfiles.cpp
|   |   |-- module_bindings.hpp
|   |   |-- native.cpp
|   |   |-- native.hpp
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

`massive_speedup.FlatFiles` and `massive_speedup.WebSocket` are exposed from the
single native module, `massive_speedup._native`. The top-level package imports
that module directly and falls back to the pure Python compatibility layer only
when the native extension is unavailable.


sudo apt install cmake g++ libgrpc++-dev protobuf-compiler-grpc libprotobuf-dev libgflags-dev protobuf-compiler-grpc

