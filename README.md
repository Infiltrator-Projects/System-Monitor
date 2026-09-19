<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# System Monitor

[![Verify](https://github.com/Infiltrator-Projects/System-Monitor/actions/workflows/ci.yml/badge.svg)](https://github.com/Infiltrator-Projects/System-Monitor/actions/workflows/ci.yml)

System Monitor is a native C17/GTK 3 desktop system manager for Linux. It provides Task-Manager-style process, performance, hardware, service and user views while collecting data directly from native operating-system interfaces wherever practical.

**Current source version:** 1.0.36 ([version file](support/VERSION))\
**Shared foundation:** exact Common 1.19.2 gitlink at `src/infiltratr-common`  
**Platform:** Linux desktop; additional native backends are planned  
**Licence:** GPL-3.0-or-later

## Engineering ethos

System Monitor is built from first principles: establish what the operating system or hardware interface actually guarantees, then implement the required behaviour directly where practical. It deliberately avoids depending on the output or behaviour of external monitoring utilities when an authoritative native interface is available, because those dependencies can change independently of this project. External libraries are used when their documented contract is the stronger solution; important product behaviour remains owned by System Monitor.

The project does not equate newer with better. Proven kernel interfaces and collection methods remain when they are the strongest source of truth; a replacement or dependency must improve correctness, coverage, resilience, performance or maintainability rather than merely moving responsibility elsewhere.

The installed product is one GUI executable, `system-monitor`. It does not install project-owned helper daemons, shell launchers or telemetry command wrappers. Unsupported or inaccessible metrics are shown as unavailable rather than guessed.

Slow collection work is kept away from the GTK main thread. Reusable parsing, formatting, timing, path, allocation and durable-I/O primitives come from the pinned Common library; Linux hardware and product-specific behaviour remain in System Monitor.

Storage and memory use 1024-based scaling with traditional KB, MB, GB and TB labels. Network rates and negotiated link speeds use decimal 1000-based scaling.

## Appearance

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

Common 1.19.2
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

Run `make check` for the project verification suite. The installed application remains C17; the developer-only source auditor uses C++17 RAII/filesystem facilities and is not shipped in the package. CI also exercises CMake/CTest, sanitizers, 32-bit compilation, generated Doxygen documentation with warnings treated as errors, and release-package construction.

Direct `make install` is disabled. Installation is owned by the Debian package or native installer.

## Release assets

Each numbered release publishes:

- `infiltrator-system-monitor_<version>_amd64.deb`
- `system-monitor-<version>-native-installer.run`

The `.deb` is the generic amd64 package. Its Debian/APT identity is `infiltrator-system-monitor`; the user-facing application and executable remain **System Monitor** and `system-monitor`. Version 1.0.36 also migrates existing `system-monitor` package installations through the repository transition package. The `.run` performs a native local build/test/install.

## Repository policy

Development is kept on `main`. A release commit is publishable only after the full Verify workflow succeeds for the exact current commit. Published tags and release assets are immutable; changing source after a published version requires advancing `support/VERSION`.

Contribution guidance lives in [CONTRIBUTING.md](.github/CONTRIBUTING.md). Security reports follow [SECURITY.md](.github/SECURITY.md).

## Licence

Copyright © 2016 Shannon Smith.

Shannon Smith-owned source, documentation and application artwork are licensed under GPL-3.0-or-later. Retained third-party notices for SysMonTask project ancestry and bundled PCI-name data are preserved in `support/legal/THIRD_PARTY_NOTICES`.
