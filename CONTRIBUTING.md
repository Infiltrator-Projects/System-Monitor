<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Contributing to System Monitor

System Monitor is an original clean-sheet implementation. Do not copy, translate, adapt, port or derive implementation code from another system-monitoring application.

Follow the maintained architecture contract in [Architecture](docs/ARCHITECTURE.md). Behavioural changes should update the owning tests and, when a durable contract changes, the one authoritative document that owns that contract.

## Build and test

```bash
git clone --recurse-submodules https://github.com/Infiltrator-Projects/System-Monitor.git
cd System-Monitor
make check
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --target system-monitor --parallel
```

`make check` is the authoritative executed verification suite. Use `make docs` for Doxygen validation and `make -j2 release` for a release-equivalent packaging pass. Direct `make install` is disabled.

Comments should explain non-obvious invariants, ownership, concurrency, units, ABI quirks and security boundaries rather than narrating straightforward code.

## Repository discipline

Normal development stays on `main`. Keep changes focused, preserve strict warnings and represent unsupported or inaccessible telemetry as unavailable rather than inventing a value.

Participation standards remain in [.github/CODE_OF_CONDUCT.md](.github/CODE_OF_CONDUCT.md).

## Licence

Contributions are accepted under GPL-3.0-or-later unless explicitly agreed otherwise.
