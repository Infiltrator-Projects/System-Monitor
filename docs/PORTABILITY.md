<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Portability

Linux System Monitor targets ISO C17 while deliberately avoiding newer language/runtime requirements that do not materially improve the design. Portability is treated as an architectural property, not as a claim that the current Linux backend already runs unchanged on every Unix-like system.

## Language baseline

The project baseline is ISO C17. Code should prefer constructs available in C11 or C99 when they express the same design clearly, because that keeps the source friendly to older and less-common native toolchains without forcing the code back to C90-era style.

C23-only features and C++ are not introduced merely for convenience. A newer language feature is justified only when it provides a concrete project benefit that cannot be obtained cleanly within the existing baseline.

Compiler-specific extensions are avoided in product logic. Where the build uses compiler capability probes, unsupported warning or analysis options must degrade cleanly rather than becoming a hidden compiler lock-in.

## Plain-C interfaces

Application-facing snapshots and platform contracts remain plain C structures and functions. They should not expose Linux-native handles, GTK objects, implementation-owned paths or compiler-specific types.

Ownership must be explicit. Resource-owning contexts have one clear lifetime, cleanup path and failure behaviour. A C module should not rely on hidden global ownership merely because a C++ destructor would otherwise have been convenient.

## Platform boundaries

Linux-specific implementation details belong below platform seams. This includes:

- procfs and sysfs paths;
- Linux ioctls and netlink/wireless interfaces;
- signals, scheduler and affinity operations;
- D-Bus service calls used by Linux desktop/system services;
- Linux driver ABI knowledge and dynamically loaded driver libraries.

Presentation and platform-neutral model code should consume normalized data and explicit availability rather than native mechanisms.

A future Windows, BSD, Solaris or other native backend should implement the existing contracts with that platform's own interfaces. It should not emulate Linux paths or invoke a Linux compatibility layer merely to satisfy the current implementation.

## Direct collection

Portability does not mean delegating collection to whichever command happens to exist. Normal telemetry should not depend on external programs such as `lspci`, `lshw`, `lsblk`, `sensors`, `dmidecode`, `nvidia-smi`, `systemctl`, `loginctl` or `nmcli`.

Direct interfaces make the dependency visible in code and allow each platform backend to provide equivalent information using its own native API. Optional native libraries may be loaded in-process when they are the appropriate vendor interface and the application remains functional without them.

## Data types and arithmetic

Persistent/public quantities use explicit-width integer types where width matters. Counter arithmetic must handle rollback, overflow and invalid timing without turning errors into spikes.

Storage, memory and network presentation uses 1024-based scaling with the traditional KB, MB, GB and TB labels used throughout the project.

Do not encode host pointer size, byte order or structure padding into file formats or cross-platform contracts without an explicit format definition.

## Filesystem and path handling

Platform-neutral code should not contain hard-coded `/proc`, `/sys` or `/dev` paths. Native path discovery belongs to the backend or a Linux-specific provider.

Fixed-size path buffers are acceptable when the bound is explicit and every join/copy operation is checked. Heap allocation is appropriate where a resource genuinely has dynamic lifetime or unbounded membership, but repeated tiny allocations should not be used when a bounded context-owned buffer is clearer and safer.

## Threads and synchronization

Background work should expose a platform-neutral result rather than leaking thread primitives into public models. Thread start/stop state, cancellation objects, file descriptors, mutexes and condition variables are owned by the implementing subsystem and must be joined/released deterministically.

The current Linux implementation may use POSIX and GLib facilities internally. Those choices do not become part of the application-facing contract.

## GTK boundary

GTK 3 is currently the Linux presentation toolkit. GTK types belong in presentation/application code and should not be required by reusable accounting, parsing or model layers.

A future non-GTK target can therefore reuse the platform-neutral data/model contracts without importing GTK just to collect or interpret system state.

## Shared Common library

When a primitive is genuinely useful across projects, prefer an existing Infiltratr Common API. System Monitor pins one exact Common commit at `src/infiltratr-common`.

Do not copy Common implementation into System Monitor, reach into Common private source membership, or modify the submodule from this repository. Hardware-specific and application-specific policy remains local even when it uses Common primitives.

## Portability review checklist

A change is portability-friendly when it can answer yes to these questions:

1. Does platform-neutral code remain free of new Linux-native types and paths?
2. Does the change compile as standard C17 without a new runtime language dependency?
3. Are ownership and cleanup explicit without relying on process-global state unnecessarily?
4. Can unsupported hardware/API capability degrade to unavailable rather than guessed data?
5. Could another native backend implement the same contract without reproducing Linux internals?
6. Does the change avoid turning an external command into a required telemetry provider?
7. Are unit, width, overflow and timing assumptions explicit?

The portability checker and CI enforce mechanical parts of this policy; code review remains responsible for architectural intent.
