<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Architecture

Linux System Monitor is organised around a deliberately small set of boundaries: presentation, platform-neutral snapshots/contracts, platform backends, and reusable Common primitives. The goal is to keep the Linux implementation direct while avoiding Linux-specific assumptions in the application-facing model.

## High-level structure

```text
GTK 3 presentation
        ↓
LsmMonitor / process model / application state
        ↓
platform contracts
        ↓
Linux backend and collectors
        ↓
procfs / sysfs / ioctls / D-Bus / optional in-process driver APIs

Infiltratr Common
        ↓
shared parsing / formatting / timing / durable I/O primitives
```

The GTK layer consumes snapshots and model state. It should not know where a metric came from, which Linux path produced it, or how a driver-specific counter is retained between samples.

## Platform-neutral contracts

Public monitor and process structures are plain C data. They carry current values, identities and explicit availability state required by the presentation layer. Native implementation details such as file descriptors, cumulative-counter baselines, Linux path ownership and driver handles do not belong in those public snapshots.

Platform seams keep operating-system work below the application model. The Linux monitor backend owns Linux-specific retained state and coordinates collector lifecycle. A future native backend should implement the same application-facing contract rather than teaching GTK or the public data model about another operating system.

## Linux backend ownership

The Linux monitor backend owns retained resources needed across samples, including native source contexts, Wi-Fi metadata state, CPU/disk/network accounting state and hardware telemetry state. Resource-owning subsystems use explicit create/initialise, update and destroy/shutdown paths.

GPU and NPU telemetry caches are tied to the active monitor backend rather than process-global mutable state. Topology reconciliation retains state by stable device identity where possible, destroys unmatched resources and creates new resources only for newly discovered devices.

Optional telemetry is independently degradable. Losing one driver attribute must not make unrelated metrics disappear, and a failed read must not silently reuse stale availability from an earlier sample.

## Collection versus presentation

Collection modules read and interpret operating-system or driver state. Presentation modules format current snapshots for GTK widgets and graphs. Expensive discovery work is kept off the high-frequency sample path where practical.

The application does not use shell commands as normal telemetry providers. Direct collection keeps ownership, error handling and units inside the program and avoids making the GUI an orchestration layer around unrelated tools.

## Infiltratr Common

`src/infiltratr-common` is an exact git submodule pin to Infiltratr Common. Common owns genuinely reusable primitives such as strict parsing, formatting, monotonic timing, checked arithmetic, durable atomic I/O and ordered readable-path selection.

System Monitor owns product-specific policy: Linux hardware discovery, driver interpretation, monitor snapshot semantics, UI behaviour and hardware capability decisions. Common must not be modified from this repository merely to make one System Monitor call site convenient.

## Process architecture

Process collection follows the same separation. Platform-neutral process records and controls are kept distinct from Linux-native implementation details such as `/proc`, signals, scheduler calls, affinity masks and user identifiers. Inspection and control operations return explicit success/failure information rather than leaking native handles into presentation code.

## GUI and privilege boundary

The installed product is one GUI executable. There is no project-owned privileged daemon or helper. Running the GUI with elevated privileges is an explicit user choice outside the normal package architecture.

Collectors must therefore degrade gracefully when information is unavailable to the current user. Missing permission is not a reason to guess a value or to introduce an always-running privileged component.

## Build-system contract

Make and CMake both build the same application and pinned Common dependency. CI exercises both build paths, the C test suite, sanitizer checks where supported, a 32-bit compile gate and release-package construction.

Direct `make install` is intentionally disabled. Installation belongs to the Debian package or native installer so installed files remain auditable and removable through one package boundary.

## Documentation ownership

This file owns the architectural boundaries. `PORTABILITY.md` owns language/toolchain and platform-extension rules. `HARDWARE.md` owns telemetry-source and availability behaviour. Contribution and security policy live under `.github/` so engineering contracts are not duplicated across multiple documents.
