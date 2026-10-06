<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Contributing to System Monitor

System Monitor is an original clean-sheet implementation. Do not copy, translate, adapt, port or derive implementation code from another system-monitoring application.

Follow the durable contracts in [Architecture](docs/ARCHITECTURE.md). Behavioural changes should update the owning tests; update documentation only when a documented contract changes.

`make check` is the authoritative verification suite. Use `make docs` for Doxygen validation and `make -j2 release` for a release-equivalent packaging pass.

Normal development stays on `main`. Keep changes focused, preserve strict warnings, and represent unsupported or inaccessible telemetry as unavailable rather than inventing a value.

Comments should explain non-obvious invariants, ownership, concurrency, units, ABI quirks and security boundaries rather than narrating straightforward code.
