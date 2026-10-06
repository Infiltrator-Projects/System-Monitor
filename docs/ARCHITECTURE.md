<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Architecture

System Monitor separates presentation, platform-neutral state, native platform backends and reusable Common mechanisms. Presentation consumes completed state; collectors own operating-system knowledge and retained native state.

## Provenance

System Monitor is an original clean-sheet implementation designed and written from the ground up for this project. Its application source was not forked, copied, translated, adapted, ported or derived from another system-monitoring application. No external monitor's source code, internal architecture, algorithms or implementation behaviour is an implementation authority.

## Structure

```text
presentation: GTK 3 (Linux) / Win32 (Windows)
                    ↓
       presentation contracts and models
                    ↓
             platform contracts
              ↙           ↘
     Linux backends     Windows backends
              ↓           ↓
       native OS / kernel / driver APIs

Common 1.19.35
    ↓
shared formatting, parsing, timing, path,
arithmetic and design primitives
```

Renderers own native widgets, drawing, events and accessibility. Shared labels, field order, units and availability semantics stay above the renderer. Native paths, handles, ioctls, D-Bus details, driver knowledge and retained platform state stay below platform contracts.

Application-facing monitor and process contracts remain plain C. Platform differences use native implementations or explicit unavailability rather than silently changing the shared product contract.

## State and ownership

Availability is separate from numeric value; zero is never used to mean unavailable when zero is valid.

Resource-owning subsystems have explicit initialise/update and shutdown paths. Native handles, worker synchronization, paths and cumulative baselines remain private to the owning backend. Retained device history is keyed by stable identity so replacement or reordering cannot inherit another device's state.

External binary structures use explicit widths, interface-defined byte order and alignment-safe decoding. Allocation and cumulative arithmetic reject or saturate overflow according to the owning contract.

## Collection and concurrency

GTK objects stay on the GTK main thread. Work that may block on procfs, NSS, D-Bus, device I/O or durable persistence runs off the UI thread where practical.

Workers exchange plain data or immutable request snapshots. A native collection cycle is published only when complete, with generation and monotonic completion time assigned after collection finishes. Re-presenting an unchanged snapshot is not a new measurement.

Periodic work coalesces duplicate requests and must not create catch-up loops. Persistence is generation ordered so stale work cannot overwrite newer state. Shutdown joins owned workers or uses explicit lifetime rules; the GUI must not wait indefinitely on a native call that cannot be cancelled safely.

## Startup and presentation

Startup is first-paint oriented. Build only the shell and initial presentation before showing the window; create other pages on first use where practical.

Persistent non-visual models do not depend on whether their page has been opened. Slow page-specific periodic work runs only while its page is active when continuous background sampling is unnecessary. Completed generations feed retained history once; unchanged data does not force unrelated formatting, layout or redraw work.

## Failure model

Optional telemetry fails independently. A failed read invalidates only the affected metric unless the owning contract requires rejection of the whole inventory.

Cumulative rates require the same stable identity and a positive monotonic interval. Startup, reset, rollback, replacement, missing samples or invalid elapsed time break the baseline; the next valid observation establishes a new one rather than producing a fabricated spike.

Malformed external data is rejected at the narrowest practical boundary. Bounded inventories are complete-or-preserved: overflow or incomplete discovery never publishes a plausible-looking prefix as complete. Unsupported or inaccessible information remains unavailable rather than guessed.

## Native interfaces and dependencies

Prefer direct procfs, sysfs, ioctl, D-Bus, Win32, kernel, driver or documented in-process interfaces when they provide a stronger contract than external command output.

GTK/GLib/GIO form the Linux desktop boundary. Windows uses native Win32/GDI/common-controls and carries no GTK runtime dependency. Optional vendor libraries may be loaded in-process but cannot be required for core startup.

Project-owned declarations may cover narrow stable native ABIs but do not justify copying library internals. Dependencies that provide substantial semantics remain dependencies unless independently replaced. Platform-neutral code does not hard-code Linux native roots such as `/proc`, `/sys` or `/dev`.

## Common

`src/infiltratr-common` is pinned to one exact Infiltrator Common commit. Generic mechanisms move to Common when its contract is at least as strong as the local implementation; monitoring policy, hardware interpretation and product presentation remain local to System Monitor.

Hardware-specific collection rules live in [Hardware collection](HARDWARE.md). Security reporting and privilege boundaries live in [Security](../SECURITY.md).
