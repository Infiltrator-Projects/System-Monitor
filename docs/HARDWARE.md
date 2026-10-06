<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Hardware collection

Hardware-specific collection follows the shared architecture rules for explicit availability, stable identity, monotonic timing and complete-or-preserved inventories. This document records only device and interface details that are not obvious from those general contracts.

## Evidence

Prefer, in order:

1. documented kernel or driver ABIs with explicit field and unit semantics;
2. documented native operating-system interfaces with equivalent semantics;
3. conservative project fallbacks whose interpretation is independently verifiable;
4. unavailable when identity or meaning cannot be established safely.

A convenient filename, vendor convention or plausible numeric range is not sufficient evidence.

## CPU and memory

CPU and memory data comes from Linux procfs/sysfs, CPUID where applicable, `sysinfo` and retained accounting state. Missing temperature or frequency data does not invalidate independent utilisation or memory values. Linux `kB` memory quantities are converted using exactly 1024 bytes per KB.

CPU thermal policy is processor-aware. Linux prefers the selected CPU/package hwmon channel's native `temp*_max`/Tcontrol warning point and `temp*_crit`/Tjmax critical point. Explicitly CPU-labelled thermal-zone hot/critical trip points are the fallback native source. If Linux exposes no threshold, a manufacturer-documented SKU table supplies Tjmax and derives warning at 10 °C below it. Unknown processors use the conservative 80 °C warning / 95 °C fault fallback. Overview and the CPU Performance page consume the same resolved policy.

## Storage and filesystems

Disk inventory, activity and filesystem state use native Linux metadata. Linux diskstats sector accounting uses the documented 512-byte accounting unit rather than the device's physical or logical block size. Incomplete devices are not assigned invented filesystem or hardware metadata.

## Network and wireless

Link utilisation is exposed only when a meaningful negotiated link rate is known. Wireless metadata uses native Linux interfaces supported by the driver; failed refreshes invalidate short-lived cached metadata rather than preserving it indefinitely.

## Bluetooth

BlueZ supplies controller/device identity and connection state over D-Bus. Connected remote devices may appear as individual Performance entries keyed by controller and Bluetooth address.

Exact per-device throughput uses Linux's read-only HCI monitor channel. Packet controller, direction and connection handle are combined with a read-only connection snapshot to attribute payload bytes to the matching remote device. HCI connection handles are controller-local and reusable, so a handle observed with a different remote address resets its retained counters.

The application owns only the narrow kernel-facing HCI declarations it consumes and validates them against that ABI; it does not require the BlueZ development library for traffic capture.

Binding the monitor channel requires `CAP_NET_RAW`. The packaged executable carries only that file capability, opens the monitor channel during startup and clears its process capability sets before normal GTK or monitoring workers begin. Failure to drop capabilities aborts startup. The monitor path issues no HCI commands, resets or controller reconfiguration.

Without the capability or monitor interface, Bluetooth identity remains available and traffic remains unavailable.

## GPUs

GPU discovery uses native DRM/device identity. Generic sysfs attributes are used only when their semantics are explicit.

Intel GPUs use the project-owned native backend where supported. AMD metrics use explicit driver attributes. NVIDIA NVML may be loaded in-process when present and remains optional.

Temperature, clocks, power, fan and engine metrics are independently capability-detected so one failed sensor does not invalidate unrelated data.

## NPUs and accelerators

NPU discovery uses `/sys/class/accel` and resolved device identity. Intel IVPU support consumes explicit busy-time, resident-memory and frequency attributes with known units. Unknown accelerator drivers expose only attributes whose interface establishes meaning safely.

## Batteries and peripherals

System and peripheral batteries use Linux power-supply interfaces, with device-specific enrichment where available. Logitech HID++ may provide authoritative peripheral battery values; Bluetooth battery data may use in-process GLib/D-Bus sources.

Timed peripheral workers use monotonic deadlines so wall-clock corrections cannot distort refresh or retry cadence.

## Units

Storage and memory use binary-sized labels: 1 KB = 1024 bytes, 1 MB = 1024 KB, 1 GB = 1024 MB and 1 TB = 1024 GB. Network rates and negotiated link speeds use decimal 1000-based scaling. Driver-specific raw units are converted only when the relevant ABI defines them.

## PCI identity provenance

System Monitor does not distribute an external PCI database file or reproduce its hierarchy. The project registry retains factual top-level vendor assignments and direct vendor:device mappings, normalizes them into a flat System Monitor schema, drops subsystem/class/comment/version records, and generates sorted numeric lookup tables with a deduplicated name pool.

Runtime lookup is a binary search over those generated tables; it does not parse a pci.ids-style document and does not require lspci, pciutils or another runtime database. A missing friendly name is non-fatal: native firmware/sysfs identity is preferred where available and exact numeric vendor:device identity remains the fallback.

## Adding hardware support

New support must establish stable identity, a native interface with known semantics, explicit availability, safe retained state and deterministic tests. Keep expensive discovery away from high-frequency sampling and do not add shell-command providers or vendor guesses merely to fill a field.
