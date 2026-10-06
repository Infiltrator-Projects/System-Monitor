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

Common → shared generic mechanisms
```

Renderers own widgets, drawing, events and accessibility. Shared labels, units and availability semantics stay above renderer boundaries; native paths, handles and driver state stay below platform contracts. Application-facing monitor and process contracts remain plain C.

## Runtime contracts

- Availability is distinct from value; valid zero never means unavailable.
- Retained state and history use stable resource identity.
- External binary data uses explicit widths, defined byte order and alignment-safe decoding.
- GTK objects stay on the GTK main thread; potentially blocking native work runs off it where practical.
- Workers publish completed collection cycles, not partial inventories.
- Measurements use monotonic time; reset, replacement or invalid elapsed time breaks rate baselines.
- Duplicate periodic work coalesces, and shutdown owns worker lifetime.
- Optional telemetry fails independently unless its contract requires rejecting the whole inventory.
- Malformed or incomplete external data is rejected rather than published as complete.

Startup favours first paint. Persistent models do not depend on a page being opened, and page-specific slow work runs only while needed unless continuous sampling is part of the model.

## Native boundaries

Prefer direct procfs, sysfs, ioctl, D-Bus, Win32, kernel, driver or documented in-process interfaces over parsing external command output when they provide the stronger contract.

GTK/GLib/GIO form the Linux desktop boundary. Windows uses native Win32/GDI/common-controls and has no GTK runtime dependency. Optional vendor libraries may be loaded in-process but are not required for core startup.

Platform-neutral code does not hard-code Linux roots such as `/proc`, `/sys` or `/dev`. Project-owned declarations may cover narrow stable native ABIs; substantial external semantics remain dependencies unless independently replaced.

## Common

`src/infiltratr-common` is pinned to an exact Infiltrator Common commit. Generic mechanisms may move to Common; monitoring policy, hardware interpretation and product presentation remain local.

Hardware-specific rules live in [Hardware collection](HARDWARE.md). Security boundaries live in [Security](../SECURITY.md).
