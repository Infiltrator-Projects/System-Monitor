<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Contributing to Linux System Monitor

Linux System Monitor is a portability-first native C project. Contributions should preserve the separation between platform-neutral models, Linux-native providers and GTK presentation while keeping collection direct and auditable.

## Engineering rules

- Target ISO C17. Prefer C11/C99 constructs when they express the same design clearly.
- Do not introduce C23-only or C++ runtime/toolchain requirements merely for convenience.
- Keep Linux paths, native handles, ioctls, scheduler calls and driver details below platform contracts.
- Keep GTK types out of reusable accounting, parsing and model layers.
- Prefer direct native interfaces over command orchestration for telemetry.
- Reuse Infiltratr Common when an existing shared API matches the requirement; do not modify the pinned Common submodule from this repository.
- Keep hardware-specific collection policy in System Monitor rather than pushing it into Common.
- Make ownership, lifetime, cleanup and failure behaviour explicit for resource-owning modules.
- Treat unsupported or ambiguous metrics as unavailable rather than guessing units or meaning.
- Add deterministic regression coverage when changing parsers, accounting, lifecycle, topology or hardware interpretation.

## C style and quality

Code must compile cleanly under the repository warning policy and pass the source-style/portability checks. The checker enforces source membership, documentation contracts, licensing, platform boundaries and other project invariants.

Source files carry `// SPDX-License-Identifier: GPL-3.0-or-later` and a concise Doxygen-style file header. Public `lsm_` declarations document parameters, return semantics and non-obvious ownership/lifetime behaviour.

Prefer checked sizes, explicit-width integer types where width matters, saturating/checked arithmetic where overflow matters, deterministic cleanup and narrow interfaces. Comments should explain ownership, units, invariants, hardware quirks or architectural reasons rather than narrating obvious syntax.

## Build and test

Clone recursively because System Monitor pins Infiltratr Common as a submodule:

```bash
git clone --recurse-submodules https://github.com/The-First-Infiltrator/System-Monitor.git
cd System-Monitor
make check
```

The CMake path is also required:

```bash
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --parallel
ctest --test-dir build-cmake --output-on-failure
```

For a release-equivalent local packaging pass:

```bash
make -j2 release
```

Do not use direct `make install`; installation is owned by the Debian package or native installer.

## Architecture ownership

| Layer | Owns |
| --- | --- |
| GTK presentation | widgets, graph presentation, user interaction |
| Platform-neutral model/contracts | current snapshots, normalized state, application-facing process/monitor contracts |
| Linux backend/providers | procfs/sysfs, ioctls, D-Bus, driver APIs, retained native state |
| Infiltratr Common | reusable parsing, formatting, timing, durable I/O and other cross-project primitives |

See `docs/ARCHITECTURE.md`, `docs/PORTABILITY.md` and `docs/HARDWARE.md` before changing a boundary.

## Repository discipline

The maintained repository works from `main`; the release workflow only publishes tested commits from exact current `main`. Do not add project-side feature/release branch machinery as a requirement for normal development.

Keep commits focused. Do not mix unrelated formatting, hardware behaviour, packaging and documentation changes without a reason. Do not add new Markdown files casually: the source-style audit deliberately allowlists the maintained documentation set so the repository does not return to duplicated, contradictory documentation.

## Bug reports

Use the bug issue form and include the exact release/tag/commit, Linux distribution, desktop environment where relevant, affected hardware, whether the GUI was run as an ordinary user or deliberately elevated, and the smallest reliable reproduction sequence.

Do not put security-sensitive details in a public bug report; follow `SECURITY.md` instead.

## Licence

Contributions are accepted under `GPL-3.0-or-later` unless explicitly agreed otherwise beforehand. Third-party material must retain its original licence and attribution and must be compatible with the project distribution.
