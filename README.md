<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Linux System Monitor

Linux System Monitor is a native C17/GTK 3 desktop system manager for Linux.
It presents the useful parts of Windows Task Manager while collecting Linux
hardware and process information directly wherever practical.

Author and maintainer: Shannon Smith

Repository: <https://github.com/The-First-Infiltrator/System-Monitor>

Downloads: <https://github.com/The-First-Infiltrator/System-Monitor/releases/latest>

## Download

Each GitHub release provides:

- a generic AMD64 Debian package (`.deb`);
- a hardware-native self-extracting installer (`.run`); and
- GitHub's automatic source archives for the tagged source tree.

Install the Debian package through the desktop package installer or with:

```bash
sudo apt install ./linux-system-monitor_VERSION_amd64.deb
```

The native installer compiles for the current machine before installing:

```bash
chmod +x linux-system-monitor-VERSION-native-installer.run
./linux-system-monitor-VERSION-native-installer.run
```

## Capabilities

- Performance pages for CPU, memory, disks, partitions, networks, GPUs,
  batteries and supported NPUs.
- Processes, Application history, Startup, Users, Details, Services and File
  systems pages.
- Process inspection, termination, suspend/resume, priority, efficiency mode
  and CPU affinity controls.
- Hardware identity and telemetry from native Linux interfaces, with optional
  metrics shown as unavailable when the hardware, driver or permissions do not
  expose them.
- Snapshot and process-table export through explicit GUI actions.

## Design

The installed product is one GUI executable, `linux-system-monitor`. It does
not install project-owned command-line tools, helper daemons, shell launchers,
privileged collectors or policy changes. Running it with elevated rights is an
explicit external user choice.

Monitoring prefers project-owned C parsers over command orchestration. It does
not require `lspci`, `lshw`, `lsblk`, `sensors`, `dmidecode`, `nvidia-smi`,
`systemctl`, `loginctl` or `nmcli` as telemetry providers. Optional vendor APIs
and desktop services remain isolated capabilities.

Plain-C snapshots and platform-neutral monitor/process contracts separate the
GTK interface from Linux providers. Linux-specific paths, procfs/sysfs data,
ioctls and D-Bus details stay below those contracts so another operating-system
backend can be added without rewriting the application model.

`src/infiltratr-common` is a Git submodule pinned to Infiltratr Common 1.2.0 in
the [Infiltrator Libraries](https://github.com/The-First-Infiltrator/Infiltrator-Libraries)
repository. It is linked statically into this application, so the source has
one canonical owner while the installed package has no cross-project runtime
dependency.

Storage, memory and network quantities use 1024-based scaling with traditional
labels: 1 KB = 1024 bytes, 1 MB = 1024 KB, 1 GB = 1024 MB and 1 TB = 1024 GB.

## Build from source

Required development tools are a C17 compiler, Make, Git, pkg-config and GTK 3.22 or
newer development files.

On Debian, Ubuntu or Linux Mint:

```bash
sudo apt install build-essential git pkg-config libgtk-3-dev
git clone --recurse-submodules https://github.com/The-First-Infiltrator/System-Monitor.git
cd System-Monitor
make
./build/linux-system-monitor
```

GitHub's automatic source archives preserve the pinned dependency reference
but do not expand submodules. After extraction, an ordinary `make` automatically
retrieves the exact pinned Infiltratr Common release into `src/infiltratr-common`;
no separate shared-library setup is required.

Useful build targets:

```bash
make check             # complete engineering verification
make deb               # generic Debian-family package
make native-installer  # hardware-native .run installer
make dist              # deterministic standalone source ZIP
make release           # build all local release artifacts
```

Direct `make install` is disabled. Installation remains owned by the Debian
package or native installer.

## Source layout

- `src/` — application code, Linux providers and the pinned Infiltratr Common
  submodule.
- `support/` — tests, release tools, resources, packaging and developer
  configuration kept away from the repository landing page.

## Verification

`make check` enforces strict warnings, source and licence policy, static analysis
where supported, parser and backend tests, hardware fixtures, process controls,
coverage floors, lifecycle stability, package boundaries and installer safety.
The Debian builder also checks the executable's declared glibc 2.34 ceiling and
requires a byte-reproducible package.

This README is the sole maintained project manual. Release history belongs on
GitHub Releases, and API detail belongs in the source's Doxygen contracts. The
licence and attribution files remain separate because they are legal records,
not additional project manuals.

## Licence

Copyright © 2026 Shannon Smith.

Shannon Smith-owned source and documentation are licensed under the GNU General
Public License version 3 or, at your option, any later version
(`GPL-3.0-or-later`). See `LICENSE`.

The retained SysMonTask icon and bundled PCI-name data remain under compatible
BSD 3-Clause terms. Their authorship and complete notices are preserved in
`support/legal/THIRD_PARTY_NOTICES` and the corresponding file-specific licence
records.
