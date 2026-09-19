<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Contributing to System Monitor

System Monitor uses C and C++ as equal first-class project languages. C, procedural C++ and object-oriented C++ are implementation styles, not competing identities: choose the style that expresses the component most clearly and strongly. The decision is based on correctness, clarity, performance, maintainability and control. Other language/runtime ecosystems require a concrete capability that C/C++ cannot reasonably provide.

## Engineering rules

- Go as low in the stack as practical and prefer authoritative native interfaces over parsing external monitoring utilities.
- Do not introduce objects, inheritance or virtual dispatch merely because C++ permits them; use OO where encapsulated state or genuine polymorphism makes the design stronger, and prefer direct procedural/value-oriented code otherwise.
- Use C++ facilities such as RAII, stronger types, templates or scoped ownership when they make the implementation safer or clearer without hiding important control flow or machine semantics.
- Keep Linux paths, handles, ioctls, scheduler calls and driver knowledge below platform contracts.
- Keep GTK types out of reusable accounting, parsing and model layers.
- Keep System-Monitor-specific hardware and UI policy local.
- Treat Common as the authoritative home for generic reusable mechanisms; improve Common before replacing a stronger local implementation.
- Never invent telemetry when an interface cannot establish a value safely.
- Make units, ownership, cleanup, availability and failure semantics explicit.
- Add deterministic regression coverage for parser, accounting, lifecycle, topology and hardware-behaviour changes.

## Build and test

```bash
git clone --recurse-submodules https://github.com/Infiltrator-Projects/System-Monitor.git
cd System-Monitor
make check
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --parallel
ctest --test-dir build-cmake --output-on-failure
```

Use `make docs` for Doxygen validation and `make -j2 release` for a release-equivalent packaging pass. Do not use direct `make install`.

## Documentation and comments

The canonical map is `docs/README.md`. Maintain architecture, design, decisions, roadmap and validation in their named documents instead of creating overlapping Markdown.

Comments document invariants, concurrency ordering, ownership transfer, units, ABI quirks, security boundaries, complexity choices and non-obvious reasons. They should not narrate straightforward statements.

## Repository discipline

Normal development stays on `main`. Keep commits focused, preserve strict warnings and keep documentation/test updates in the same change as the contract they describe.

Participation standards remain in [.github/CODE_OF_CONDUCT.md](.github/CODE_OF_CONDUCT.md).

## Licence

Contributions are accepted under GPL-3.0-or-later unless explicitly agreed otherwise.