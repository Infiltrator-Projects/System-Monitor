<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# System Monitor

**Project copyright:** © 2000-2026 Shannon Smith

[![Verify](https://github.com/Infiltrator-Projects/System-Monitor/actions/workflows/ci.yml/badge.svg)](https://github.com/Infiltrator-Projects/System-Monitor/actions/workflows/ci.yml)

System Monitor is a native C17/GTK 3 desktop system manager for Linux, with a native Windows GUI preview. It provides process, performance, hardware, service, user and filesystem views using native operating-system and hardware interfaces wherever practical.

## Clean-sheet provenance

System Monitor is an original clean-sheet implementation designed and written from the ground up for this project. Its application source was not forked, copied, translated, adapted, ported or derived from GNOME System Monitor, Linux Mint System Monitor, Windows Task Manager or any other system-monitoring application. No other system monitor's source code, internal architecture, algorithms or implementation behaviour is an implementation authority for this project.

**Current source version:** 1.0.175 ([version file](support/VERSION))  
**Shared foundation:** Common 1.19.35 pinned at `src/infiltratr-common`  
**Platform:** Linux desktop; native Windows GUI preview  
**Licence:** GPL-3.0-or-later

## Capabilities

- CPU, memory, storage, network, GPU, NPU, temperature, pressure, battery and supported peripheral monitoring.
- Processes, application history, startup, users, details, services and filesystems.
- Process inspection and control, including termination, suspend/resume, priority, efficiency mode and CPU affinity.
- Native hardware identity and telemetry with explicit unavailable states for unsupported data.
- cgroup-v2-aware application grouping, process history, snapshots and process-table export.

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

## Releases

Numbered releases publish the Debian package, native Linux installer and native Windows executable. Development stays on `main`; releases are produced from the exact tested commit and published tags/assets are immutable.

## Documentation

- [Architecture](docs/ARCHITECTURE.md) — durable software contracts.
- [Hardware collection](docs/HARDWARE.md) — hardware interfaces, units and evidence rules.
- [Contributing](CONTRIBUTING.md) — build, test and repository rules.
- [Security](SECURITY.md) — vulnerability scope and reporting.

## Licence

Shannon Smith-owned System Monitor source, documentation and application artwork are licensed under GPL-3.0-or-later. Documented dependencies, first-party Common infrastructure and separately licensed assets retain their own identities and licences.
