<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Hardware collection

System Monitor reports hardware values only when the active native interface establishes both the value and its meaning. Unknown units, stale reads and unsupported capabilities are unavailable rather than guessed.

## Evidence hierarchy

A collector should prefer evidence in this order:

1. a documented kernel or driver ABI whose field and unit semantics are explicit;
2. a documented native operating-system interface that exposes equivalent semantics;
3. a conservative project fallback whose interpretation is independently verifiable;
4. unavailable, when the source cannot establish both identity and meaning.

A convenient file name, vendor convention or plausible numeric range is not enough to promote a field into telemetry. Vendor-specific support belongs behind capability checks and should not weaken a generic device path.

## Availability and sampling

Optional metrics keep availability separate from numeric storage, so zero remains a valid reading. Collectors clear per-sample availability before refresh. Failed reads do not preserve stale values.

Discovery may run more slowly than telemetry. Retained contexts keep resolved paths, driver handles and cumulative baselines between samples, while topology reconciliation determines whether that retained state still belongs to the same device.

## Counter discontinuities

Rates are derived only from cumulative counters that belong to the same stable identity and are separated by a valid positive monotonic interval. Any of the following breaks the baseline: counter rollback, device replacement, failed source read, invalid elapsed time, or a backend-specific reset indication.

A discontinuity publishes no synthetic rate for that interval. The current cumulative value becomes the next baseline. This rule prevents wrap/reset/re-enumeration events from appearing as impossible throughput or utilisation spikes.

Saturating arithmetic is used where an aggregate can legitimately exceed an intermediate integer range. Overflow is not allowed to wrap into a smaller plausible-looking measurement.

## CPU and memory

CPU and memory data comes from Linux procfs/sysfs, CPUID where applicable, `sysinfo` and retained accounting state. Linux scheduler counters, frequency-source paths and baselines remain private to the backend.

Missing frequency or temperature data must not invalidate independently collected CPU utilisation or memory values. Memory quantities derived from Linux `kB` interfaces use exactly 1024 bytes per KB.

## Storage and filesystems

Disk inventory, activity and filesystem state use native Linux metadata. Retained accounting state is separate from public snapshots and is reconciled against current device identity.

Linux diskstats sector accounting follows the documented 512-byte accounting unit rather than assuming the device's physical or logical block size. Incomplete devices are not assigned invented filesystem or hardware metadata.

## Network and wireless

Network counters are converted to rates using monotonic elapsed time; rollback or invalid timing is rejected. Link utilisation is exposed only when a meaningful negotiated link rate is also known.

Wireless metadata uses native Linux interfaces when supported by the driver. Short-lived caches are invalidated after failed refreshes rather than kept indefinitely.

## Bluetooth

BlueZ supplies controller/device identity and connection state over D-Bus. Connected remote devices can appear as individual Performance entries keyed by controller and Bluetooth address. The application does not require the BlueZ development library for HCI traffic capture: kernel-facing HCI socket and connection-list declarations are owned narrowly in the Linux backend and checked against the ABI System Monitor consumes.

Exact per-device throughput uses Linux's read-only HCI monitor channel. Packet controller, direction and connection handle are combined with a read-only connection snapshot to attribute payload bytes to the matching remote device. HCI connection handles are controller-local and reusable, so a handle associated with a different remote address resets its retained counters.

Binding the monitor channel requires `CAP_NET_RAW`. The packaged executable carries only that file capability, opens the monitor channel during startup and then clears its process capability sets before normal GTK or monitoring workers start. Installation should apply that capability through the executable's own narrowly scoped Linux `security.capability` xattr path rather than depending on the external `setcap` utility. No HCI commands, resets or controller reconfiguration are issued through this path.

Without the capability or monitor interface, Bluetooth identity remains available and traffic is shown as unavailable. Failure to drop capabilities aborts startup.

## GPUs

GPU discovery uses native DRM/device identity. Generic sysfs attributes are used only when their semantics are explicit.

Intel GPUs use the project-owned native backend where supported. AMD metrics come from explicit driver attributes. NVIDIA NVML may be loaded in-process when present and remains optional.

Temperature, clocks, power, fan and engine metrics are independently capability-detected so one failed sensor does not poison unrelated data.

## NPUs and accelerators

NPU discovery uses `/sys/class/accel` and resolved device identity. Intel IVPU support consumes explicit busy-time, resident-memory and frequency attributes with known units.

Unknown accelerator drivers expose only attributes whose names and interface establish meaning safely; ambiguous vendor fields remain unsupported.

## Batteries and peripherals

System and peripheral batteries use Linux power-supply interfaces, with direct device-specific enrichment where available. Logitech HID++ may provide authoritative peripheral battery values. Bluetooth battery data may use in-process GLib/D-Bus sources.

Timed peripheral workers use monotonic deadlines so wall-clock corrections cannot distort refresh or retry cadence.

## Units

Storage and memory use traditional binary-sized labels:

- 1 KB = 1024 bytes
- 1 MB = 1024 KB
- 1 GB = 1024 MB
- 1 TB = 1024 GB

Network rates and negotiated link speeds use decimal 1000-based scaling. Driver-specific raw units are converted only when the relevant ABI defines them.

## Adding hardware support

New support should establish a stable identity, a native interface with known semantics, explicit availability, safe retained state and deterministic tests. Evidence for units and field meaning should be traceable to an ABI, interface specification or reproducible fixture.

Keep expensive discovery away from high-frequency sampling, reset cumulative baselines after discontinuity, and do not add shell-command providers or vendor guesses merely to fill a field.

## PCI identity provenance

System Monitor does not distribute an external PCI database file or reproduce its hierarchy. The project registry retains only factual top-level vendor assignments and direct vendor:device mappings, normalizes them into a flat System Monitor schema, drops subsystem/class/comment/version records, and generates sorted numeric lookup tables with a deduplicated name pool.

Normal runtime lookup is a binary search over those generated tables; it does not parse a pci.ids-style document and does not require lspci, pciutils or another runtime database. A missing friendly name remains non-fatal: native firmware/sysfs identity is preferred when available and exact numeric vendor:device identity remains available as the fallback.
