<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Contributing to System Monitor

System Monitor is an original clean-sheet implementation designed and written from the ground up for this project. Its application source is not forked, copied, translated, adapted, ported or derived from another system-monitoring application. External monitor source code is not an implementation input; project requirements, documented standards, authoritative native interfaces and project-owned Common contracts are the implementation authorities.

## Engineering rules

- Preserve the clean-sheet boundary.
- Prefer authoritative native interfaces over external monitoring-command output where practical.
- Keep platform-specific handles, paths, ioctls, driver knowledge and retained native state below platform contracts.
- Keep GTK types out of reusable accounting, parsing and model layers.
- Keep generic reusable mechanisms in Common when its contract is at least as strong as the local implementation.
- Keep System-Monitor-specific hardware, product and presentation policy local.
- Represent unavailable telemetry explicitly; never invent a plausible value.
- Make ownership, units, timing, cleanup and counter-reset behaviour explicit.
- Add deterministic regression coverage for parser, accounting, lifecycle, topology and hardware changes.
- Choose C or C++ according to the component; abstraction is justified by a concrete improvement, not by language fashion.

The durable rationale behind these rules lives in [Architecture](docs/ARCHITECTURE.md) and [Decisions](docs/DECISIONS.md). Do not duplicate those explanations in new documents or comments.

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

Keep maintained documentation small. [docs/README.md](docs/README.md) is the canonical map. Put each durable rule in one authoritative document and link to it rather than restating the same policy elsewhere.

Comments should explain invariants, concurrency ordering, ownership transfer, units, ABI quirks, security boundaries and non-obvious reasons. They should not narrate straightforward code.

## Repository discipline

Normal development stays on `main`. Keep commits focused, preserve strict warnings, and update documentation or tests in the same change when their owning contract changes.

Participation standards remain in [.github/CODE_OF_CONDUCT.md](.github/CODE_OF_CONDUCT.md).

## Licence

Contributions are accepted under GPL-3.0-or-later unless explicitly agreed otherwise.
