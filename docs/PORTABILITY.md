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

The Windows implementation uses `src/main_windows.c` as a native Win32/GDI renderer and `src/monitor_backend_windows.c` / `src/process_backend_windows.c` as native data adapters. Those files are not allowed to redefine System Monitor product semantics. The platform-neutral `presentation_contract.[ch]` owns top-level tabs, Performance resource identity, canonical geometry, colours, captions and CPU/Memory field placement; `performance_view.[ch]` owns the cross-tab summary plus canonical CPU/Memory and disk/network/GPU value projection and availability-to-text decisions. Linux GTK and Windows Win32 both consume those same modules.

That division makes “thin adapter” concrete: the native renderer translates shared labels, positions, formatted values and state into toolkit operations; it may not maintain a second list of product pages, a second field order, or Windows-specific units/formatting for the same metric. Native differences belong below platform contracts. Unsupported Windows telemetry remains present as `N/A` in the shared view model rather than causing Windows-specific layout collapse or guessed zero values.

Windows typography follows the same seam. Family names, weights, filenames and verified font-asset provenance come from Common. CI/release tooling validates Common's immutable hashes, embeds the three canonical faces as PE resources, and the Win32 process registers them privately with `AddFontMemResourceEx`; no global font installation or persistent Windows font-state mutation is performed. CPU topology follows the same rule: native Windows APIs populate shared `LsmCpuInfo` fields while `performance_view.c` remains solely responsible for presenting those values.

The current Windows monitor adapter supplies aggregate CPU utilisation, logical processor count, uptime, system process/thread/handle totals and physical/commit memory through Win32/PSAPI. Native storage IOCTLs populate the shared disk model with physical-device identity, size, throughput/activity/response counters and mapped volume usage; IP Helper populates the shared network model with interface identity, addresses, link state/speed and traffic counters/rates. Native display enumeration supplies graphics-adapter identity, PDH supplies supported GPU engine utilisation and DXGI establishes dedicated-memory capacity. DXGI `QueryVideoMemoryInfo.CurrentUsage` describes the calling process, so it is not used as adapter-wide consumption; used memory remains unavailable until an adapter-wide provider is implemented. Windows publishes the same completed-sample generation/timestamp contract as Linux and the native Overview consumes the shared bounded history while retaining Win32/GDI rendering. The process adapter is read-only: Tool Help provides PID/name/parent/thread inventory, access-token SIDs provide stable account identity and current-user ownership, and permission-dependent Win32 queries provide creation identity, CPU time, working set, I/O, priority, executable path and handle count with retained PID-reuse-safe rate baselines.

GTK 3 remains the Linux presentation toolkit. Windows uses native Win32/GDI/common-controls and therefore carries no GTK runtime dependency. Both consume Common 1.19.35: Linux through the normal Common target and Windows through the same pinned portable Common source set linked statically into the preview/release executable. The Windows adapter resolves the shared Day/Night roles and design metrics directly from Common into native drawing, uses the Windows application-theme setting for Follow system, persists only the platform-specific theme selection in HKCU, and uses the three verified MB Corpo faces embedded in the portable executable as process-private fonts. The Windows release does not silently substitute Segoe UI when those resources are expected. Reusable accounting, parsing, formatting, presentation-model and application-state layers must not depend on either presentation toolkit.

Portability is judged at the contract boundary, not by forcing implementations toward a lowest common denominator. Linux implementation files should use Linux-native facilities when they provide the strongest semantics, even when that requires more platform-specific code; the requirement is that those facilities do not leak into a platform-neutral API. A weaker implementation must not be chosen merely because it is easier to share across platforms.

## Representation assumptions

Use explicit-width integers where an external ABI or counter width matters. Binary structures must be decoded without assuming host alignment, and multi-byte protocol fields must apply the byte order defined by their interface.

Do not cast potentially unaligned netlink, device or packet payloads directly to wider integer pointers. Copy or decode them into naturally aligned objects first. Size arithmetic, allocation growth and cumulative aggregation must reject or saturate overflow according to the owning contract.

Time used for rates, refresh cadence and retry deadlines remains canonical monotonic SI time, and persisted timestamps/counters remain Unix/SI values. Human-facing civil timestamps and duration strings are presentation only: when the validated temporal-v3 provider from Infiltrator System Settings is available they use Common 1.19.35's clock-mode and duration contracts, including configured geographic context where the selected mode requires it; otherwise native OS presentation remains authoritative.

## Dependency budget

The supported build should not require third-party development packages whose only purpose is to provide declarations for stable Linux kernel ABIs that System Monitor can define narrowly and test itself. Likewise, installation should not depend on command-line helpers when the same privileged operation can be performed directly and safely through a documented kernel interface.

This rule is not a licence to copy arbitrary library internals. Project-owned ABI declarations must contain only the minimum structures, constants and ioctl encodings required by the kernel contract, with compile-time size/offset checks and runtime fixtures where practical. When a dependency supplies substantial semantics rather than declarations, removal requires an independently complete replacement.

The current deliberate platform boundary is GTK 3 plus the GLib/GIO stack it requires. Toolkit-neutral filesystem, identifier, stable-signature, monotonic-counter and POSIX deadline mechanics must not use that boundary merely for convenience: Common 1.19.35 supplies HOME/XDG resolution, recursive directory creation, complete text reads, deterministic ASCII matching/ordering, stable non-cryptographic FNV-1a mixing, counter delta/rate handling and normalized absolute-deadline conversion for those paths. Removing GTK/GLib/GIO themselves would mean implementing a desktop toolkit, text/input/accessibility integration and D-Bus/runtime services rather than ordinary dependency cleanup, so that remains a separate architectural phase.

## Native collection

Normal telemetry should use direct APIs rather than external commands such as `lspci`, `lsblk`, `sensors`, `nvidia-smi`, `systemctl` or `nmcli`. Optional vendor libraries may be loaded in-process when appropriate and must remain optional.

Capability detection is preferred to platform guessing. A backend should establish that an interface is present and semantically usable rather than infer support from a distribution name, desktop environment or vendor string alone.

## Data, paths and lifetime

Counter arithmetic must handle overflow, rollback and invalid timing without producing false spikes.

Storage and memory presentation uses 1024-based KB/MB/GB/TB labels. Network rates and negotiated link speeds use decimal 1000-based scaling.

Platform-neutral code must not hard-code `/proc`, `/sys` or `/dev`. Path construction, allocation growth and similar generic mechanics belong in pinned Common APIs when they are fundamentally reusable. If a local generic mechanism is stronger than Common, improve Common to retain those advantages rather than accepting weaker reuse or preserving permanent duplication.

Background workers expose platform-neutral results. Threading primitives, cancellation state, descriptors and synchronization objects remain owned by the implementing subsystem and are released deterministically. Detached lifetime is permitted only when ownership is explicitly transferred or reference-counted.

Windows-native translation units, including the GUI executable, are cross-compiled and linked independently with MinGW in CI. The Linux i386 portability gate intentionally checks only Linux translation units, because compiling Win32 sources as Linux ELF objects would test the wrong platform contract.

## Review test

A portable change should preserve these properties:

- platform-neutral code does not gain Linux-native types or paths;
- external binary data is decoded with explicit width, alignment and byte-order assumptions;
- no new language/runtime dependency is introduced without a concrete benefit that C/C++ cannot reasonably provide;
- portability does not force a weaker or lowest-common-denominator implementation;
- ownership, units, timing and failure behaviour remain explicit;
- unsupported capabilities degrade to unavailable rather than guessed data;
- another native backend could implement the same public contract without reproducing Linux internals.
