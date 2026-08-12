<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Linux System Monitor

Linux System Monitor is a native C/GTK desktop system manager for Linux. It is
built as one GUI process and collects its own monitoring data wherever practical
instead of orchestrating command-line tools. The interface follows the useful
parts of Windows Task Manager while retaining Linux-specific hardware detail when
that detail is genuinely available.

Author and maintainer: Shannon Smith. This native C reimplementation was
inspired by SysMonTask by Neeraj Kumar and contributors. Its retained upstream
icon and the bundled PCI-name data are identified separately in
`THIRD_PARTY_NOTICES`; they are not presented as Shannon-owned work.

Project repository: <https://github.com/The-First-Infiltrator/System-Monitor>

## Product contract: GUI only

The installed application contains one project executable:
`linux-system-monitor`. It does not install project helper programs, shell
launchers, privileged collectors, configuration commands or command-line modes.
Unsupported or permission-restricted metrics become unavailable rather than
causing automatic elevation.

Run new task is an explicit GUI user action. It launches the accepted argument
vector directly and does not invoke a shell.

## User-facing capabilities

The default tab is Performance. The application also provides Processes,
Application history, Startup, Users, Details, Services and File systems.

Performance can show CPU and logical-processor history, memory, disks and
partitions, network interfaces, GPU engines and memory, batteries and supported
NPUs. Availability depends on the hardware, driver and permissions. A basic
hardware identity can remain visible even when a driver exposes no useful live
counter.

Processes and Details share one retained process snapshot. The UI supports
sorting, grouping, inspection, termination, suspend/resume, priority, efficiency
mode and CPU affinity through a platform-neutral process contract. Linux maps
those operations to its native process mechanisms below that contract.

## Unit convention

Storage and memory use binary scaling with traditional labels:

- 1 KB = 1024 bytes
- 1 MB = 1024 KB
- 1 GB = 1024 MB
- 1 TB = 1024 GB

Network rates are converted from the raw bit rate and then displayed with the
same 1024-based scaling. The formatter retains useful decimal precision and does
not promote to a unit that would begin with `0.`.

## Engineering architecture

The program is intentionally layered so operating-system details can be replaced
without rewriting the application model.

`shared/infiltratr-common` is the versioned, GPL-3.0-or-later C11 foundation also
used by Calendar Plus. It owns project identity, bounded strings, safe numeric
operations, binary quantity formatting and optional POSIX file/path/clock
adapters. Linux System Monitor keeps its established `lsm_` surface as a thin
compatibility facade and links the vendored library statically, preserving a
self-contained package with no runtime dependency on another Shannon Smith
application.

```text
GTK presentation / application policy
                |
        plain-C snapshots
                |
     platform-neutral contracts
                |
  ---------------------------------
                |
       current Linux providers
                |
 kernel / firmware / device interfaces
```

`monitor.c` owns the platform-neutral Performance lifecycle. The selected Linux
implementation is `monitor_backend_linux.c`; it coordinates the Linux collectors
through `monitor_linux_internal.h`. The public `LsmMonitor` snapshot contains
current values, availability flags, capabilities and opaque identities, not
retained Linux sampling baselines.

`process_model.h` and `process_backend.h` form the equivalent process boundary.
The common model uses neutral process, account and instance identities, neutral
priority levels, CPU time in nanoseconds, handle counts and abstract control
actions. `process_backend_linux.c` translates Linux PIDs, procfs start times,
UID/NSS identities, nice values, signals and scheduler affinity below that
boundary.

The preferred collection design is our own C parser or accounting logic over the
lowest practical trustworthy data source. External commands are not telemetry
providers. OS-specific paths, ioctls, procfs/sysfs attributes, D-Bus services and
vendor APIs are permitted only inside the provider that needs them. Their native
representations must not become application-wide assumptions.

Hardware and process identity above a backend is opaque. Presentation code may
compare an identity for continuity but must not parse it or construct Linux
paths from it.

The GTK application state is composed from named subsystem states rather than a
flat cross-module field list. Shell/runtime state, Performance state, the shared
process workspace, Processes and Details page state, History, File Systems,
Startup, Services and Users each have an explicit ownership block. Performance
construction is further divided between controller/lifecycle code,
CPU-memory-disk-network page builders and accelerator/peripheral page builders;
process mutation/filtering/recording is isolated from Details table rendering.

## Performance model

Fast cumulative counters and slower hardware/detail discovery have separate
cadences. A rate is calculated only when the current sample has the same stable
identity as its private prior sample and the monotonic interval is positive.
Hot-plug reconciliation preserves history only across a valid identity match.

The implementation avoids repeated directory enumeration on the fast path,
caches immutable values and discovered attribute paths, bounds caller-owned
arrays, suppresses unchanged GTK text and avoids rebuilding hidden tables until
needed. Process prior-sample lookup is sorted/binary-searched rather than a
nested full-table scan.

A failed field read clears that field's availability. It must not silently turn
an unknown metric into zero or invalidate unrelated fields.

## Data semantics and failure handling

All optional metrics have explicit availability. Vendor-specific failures are
isolated: for example, failure of an optional NVIDIA or Intel telemetry path must
not prevent generic GPU identity or unrelated hardware from being displayed.

The application does not treat a Linux device path as hardware identity at the
presentation layer. If a backend exposes a path for diagnostic display, it is a
display-ready string supplied by that backend, not something constructed by the
GUI.

## Security and privilege model

The application runs with the effective credentials of its one process. It does
not install a privileged companion, grant capabilities, modify system policy or
invoke an elevation helper. Running the same GUI as an administrator is an
explicit external user choice.

Process-control requests validate the target and call the selected backend.
Read-only collector failures are reported as unavailable information or GUI
errors. Files chosen for export are replaced atomically.

## Portability contract

Portability means keeping hardware concepts and application policy independent
of the current OS representation. It does not mean replacing native code with a
third-party abstraction library.

The reusable core should own formatting, accounting, histories, parsers, data
models and UI policy wherever practical. A platform provider supplies the small
amount of data that inherently depends on the operating system. Linux may use
procfs, sysfs, rtnetlink, ioctls, DRM, accelerator attributes or selected D-Bus
interfaces below the boundary; another operating system can satisfy the same
contract differently.

The complete `shared/infiltratr-common` directory is synchronised as one
component between participating projects. Its own `VERSION` changes whenever
the common API or behaviour changes; its standalone Makefile can produce both
the vendored static archive and a conventional shared object.

Infiltratr Common 1.1 also owns strict scalar parsing, null-safe string
predicates and range clamping, so platform collectors and Calendar Plus no
longer maintain separate versions of those low-level rules.

Runtime dependencies are kept small. The application does not depend on
`lspci`, `lshw`, `lsblk`, `sensors`, `dmidecode`, `nvidia-smi`, `systemctl`,
`loginctl`, `nmcli`, libudev, libmount or libsensors for monitoring. PCI names are
compiled into the program from a separately licensed data set. Optional NVIDIA
NVML and desktop D-Bus providers are isolated capabilities rather than required
application architecture.

## Building from source

A normal development build requires a C17 compiler, Make, pkg-config and GTK 3.22
or newer development files:

```bash
make
./build/linux-system-monitor
```

The generic Debian-family package is produced with:

```bash
make deb
```

The hardware-native self-extracting installer is produced with:

```bash
make native-installer
```

The standard source archive is produced with `make dist`. A complete release set
can be built with `make release`; it creates the generic `.deb`, hardware-native
`.run` and `Linux-System-Monitor-VERSION-source.zip`. The Debian builder
normalises package metadata timestamps and `make deb` verifies a second package
is byte-for-byte identical before accepting the result. Before assembly, the
finished executable is also inspected directly and rejected if any imported
GLIBC symbol version exceeds the package's declared glibc 2.34 baseline.

`install.sh` is only the source/bootstrap entry point used by the native build
workflow. Direct `make install` is intentionally disabled so release installation
remains package-owned.

The native installer is permitted to install build/runtime prerequisites through
the host package manager before compilation. The installed end-user application
remains GUI-only.

## Coding and documentation standard

Application code is C17. Public `lsm_` APIs require Doxygen contracts including
parameter and return semantics. Every C/header file has a concise file-level
purpose comment. Internal comments should explain invariants, ownership,
non-obvious calculations, compatibility decisions and failure semantics; they
should not narrate obvious statements line by line.

The maintained written documentation is deliberately small:

- `README.md` is the product, architecture, build and maintenance reference.
- `CHANGELOG.md` records current releases and consolidated historical milestones.
- `LICENSE` contains GPL version 3, and every maintained code or documentation
  file carries the appropriate `GPL-3.0-or-later` SPDX identifier.
- `THIRD_PARTY_NOTICES`, the file-specific `.license` sidecars and
  `data/PCI_IDS_LICENSE` preserve the required upstream notices.
- `packaging/copyright` is the canonical Debian copyright declaration installed
  with the binary package.
- Doxygen can generate API reference directly from this README and source comments.

Do not create release-notes, architecture, build, testing or design Markdown files
that duplicate these sources of truth.

## Verification and release gates

`make check` combines source/documentation/package checks with the engineering
build checks. The suite includes strict compiler warnings, static analysis where
supported, native-command/dependency audits, parser and policy smoke tests,
backend-free monitor and process-contract tests, hardware fixtures, process
controls, package-content validation and repeated lifecycle checks for descriptor,
thread and resident-memory growth.

The deterministic line-coverage gate covers eleven accounting, parser, selection
and grouping modules, with a per-module minimum rather than a misleading single
aggregate percentage. GUI behaviour remains protected primarily by dedicated
smoke/regression tests and the repeated lifecycle tests.

The release package must contain one project executable and normal desktop
integration/resources only. Missing optional hardware capabilities may reduce
reported detail but must not prevent startup.

## Licence

Copyright © 2026 Shannon Smith. Linux System Monitor and the vendored
Infiltratr Common source are distributed under the GNU General Public License,
version 3 or (at your option) any later version: `GPL-3.0-or-later`. See
`LICENSE` for the complete GPL version 3 text.

The retained SysMonTask icon and bundled PCI-name database remain under their
compatible BSD 3-Clause terms. Their authorship, source locations and complete
notices are recorded in `THIRD_PARTY_NOTICES` and the corresponding file-specific
licence files.
