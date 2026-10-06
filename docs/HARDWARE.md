<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Hardware collection

This document records hardware-specific rules not already defined by [Architecture](ARCHITECTURE.md).

## CPU and memory

CPU and memory data comes from Linux procfs/sysfs, CPUID where applicable, `sysinfo` and retained accounting state. Linux `kB` memory quantities use 1024 bytes per KB.

CPU thermal policy prefers the selected CPU/package hwmon `temp*_max`/Tcontrol warning point and `temp*_crit`/Tjmax critical point. CPU-labelled thermal-zone hot/critical trips are the fallback native source. If neither exists, a manufacturer-documented SKU table may supply Tjmax and warning at 10 °C below it. Unknown processors use 80 °C warning / 95 °C fault. Overview and CPU Performance share the same resolved policy.

## Storage and filesystems

Linux diskstats sector accounting uses 512-byte sectors regardless of device block size. Missing filesystem or hardware metadata remains unavailable.

## Network and wireless

Link utilisation is shown only when a meaningful negotiated link rate is known. Failed wireless metadata refreshes invalidate short-lived cached values.

## Bluetooth

BlueZ supplies identity and connection state over D-Bus. Connected devices may appear as Performance entries keyed by controller and Bluetooth address.

Per-device throughput uses Linux's read-only HCI monitor channel. Controller, direction and connection handle are matched against a connection snapshot. Because HCI handles are controller-local and reusable, a handle observed with a different remote address resets retained counters.

Without HCI monitor access, identity remains available and traffic remains unavailable. Privilege handling is defined in [Security](../SECURITY.md).

## GPUs, NPUs and accelerators

GPU discovery uses native DRM/device identity. Generic sysfs attributes are used only when their semantics are explicit. Intel uses the project-owned native backend where supported; AMD uses explicit driver attributes; NVIDIA NVML is optional and loaded in-process when present. Metrics are capability-detected independently.

NPU discovery uses `/sys/class/accel` and resolved device identity. Intel IVPU support consumes attributes with known units. Unknown accelerator drivers expose only attributes whose interface establishes meaning.

## Batteries and peripherals

System and peripheral batteries use Linux power-supply interfaces with device-specific enrichment where available. Logitech HID++ and Bluetooth/D-Bus sources may provide peripheral battery data. Timed peripheral workers use monotonic deadlines.

## Units

Storage and memory use binary scaling: 1 KB = 1024 bytes, 1 MB = 1024 KB, 1 GB = 1024 MB and 1 TB = 1024 GB. Network rates and link speeds use decimal 1000-based scaling. Driver-specific raw units are converted only when the ABI defines them.

## PCI identity provenance

System Monitor does not distribute an external PCI database file or reproduce its hierarchy. The project registry retains factual top-level vendor assignments and direct vendor:device mappings in a flat System Monitor schema, omitting subsystem/class/comment/version records before generating sorted numeric lookup tables with a deduplicated name pool.

Runtime lookup uses those generated tables and does not require `lspci`, pciutils or another runtime database. If no friendly name exists, native firmware/sysfs identity is preferred and exact numeric vendor:device identity remains the fallback.
