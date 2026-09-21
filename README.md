<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# System Monitor

**Project copyright:** © 2016-2026 Shannon Smith

[![Verify](https://github.com/Infiltrator-Projects/System-Monitor/actions/workflows/ci.yml/badge.svg)](https://github.com/Infiltrator-Projects/System-Monitor/actions/workflows/ci.yml)

System Monitor is a native C17/GTK 3 desktop system manager for Linux. It provides Task-Manager-style process, performance, hardware, service and user views while collecting data directly from native operating-system interfaces wherever practical.

**Current source version:** 1.0.65 ([version file](support/VERSION))\
**Shared foundation:** exact Common 1.19.20 gitlink at `src/infiltratr-common`  
**Platform:** Linux desktop; additional native backends are planned  
**Licence:** GPL-3.0-or-later

## Engineering ethos

System Monitor is built from first principles: establish what the operating system or hardware interface actually guarantees, then implement the required behaviour directly where practical. It deliberately avoids depending on the output or behaviour of external monitoring utilities when an authoritative native interface is available, because those dependencies can change independently of this project. External libraries are used when their documented contract is the stronger solution; important product behaviour remains owned by System Monitor.

The project does not equate newer with better. Proven interfaces and techniques remain when they are the strongest solution, and newer ones replace them when they are demonstrably better. Age, popularity, fashion, convenience and implementation effort do not decide the architecture: the strongest practical implementation does. System Monitor should not knowingly accept a weaker solution merely because it is easier, quicker, more conventional or more portable.

C and C++ are equal, first-class implementation languages for the project. The choice between them is made according to the needs of the component, with no general preference for one over the other. C, procedural C++ and object-oriented C++ are treated as different ways of expressing the same underlying systems work: each makes some problems easier to express and some harder. Language or paradigm purity is not a design goal.

Use the most direct style that fits the problem. Plain C is often the clearest match for kernel ABIs, simple data transforms and explicit ownership. C++ features such as stronger types, RAII, templates and scoped lifetime are used when they materially improve the implementation. Object-oriented C++ is used when encapsulated state or genuine runtime polymorphism improves the model, not to impose class hierarchies on naturally procedural operating-system or hardware interfaces. The language preference remains C/C++ over other ecosystems; another language is introduced only when it offers a concrete technical advantage that C or C++ cannot reasonably provide.

The installed product is one GUI executable, `system-monitor`. It does not install project-owned helper daemons, shell launchers or telemetry command wrappers. Bluetooth HCI capture uses a narrow project-owned Linux ABI declaration rather than BlueZ development headers, and package installation applies the executable's CAP_NET_RAW file capability directly through the Linux xattr ABI rather than requiring the external `setcap` program. Unsupported or inaccessible metrics are shown as unavailable rather than guessed.

Dependency minimisation is an explicit engineering goal. System Monitor should own every mechanism that can reasonably be implemented from a stable Linux or C/C++ contract without making the result weaker, less secure or less maintainable. Build-only headers and command-line helpers are not accepted merely because they are conventional. The target is zero avoidable third-party dependencies: retain only platform/runtime boundaries that would otherwise require recreating a substantial operating-system or desktop subsystem. Dependency removal must preserve every documented feature and must be proven by the same verification gates as feature work.

Slow collection work is kept away from the GTK main thread. Reusable mechanisms belong in the pinned Common library, whose goal is reference-quality, leading-edge, complete implementations that can be reused across projects without sacrificing the strengths of specialised code. Common 1.19.20 owns toolkit-neutral HOME/XDG path discovery, recursive directory creation, deterministic ASCII classification/matching/ordering, stable non-cryptographic FNV-1a mixing, monotonic unsigned-counter delta/rate mechanics and POSIX absolute-deadline conversion, so System Monitor no longer carries equivalent GLib/libc helpers, private byte-hash loops or repeated generic counter/deadline arithmetic. When System Monitor develops a stronger generic implementation, those advantages should be incorporated into Common and the duplicate local implementation removed once Common is at least as strong. Linux hardware, signature composition and genuinely product-specific behaviour remain in System Monitor.

Storage and memory use 1024-based scaling with traditional KB, MB, GB and TB labels. Network rates and negotiated link speeds use decimal 1000-based scaling.

## Appearance

System Monitor packages the MB Corpo typefaces used by its interface. The family names and role weights are taken directly from the pinned Common 1.19.20 typography contract: normal UI text uses **MB Corpo S Title WEB** and title text uses **MB Corpo A Title Cond WEB**, with the S family as the only application-level fallback.

**View → Theme** provides **Follow system**, **Day** and **Night**. Follow system detects the current GTK/Mint light/dark preference and resolves it to the same Day or Night presentation used by the explicit choices; it never inherits an unrelated toolkit palette. Day is the white Infiltrator palette. Night consumes the complete Common 1.19.20 Linux MBLINK reference face: the `#050608` canvas, distinct graphite titlebar/connection/card/surface layers, their matching borders and text greys, and the canonical `#00ADEF` accent. System Monitor follows the same Linux presentation grammar used by MBLINK: layered graphite surfaces use subtle gradients and Common radii, the performance rail uses MBLINK's translucent cyan selection and cyan selected-title treatment, active notebook tabs use the canonical cyan accent, and performance pages are composed from rounded header/detail/graph cards rather than flat GTK regions. Primary graphs are rounded Common-card surfaces, hot-temperature states become MBLINK-style gold/red status pills, CPU and battery telemetry use the canonical cyan and gold accents, and metric captions/values use the Common detail/heading roles. General GTK controls use the Common button background/foreground contract rather than being flattened into one dark surface. The choice is stored per user.

Startup is first-paint oriented: only the shell and Performance page are constructed before the window is shown. The application catalogue is scanned on a worker thread. Every other notebook page is genuinely first-use lazy and remains unconstructed until it is selected; a previously selected tab is restored only after the first Performance frame can be presented. App History's non-visual accounting model starts after that first frame independently of its lazy tab, so process history is retained even when the user never opens the page. Services, Users and File Systems do not install their periodic refresh timers until their page exists. Startup-application discovery and durable enable/disable overrides also run on workers, so selecting or changing the Startup Apps page cannot turn XDG filesystem I/O into a GTK main-thread stall.

## Capabilities

- Performance pages for CPU, memory, disks, partitions, networks, Bluetooth devices, GPUs, batteries and supported NPUs.
- Native Linux Pressure Stall Information for CPU, memory and I/O contention, including 10-second some/full pressure where the kernel exposes it.
- Processes, Application history, Startup, Users, Details, Services and File systems pages.
- cgroup-v2-aware application grouping using the cross-desktop systemd application-unit convention as an additional identity source, with the existing XDG executable/ancestor logic retained as fallback.
- Process inspection, termination, suspend/resume, priority, efficiency mode and CPU-affinity controls.
- Native hardware identity and telemetry with explicit per-metric availability.
- Per-device Bluetooth traffic where the packaged capability and kernel interface permit it.
- Snapshot and process-table export.

## Architecture

```text
GTK 3 presentation
        ↓
plain-C snapshots and application models
        ↓
platform contracts
        ↓
Linux backends and collectors
        ↓
procfs / PSI / cgroup v2 / sysfs / ioctls / D-Bus / optional in-process driver libraries

Common 1.19.20
        ↓
shared parsing / formatting / timing / path / durable-I/O / allocation primitives
```

See [Architecture](docs/ARCHITECTURE.md), [Portability](docs/PORTABILITY.md) and [Hardware collection](docs/HARDWARE.md) for the maintained engineering contracts.

## Build and test

On Debian, Ubuntu or Linux Mint:

```bash
sudo apt install build-essential git pkg-config libgtk-3-dev
git clone --recurse-submodules https://github.com/Infiltrator-Projects/System-Monitor.git
cd System-Monitor
make
./build/system-monitor
```

Run `make check` for the project verification suite. The current installed application is C17; the developer-only source auditor is C++17 and is not shipped in the package. CI builds the application through CMake for build-system parity, runs the canonical Make verification suite once, then applies sanitizer, 32-bit, documentation and release-package gates without replaying the full smoke suite in another hosted job or during publication.

Direct `make install` is disabled. Installation is owned by the Debian package or native installer.

## Release assets

Each numbered release publishes:

- `infiltrator-system-monitor_<version>_amd64.deb`
- `infiltrator-system-monitor-<version>-native-installer.run`

The `.deb` is the generic amd64 package. Its sole Debian/APT identity is `infiltrator-system-monitor`; the user-facing application and executable remain **System Monitor** and `system-monitor`. The `.run` performs a native local build/test/install. Its `native` profile uses machine-specific ISA/tuning at `-O2`; `aggressive` uses `-O3`, the same machine-specific ISA/tuning and LTO, then performs a two-pass profile-guided rebuild trained on System Monitor's real native collector and process-scan paths on the target machine. The PGO pass uses partial-training semantics so unvisited code keeps normal optimisation instead of being penalised. `portable` avoids machine-specific ISA selection.

## Repository policy

Development is kept on `main`. A release commit is publishable only after the full Verify workflow succeeds for the exact current commit. Published tags and release assets are immutable; changing source after a published version requires advancing `support/VERSION`.

Contribution guidance lives in [CONTRIBUTING.md](CONTRIBUTING.md). Security reports follow [SECURITY.md](SECURITY.md).

## Licence

Copyright © 2016-2026 Shannon Smith.

Shannon Smith-owned source, documentation and application artwork are licensed under GPL-3.0-or-later. Retained third-party notices for SysMonTask project ancestry, bundled PCI-name data and the bundled MB Corpo typeface resources are preserved in `support/legal/THIRD_PARTY_NOTICES`.
