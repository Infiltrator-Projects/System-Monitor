<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Linux System Monitor

[![Verify](https://github.com/The-First-Infiltrator/System-Monitor/actions/workflows/ci.yml/badge.svg)](https://github.com/The-First-Infiltrator/System-Monitor/actions/workflows/ci.yml)

Linux System Monitor is a native C17/GTK 3 desktop system manager for Linux. It presents the useful parts of Windows Task Manager while collecting Linux hardware, process and service information directly wherever practical.

**Current source version:** 1.0.0  
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

`src/infiltratr-common` is pinned to Infiltratr Common 1.12.0 and linked statically. Common owns reusable formatting, parsing, timing, durable I/O, ordered path selection and dynamic-library mechanics; hardware-specific collection policy stays in System Monitor.

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

The publisher checks out the exact tested commit, verifies it is still current `main`, reruns CMake/CTest and Make verification, builds the `.deb`, `.run` and standalone source ZIP, then creates the version tag and GitHub release. Published version tags and releases are treated as immutable release identities.

Manually runnable build/test helpers, where present, are diagnostic tools only and are not release-approval mechanisms.

## Source layout

- `src/` — application code, Linux providers and the pinned Common submodule.
- `support/` — tests, release tooling, resources, packaging, version metadata, installer internals and legal notices.

## Release identity

Linux System Monitor is presented as version **1.0.0**. The `v1.0.0` tag identifies the verified release commit, and the GitHub release contains the three supported distribution artifacts listed above.

## Licence

Copyright © 2026 Shannon Smith.

Shannon Smith-owned source and documentation are licensed under the GNU General Public License version 3 or, at your option, any later version (`GPL-3.0-or-later`). See `LICENSE`.

The retained SysMonTask icon and bundled PCI-name data remain under compatible BSD 3-Clause terms; their authorship and complete notices are preserved in `support/legal/THIRD_PARTY_NOTICES` and the corresponding file-specific licence records.
