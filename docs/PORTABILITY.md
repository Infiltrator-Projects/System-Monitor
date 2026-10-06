<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Portability

Portability means keeping application contracts independent of one platform's implementation details. It does not mean forcing Linux and Windows toward the weakest common denominator.

## Language and interface policy

C and C++ are equal first-class project languages. Choose the form that gives the strongest implementation for the component: correctness, clarity, performance, maintainability and control matter more than language or paradigm preference.

Application-facing monitor and process contracts remain plain C because that is the established shared ABI. They must not expose Linux handles, GTK objects, Win32 handles, implementation-owned paths or hidden global ownership.

Another language/runtime requires a concrete technical advantage that C/C++ cannot reasonably provide.

## Platform boundary

Linux-specific paths, ioctls, signals, scheduler operations, D-Bus calls and driver knowledge stay below Linux platform seams. Windows-native handles and APIs stay below Windows seams.

The native Windows renderer and backends consume the same application-facing presentation and monitor contracts as Linux where those surfaces overlap. A native renderer may translate shared labels, positions, formatted values and availability into toolkit operations; it must not create a second product definition with different field order, units or semantics.

Platform differences are represented as native implementation differences or unavailable data, not by silently changing the product contract.

GTK 3 with GLib/GIO remains the current Linux desktop boundary. Windows uses native Win32/GDI/common-controls and carries no GTK runtime dependency. Reusable accounting, parsing, formatting, presentation-model and application-state code must not depend on either renderer.

## Representation assumptions

Use explicit-width integers where an external ABI or counter width matters. Binary structures must be decoded without assuming host alignment. Multi-byte protocol fields must apply the byte order defined by their interface.

Do not cast potentially unaligned netlink, device or packet payloads directly to wider integer pointers. Copy or decode them into naturally aligned objects first.

Allocation, size and cumulative arithmetic must reject or saturate overflow according to the owning contract. Counter rollback, reset and invalid elapsed time must not produce wrapped or fabricated rates.

Time used for sampling, rate calculation, retry and worker deadlines remains canonical monotonic SI time. Persisted machine timestamps remain Unix/SI values; human-facing formatting is a presentation concern.

## Dependency boundary

The supported build should not require third-party development packages whose only purpose is to provide declarations for stable kernel ABIs that System Monitor can define narrowly and validate itself.

That rule does not permit copying arbitrary library internals. A project-owned ABI declaration contains only the structures, constants and encodings required by the documented native contract, with size/offset checks or fixtures where practical.

Dependencies that provide substantial semantics are retained unless an independently complete replacement is stronger. GTK/GLib/GIO are a deliberate Linux platform boundary rather than ordinary dependency-cleanup targets.

Generic mechanisms such as path discovery, deterministic text handling, timing and counter arithmetic belong in pinned Common when Common provides an equal or stronger contract. Product-specific collection and policy remain local.

## Native collection

Normal telemetry should use direct platform interfaces rather than external commands such as `lspci`, `lsblk`, `sensors`, `nvidia-smi`, `systemctl` or `nmcli` when an authoritative in-process interface exists.

Capability detection is preferred to platform guessing. A backend should establish that an interface is present and semantically usable rather than infer support from a distribution name, desktop environment or vendor string.

Optional vendor libraries may be loaded in-process where useful, but failure to load them must not make the core monitor unusable.

## Data, paths and lifetime

Platform-neutral code must not hard-code `/proc`, `/sys` or `/dev`. Native roots and paths belong to the implementing backend or are supplied through testable contracts.

Background workers publish platform-neutral results. Threading primitives, cancellation state, descriptors and synchronization objects remain private to the owning subsystem and are released deterministically. Detached lifetime is permitted only when ownership is explicit or reference-counted.

Bounded inventories must not publish a valid-looking prefix after overflow or incomplete native enumeration. The prior complete topology may be preserved while the backend retries.

Storage and memory presentation uses binary-sized KB/MB/GB/TB labels. Network rates and negotiated link speeds use decimal 1000-based scaling.

## Build portability

Both Make and CMake build the application against the same pinned Common source state. Windows translation units are cross-compiled and linked as Windows code; Linux portability gates do not pretend Win32 sources are Linux ELF translation units.

The portability contract is evaluated at the application/platform seam, not by making platform-specific collectors artificially similar.

## Review test

A portable change should preserve these properties:

- platform-neutral code does not gain native platform handles, paths or toolkit objects;
- external binary data is decoded with explicit width, alignment and byte-order assumptions;
- ownership, units, timing and failure behaviour remain explicit;
- unsupported capabilities degrade to unavailable rather than guessed data;
- no new runtime/language dependency is introduced without a concrete technical benefit;
- portability does not force a weaker implementation; and
- another native backend could implement the same public contract without reproducing another platform's internals.
