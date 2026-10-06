<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Architecture

System Monitor separates presentation, platform-neutral state, native backends and reusable Common mechanisms. Presentation consumes completed state; backends own operating-system knowledge and native state.

## Provenance

System Monitor is an original clean-sheet implementation designed and written from the ground up for this project. No other system-monitoring application's source code, architecture, algorithms or implementation behaviour is an implementation authority.

## Structure

```text
GTK 3 (Linux) / Win32 (Windows)
              ↓
 presentation contracts/models
              ↓
       platform contracts
          ↙       ↘
      Linux       Windows
          ↓       ↓
       native OS/kernel/driver APIs

Common 1.19.35 → shared generic mechanisms
```

Renderers own widgets, drawing, events and accessibility. Shared labels, units and availability semantics stay above renderer boundaries; native paths, handles, ioctls, D-Bus and driver state stay below platform contracts. Application-facing monitor and process contracts remain plain C.

## Core contracts

- Availability is distinct from value; valid zero is never used to mean unavailable.
- Retained device state and history use stable resource identity.
- External binary data uses explicit widths, defined byte order and alignment-safe decoding.
- GTK objects stay on the GTK main thread; potentially blocking native work runs off it where practical.
- Workers exchange plain data or immutable requests and publish only completed collection cycles.
- Published snapshots carry a generation and monotonic completion time. Re-presenting one is not a new measurement.
- Duplicate periodic work coalesces. Persisted state is generation ordered.
- Shutdown owns worker lifetime and must not wait indefinitely on an uncancellable native call.

## Startup and refresh

Startup favours first paint: build the shell and initial presentation first, then create other pages on first use where practical. Persistent models do not depend on a page being opened. Page-specific slow work runs only while needed unless continuous sampling is part of the model.

## Failure model

Optional telemetry fails independently unless its contract requires rejecting the whole inventory. Unsupported or inaccessible information remains unavailable rather than guessed.

Rates require stable identity and a positive monotonic interval. Reset, rollback, replacement, missing samples or invalid elapsed time break the baseline rather than creating a fabricated rate.

Malformed external data is rejected at the narrowest practical boundary. Bounded inventories are complete-or-preserved: incomplete discovery is not published as complete.

## Native interfaces and dependencies

Prefer direct procfs, sysfs, ioctl, D-Bus, Win32, kernel, driver or documented in-process interfaces over external command output when they provide the stronger contract.

GTK/GLib/GIO form the Linux desktop boundary. Windows uses native Win32/GDI/common-controls and has no GTK runtime dependency. Optional vendor libraries may be loaded in-process but are not required for core startup.

Project-owned declarations may cover narrow stable native ABIs but do not justify copying library internals. Dependencies that provide substantial semantics remain dependencies unless independently replaced. Platform-neutral code does not hard-code Linux roots such as `/proc`, `/sys` or `/dev`.

## Common

`src/infiltratr-common` is pinned to one exact Infiltrator Common commit. Generic mechanisms may move to Common; monitoring policy, hardware interpretation and product presentation remain local.

Hardware-specific rules live in [Hardware collection](HARDWARE.md). Security boundaries live in [Security](../SECURITY.md).
