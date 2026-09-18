<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Contributing to Linux System Monitor

Linux System Monitor is a native C project with explicit boundaries between GTK presentation, platform-neutral models, Linux backends and Infiltratr Common.

## Engineering rules

- Target ISO C17; prefer older standard constructs when they are equally clear.
- Keep Linux paths, handles, ioctls, scheduler calls and driver knowledge below platform contracts.
- Keep GTK types out of reusable accounting, parsing and model layers.
- Prefer direct native interfaces over command orchestration for telemetry.
- Reuse the pinned Infiltratr Common APIs when their contract matches the requirement; do not modify the Common submodule from this repository.
- Keep System-Monitor-specific hardware and UI policy local.
- Make ownership, cleanup, units, availability and failure behaviour explicit.
- Do not invent telemetry when an interface cannot establish a value safely.
- Add deterministic regression coverage for parser, accounting, lifecycle, topology or hardware-behaviour changes.

Code must compile cleanly under the repository warning policy. Public `lsm_` declarations should document non-obvious parameters, ownership and return semantics. Comments should explain invariants, units, hardware quirks or architectural reasons rather than restating syntax.

For boundary details, read [ARCHITECTURE.md](../docs/ARCHITECTURE.md), [PORTABILITY.md](../docs/PORTABILITY.md) and [HARDWARE.md](../docs/HARDWARE.md).

## Build and test

Clone recursively because Common is pinned as a submodule:

```bash
git clone --recurse-submodules https://github.com/Infiltrator-Projects/System-Monitor.git
cd System-Monitor
make check
```

The CMake path must also remain healthy:

```bash
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --parallel
ctest --test-dir build-cmake --output-on-failure
```

Use `make -j2 release` for a release-equivalent local packaging pass. Do not use direct `make install`.

## Repository discipline

Normal development stays on `main`; do not introduce project-side feature/release branch machinery as a requirement. Keep commits focused and avoid adding Markdown files unless the information has a distinct maintained purpose.

Bug reports should include the exact release or commit, Linux distribution, relevant desktop/hardware context, privilege level and a minimal reproduction. Security-sensitive reports belong in [SECURITY.md](SECURITY.md), not public issues.

Contributions are accepted under GPL-3.0-or-later unless explicitly agreed otherwise. Third-party material must retain compatible licensing and attribution.
