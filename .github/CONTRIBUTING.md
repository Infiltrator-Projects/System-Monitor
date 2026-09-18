<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Contributing to System Monitor

System Monitor's installed application is native C17/GTK 3. Developer-only tooling may use C++17 where its standard library materially improves safety or maintainability. The project keeps explicit boundaries between GTK presentation, platform-neutral models, Linux backends and Common.

## Engineering rules

- Target ISO C17 for the installed application; prefer older standard constructs when they are equally clear.
- Use ISO C++17 only in developer tooling where it provides a concrete correctness, safety or maintainability benefit; do not add a C++ runtime dependency to the installed application merely for convenience.
- Keep Linux paths, handles, ioctls, scheduler calls and driver knowledge below platform contracts.
- Keep GTK types out of reusable accounting, parsing and model layers.
- Prefer direct native interfaces over command orchestration for telemetry.
- Reuse the pinned Common APIs when their contract matches the requirement; do not modify the Common submodule from this repository.
- Keep System-Monitor-specific hardware and UI policy local.
- Make ownership, cleanup, units, availability and failure behaviour explicit.
- Do not invent telemetry when an interface cannot establish a value safely.
- Add deterministic regression coverage for parser, accounting, lifecycle, topology or hardware-behaviour changes.

Code must compile cleanly under the repository warning policy. Public `lsm_` declarations document parameters, ownership and return semantics when those are not self-evident.

Comments should capture information that is expensive to reconstruct: invariants, concurrency ordering, ownership transfer, units, ABI quirks, security boundaries, complexity choices and the reason a less-obvious design was selected. Do not narrate straightforward statements or keep historical commentary that no longer describes the code.

Maintained documentation describes contracts rather than implementation trivia. A behaviour claim should be supportable by the current source, a native-interface specification or a deterministic test. Keep the small documentation set authoritative instead of adding overlapping Markdown files.

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

Use `make docs` to generate the Doxygen reference with documentation warnings treated as errors. Use `make -j2 release` for a release-equivalent local packaging pass. Do not use direct `make install`.

## Repository discipline

Normal development stays on `main`; do not introduce project-side feature/release branch machinery as a requirement. Keep commits focused and avoid adding Markdown files unless the information has a distinct maintained purpose.

Bug reports should include the exact release or commit, Linux distribution, relevant desktop/hardware context, privilege level and a minimal reproduction. Security-sensitive reports belong in [SECURITY.md](SECURITY.md), not public issues.

Contributions are accepted under GPL-3.0-or-later unless explicitly agreed otherwise. Third-party material must retain compatible licensing and attribution.
