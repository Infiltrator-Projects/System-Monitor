<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Architecture

Linux System Monitor separates presentation, platform-neutral state, platform backends and reusable Common primitives.

## Structure

```text
GTK 3 presentation
        ↓
LsmMonitor / process model / application state
        ↓
platform contracts
        ↓
Linux backend and collectors
        ↓
procfs / sysfs / ioctls / D-Bus / optional driver APIs

Infiltratr Common
        ↓
shared parsing / formatting / timing / path / durable-I/O / allocation primitives
```

GTK consumes completed snapshots and model state. It should not need to know which Linux path, ioctl or driver produced a metric.

## Contracts and ownership

Public monitor and process structures are plain C data with explicit availability. Native implementation details such as file descriptors, driver handles, Linux paths and retained counter baselines stay below those contracts.

The Linux backend owns Linux-specific retained state and collector lifetimes. Resource-owning subsystems use explicit create/initialise, update and destroy/shutdown paths. Device-oriented state should be retained by stable identity where possible so topology changes do not corrupt baselines or leak resources.

Optional telemetry degrades independently. A failed read must not silently preserve stale availability or invalidate unrelated metrics.

## Collection and presentation

Collection modules read and interpret operating-system or driver state. Presentation modules format snapshots for GTK widgets and graphs. Expensive discovery work should stay off high-frequency paths and off the GTK main thread where practical.

Process collection follows the same rule: platform-neutral process records and controls remain distinct from Linux `/proc`, signals, scheduler operations, affinity masks and user IDs.

## Infiltratr Common

`src/infiltratr-common` is pinned to one exact Common release commit. Common owns reusable mechanisms; System Monitor owns product and hardware policy.

Use Common when its contract is at least as strong as the local requirement. Do not weaken a Linux-specific contract merely to replace it with a broader generic helper, and do not modify the Common repository from this project.

## Privilege boundary

The installed product is one GUI executable. It has no project-owned privileged helper or daemon.

The Debian package grants only the capability required for the read-only Bluetooth HCI monitor path. Startup drops all capabilities before normal GTK and monitoring work continues. Other privileged information must degrade to unavailable rather than triggering implicit elevation.

## Build contract

Make and CMake build the same application and exact Common pin. CI exercises both paths, tests, sanitizers, 32-bit compilation and package construction. Direct `make install` is disabled; installation is owned by the package or native installer.
