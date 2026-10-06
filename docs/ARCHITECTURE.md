<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Architecture

System Monitor separates presentation, platform-neutral state, native platform backends and reusable Common mechanisms. Presentation consumes completed state; collectors own operating-system knowledge and retained native state.

## Provenance

System Monitor is an original clean-sheet implementation designed and written from the ground up for this project. Its application source was not forked, copied, translated, adapted, ported or derived from another system-monitoring application. No external monitor's source code, internal architecture, algorithms or implementation behaviour is an implementation authority.

Implementation comes from System Monitor's own requirements, authoritative operating-system and hardware interfaces, documented standards and project-owned Common contracts.

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

Renderers own native widgets, drawing, events and accessibility. Shared product labels, field order, units and availability semantics belong above the renderer.

Linux paths, descriptors, ioctls, D-Bus details and driver state stay below Linux contracts. Win32 handles and Windows-native state stay below Windows contracts. Platform differences are represented by native implementations or unavailable data, not by silently changing the shared product contract.

Application-facing monitor and process contracts remain plain C. C and C++ are both valid implementation languages; use whichever gives the stronger result for correctness, clarity, performance, maintainability and control.

## Ownership and representation

Availability is separate from numeric value; zero is never overloaded to mean unavailable when zero is valid.

Resource-owning subsystems have explicit create/initialise, update and destroy/shutdown paths. Native handles, retained paths, worker synchronization and cumulative baselines remain private to the owning backend. Device history is reconciled by stable identity so replacement or reordering cannot inherit another device's state.

External binary structures use explicit widths, interface-defined byte order and safe alignment. Potentially unaligned packet, netlink or device payloads are decoded or copied rather than cast directly to wider pointers. Allocation and cumulative arithmetic reject or saturate overflow according to the owning contract.

## Collection and concurrency

GTK objects stay on the GTK main thread. Work that may block on procfs, NSS, D-Bus, device I/O or durable persistence runs off the UI thread where practical.

Workers exchange plain data or immutable request snapshots. A completed native collection cycle is published as one coherent snapshot with its generation and monotonic completion timestamp assigned only after collection finishes. Re-presenting an unchanged snapshot does not create a new measurement.

Worker queues coalesce duplicate periodic requests. Slow work must not create a catch-up loop. Asynchronous persistence is generation ordered so older work cannot overwrite newer state.

Shutdown joins owned workers or uses explicit lifetime/reference rules. The GUI must not wait indefinitely on a native call that cannot be cancelled safely.

## Startup and presentation

Startup is first-paint oriented. Only the shell and presentation required for the initial frame are constructed before the window is shown; other pages are created on first use where practical.

Persistent non-visual models are independent of whether their page has been opened. Slow page-specific periodic work runs only while its page is active when continuous background sampling is unnecessary.

Collection is independent of presentation. Completed generations feed retained history once; unchanged data does not force unrelated formatting, layout or redraw work.

## Failure model

Optional telemetry fails independently. A failed read clears or withholds only the affected metric unless the owning contract requires the whole inventory to be rejected.

Cumulative rates are valid only across the same stable identity and a positive monotonic interval. Startup, reset, rollback, replacement, missing samples or invalid elapsed time break the baseline. The next valid observation establishes a new baseline instead of producing a fabricated spike.

Malformed external data is rejected at the narrowest practical boundary. Bounded inventories are complete-or-preserved: overflow or incomplete topology discovery does not publish a plausible-looking prefix as a complete inventory.

Unsupported or inaccessible information remains unavailable rather than guessed.

## Native interfaces and dependencies

Direct procfs, sysfs, ioctl, D-Bus, Win32, kernel, driver or documented in-process interfaces are preferred when they provide a stronger contract than external command output.

GTK/GLib/GIO are the Linux desktop boundary. Windows uses native Win32/GDI/common-controls and carries no GTK runtime dependency. Optional vendor libraries may be loaded in-process but cannot be required for core startup.

Project-owned declarations may cover a narrow stable native ABI when only a small documented structure/constant set is required. They do not justify copying library internals. Dependencies that provide substantial semantics remain dependencies unless an independently complete replacement is stronger.

Platform-neutral code does not hard-code Linux native roots such as `/proc`, `/sys` or `/dev`; those belong to the implementing backend or test contract.

## Common

`src/infiltratr-common` is pinned to one exact Infiltrator Common commit. Common is first-party shared infrastructure, not ancestry from another monitoring product.

Generic mechanisms move to Common when its contract is at least as strong as the local implementation. System Monitor keeps product-specific monitoring, hardware interpretation, platform policy and presentation behaviour local.

## Product boundary

Features are admitted because they strengthen System Monitor's own purpose, not to match another application's feature count. Prefer capabilities that connect existing measurements, navigation and actions over isolated feature islands. Another product is not a design source or implementation blueprint.

## Security boundary

The installed Linux product is one GUI executable with no project-owned privileged daemon or helper. Kernel, driver, D-Bus and configuration data is treated as untrusted external input that may disappear, be malformed, unsupported or inaccessible.

Bluetooth HCI monitoring uses only the file capability required for its read-only monitor channel. The endpoint is acquired during bootstrap and process capability sets are cleared before normal GTK and monitoring work begins; failure to drop them aborts startup. The monitor path issues no HCI commands, resets or controller reconfiguration.

Process-control operations use the native operating-system permission model. Optional privileged or vendor-specific telemetry degrades to unavailable instead of triggering implicit elevation.

Hardware-specific collection rules live in [Hardware collection](HARDWARE.md). Verification and evidence requirements live in [Validation](VALIDATION.md).
