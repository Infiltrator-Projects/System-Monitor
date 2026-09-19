<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# System Monitor

[![Verify](https://github.com/Infiltrator-Projects/System-Monitor/actions/workflows/ci.yml/badge.svg)](https://github.com/Infiltrator-Projects/System-Monitor/actions/workflows/ci.yml)

System Monitor is a native C17/GTK 3 desktop system manager for Linux. It provides Task-Manager-style process, performance, hardware, service and user views while collecting data directly from native operating-system interfaces wherever practical.

**Current source version:** 1.0.41 ([version file](support/VERSION))\
**Shared foundation:** exact Common 1.19.3 gitlink at `src/infiltratr-common`  
**Platform:** Linux desktop; additional native backends are planned  
**Licence:** GPL-3.0-or-later

## Engineering ethos

System Monitor is built from first principles: establish what the operating system or hardware interface actually guarantees, then implement the required behaviour directly where practical. It deliberately avoids depending on the output or behaviour of external monitoring utilities when an authoritative native interface is available, because those dependencies can change independently of this project. External libraries are used when their documented contract is the stronger solution; important product behaviour remains owned by System Monitor.

The project does not equate newer with better. Proven interfaces and techniques remain when they are the strongest solution, and newer ones replace them when they are demonstrably better. Age, popularity, fashion, convenience and implementation effort do not decide the architecture: the strongest practical implementation does. System Monitor should not knowingly accept a weaker solution merely because it is easier, quicker, more conventional or more portable.

C and C++ are equal, first-class implementation languages for the project. The choice between them is made according to the needs of the component, with no general preference for one over the other. C, procedural C++ and object-oriented C++ are treated as different ways of expressing the same underlying systems work: each makes some problems easier to express and some harder. Language or paradigm purity is not a design goal.

Use the most direct style that fits the problem. Plain C is often the clearest match for kernel ABIs, simple data transforms and explicit ownership. C++ features such as stronger types, RAII, templates and scoped lifetime are used when they materially improve the implementation. Object-oriented C++ is used when encapsulated state or genuine runtime polymorphism improves the model, not to impose class hierarchies on naturally procedural operating-system or hardware interfaces. The language preference remains C/C++ over other ecosystems; another language is introduced only when it offers a concrete technical advantage that C or C++ cannot reasonably provide.

The installed product is one GUI executable, `system-monitor`. It does not install project-owned helper daemons, shell launchers or telemetry command wrappers. Unsupported or inaccessible metrics are shown as unavailable rather than guessed.

Slow collection work is kept away from the GTK main thread. Reusable mechanisms belong in the pinned Common library, whose goal is reference-quality, leading-edge, complete implementations that can be reused across projects without sacrificing the strengths of specialised code. When System Monitor develops a stronger generic implementation, those advantages should be incorporated into Common and the duplicate local implementation removed once Common is at least as strong. Linux hardware and genuinely product-specific behaviour remain in System Monitor.

Storage and memory use 1024-based scaling with traditional KB, MB, GB and TB labels. Network rates and negotiated link speeds use decimal 1000-based scaling.

## Appearance

System Monitor packages the MB Corpo typefaces used by its interface. Normal UI text uses **MB Corpo S Title WEB** and title text uses **MB Corpo A Title Cond WEB**, with the S family as the only application-level fallback.

**View → Theme** provides **Follow system**, **Day** and **Night**. Follow system keeps the host GTK/Mint palette authoritative. Day and Night use the semantic palettes supplied by the pinned Common release, and the choice is stored per user.

## Capabilities

- Performance pages for CPU, memory, disks, partitions, networks, Bluetooth devices, GPUs, batteries and supported NPUs.
- Processes, Application history, Startup, Users, Details, Services and File systems pages.
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
procfs / sysfs / ioctls / D-Bus / optional in-process driver libraries

Common 1.19.3
        ↓
shared parsing / formatting / timing / path / durable-I/O / allocation primitives
```

See [Architecture](docs/ARCHITECTURE.md), [Portability](docs/PORTABILITY.md) and [Hardware collection](docs/HARDWARE.md) for the maintained engineering contracts.

## Build and test

On Debian, Ubuntu or Linux Mint:

```bash
sudo apt install build-essential git pkg-config libgtk-3-dev libbluetooth-dev libcap2-bin
git clone --recurse-submodules https://github.com/Infiltrator-Projects/System-Monitor.git
cd System-Monitor
make
./build/system-monitor
```

Run `make check` for the project verification suite. The current installed application is C17; the developer-only source auditor is C++17 and is not shipped in the package. CI also exercises CMake/CTest, sanitizers, 32-bit compilation, generated Doxygen documentation with warnings treated as errors, and release-package construction.

Direct `make install` is disabled. Installation is owned by the Debian package or native installer.

## Release assets

Each numbered release publishes:

- `infiltrator-system-monitor_<version>_amd64.deb`
- `infiltrator-system-monitor-<version>-native-installer.run`

The `.deb` is the generic amd64 package. Its Debian/APT identity is `infiltrator-system-monitor`; the user-facing application and executable remain **System Monitor** and `system-monitor`. Version 1.0.36 migrates existing `system-monitor` package installations through the repository transition package, leaving that old package name as compatibility-only; the protected `infiltrator-system-monitor` identity is authoritative for future APT releases. The `.run` performs a native local build/test/install.

## Repository policy

Development is kept on `main`. A release commit is publishable only after the full Verify workflow succeeds for the exact current commit. Published tags and release assets are immutable; changing source after a published version requires advancing `support/VERSION`.

Contribution guidance lives in [CONTRIBUTING.md](CONTRIBUTING.md). Security reports follow [SECURITY.md](SECURITY.md).

## Licence

Copyright © 2016–2026 Shannon Smith.

Shannon Smith-owned source, documentation and application artwork are licensed under GPL-3.0-or-later. Retained third-party notices for SysMonTask project ancestry, bundled PCI-name data and the bundled MB Corpo typeface resources are preserved in `support/legal/THIRD_PARTY_NOTICES`.
