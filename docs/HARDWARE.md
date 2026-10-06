<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Hardware collection

This document records hardware-specific rules not already defined by [Architecture](ARCHITECTURE.md).

## CPU and memory

Linux memory `kB` quantities use 1024 bytes per KB.

CPU thermal policy prefers native CPU/package warning and critical points. A manufacturer-documented Tjmax may be used when native limits are unavailable; otherwise the project fallback is 80 °C warning / 95 °C fault. Overview and CPU Performance use the same resolved policy.

## Storage and network

Linux diskstats sector accounting uses 512-byte sectors regardless of device block size. Missing filesystem or hardware metadata remains unavailable.

Link utilisation is shown only when a meaningful negotiated link rate is known. Failed wireless metadata refreshes invalidate short-lived cached values.

## Bluetooth and peripherals

Bluetooth identity and connection state come from native BlueZ/D-Bus interfaces. Per-device traffic may use Linux's read-only HCI monitor channel.

HCI connection handles are controller-local and reusable; observing a retained handle with a different remote address resets its counters. Without HCI monitor access, identity remains available and traffic remains unavailable.

System and peripheral batteries use native power-supply, Bluetooth or device-specific interfaces only where their semantics are known.

## GPUs, NPUs and accelerators

Metrics are capability-detected independently from native driver or documented in-process interfaces. Unsupported, inaccessible or ambiguous measurements remain unavailable rather than inferred.

## Units

Storage and memory use binary 1024-based scaling. Network rates and link speeds use decimal 1000-based scaling. Raw driver values are converted only when the interface defines their units.

## PCI identity provenance

System Monitor does not distribute an external PCI database file or reproduce its hierarchy. Its registry stores factual vendor and vendor:device mappings in a flat project schema and generates the runtime lookup tables from that data.

Runtime lookup does not require `lspci`, pciutils or another runtime database. If no friendly name exists, native firmware/sysfs identity is preferred and exact numeric vendor:device identity remains the fallback.
