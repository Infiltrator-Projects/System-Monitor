<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Portability

System Monitor uses C and C++ as equal, first-class project languages. The current installed application targets ISO C17 and current C++ developer tools target ISO C++17, but that division is an implementation state rather than a language hierarchy. Portability means keeping application contracts independent of Linux implementation details, not pretending the current Linux backend already runs unchanged everywhere.

## Language and interfaces

Choose C or C++ according to which gives the stronger implementation for the component: correctness, clarity, performance, maintainability and control are more important than language preference. Neither C nor C++ is subordinate to the other.

Treat C, procedural C++ and object-oriented C++ as different expressive tools for the same systems-programming domain. Some problems are clearest as plain data and functions; others benefit from C++ value types, RAII, stronger type relationships, templates or scoped ownership. Object orientation is appropriate when the problem genuinely contains encapsulated state or interchangeable runtime behaviour. It is not a reason to manufacture inheritance hierarchies around kernel, driver or hardware interfaces that are naturally procedural.

Project-owned code should prefer C or C++ over introducing another language ecosystem. A different language or runtime requires a concrete technical advantage that C/C++ cannot reasonably provide; novelty or convenience alone is not sufficient. Within C and C++, newer features and OO techniques are adopted for demonstrated benefit rather than because the language makes them available.

Application-facing snapshots and contracts currently remain plain C because that is the established interface contract, not because C is preferred over C++. They must not expose Linux handles, GTK objects, implementation-owned paths or hidden global ownership.

## Platform boundary

Linux-specific paths, ioctls, signals, scheduler/affinity operations, D-Bus calls and driver ABI knowledge belong below platform seams. A future Windows, BSD, Solaris or other backend should implement the same application-facing contracts using that platform's native interfaces.

The first Windows implementation now exists at `src/monitor_backend_windows.c` and `src/process_backend_windows.c`. The deliberately narrow monitor slice covers aggregate CPU utilisation, logical processor count, uptime, system process/thread/handle totals and physical/commit memory through Win32/PSAPI. The process slice is read-only: Tool Help provides PID/name/parent/thread inventory, access-token SIDs provide stable account identity and current-user ownership, and permission-dependent Win32 queries provide creation identity, CPU time, working set, I/O, priority, executable path and handle count with retained PID-reuse-safe rate baselines. Command lines, GPU/cgroup fields and every process-control operation remain unsupported. This does not yet make System Monitor a supported Windows application: disks, networks, pressure, accelerators, batteries, service/user/startup backends and Windows presentation/build integration remain future native work. Unsupported fields stay zero/unavailable rather than being inferred from Linux semantics.

GTK 3 is currently the Linux presentation toolkit; reusable accounting, parsing and model layers must not depend on GTK.

Portability is judged at the contract boundary, not by forcing implementations toward a lowest common denominator. Linux implementation files should use Linux-native facilities when they provide the strongest semantics, even when that requires more platform-specific code; the requirement is that those facilities do not leak into a platform-neutral API. A weaker implementation must not be chosen merely because it is easier to share across platforms.

## Representation assumptions

Use explicit-width integers where an external ABI or counter width matters. Binary structures must be decoded without assuming host alignment, and multi-byte protocol fields must apply the byte order defined by their interface.

Do not cast potentially unaligned netlink, device or packet payloads directly to wider integer pointers. Copy or decode them into naturally aligned objects first. Size arithmetic, allocation growth and cumulative aggregation must reject or saturate overflow according to the owning contract.

Time used for rates, refresh cadence and retry deadlines is elapsed time and therefore uses monotonic clocks. Civil time remains appropriate for user-visible timestamps and persisted dates.

## Dependency budget

The supported build should not require third-party development packages whose only purpose is to provide declarations for stable Linux kernel ABIs that System Monitor can define narrowly and test itself. Likewise, installation should not depend on command-line helpers when the same privileged operation can be performed directly and safely through a documented kernel interface.

This rule is not a licence to copy arbitrary library internals. Project-owned ABI declarations must contain only the minimum structures, constants and ioctl encodings required by the kernel contract, with compile-time size/offset checks and runtime fixtures where practical. When a dependency supplies substantial semantics rather than declarations, removal requires an independently complete replacement.

The current deliberate platform boundary is GTK 3 plus the GLib/GIO stack it requires. Toolkit-neutral filesystem, identifier, stable-signature, monotonic-counter and POSIX deadline mechanics must not use that boundary merely for convenience: Common 1.19.24 supplies HOME/XDG resolution, recursive directory creation, complete text reads, deterministic ASCII matching/ordering, stable non-cryptographic FNV-1a mixing, counter delta/rate handling and normalized absolute-deadline conversion for those paths. Removing GTK/GLib/GIO themselves would mean implementing a desktop toolkit, text/input/accessibility integration and D-Bus/runtime services rather than ordinary dependency cleanup, so that remains a separate architectural phase.

## Native collection

Normal telemetry should use direct APIs rather than external commands such as `lspci`, `lsblk`, `sensors`, `nvidia-smi`, `systemctl` or `nmcli`. Optional vendor libraries may be loaded in-process when appropriate and must remain optional.

Capability detection is preferred to platform guessing. A backend should establish that an interface is present and semantically usable rather than infer support from a distribution name, desktop environment or vendor string alone.

## Data, paths and lifetime

Counter arithmetic must handle overflow, rollback and invalid timing without producing false spikes.

Storage and memory presentation uses 1024-based KB/MB/GB/TB labels. Network rates and negotiated link speeds use decimal 1000-based scaling.

Platform-neutral code must not hard-code `/proc`, `/sys` or `/dev`. Path construction, allocation growth and similar generic mechanics belong in pinned Common APIs when they are fundamentally reusable. If a local generic mechanism is stronger than Common, improve Common to retain those advantages rather than accepting weaker reuse or preserving permanent duplication.

Background workers expose platform-neutral results. Threading primitives, cancellation state, descriptors and synchronization objects remain owned by the implementing subsystem and are released deterministically. Detached lifetime is permitted only when ownership is explicitly transferred or reference-counted.

Windows-native translation units are cross-compiled independently with MinGW in CI. The Linux i386 portability gate intentionally checks only Linux translation units, because compiling Win32 sources as Linux ELF objects would test the wrong platform contract.

## Review test

A portable change should preserve these properties:

- platform-neutral code does not gain Linux-native types or paths;
- external binary data is decoded with explicit width, alignment and byte-order assumptions;
- no new language/runtime dependency is introduced without a concrete benefit that C/C++ cannot reasonably provide;
- portability does not force a weaker or lowest-common-denominator implementation;
- ownership, units, timing and failure behaviour remain explicit;
- unsupported capabilities degrade to unavailable rather than guessed data;
- another native backend could implement the same public contract without reproducing Linux internals.
