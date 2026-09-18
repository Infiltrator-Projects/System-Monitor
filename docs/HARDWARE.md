<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Hardware collection

Linux System Monitor reports hardware values only when the active native interface establishes both the value and its meaning. Unknown units, stale reads and unsupported capabilities are unavailable rather than guessed.

## Availability and sampling

Optional metrics keep availability separate from numeric storage, so zero remains a valid reading. Collectors clear per-sample availability before refresh. Failed reads do not preserve stale values.

Cumulative counters reset their baseline after rollback, failed reads or device replacement so recovery cannot create false spikes. Discovery may run more slowly than telemetry; retained contexts keep resolved paths and baselines between samples.

## CPU and memory

CPU and memory data comes from Linux procfs/sysfs, CPUID where applicable, `sysinfo` and retained accounting state. Linux scheduler counters, frequency-source paths and baselines remain private to the backend.

Missing frequency or temperature data must not invalidate independently collected CPU utilisation or memory values.

## Storage and filesystems

Disk inventory, activity and filesystem state use native Linux metadata. Retained accounting state is separate from public snapshots and is reconciled against current device identity.

Incomplete devices are not assigned invented filesystem or hardware metadata.

## Network and wireless

Network counters are converted to rates using monotonic elapsed time; rollback or invalid timing is rejected.

Wireless metadata uses native Linux interfaces when supported by the driver. Short-lived caches are invalidated after failed refreshes rather than kept indefinitely.

## Bluetooth

BlueZ supplies controller/device identity and connection state. Connected remote devices can appear as individual Performance entries keyed by controller and Bluetooth address.

Exact per-device throughput uses Linux's read-only HCI monitor channel. Packet controller, direction and connection handle are combined with a read-only connection snapshot to attribute payload bytes to the matching remote device.

Binding the monitor channel requires `CAP_NET_RAW`. The packaged executable carries only that file capability, opens the monitor channel during startup and then clears its capability set before normal GTK or monitoring workers start. No HCI commands, resets or controller reconfiguration are issued through this path.

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

## Units

Storage and memory use traditional binary-sized labels:

- 1 KB = 1024 bytes
- 1 MB = 1024 KB
- 1 GB = 1024 MB
- 1 TB = 1024 GB

Network rates and negotiated link speeds use decimal 1000-based scaling. Driver-specific raw units are converted only when the relevant ABI defines them.

## Adding hardware support

New support should establish a stable identity, a native interface with known semantics, explicit availability, safe retained state and deterministic tests. Keep expensive discovery away from high-frequency sampling, reset cumulative baselines after discontinuity, and do not add shell-command providers or vendor guesses merely to fill a field.
