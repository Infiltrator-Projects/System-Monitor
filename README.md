<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# System Monitor

**Project copyright:** © 2000-2026 Shannon Smith  
**Current source version:** 1.0.177 ([version file](support/VERSION))

[![Verify](https://github.com/Infiltrator-Projects/System-Monitor/actions/workflows/ci.yml/badge.svg)](https://github.com/Infiltrator-Projects/System-Monitor/actions/workflows/ci.yml)

System Monitor is a native C17/GTK 3 desktop system manager for Linux, with a native Windows GUI preview. It provides process, performance, hardware, service, user and filesystem views using native operating-system and hardware interfaces wherever practical.

## Clean-sheet provenance

System Monitor is an original clean-sheet implementation designed and written from the ground up for this project. Its application source was not forked, copied, translated, adapted, ported or derived from GNOME System Monitor, Linux Mint System Monitor, Windows Task Manager or any other system-monitoring application. No other system monitor's source code, internal architecture, algorithms or implementation behaviour is an implementation authority for this project.

## Capabilities

- CPU, memory, storage, network, GPU, NPU, temperature, pressure, battery and supported peripheral monitoring.
- Processes, application history, startup, users, details, services and filesystems.
- Process inspection and control, including termination, suspend/resume, priority, efficiency mode and CPU affinity.
- cgroup-v2-aware application grouping, process history, snapshots and process-table export.
- Exact first-party shared infrastructure is retained at `src/infiltratr-common`.

## Build and test

```bash
sudo apt install build-essential git pkg-config libgtk-3-dev
git clone --recurse-submodules https://github.com/Infiltrator-Projects/System-Monitor.git
cd System-Monitor
make
./build/system-monitor
```

Run `make check` for the authoritative verification suite. Installation is owned by the Debian package or native installer; direct `make install` is disabled.

## Releases

Development stays on `main`. Releases are produced from the exact tested commit; published tags and assets are immutable.

## Release assets

A published release contains the tested installable package and native installer assets defined by the release workflow; release assets identify the same immutable source revision.

## Technical documentation

- [Architecture](docs/ARCHITECTURE.md) — durable software contracts.
- [Hardware collection](docs/HARDWARE.md) — hardware rules, units and provenance.
- [Validation](docs/VALIDATION.md) — compact verification entry point.
- [Contributing](CONTRIBUTING.md) — verification and repository discipline.
- [Security](SECURITY.md) — security boundaries and reporting.

## Licence and provenance

Shannon Smith-owned System Monitor source, documentation and application artwork are licensed under GPL-3.0-or-later. Documented dependencies, first-party Common infrastructure and separately licensed assets retain their own identities and licences.