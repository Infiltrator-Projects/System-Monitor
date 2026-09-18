<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Portability

Linux System Monitor targets ISO C17. Portability means keeping application contracts independent of Linux implementation details, not pretending the current Linux backend already runs unchanged everywhere.

## Language and interfaces

Prefer C11/C99 constructs when they are equally clear. Do not add C23-only features, C++ runtime requirements or compiler-specific product logic merely for convenience.

Application-facing snapshots and contracts remain plain C. They must not expose Linux handles, GTK objects, implementation-owned paths or hidden global ownership.

## Platform boundary

Linux-specific paths, ioctls, signals, scheduler/affinity operations, D-Bus calls and driver ABI knowledge belong below platform seams. A future Windows, BSD, Solaris or other backend should implement the same application-facing contracts using that platform's native interfaces.

GTK 3 is currently the Linux presentation toolkit; reusable accounting, parsing and model layers must not depend on GTK.

## Native collection

Normal telemetry should use direct APIs rather than external commands such as `lspci`, `lsblk`, `sensors`, `nvidia-smi`, `systemctl` or `nmcli`. Optional vendor libraries may be loaded in-process when appropriate and must remain optional.

## Data, paths and lifetime

Use explicit-width integers where width matters. Counter arithmetic must handle overflow, rollback and invalid timing without producing false spikes.

Storage and memory presentation uses 1024-based KB/MB/GB/TB labels. Network rates and negotiated link speeds use decimal 1000-based scaling.

Platform-neutral code must not hard-code `/proc`, `/sys` or `/dev`. Path construction, allocation growth and similar generic mechanics should use pinned Common APIs when their contracts fit.

Background workers expose platform-neutral results. Threading primitives, cancellation state, descriptors and synchronization objects remain owned by the implementing subsystem and are released deterministically.

## Review test

A portable change should preserve these properties:

- platform-neutral code does not gain Linux-native types or paths;
- no new language/runtime dependency is introduced without a concrete benefit;
- ownership, units and failure behaviour remain explicit;
- unsupported capabilities degrade to unavailable rather than guessed data;
- another native backend could implement the same public contract without reproducing Linux internals.
