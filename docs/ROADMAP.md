# Roadmap

This is a direction document, not a dated promise. The released source and tests define what is actually supported.

## Current foundation

- maintain process, performance, hardware, service, user and filesystem views
- keep collectors portable behind platform contracts where practical
- qualify direct hardware/accounting behaviour through the support/tests suite
- keep the Linux build independent of BlueZ development headers, libcap command-line tooling and an explicit Fontconfig package/cache-helper dependency
- preserve per-device Bluetooth traffic through the project-owned Linux HCI ABI and apply CAP_NET_RAW through the executable's own verified Linux xattr path
- consume Common 1.19.18 for toolkit-neutral HOME/XDG paths, recursive directory creation, complete text reads, deterministic ASCII matching/ordering, stable non-cryptographic signature hashing, monotonic counter delta/rate mechanics and POSIX deadline conversion instead of retaining equivalent GLib/libc/private helper paths

## Near-term priorities

- continue auditing direct and transitive dependencies, removing only those that can be replaced from a stable Linux, libc or project-owned Common contract without weakening correctness, security, accessibility or desktop integration
- expand hardware coverage only with explicit availability and provenance
- continue reducing duplicated generic helpers in favour of stronger Common contracts
- preserve UI responsiveness while enriching expensive views asynchronously

## Longer-term direction

- evaluate the GTK/GLib/GIO presentation boundary as the remaining major third-party code dependency, but replace any part of it only when a project-owned backend can match the required windowing, input, text, accessibility and desktop-integration semantics
- add additional native platform backends where the architecture can preserve the same snapshot contracts
- broaden device telemetry without turning optional vendor libraries into mandatory runtime dependencies

## Admission rule

A proposed capability enters the roadmap only when its ownership is clear and there is a credible way to validate it. Features that require pretending uncertain behaviour is known do not qualify.

## Completion rule

An item is complete when implementation, tests, user-visible behaviour and maintained documentation agree. A checkbox or release number cannot substitute for missing evidence.
