<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Architecture

System Monitor separates presentation, platform-neutral state, native platform backends and reusable Common mechanisms. The separation is a correctness boundary: presentation consumes completed state; collectors own the operating-system knowledge and retained baselines required to produce it.

## Provenance

System Monitor is an original clean-sheet implementation designed and written from the ground up for this project. Its application source was not forked, copied, translated, adapted, ported or derived from another system-monitoring application. No external monitor's source code, internal architecture, algorithms or implementation behaviour is an implementation authority.

Implementation comes from System Monitor's product requirements, authoritative operating-system and hardware interfaces, documented standards and project-owned Common contracts.

## Structure

```text
                    System Monitor semantics
                            ↓
          presentation contracts and view models
                 ↙                      ↘
        GTK renderer                 Win32 renderer
             ↓                           ↓
      Linux adapters              Windows adapters
             ↓                           ↓
 native OS/kernel/driver APIs   native Windows APIs

                   shared plain-C models
                            ↑
                    platform contracts

                    Common 1.19.35
                            ↓
       reusable formatting / parsing / timing /
            path / arithmetic / design primitives
```

Renderers own native widget creation, drawing, event delivery and accessibility integration. They do not own separate product labels, field order, unit policy or availability semantics for shared surfaces.

Collectors publish platform-neutral monitor and process models. Linux paths, file descriptors, ioctls, D-Bus details and driver state stay below Linux contracts; Win32 handles and native Windows state stay below Windows contracts. A presentation component should not need to know which native interface supplied a metric.

## Ownership

Public monitor and process structures use explicit availability. Zero is never overloaded to mean unavailable when zero is a valid reading.

Resource-owning subsystems use explicit initialise/create, update and destroy/shutdown paths. Native handles, paths, worker synchronization and retained counter baselines remain private to the owning backend. Device-oriented state is reconciled by stable identity so replacement or reordering cannot silently transfer history to a different device.

Caller-owned buffers, returned allocations, borrowed data and subsystem-owned resources should be explicit at API boundaries.

## Collection and snapshot consistency

GTK object ownership remains on the GTK main thread. Work that can block on procfs, NSS, D-Bus, device I/O or durable persistence is moved to bounded workers where practical. Workers exchange plain data or immutable request snapshots with the application rather than sharing GTK objects.

A completed native sample is published as one coherent snapshot. Monitor backends assign the completed-sample generation and monotonic completion timestamp only after the collection cycle finishes. Presentation may update at a different cadence, but an unchanged completed snapshot is not represented as a newly measured sample.

Topology generations and stable identities distinguish replacement from ordinary refresh. A retained device identity is resolved against the current topology before navigation or history association.

Worker queues coalesce duplicate periodic requests. A slow sample must not create a catch-up loop. Asynchronous persistence is generation ordered so an older worker cannot overwrite newer state.

Shutdown either joins owned workers or uses explicit reference/lifetime rules. Bounded shutdown is preferred to indefinitely blocking the GUI on a native call that cannot be safely cancelled.

## Startup and presentation work

Startup is first-paint oriented. Only the shell and Performance surface required for the initial frame are constructed before the window is shown; other product pages are created on first use where practical.

Persistent non-visual models, such as application history, are independent of whether their GTK page has been opened. Slow page-specific periodic work runs only while its owning page is active when continuous background sampling is unnecessary.

Performance sampling is independent of Performance presentation. Completed monitor generations feed retained history once; label formatting, detail-table work and style updates are limited to the views that need presentation. Overview likewise tracks monitor and process generations separately so one changing source does not force the unchanged half of the dashboard to redraw.

## Failure model

Optional telemetry fails independently. A failed read clears or withholds only the affected metric's availability unless the owning contract requires the whole inventory to be rejected.

Cumulative rates are valid only across the same stable identity and a valid positive monotonic interval. Startup, reset, rollback, replacement, missing samples or invalid elapsed time break the baseline; the next valid observation establishes a new baseline instead of generating a spike.

Malformed external data is rejected or skipped at the narrowest practical boundary. Path construction, allocation growth and numeric conversion are checked. Bounded inventories are complete-or-preserved: overflow or incomplete topology discovery must not publish a plausible-looking prefix as a complete device set.

Unsupported or inaccessible information remains unavailable rather than being guessed.

## Native interfaces and dependencies

System Monitor prefers direct native interfaces when they provide the strongest practical contract. External command output is not treated as an API when equivalent data can be obtained safely from procfs, sysfs, ioctls, D-Bus, Win32 or a documented in-process interface.

Dependency minimisation is a means to stronger ownership, not a goal that justifies reimplementing mature platform subsystems. GTK/GLib/GIO remain the Linux desktop boundary. Optional vendor libraries remain optional and cannot be required for core startup.

Project-owned narrow ABI declarations are appropriate when the kernel contract is stable and only a small set of structures/constants is required. They must not become copied library internals.

## Common

`src/infiltratr-common` is pinned to one exact Infiltrator Common commit. Common is first-party shared infrastructure, not ancestry from another monitoring product.

Common owns generic reusable mechanisms when its contract is at least as strong as the best local implementation. System Monitor keeps product policy, Linux/Windows collection policy, hardware interpretation and presentation behaviour that are genuinely product-specific.

If System Monitor develops a stronger implementation of a fundamentally generic mechanism, the intended direction is to improve Common, consume the stronger shared contract, then remove the duplicate local implementation. Specialised code is not weakened merely to increase reuse.

## Security boundary

The installed Linux product is one GUI executable with no project-owned privileged daemon or helper. Local kernel, driver, D-Bus and configuration data is treated as untrusted external input that may disappear, be malformed, be unsupported or be inaccessible.

Bluetooth HCI monitoring uses only the file capability required to bind the read-only monitor channel. The endpoint is acquired during bootstrap and process capability sets are cleared before normal GTK and monitoring workers start. Failure to drop those capabilities aborts startup. The monitor path does not issue HCI commands, resets or controller reconfiguration.

Process-control operations use the native operating-system permission model. Optional privileged or vendor-specific telemetry degrades to unavailable instead of triggering implicit elevation.

## Verification and release boundary

Correctness is demonstrated at multiple levels: deterministic parser/accounting tests, lifecycle and worker tests, topology identity checks, hardware unit/discontinuity fixtures, runtime-stability checks, portability gates and package/release verification.

Make owns the canonical executed verification suite. CMake proves the alternate build path. A release is publishable only from the exact `main` commit that passed the required Verify workflow. Published tags and release assets are immutable.

Detailed evidence rules live in [Validation](VALIDATION.md); hardware-specific collection rules live in [Hardware collection](HARDWARE.md); platform/ABI constraints live in [Portability](PORTABILITY.md); durable rationale lives in [Decisions](DECISIONS.md).
