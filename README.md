<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Linux System Monitor

[![Verify](https://github.com/The-First-Infiltrator/System-Monitor/actions/workflows/ci.yml/badge.svg)](https://github.com/The-First-Infiltrator/System-Monitor/actions/workflows/ci.yml)

Linux System Monitor is a native C17/GTK 3 desktop system manager for Linux. It presents the useful parts of Windows Task Manager while collecting Linux hardware, process and service information directly wherever practical.

**Current source version:** 1.13.19  
**Platform:** Linux desktop  
**Licence:** GPL-3.0-or-later

## Capabilities

- Performance pages for CPU, memory, disks, partitions, networks, GPUs, batteries and supported NPUs.
- Processes, Application history, Startup, Users, Details, Services and File systems pages.
- Process inspection, termination, suspend/resume, priority, efficiency mode and CPU-affinity controls.
- Hardware identity and telemetry from native Linux interfaces, with unsupported metrics shown as unavailable rather than guessed.
- Explicit snapshot and process-table export actions.

Storage, memory and network quantities use 1024-based scaling with traditional labels: 1 KB = 1024 bytes, 1 MB = 1024 KB, 1 GB = 1024 MB and 1 TB = 1024 GB.

## Architecture

The installed product is one GUI executable, `linux-system-monitor`. It does not install project-owned helper daemons, shell launchers, privileged collectors or policy changes. Elevated execution is an explicit external user choice.

Monitoring prefers project-owned C parsers over command orchestration. It does not require `lspci`, `lshw`, `lsblk`, `sensors`, `dmidecode`, `nvidia-smi`, `systemctl`, `loginctl` or `nmcli` as telemetry providers.

Plain-C snapshots and platform-neutral monitor/process contracts separate the GTK interface from Linux-specific providers. procfs/sysfs paths, ioctls and D-Bus details stay below those contracts so another operating-system backend can be added without rewriting the application model.

`src/infiltratr-common` is pinned to Infiltratr Common 1.11.0 and linked statically. Common owns reusable formatting, parsing, timing and dynamic-library mechanics; hardware-specific collection policy stays in System Monitor.

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

`make check` covers strict warnings, source/licence policy, static analysis where supported, parser/backend tests, hardware fixtures, process controls, coverage floors, lifecycle stability, package boundaries and installer safety. CI also runs CMake/CTest and a required 32-bit compile check.

Direct `make install` is disabled. Installation is owned by the Debian package or native installer.

## Release assets

A numbered release publishes:

| File | Purpose |
| --- | --- |
| `linux-system-monitor_<version>_amd64.deb` | Generic amd64 Debian package. |
| `linux-system-monitor-<version>-native-installer.run` | Hardware-native local build/install program. |
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

## Repository and release policy

This repository uses `main` as its working branch. Development changes are made directly on `main`; the normal project workflow does not depend on PR, feature or release branches.

Every push to `main` runs the full Verify workflow. Ordinary commits do not publish. A commit is release-eligible only when its subject begins with the exact source version as `Release <version>` and the complete Verify workflow succeeds.

The publisher checks out the exact tested commit, verifies it is still current `main`, reruns CMake/CTest and Make verification, builds the `.deb`, `.run` and standalone source ZIP, then creates the version tag and GitHub release. Existing version tags and published releases are immutable and are never moved, replaced or edited in place.

Manually runnable build/test helpers, where present, are diagnostic tools only and are not release-approval mechanisms.

## Source layout

- `src/` — application code, Linux providers and the pinned Common submodule.
- `support/` — tests, release tooling, resources, packaging, version metadata, installer internals and legal notices.

## Release provenance

Published release assets and tags are immutable. Historical source identities retained from the 1.13 line are:

| Version | Published commit |
| --- | --- |
| 1.13.2 | `a2562d6509e263d00e9aacdec623a6560207ba7e` |
| 1.13.3 | `f30df5a8d7722286a96d8ba5b2ef6d8f94492bdb` |
| 1.13.4 | `a4b7de1a382b9709accf191d846ea1113d49e48e` |
| 1.13.5 | `e2c87dc0cb1f2074308307a8a0d82a6f0a264950` |
| 1.13.6 | `86cad6d00da617e42632127bf3bc5cd0494a66fd` |
| 1.13.7 | `99fb4a5264f014d84acfb9430536a915f6741fcf` |
| 1.13.8 | `a46b47007fa899b7af46cded183faab7c43b1dfd` |
| 1.13.9 | `66f7be218da025f6c38c89008493c04eb88cf90d` |
| 1.13.10 | `84ddd343cdf6fe73bc303ec406a48bb90e9fb755` |
| 1.13.11 | `5a3b63653ed8e551176bce9ee09e798d985a78c8` |
| 1.13.12 | `e1110329ef9ee27a3705b7539652cb24f250f1c6` |
| 1.13.13 | `d98412f9c1a813f634a3f53240d6a602da59cde2` |
| 1.13.14 | `9b687a8b1f8bc0936120bf3adb30518372481706` |
| 1.13.15 | `12927f1f4414c34129907bc19513612f3b52aee3` |
| 1.13.16 | `50ff82ef33842de9a1287cfdacca835a117e8819` |
| 1.13.17 | `313310f213db826dde2de9dd1befae401b2e0f3f` |
| 1.13.18 | `24cc244db5810be8b44fdf09b40ee386bf890f63` |

The v1.13.2 through v1.13.11 release pages/tags were removed during repository maintenance in August 2026; the commit identities above are retained for reconstruction and audit purposes. The current source version is stated at the top of this README and is checked against `support/VERSION` by CI.

## Licence

Copyright © 2026 Shannon Smith.

Shannon Smith-owned source and documentation are licensed under the GNU General Public License version 3 or, at your option, any later version (`GPL-3.0-or-later`). See `LICENSE`.

The retained SysMonTask icon and bundled PCI-name data remain under compatible BSD 3-Clause terms; their authorship and complete notices are preserved in `support/legal/THIRD_PARTY_NOTICES` and the corresponding file-specific licence records.
