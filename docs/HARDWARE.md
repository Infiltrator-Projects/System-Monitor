<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Hardware collection

This document records hardware-specific rules that are not already defined by [Architecture](ARCHITECTURE.md).

## Evidence

Use documented kernel/driver or native operating-system interfaces with defined semantics and units. Conservative fallbacks are acceptable only when independently verifiable; otherwise report the value unavailable. A convenient filename, vendor convention or plausible numeric range is not evidence.

## CPU and memory

CPU and memory data comes from Linux procfs/sysfs, CPUID where applicable, `sysinfo` and retained accounting state. Linux `kB` memory quantities use exactly 1024 bytes per KB.

For CPU thermal policy, Linux prefers the selected CPU/package hwmon channel's native `temp*_max`/Tcontrol warning point and `temp*_crit`/Tjmax critical point. CPU-labelled thermal-zone hot/critical trip points are the fallback native source. If Linux exposes no threshold, a manufacturer-documented SKU table supplies Tjmax and derives warning at 10 °C below it. Unknown processors use 80 °C warning / 95 °C fault. Overview and CPU Performance use the same resolved policy.

## Storage and filesystems

Linux diskstats sector accounting uses the documented 512-byte accounting unit, not the device's physical or logical block size. Incomplete devices are not assigned invented filesystem or hardware metadata.

## Network and wireless

Link utilisation is exposed only when a meaningful negotiated link rate is known. Failed wireless metadata refreshes invalidate short-lived cached values rather than preserving them indefinitely.

## Bluetooth

BlueZ supplies controller/device identity and connection state over D-Bus. Connected remote devices may appear as individual Performance entries keyed by controller and Bluetooth address.

Per-device throughput uses Linux's read-only HCI monitor channel. Controller, direction and connection handle are combined with a read-only connection snapshot to attribute payload bytes to the matching device. HCI handles are controller-local and reusable, so observing a handle with a different remote address resets its retained counters.

The application owns only the narrow kernel-facing HCI declarations it consumes and does not require the BlueZ development library for traffic capture.

The packaged executable carries only the `CAP_NET_RAW` file capability required to open the monitor channel. It opens that endpoint during startup and clears process capability sets before normal GTK or monitoring work begins. Failure to drop capabilities aborts startup. The monitor path issues no HCI commands, resets or controller reconfiguration. Without the capability or monitor interface, identity remains available and traffic remains unavailable.

## GPUs

GPU discovery uses native DRM/device identity. Generic sysfs attributes are used only when their semantics are explicit.

Intel GPUs use the project-owned native backend where supported. AMD metrics use explicit driver attributes. NVIDIA NVML may be loaded in-process when present and remains optional. Temperature, clocks, power, fan and engine metrics are capability-detected independently.

## NPUs and accelerators

NPU discovery uses `/sys/class/accel` and resolved device identity. Intel IVPU support consumes explicit busy-time, resident-memory and frequency attributes with known units. Unknown accelerator drivers expose only attributes whose interface establishes meaning safely.

## Batteries and peripherals

System and peripheral batteries use Linux power-supply interfaces with device-specific enrichment where available. Logitech HID++ may provide authoritative peripheral battery values; Bluetooth battery data may use in-process GLib/D-Bus sources. Timed peripheral workers use monotonic deadlines.

## Units

Storage and memory labels use binary scaling: 1 KB = 1024 bytes, 1 MB = 1024 KB, 1 GB = 1024 MB and 1 TB = 1024 GB. Network rates and negotiated link speeds use decimal 1000-based scaling. Driver-specific raw units are converted only when the relevant ABI defines them.

## PCI identity provenance

System Monitor does not distribute an external PCI database file or reproduce its hierarchy. The project registry retains factual top-level vendor assignments and direct vendor:device mappings, normalizes them into a flat System Monitor schema, drops subsystem/class/comment/version records, and generates sorted numeric lookup tables with a deduplicated name pool.

Runtime lookup is a binary search over those generated tables; it does not parse a pci.ids-style document or require lspci, pciutils or another runtime database. A missing friendly name is non-fatal: native firmware/sysfs identity is preferred where available and exact numeric vendor:device identity remains the fallback.

## Adding hardware support

New support must establish stable identity, a native interface with known semantics, explicit availability, safe retained state and deterministic tests. Keep expensive discovery away from high-frequency sampling and do not add shell-command providers or vendor guesses merely to fill a field.
