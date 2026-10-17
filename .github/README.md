<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# System Monitor

**Project copyright:** © 2000-2026 Shannon Smith  
**Release identity:** [`support/VERSION`](../support/VERSION)

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
sudo apt install build-essential clang doxygen git pkg-config libgtk-3-dev
git clone --recurse-submodules https://github.com/Infiltrator-Projects/System-Monitor.git
cd System-Monitor
make
./build/system-monitor
```

Dependency preparation is explicit. A recursive clone already contains the pinned Common revision; for a non-recursive Git checkout, run `make common-bootstrap` once before configuring or building. Normal Make and CMake verification never fetch or mutate dependencies implicitly.

Run `make check` for the authoritative verification suite. Its documentation gates require Clang and Doxygen rather than silently degrading when either tool is absent. CMake intentionally owns only the Linux application build rather than duplicating the regression graph. Installation is owned by the Debian package or native installer; direct `make install` is disabled. Main-branch release qualification also runs ThreadSanitizer over the asynchronous process-worker path before Linux release artifacts are accepted.

## Releases

Development stays on `main`. `support/VERSION` is the sole release-intent source of truth. Releases are produced from the exact tested commit, and published tags and assets are immutable. The latest published release is the repository's GitHub **Latest** release rather than a second version value maintained in this document.

## Release assets

A published release contains the tested installable package and native installer assets defined by the release workflow; release assets identify the same immutable source revision. Windows verification and release packaging use `support/windows/build-release.ps1`, which consumes the canonical `support/source-groups/windows-product.txt` source manifest so compiler policy, source membership and the published executable cannot drift into separate workflow definitions.

The native `.run` installer downloads the exact release source and its pinned Common source over HTTPS, automatically installs missing Debian-family build prerequisites, then builds for the local CPU with `-O3`, native CPU tuning, LTO and trained two-pass PGO before installing the resulting Debian package. There is no weaker production profile to select. The installer contains no embedded source archive. It needs an Internet connection and either curl or wget; `--help` works offline.

## Technical documentation

- [Engineering philosophy](../docs/PHILOSOPHY.md) — governing engineering and documentation standard.
- [Architecture](../docs/ARCHITECTURE.md) — durable software contracts.
- [Hardware collection](../docs/HARDWARE.md) — hardware rules, units and provenance.
- [Validation](../docs/VALIDATION.md) — compact verification entry point.
- [Contributing](CONTRIBUTING.md) — verification and repository discipline.
- [Security](SECURITY.md) — security boundaries and reporting.

## Licence and provenance

Shannon Smith-owned System Monitor source, documentation and application artwork are licensed under GPL-3.0-or-later. Documented dependencies, first-party Common infrastructure and separately licensed assets retain their own identities and licences.
