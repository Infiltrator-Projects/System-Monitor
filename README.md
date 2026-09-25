<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# System Monitor

**Project copyright:** © 2000-2026 Shannon Smith

[![Verify](https://github.com/Infiltrator-Projects/System-Monitor/actions/workflows/ci.yml/badge.svg)](https://github.com/Infiltrator-Projects/System-Monitor/actions/workflows/ci.yml)

System Monitor is a native C17/GTK 3 desktop system manager for Linux. It provides Task-Manager-style process, performance, hardware, service and user views while collecting data directly from native operating-system interfaces wherever practical.

**Current source version:** 1.0.106 ([version file](support/VERSION))\
**Shared foundation:** exact Common 1.19.27 gitlink at `src/infiltratr-common`  
**Platform:** Linux desktop; native Windows GUI preview  
**Licence:** GPL-3.0-or-later

The Windows preview now uses the same application-facing Performance contract and shared view-model layer as Linux while retaining native Win32/GDI rendering and native Windows collectors. CPU usage and memory remain live through Win32/PSAPI; native processor topology supplies physical/logical core counts, sockets, NUMA nodes and cache totals, while Windows processor-power and registry interfaces provide current/max/base frequency when the platform exposes them. Physical disks are discovered with storage IOCTLs and expose identity, capacity, live throughput, activity, response and queue data plus mapped volume/filesystem usage. Network adapters are discovered through IP Helper and expose addresses, MAC identity, connection state, link speed, cumulative traffic and live send/receive utilisation. Graphics adapters are identified natively; PDH supplies GPU engine utilisation where supported, and DXGI supplies dedicated-memory capacity. Adapter-wide memory consumption and unsupported temperature telemetry remain explicit `N/A`: DXGI process-local usage is not a whole-adapter measurement. The Performance rail now enumerates every discovered disk, network adapter and GPU, retains type-plus-device-index selection, and scrolls when the native device set exceeds the visible rail. CPU/Memory keep the richer canonical graph/detail composition; device pages consume toolkit-neutral disk/network/GPU titles, summaries and formatted metrics from `performance_view.[ch]`. View → Theme continues to provide Follow system, Day and Night by projecting Common 1.19.27's palette and geometry contracts directly into Win32 rather than mirroring their values locally. Processes remains read-only. Overview is now native on Windows and is fed by the same completed-snapshot history contract as Linux; the remaining Windows product pages stay explicit placeholders until their native backends are implemented.

The cross-tab summary is projected once in the platform-neutral presentation layer, so Linux and Windows share the same CPU, memory, disk, network and GPU formatting/availability semantics. A bounded Overview history records only backend-completed snapshots, with completion generations and monotonic timestamps preventing a busy asynchronous collector from being redrawn as false flat-line samples. Overview presents CPU, memory, aggregate physical-disk activity/throughput, busiest network/GPU, hottest available temperature and CPU/memory/I/O pressure plus the three busiest CPU processes. Device cards resolve retained stable identities against current topology before navigating to Performance, so hotplug or reordering cannot redirect a retained card to another device. The Windows shell is painted before any monitoring or process backend is initialised. Backend startup is lazy and fail-open: unavailable telemetry is reported inside the already-visible GUI rather than preventing the application window from appearing.

## Engineering ethos

System Monitor is built from first principles: establish what the operating system or hardware interface actually guarantees, then implement the required behaviour directly where practical. It deliberately avoids depending on the output or behaviour of external monitoring utilities when an authoritative native interface is available, because those dependencies can change independently of this project. External libraries are used when their documented contract is the stronger solution; important product behaviour remains owned by System Monitor.

The project does not equate newer with better. Proven interfaces and techniques remain when they are the strongest solution, and newer ones replace them when they are demonstrably better. Age, popularity, fashion, convenience and implementation effort do not decide the architecture: the strongest practical implementation does. System Monitor should not knowingly accept a weaker solution merely because it is easier, quicker, more conventional or more portable.

Competitor products are treated as evidence and idea sources, not feature checklists. System Monitor favours a coherent monitoring and diagnostic model over feature-count parity: a new capability must materially strengthen what the product already exists to do and should integrate with its measurements, navigation, actions and visual language rather than becoming an isolated feature island. The maintained admission rules live in [Design](docs/DESIGN.md) and ADR-006 in [Decisions](docs/DECISIONS.md).

The graphical north star for the current UI programme is recorded in [System Monitor UI Vision](docs/design/system-monitor-ui-vision.md). The redesign is incremental: preserve native behaviour and diagnostic depth while making the approachable surfaces more visual, colourful, card-oriented and GUI-first. The Overview now uses native GTK/Cairo resource icons, radial gauges, grid-backed gradient history plots and graphical process activity rows as the first concrete dashboard implementation of that direction.

C and C++ are equal, first-class implementation languages for the project. The choice between them is made according to the needs of the component, with no general preference for one over the other. C, procedural C++ and object-oriented C++ are treated as different ways of expressing the same underlying systems work: each makes some problems easier to express and some harder. Language or paradigm purity is not a design goal.

Use the most direct style that fits the problem. Plain C is often the clearest match for kernel ABIs, simple data transforms and explicit ownership. C++ features such as stronger types, RAII, templates and scoped lifetime are used when they materially improve the implementation. Object-oriented C++ is used when encapsulated state or genuine runtime polymorphism improves the model, not to impose class hierarchies on naturally procedural operating-system or hardware interfaces. The language preference remains C/C++ over other ecosystems; another language is introduced only when it offers a concrete technical advantage that C or C++ cannot reasonably provide.

The Linux installed product is one GUI executable, `system-monitor`. The Windows preview is also a single native GUI executable and currently ships as a portable release asset. Neither path installs project-owned helper daemons, shell launchers or telemetry command wrappers. Bluetooth HCI capture uses a narrow project-owned Linux ABI declaration rather than BlueZ development headers, and package installation applies the executable's CAP_NET_RAW file capability directly through the Linux xattr ABI rather than requiring the external `setcap` program. Unsupported or inaccessible metrics are shown as unavailable rather than guessed.

Dependency minimisation is an explicit engineering goal. System Monitor should own every mechanism that can reasonably be implemented from a stable Linux or C/C++ contract without making the result weaker, less secure or less maintainable. Build-only headers and command-line helpers are not accepted merely because they are conventional. The target is zero avoidable third-party dependencies: retain only platform/runtime boundaries that would otherwise require recreating a substantial operating-system or desktop subsystem. Dependency removal must preserve every documented feature and must be proven by the same verification gates as feature work.

Slow collection work is kept away from the GTK main thread. Reusable mechanisms belong in the pinned Common library, whose goal is reference-quality, leading-edge, complete implementations that can be reused across projects without sacrificing the strengths of specialised code. Common 1.19.27 owns toolkit-neutral HOME/XDG path discovery, recursive directory creation, deterministic ASCII classification/matching/ordering, stable non-cryptographic FNV-1a mixing, monotonic unsigned-counter delta/rate mechanics and POSIX absolute-deadline conversion, so System Monitor no longer carries equivalent GLib/libc helpers, private byte-hash loops or repeated generic counter/deadline arithmetic. When System Monitor develops a stronger generic implementation, those advantages should be incorporated into Common and the duplicate local implementation removed once Common is at least as strong. Linux hardware, signature composition and genuinely product-specific behaviour remain in System Monitor.

Storage and memory use 1024-based scaling with traditional KB, MB, GB and TB labels. Network rates and negotiated link speeds use decimal 1000-based scaling.

## Appearance

System Monitor consumes the MB Corpo family names, weights, filenames and verified asset provenance from the pinned Common 1.19.27 typography contract. Linux requests **MB Corpo S Title WEB** for normal UI text and **MB Corpo A Title Cond WEB** for display titles, retaining the GTK system font as a platform fallback when those faces are absent. The portable Windows executable embeds the three Common-verified faces as process-private resources, registers them only for the lifetime of System Monitor and refuses silent GDI substitution, so its typography remains identical without installing fonts into Windows.

**View → Theme** provides **Follow system**, **Day** and **Night**. Follow system detects the current GTK/Mint light/dark preference and resolves it to the same Day or Night presentation used by the explicit choices; it never inherits an unrelated toolkit palette. Day is the white Infiltrator palette. Night consumes the complete Common 1.19.27 Linux MBLINK reference face: the `#050608` canvas, distinct graphite titlebar/connection/card/surface layers, their matching borders and text greys, and the canonical `#00ADEF` accent. System Monitor follows the same Linux presentation grammar used by MBLINK: layered graphite surfaces use subtle gradients and Common radii, the primary application navigation is now a persistent icon-led left rail with a bright selected state, the Performance device rail retains MBLINK's translucent cyan selection and cyan selected-title treatment, and performance pages are composed from rounded header/detail/graph cards rather than flat GTK regions. Primary graphs are rounded Common-card surfaces, hot-temperature states become MBLINK-style gold/red status pills, CPU and battery telemetry use the canonical cyan and gold accents, and metric captions/values use the Common detail/heading roles. General GTK controls use the Common button background/foreground contract rather than being flattened into one dark surface. The choice is stored per user.

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
- Human-facing civil timestamps and displayed durations follow the temporal policy published by Infiltrator System Settings when that authority is installed and configured. Fixed-unit modes use their real elapsed units (decimal, Internet beats, binary, hexadecimal, Julian-day fractions, sidereal, Chinese day partitions and Indian ghaṭī); apparent-solar elapsed intervals are civil-date anchored; Roman/Edo unequal seasonal labels retain conventional elapsed H:M:S because they do not define one fixed duration unit. Internal sampling, rate calculations, accounting and persisted machine values remain canonical SI/Unix time.

## Architecture

```text
GTK 3 presentation (Linux) / Win32 presentation (Windows preview)
        ↓
plain-C snapshots and application models
        ↓
platform contracts
        ↓
Linux backends and collectors / Windows native backends
        ↓
procfs / PSI / cgroup v2 / sysfs / ioctls / D-Bus / optional in-process driver libraries

Common 1.19.27
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
- `system-monitor-<version>-windows.exe`

The Windows `.exe` is the current native GUI preview: Performance and Processes are live and read-only, while the remaining product pages are visible placeholders. The `.deb` is the generic amd64 package. Its sole Debian/APT identity is `infiltrator-system-monitor`; the user-facing application and executable remain **System Monitor** and `system-monitor`. The `.run` performs a native local build/test/install. Its `native` profile uses machine-specific ISA/tuning at `-O2`; `aggressive` uses `-O3`, the same machine-specific ISA/tuning and LTO, then performs a two-pass profile-guided rebuild trained on System Monitor's real native collector and process-scan paths on the target machine. The PGO pass uses partial-training semantics so unvisited code keeps normal optimisation instead of being penalised. `portable` avoids machine-specific ISA selection.

## Repository policy

Development is kept on `main`. A release commit is publishable only after the full Verify workflow succeeds for the exact current commit. Published tags and release assets are immutable; changing source after a published version requires advancing `support/VERSION`.

Contribution guidance lives in [CONTRIBUTING.md](CONTRIBUTING.md). Security reports follow [SECURITY.md](SECURITY.md).

## Licence

Copyright © 2000-2026 Shannon Smith.

Shannon Smith-owned source, documentation and application artwork are licensed under GPL-3.0-or-later. The embedded PCI identity registry contains normalized factual vendor/device mappings in a project-defined flat schema and generated numeric lookup tables; no external PCI database file, hierarchy, comments or formatting are distributed. Linux treats the preferred MB Corpo families as presentation hints and does not install font files. The portable Windows executable embeds the three Common-verified MB Corpo faces as process-private resources so its typography is self-contained without modifying the user's global Windows font installation.
