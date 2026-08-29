<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Hardware collection

Linux System Monitor collects hardware identity and telemetry directly from native Linux interfaces wherever practical. The guiding rule is simple: show a value only when the active interface establishes both the value and its meaning. Unknown units, stale reads and unsupported capabilities are represented as unavailable rather than guessed.

## Availability semantics

Every optional live metric has availability separate from its numeric storage. A valid zero-percent reading is not the same thing as an unavailable reading.

Collectors clear per-sample availability before refreshing optional metrics. If a native read fails, the application does not keep presenting a previous value as though it were current. Cumulative counters reset their baseline after rollback or failed reads so recovery cannot create false spikes.

Topology and telemetry are separate concerns. Device discovery can run at a slower cadence while retained telemetry contexts keep already-resolved driver paths and counter baselines between normal samples.

## CPU and memory

CPU and memory information is collected through Linux-native procfs/sysfs sources and retained accounting state. Public snapshots contain normalized values; Linux scheduler counters, frequency-source details and implementation baselines remain private to the backend.

Missing optional frequency or temperature information does not invalidate scheduler utilization or memory information that was collected successfully from an independent source.

## Storage and filesystems

Disk inventory and activity are obtained from native Linux system sources and direct filesystem metadata. Device topology is reconciled by stable identity/name and retained accounting state is kept separately from the public snapshot.

Unmounted or partially described devices are not assigned invented filesystem metadata. If the kernel/native metadata is insufficient, the corresponding field remains unavailable or uses the explicit unknown state expected by the UI.

## Network interfaces

Network byte counters are sampled directly and converted into rates from monotonic elapsed time. Counter rollback or an invalid interval is rejected instead of being presented as an extreme throughput spike.

Wireless enrichment uses the Linux Wireless Extensions ioctl ABI directly for fields such as SSID, access-point identity, signal quality, frequency and negotiated bitrate when the driver supports those requests. The cache is short-lived, and a failed refresh invalidates stale descriptive data rather than preserving it indefinitely.

## Bluetooth controllers

Bluetooth topology and device identity come from the cached BlueZ ObjectManager
snapshot, while traffic is sampled independently from the native Linux HCI
controller ABI. The collector reads the cumulative `byte_rx` and `byte_tx`
values returned by `HCIGETDEVINFO` for each `hciN` controller and converts
their deltas to receive/send rates using monotonic elapsed time.

The HCI counters are 32-bit. A rollback is accepted as wrap only at the narrow
unsigned-counter boundary; an ordinary rollback, controller reset or power
cycle establishes a new baseline and publishes a zero rate rather than a false
throughput spike. A failed HCI read marks traffic unavailable and invalidates
the baseline so stale values are not shown as live data.

The traffic collector is an in-process kernel interface. It does not launch
`hcitop`, `btmon` or another command, and it requires no project-owned
privileged helper. BlueZ development headers are a source-build requirement;
the installed monitor does not call libbluetooth at runtime for these counters.

## GPUs

GPU discovery starts from native DRM/device information and stable driver/device identity. Generic driver-readable sysfs attributes are capability-detected and resolved outside the fast sample path where practical.

Intel GPUs use the project-owned native Intel backend when the active driver exposes the supported interfaces. Cumulative engine counters retain baselines by stable device identity and are reset safely after failed reads or topology change.

AMD metrics are read from documented/explicit driver attributes when available. A metric is not inferred from an ambiguous attribute merely because the file name looks plausible.

For NVIDIA hardware, NVML may be loaded in-process when the installed NVIDIA driver supplies it. It is optional: absence of NVML must not prevent the rest of the application from running.

GPU temperature, clocks, power and fan information are independently capability-detected. One failed sensor must not poison unrelated GPU data.

## NPUs and accelerators

NPU discovery uses `/sys/class/accel` and the device's resolved platform identity. The telemetry context retains bounded, context-owned paths for attributes discovered at creation time.

Intel IVPU support recognises the driver's explicit busy-time, resident-memory and frequency attributes with their documented units. Cumulative busy time is converted into utilization only after a valid baseline and interval exist.

For unknown accelerator drivers, only attribute names that explicitly state their unit/meaning are considered safe generic candidates. Ambiguous manufacturer-specific attributes remain unsupported until a driver-specific profile can establish their semantics.

## Batteries and peripherals

System batteries and peripheral power-supply records are collected from native Linux power-supply interfaces. Peripheral-specific direct sources may enrich those records when available.

Logitech HID++ support is handled in-process and can provide authoritative peripheral battery readings. Generic power-supply information remains useful when direct HID++ data is unavailable.

Bluetooth-related battery information may use native Linux desktop/service interfaces through GLib/D-Bus. This remains an in-process provider rather than a shell-command dependency.

## Privilege behaviour

The package installs no project-owned privileged helper and no always-running daemon. The normal GUI must remain useful as an ordinary user.

Some operating-system or hardware interfaces may expose more information when the entire application is deliberately launched with elevated privileges. That is an external user choice; unsupported unprivileged metrics degrade cleanly rather than triggering an implicit privilege escalation path.

## Units

Storage, memory and network quantities use 1024-based scaling with traditional project labels:

- 1 KB = 1024 bytes
- 1 MB = 1024 KB
- 1 GB = 1024 MB
- 1 TB = 1024 GB

Driver-specific raw units are converted only when the interface establishes the unit. For example, explicit microsecond busy counters, micro-watt power values or milli-degree temperature values are converted according to the relevant native ABI.

## Adding hardware support

New hardware support should follow this sequence:

1. Establish a stable device identity and a native interface with known semantics.
2. Keep discovery separate from high-frequency sampling when path resolution is expensive.
3. Store retained implementation state below the public snapshot contract.
4. Add explicit availability for every optional metric.
5. Reset cumulative baselines after rollback, failed reads or device replacement.
6. Add deterministic fixtures/smoke tests for parsing, units, stale-value behaviour and cleanup.
7. Avoid shell-command providers when a direct kernel/driver API is available.
8. Do not add vendor guesses merely to make a field non-empty.

This contract is intentionally conservative: reliable unavailable data is better than convincing but incorrect telemetry.
