<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Linux Mint System Monitor Capability Coverage and Ownership

## Purpose

This document maps the relevant Linux Mint System Monitor capability baseline onto the Infiltrator software family. It is a behavioural coverage and ownership specification, not a one-for-one UI-cloning specification.

The objective is to give each capability one clear home: an existing System Monitor surface, a stronger existing capability, a reusable Common primitive, or an explicitly optional future extension. Product ownership follows behaviour rather than naming similarity.

Monitoring, process inspection and control, resource telemetry and mounted-filesystem observation belong to **System Monitor**. Common may own reusable primitives, but it does not own System Monitor product policy.

## Reference baseline

The reference application is GNOME System Monitor 45.0.2 as shipped in the Linux Mint 22.x GTK3 desktop generation.

Reference source identity:

- upstream repository: `GNOME/gnome-system-monitor`;
- release tag: `45.0.2`;
- tag object: `1887b54c4b1c1b183dd8a4c7465b3d015bdd01d2`;
- release commit: `e74545dbdebd8e591c1679181c5ff3d53c53386e`;
- release tree: `68f31f4d429a2f8fcbcff74e9378e81384ab7ebf`;
- Mint package baseline: `gnome-system-monitor 45.0.2-1+wilma`.

The comparison covers the application shell, process tables and actions, process properties, open files, memory maps, resource graphs, filesystems, cgroups/systemd integration, preferences and task-oriented help behaviour.

## Suite ownership

| Repository | Correct boundary |
| --- | --- |
| **System-Monitor** | Monitor UX, process inspection/control, resource telemetry, mounted-filesystem capacity, monitor preferences, monitor help and monitor state. |
| **Infiltrator-Libraries (Common)** | Generic formatting, timing, parsing, safe file/config access, design primitives and other reusable mechanisms. |
| **System-Settings** | Operating-system-wide policy only; not monitor-local sampling, graph or process-view settings. |
| **Filesystem-Support** | Filesystem-driver/support availability and installation; not mounted-filesystem capacity monitoring. |
| **Defragmenter** | Fragmentation analysis and defragmentation. |
| **Software** | Package/application discovery, installation, update and removal. |
| **Infiltrator-OS** | OS integration, defaults and meta-packaging rather than monitor implementation. |

## Placement inside System Monitor

| System Monitor surface | Responsibility |
| --- | --- |
| **Overview** | Headline CPU, memory, disk, network, GPU, temperature, pressure and busiest-process summary. |
| **Processes** | Friendly process/application view, search and ordinary end-task workflow. |
| **Details** | Technical process inventory, scope filters, flat/tree dependency view, configurable columns and advanced actions. |
| **Process Inspector → Overview** | Full selected-process identity and properties. |
| **Process Inspector → Performance** | Selected-process CPU, memory, I/O and GPU behaviour. |
| **Process Inspector → Open Files** | Per-process descriptors, sockets, pipes and objects. |
| **Process Inspector → Memory Map** | Virtual-memory map. |
| **Process Inspector → Threads** | Thread inventory. |
| **Process Inspector → Process Family** | Parent/child context. |
| **Performance → CPU** | CPU history, current utilisation and per-core activity. |
| **Performance → Memory** | RAM, cache and swap history/composition. |
| **Performance → Network** | Receive/send rates, totals and adapter identity. |
| **Performance → Disks/Partitions** | Physical storage I/O and device telemetry. |
| **Performance → GPU/NPU** | Accelerator activity and available native telemetry. |
| **Performance → Battery/Bluetooth** | Device telemetry where the operating system exposes it. |
| **File Systems** | Mounted source/device, mount point, filesystem type and capacity. |
| **Tools → Find process using file** | Reverse lookup from a file/path to processes holding it open. |
| **Preferences** | Monitor-local sampling, process presentation, graph presentation, warnings and column defaults. |
| **Help** | Product workflows and technical explanations. |
| **Application shell** | Navigation, refresh, shortcuts, window/page state and direct-entry routes. |

The product model is deliberate: Performance answers “what is the resource doing?”, Processes/Details/Inspector answer “what is this workload doing?”, and File Systems answers “how are mounted filesystems consuming capacity?”.

## Process coverage

System Monitor owns the equivalent of the reference application's active/all/my-process scopes, process search, process hierarchy and process actions.

The technical process model includes name, user, status, virtual/resident/shared/writable memory where available, CPU utilisation and time, start time, priority/nice information, PID/PPID, security context, command line, waiting channel, cgroup/unit/session/seat identity where exposed, disk read/write totals and rates, executable identity, thread count and additional native metrics supported by the current backend.

The friendly Processes page remains application-oriented. Technical fields and hierarchy belong in Details and Process Inspector rather than forcing every native field into the friendly surface.

### Process control

System Monitor owns:

- ordinary terminate/end-task workflow;
- force terminate where supported;
- suspend and resume;
- portable priority classes;
- Linux-native advanced nice values where exposed;
- CPU affinity; and
- narrowly scoped privilege escalation for the specific action that requires it.

The whole monitor is not run permanently privileged.

### Open files and reverse lookup

`Process Inspector → Open Files` starts with a selected process and lists its descriptors/objects. `Tools → Find process using file` starts with a path or filename and returns matching processes. These are separate workflows and both belong in System Monitor.

### Memory maps and process family

Virtual-memory mappings, threads and parent/child context are technical inspection functions and remain inside Process Inspector.

## Performance coverage

### CPU

Performance → CPU owns utilisation history, per-core activity, topology, current/max/base frequency where the platform exposes it, thermal information and the selected process-CPU scaling policy.

### Memory and swap

Performance → Memory owns memory and swap history, totals and composition. Presentation units are monitor-local unless the wider suite deliberately publishes an operating-system-wide measurement policy.

### Network

Performance → Network owns live receive/send rates, totals, link identity, addresses and negotiated speed where available. Network rate and total unit choices remain monitor-local preferences.

Bluetooth devices are grouped under Network for top-level navigation while retaining their individual Performance pages.

### Disks and partitions

Physical storage identity, throughput, activity, response and queue information belong under Performance. Mounted-filesystem capacity belongs under File Systems. Fragmentation belongs in Defragmenter, and filesystem-driver availability belongs in Filesystem-Support.

### GPU and NPU

Graphics and neural accelerators belong under Performance. Unsupported or inaccessible telemetry is shown as unavailable rather than inferred. NPUs are grouped under GPU for top-level navigation while retaining individual device pages.

### Battery and pressure

Battery records appear when the monitor has actual battery-capable devices to present. Linux Pressure Stall Information for CPU, memory and I/O contention belongs in Performance/Overview as native system telemetry.

## Mounted file systems

The File Systems page owns:

- device/source;
- mount point;
- filesystem type;
- total, free, available and used capacity;
- used percentage;
- normal/user filesystems by default with an option to include virtual/system mounts;
- mount add/change/remove observation;
- persistent column/view state; and
- row activation through the normal desktop file manager.

This keeps filesystem-capacity observation in the monitor while leaving driver support and storage reorganisation to their dedicated products.

## Sampling and presentation

Sampling policy belongs to System Monitor. Slow collection work must not block the GTK main thread, hidden views should not cause unnecessary presentation work, and completed snapshots should preserve explicit availability rather than inventing zero values.

Graph and presentation preferences are monitor-local. The application uses the suite-wide Infiltrator visual language and typography contracts rather than reproducing arbitrary per-resource styling from the reference application.

## State and identity

System Monitor owns its own window/page state, process-view state, graph settings, sampling preferences and monitor history. Stable native identity is retained where needed so topology changes, PID reuse and device reordering do not redirect retained data to a different object.

## Platform boundary

Linux is the complete current desktop product scope. The native Windows GUI shares the platform-neutral presentation and snapshot contracts while using Windows-native collectors and rendering. Performance, Processes and Overview are live on Windows; other product pages remain explicit placeholders until their native backends are implemented.

## Admission rule

A reference capability is included when it materially strengthens the monitoring or diagnostic model and has a clear owner. A capability may be deliberately superseded when System Monitor already provides a stronger or more portable model. New work should integrate with existing measurements, navigation, actions and visual language rather than becoming an isolated feature island.

Code and tests remain authoritative for executable behaviour. This document records the maintained ownership and capability-placement contract.
