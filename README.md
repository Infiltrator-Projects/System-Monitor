<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# System Monitor

**Project copyright:** © 2000-2026 Shannon Smith

[![Verify](https://github.com/Infiltrator-Projects/System-Monitor/actions/workflows/ci.yml/badge.svg)](https://github.com/Infiltrator-Projects/System-Monitor/actions/workflows/ci.yml)

System Monitor is a native C17/GTK 3 desktop system manager for Linux, with a native Windows GUI preview. It provides process, performance, hardware, service, user and filesystem views while collecting from native operating-system and hardware interfaces wherever practical.

## Clean-sheet provenance

System Monitor is an original clean-sheet implementation designed and written from the ground up for this project. Its application source was not forked, copied, translated, adapted, ported or derived from GNOME System Monitor, Linux Mint System Monitor, Windows Task Manager or any other system-monitoring application. No other system monitor's source code, internal architecture, algorithms or implementation behaviour is an implementation authority for this project.

Implementation decisions come from System Monitor's own product requirements, authoritative operating-system and hardware interfaces, documented standards and project-owned Common contracts. Comparison with another product, where performed, is capability validation only and does not establish source, design or implementation ancestry.

**Current source version:** 1.0.173 ([version file](support/VERSION))  
**Shared foundation:** exact Common 1.19.35 gitlink at `src/infiltratr-common`  
**Platform:** Linux desktop; native Windows GUI preview  
**Licence:** GPL-3.0-or-later

## Capabilities

- Overview of CPU, memory, disk, network, GPU, temperature, pressure and busiest processes.
- Performance pages for CPU, memory, disks, partitions, networks, Bluetooth devices, GPUs, batteries and supported NPUs.
- Native Linux Pressure Stall Information for CPU, memory and I/O contention.
- Processes, Application History, Startup, Users, Details, Services and File Systems pages.
- cgroup-v2-aware application grouping with executable/ancestor fallback.
- Process inspection, termination, suspend/resume, priority, efficiency mode and CPU affinity controls.
- Native hardware identity and telemetry with explicit per-metric availability.
- Per-device Bluetooth traffic where the packaged capability and kernel interface permit it.
- Snapshot and process-table export.
- Human-facing timestamps and durations can follow the temporal policy published by Infiltrator System Settings; internal accounting remains canonical SI/Unix time.

Unsupported or inaccessible telemetry is shown as unavailable rather than guessed.

## Engineering contract

System Monitor is built from first principles. Native operating-system, kernel, driver and documented vendor interfaces are preferred over parsing external monitoring utilities when they provide the stronger contract. Generic reusable mechanisms belong in pinned Infiltrator Common; product-specific monitoring, hardware and UI policy remain local.

Slow native collection stays off the GTK main thread. Collectors publish coherent completed snapshots with explicit availability, stable device identity and monotonic timing. Counter reset, rollback, replacement or invalid timing breaks the relevant baseline rather than producing a fabricated rate.

The Linux installed product is one GUI executable, `system-monitor`, with no project-owned privileged helper or daemon. The Windows preview is also a single native GUI executable. Linux Bluetooth HCI monitoring uses only the narrowly required `CAP_NET_RAW` file capability and drops process capability sets before normal GTK and monitoring work begins.

The maintained engineering contracts are intentionally concentrated in a small documentation set:

- [Architecture](docs/ARCHITECTURE.md) — ownership, data flow, concurrency, failure and security boundaries.
- [Decisions](docs/DECISIONS.md) — durable architectural decisions and rationale.
- [Hardware collection](docs/HARDWARE.md) — hardware evidence, telemetry and availability rules.
- [Portability](docs/PORTABILITY.md) — platform and ABI boundaries.
- [Validation](docs/VALIDATION.md) — evidence and release criteria.

## Architecture

```text
GTK 3 presentation (Linux) / Win32 presentation (Windows)
        ↓
application-facing presentation and plain-C models
        ↓
platform contracts
        ↓
Linux native backends / Windows native backends
        ↓
OS, kernel, driver and documented vendor interfaces

Common 1.19.35
        ↓
shared reusable formatting, parsing, timing, path and design primitives
```

Linux and Windows consume the same application-facing presentation contracts where those surfaces overlap. Native handles, paths, retained counter baselines and platform-specific collection stay below the platform boundary.

## Appearance

System Monitor consumes the project-owned Infiltrator design contracts and the MB Corpo typography contract from pinned Common. Linux packages the three verified MB Corpo faces from the repository asset archive; the portable Windows executable embeds the same verified faces as process-private resources. View → Theme provides Follow system, Day and Night.

## Build and test

On Debian, Ubuntu or Linux Mint:

```bash
sudo apt install build-essential git pkg-config libgtk-3-dev
git clone --recurse-submodules https://github.com/Infiltrator-Projects/System-Monitor.git
cd System-Monitor
make
./build/system-monitor
```

Run `make check` for the authoritative project verification suite. CI also proves the CMake build path, sanitizer and 32-bit gates, documentation validation and release-package construction. Direct `make install` is disabled; installation is owned by the Debian package or native installer.

## Release assets

Each numbered release publishes:

- `infiltrator-system-monitor_<version>_amd64.deb`
- `infiltrator-system-monitor-<version>-native-installer.run`
- `system-monitor-<version>-windows.exe`

The `.deb` is the generic amd64 package. The `.run` performs a native local build/test/install and supports native, aggressive and portable optimisation profiles; aggressive performs a two-pass profile-guided rebuild trained on System Monitor's own native collector and process-scan paths. The Windows `.exe` is the current portable native preview.

## Repository policy

Development is kept on `main`. A release is publishable only after the full Verify workflow succeeds for the exact current commit. Published tags and release assets are immutable; changing a published product state requires advancing `support/VERSION` before the next publication.

Contribution guidance lives in [CONTRIBUTING.md](CONTRIBUTING.md). Security reports follow [SECURITY.md](SECURITY.md).

## Licence and provenance

Copyright © 2000-2026 Shannon Smith.

Shannon Smith-owned System Monitor source, documentation and application artwork are licensed under GPL-3.0-or-later. Clean-sheet provenance applies to the System Monitor application implementation; documented platform/runtime dependencies, first-party Common infrastructure and separately licensed assets retain their own identities and licences.

The embedded PCI identity registry contains normalized factual vendor/device mappings in a project-defined flat schema and generated numeric lookup tables. The supplied MB Corpo font assets retain their embedded copyright and licence metadata and are separate from the project's GPL source.
