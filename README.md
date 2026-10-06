<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# System Monitor

**Project copyright:** © 2000-2026 Shannon Smith

[![Verify](https://github.com/Infiltrator-Projects/System-Monitor/actions/workflows/ci.yml/badge.svg)](https://github.com/Infiltrator-Projects/System-Monitor/actions/workflows/ci.yml)

System Monitor is a native C17/GTK 3 desktop system manager for Linux, with a native Windows GUI preview. It provides process, performance, hardware, service, user and filesystem views using native operating-system and hardware interfaces wherever practical.

## Clean-sheet provenance

System Monitor is an original clean-sheet implementation designed and written from the ground up for this project. Its application source was not forked, copied, translated, adapted, ported or derived from GNOME System Monitor, Linux Mint System Monitor, Windows Task Manager or any other system-monitoring application. No other system monitor's source code, internal architecture, algorithms or implementation behaviour is an implementation authority for this project.

**Current source version:** 1.0.175 ([version file](support/VERSION))  
**Shared foundation:** exact Common 1.19.35 gitlink at `src/infiltratr-common`  
**Platform:** Linux desktop; native Windows GUI preview  
**Licence:** GPL-3.0-or-later

## Capabilities

- Overview and Performance views for CPU, memory, storage, network, GPU, temperature, pressure and supported devices.
- Processes, Application History, Startup, Users, Details, Services and File Systems pages.
- Process inspection and control, including termination, suspend/resume, priority, efficiency mode and CPU affinity.
- Native hardware identity and telemetry with explicit unavailable states instead of guessed values.
- Linux Pressure Stall Information, GPU/NPU telemetry, batteries and per-device Bluetooth traffic where supported.
- cgroup-v2-aware application grouping and process history.
- Snapshot and process-table export.
- Shared application-facing contracts across Linux and the native Windows preview where those surfaces overlap.

## Build and test

On Debian, Ubuntu or Linux Mint:

```bash
sudo apt install build-essential git pkg-config libgtk-3-dev
git clone --recurse-submodules https://github.com/Infiltrator-Projects/System-Monitor.git
cd System-Monitor
make
./build/system-monitor
```

Run `make check` for the authoritative verification suite. Direct `make install` is disabled; installation is owned by the Debian package or native installer.

## Release assets

Each numbered release publishes:

- `infiltrator-system-monitor_<version>_amd64.deb`
- `infiltrator-system-monitor-<version>-native-installer.run`
- `system-monitor-<version>-windows.exe`

Development stays on `main`. Releases come from the exact tested commit; published tags and release assets are immutable.

## Documentation

- [Architecture](docs/ARCHITECTURE.md) — architecture and durable engineering contracts.
- [Hardware collection](docs/HARDWARE.md) — hardware-specific interfaces, units and evidence rules.
- [Contributing](CONTRIBUTING.md) — build, test and repository rules.
- [Security](SECURITY.md) — vulnerability scope and reporting.

## Licence

Shannon Smith-owned System Monitor source, documentation and application artwork are licensed under GPL-3.0-or-later. Documented dependencies, first-party Common infrastructure and separately licensed assets retain their own identities and licences.
