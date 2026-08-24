<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Linux System Monitor

[![Verify](https://github.com/The-First-Infiltrator/System-Monitor/actions/workflows/ci.yml/badge.svg)](https://github.com/The-First-Infiltrator/System-Monitor/actions/workflows/ci.yml)

Linux System Monitor is a native C17/GTK 3 desktop system manager for Linux. It presents the useful parts of Windows Task Manager while collecting hardware, process, service and user information directly from native operating-system interfaces wherever practical.

**Current source version:** 1.0.3 ([version file](support/VERSION))  
**Shared foundation:** exact Infiltratr Common 1.13.0 gitlink at `src/infiltratr-common`  
**Platform:** Linux desktop; additional native backends are planned  
**Licence:** GPL-3.0-or-later

## Design priorities

Linux System Monitor is deliberately a native application rather than an orchestration layer. The installed product is one GUI executable, `linux-system-monitor`. It does not install project-owned helper daemons, shell launchers, privileged collectors or policy changes.

Collection prefers project-owned C parsers and native kernel/driver interfaces over external commands. Unsupported metrics are presented as unavailable rather than guessed or fabricated.

Storage, memory and network quantities use 1024-based scaling with traditional labels: 1 KB = 1024 bytes, 1 MB = 1024 KB, 1 GB = 1024 MB and 1 TB = 1024 GB.

## Capabilities

- Performance pages for CPU, memory, disks, partitions, networks, GPUs, batteries and supported NPUs.
- Processes, Application history, Startup, Users, Details, Services and File systems pages.
- Process inspection, termination, suspend/resume, priority, efficiency mode and CPU-affinity controls.
- Native hardware identity and telemetry with explicit per-metric availability.
- GPU engine telemetry where the active driver exposes a safe native interface.
- Direct wireless metadata, storage inventory and system-source collection without depending on command-line providers.
- Explicit snapshot and process-table export actions.

## Architecture

```text
GTK 3 presentation
        ↓
platform-neutral snapshots and process contracts
        ↓
Linux backend ownership and sampling policy
        ↓
procfs / sysfs / ioctls / D-Bus / optional in-process driver libraries

Infiltratr Common 1.13.0
        ↓
reusable parsing / formatting / timing / durable I/O / allocation primitives
```

Plain-C snapshots separate presentation from operating-system collection. Linux-specific paths, ioctls, D-Bus calls and driver details stay below those contracts so another native backend can be added without rewriting the application model.

ISO C17 is the project language baseline. Simpler C11/C99 constructs are preferred when they express the same design clearly. C23 and C++ are not required merely for convenience. Resource-owning modules make ownership, lifetime and cleanup explicit in portable C.

`src/infiltratr-common` is pinned to one exact Common commit and linked statically. Common owns genuinely reusable primitives; hardware-specific collection policy remains in System Monitor.

See [Architecture](docs/ARCHITECTURE.md), [Portability](docs/PORTABILITY.md) and [Hardware collection](docs/HARDWARE.md) for the maintained engineering contracts.

## Build and test

On Debian, Ubuntu or Linux Mint:

```bash
sudo apt install build-essential git pkg-config libgtk-3-dev
git clone --recurse-submodules https://github.com/The-First-Infiltrator/System-Monitor.git
cd System-Monitor
make
./build/linux-system-monitor
```

Useful targets:

```bash
make check
make deb
make native-installer
make dist
make release
```

`make check` covers strict warnings, source/licence policy, portability checks, static analysis where supported, parser/backend tests, hardware fixtures, process controls, coverage floors, lifecycle stability, package boundaries and installer safety. CI also runs CMake/CTest and a required 32-bit compile check.

Direct `make install` is disabled. Installation is owned by the Debian package or native installer.

## Release assets

A numbered release publishes exactly three primary artifacts:

| File | Purpose |
| --- | --- |
| `linux-system-monitor_<version>_amd64.deb` | Generic amd64 Debian package. |
| `linux-system-monitor-<version>-native-installer.run` | Native local build/test/install program. |
| `Linux-System-Monitor-<version>-source.zip` | Tested standalone source archive including the exact pinned Common source. |

Install the generic package with:

```bash
sudo apt install ./linux-system-monitor_<version>_amd64.deb
```

Or use the native installer:

```bash
chmod +x linux-system-monitor-<version>-native-installer.run
./linux-system-monitor-<version>-native-installer.run
```

## Repository layout

| Path | Purpose |
| --- | --- |
| `src/` | Application code, platform seams, Linux providers and the pinned Common submodule. |
| `docs/` | Maintained architecture, portability and hardware-collection contracts. |
| `support/tests/` | Deterministic regression and smoke tests. |
| `support/tools/` | C-based developer, packaging and validation tools. |
| `support/packaging/` | Debian packaging metadata. |
| `support/installer/` | Native installer bootstrap/internals. |
| `support/resources/` | Desktop resources, icon and bundled data. |
| `support/legal/` | Third-party notices and retained legal material. |
| `.github/` | CI/release workflows, contribution policy and issue forms. |

The root is intentionally kept small. Internal project material belongs under `src/` or `support/`; public engineering documentation belongs under `docs/` and GitHub community policy under `.github/`.

## Repository and release policy

This repository uses `main` as its working branch. Development changes are made directly on `main`; the normal project workflow does not depend on feature or release branches.

Every push to `main` runs the full Verify workflow. Ordinary commits do not publish. A commit is release-eligible only when its subject begins with the exact source version as `Release <version>` and the complete Verify workflow succeeds.

The publisher checks out the exact tested commit, verifies it is still current `main`, reruns CMake/CTest and Make verification, builds the `.deb`, `.run` and source ZIP, then creates the version tag and GitHub release. Published version tags and releases are immutable release identities.

Once a version tag exists, further source changes must advance `support/VERSION` before Verify will accept them. This prevents `main` from silently diverging from an already-published release while claiming the same version.

## Contributing and security

Engineering contributions should follow [CONTRIBUTING.md](.github/CONTRIBUTING.md). Repository conduct is covered by the [Code of Conduct](.github/CODE_OF_CONDUCT.md). Security-sensitive reports should follow [SECURITY.md](.github/SECURITY.md) rather than being opened as public issues.

GitHub issue forms are provided for reproducible bugs and feature proposals so hardware, platform and version context is captured consistently.

## Documentation

- [Architecture](docs/ARCHITECTURE.md) — ownership, module boundaries and application lifecycle.
- [Portability](docs/PORTABILITY.md) — C17 rules, platform seams and portability requirements.
- [Hardware collection](docs/HARDWARE.md) — native telemetry sources, availability semantics and privilege behaviour.

These are the maintained engineering documents. The source-style audit rejects unplanned Markdown additions so documentation remains complete without becoming fragmented.

## Licence

Copyright © 2026 Shannon Smith.

Shannon Smith-owned source and documentation are licensed under the GNU General Public License version 3 or, at your option, any later version (`GPL-3.0-or-later`). See `LICENSE`.

The retained SysMonTask icon and bundled PCI-name data remain under compatible BSD 3-Clause terms; their authorship and complete notices are preserved in `support/legal/THIRD_PARTY_NOTICES` and the corresponding file-specific licence records.
