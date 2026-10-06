<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Hardware collection

This document records hardware-specific rules not already defined by [Architecture](ARCHITECTURE.md).

## CPU and memory

Linux memory `kB` quantities use 1024 bytes per KB.

CPU thermal policy prefers the selected CPU/package hwmon warning and critical points, then CPU-labelled thermal-zone trips. A manufacturer-documented SKU value may supply Tjmax when native limits are unavailable. Unknown processors use 80 °C warning / 95 °C fault. Overview and CPU Performance share the same resolved policy.

## Storage, filesystems and network

Linux diskstats sector accounting uses 512-byte sectors regardless of device block size. Missing filesystem or hardware metadata remains unavailable.

Link utilisation is shown only when a meaningful negotiated link rate is known. Failed wireless metadata refreshes invalidate short-lived cached values.

## Bluetooth and peripherals

Bluetooth identity and connection state come from native BlueZ/D-Bus interfaces. Per-device traffic may use Linux's read-only HCI monitor channel.

HCI connection handles are controller-local and reusable. A retained handle observed with a different remote address resets its counters. Without HCI monitor access, identity remains available and traffic remains unavailable. Privilege handling is defined in [Security](../SECURITY.md).

System and peripheral batteries use native power-supply, Bluetooth or device-specific interfaces where their semantics are known. Timed peripheral workers use monotonic deadlines.

## GPUs, NPUs and accelerators

Hardware metrics are capability-detected independently from native driver or documented in-process interfaces. Unsupported, inaccessible or semantically ambiguous measurements remain unavailable rather than inferred.

## Units

Storage and memory use binary scaling: 1 KB = 1024 bytes, 1 MB = 1024 KB, 1 GB = 1024 MB and 1 TB = 1024 GB. Network rates and link speeds use decimal 1000-based scaling. Raw driver values are converted only when the interface defines their units.

## PCI identity provenance

System Monitor does not distribute an external PCI database file or reproduce its hierarchy. The project registry retains factual top-level vendor assignments and direct vendor:device mappings in a flat System Monitor schema, omitting subsystem, class, comment and version records before generating sorted numeric lookup tables with a deduplicated name pool.

Runtime lookup uses those generated tables and does not require `lspci`, pciutils or another runtime database. If no friendly name exists, native firmware/sysfs identity is preferred and exact numeric vendor:device identity remains the fallback.
