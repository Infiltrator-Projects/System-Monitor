<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Contributing to System Monitor

## Engineering rules

- Preserve the clean-sheet boundary: do not copy, translate, adapt, port or derive implementation code from another system-monitoring application.
- Prefer authoritative native interfaces over external monitoring-command output where practical.
- Keep native handles, paths, ioctls, driver knowledge and retained native state below platform contracts.
- Keep toolkit types out of reusable accounting, parsing and model layers.
- Keep generic reusable mechanisms in Common when its contract is at least as strong as the local implementation.
- Keep System-Monitor-specific hardware, product and presentation policy local.
- Represent unavailable telemetry explicitly; never invent a plausible value.
- Make ownership, units, timing, cleanup and counter-reset behaviour explicit.
- Add deterministic regression coverage for parser, accounting, lifecycle, topology and hardware changes.
- Choose C or C++ according to the component and a concrete technical benefit.

The architectural rationale is maintained in [Architecture](docs/ARCHITECTURE.md). Do not duplicate it in new documents or comments.

## Build and test

```bash
git clone --recurse-submodules https://github.com/Infiltrator-Projects/System-Monitor.git
cd System-Monitor
make check
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --target system-monitor --parallel
```

`make check` is the authoritative executed verification suite. Use `make docs` for Doxygen validation and `make -j2 release` for a release-equivalent packaging pass. Direct `make install` is disabled.

## Documentation and comments

Keep maintained documentation small. Put each durable rule in one authoritative document and link to it instead of restating it elsewhere.

Comments should explain invariants, concurrency ordering, ownership transfer, units, ABI quirks, security boundaries and non-obvious reasons. They should not narrate straightforward code.

## Repository discipline

Normal development stays on `main`. Keep commits focused, preserve strict warnings, and update the owning documentation or tests when a contract changes.

Participation standards remain in [.github/CODE_OF_CONDUCT.md](.github/CODE_OF_CONDUCT.md).

## Licence

Contributions are accepted under GPL-3.0-or-later unless explicitly agreed otherwise.
