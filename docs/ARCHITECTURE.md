<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Architecture

System Monitor separates presentation, platform-neutral state, native platform backends and reusable Common mechanisms. The separation is a correctness boundary: GTK should consume completed state, while collectors retain the operating-system knowledge and mutable baselines required to produce it.

## First-principles design

System Monitor begins with the authoritative operating-system or hardware contract rather than treating another monitoring application as the source of truth. Where practical, the project implements collection and interpretation directly against native interfaces instead of parsing the output or inheriting the behaviour of external utilities that can change independently.

First principles does not mean reimplementing every dependency. A kernel ABI, toolkit, driver API or shared Common primitive is appropriate when it provides the strongest practical contract for the job. The project should go as low in the stack as practical and avoid unnecessary third-party layers even when doing so requires more implementation work. Dependencies are chosen deliberately; ease, convention or development time do not justify surrendering control of important behaviour.

## Dependency ownership

The architectural target is zero avoidable third-party dependencies. A dependency is avoidable when System Monitor can implement the required behaviour directly against a stable Linux, libc or language ABI with equal or stronger correctness, security, maintainability and testability. Convenience alone is not a reason to retain a dependency.

Dependency removal follows these rules:

- preserve the complete documented feature set; dependency reduction is never allowed to silently degrade a supported feature;
- prefer narrow project-owned ABI declarations over a large development package when only stable kernel structures, constants or ioctls are required;
- prefer direct system calls or native APIs over installation-time command helpers when the underlying contract is stable and can be validated safely;
- do not replace a mature platform subsystem merely to reduce a package count; GTK/GLib/GIO remain the current Linux desktop presentation/runtime boundary until a demonstrably stronger project-owned alternative exists;
- Common is project-owned shared infrastructure, not an external product dependency, and remains the preferred home for reusable mechanisms;
- optional vendor libraries remain optional and must never become necessary for core application startup.

The first dependency-reduction sequence is complete: BlueZ development headers, the installation-time `setcap` helper and the explicit Fontconfig runtime/cache-helper requirement are gone. Subsequent Common ownership passes also remove GLib/libc duplication from toolkit-neutral HOME/XDG path discovery, recursive directory creation, complete configuration-file reads and deterministic ASCII classification/matching/ordering, and remove repeated private FNV-1a mechanics from runtime identity/change signatures. Each reduction remains independently testable and reversible through source history.

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
procfs / PSI / cgroup v2 / sysfs / ioctls / D-Bus / optional driver APIs

Common 1.19.20
        ↓
shared parsing / formatting / timing / path / durable-I/O / allocation primitives
```

GTK consumes snapshots and application models. Presentation code should not need to know which Linux path, ioctl, D-Bus interface or driver supplied a metric.

## Desktop application identity

The GTK/GApplication identity `io.github.theinfiltratr.SystemMonitor` is an intentional stable compatibility key rather than current repository or Debian package branding. It participates in desktop application identity and single-instance behaviour, so changing it requires an explicit migration plan instead of an incidental rebrand.

## Contracts and ownership

Public monitor and process structures are plain C data with explicit availability. Native details such as file descriptors, driver handles, Linux paths, worker synchronization and retained counter baselines remain below those contracts.

The Linux backend owns Linux-specific retained state and collector lifetimes. Resource-owning subsystems use explicit create/initialise, update and destroy/shutdown paths. Device-oriented state is reconciled by stable identity where possible so topology changes cannot silently transfer baselines between different devices.

Ownership is intentionally visible at API boundaries. Caller-owned buffers, returned heap objects, borrowed data and subsystem-owned resources are documented rather than inferred from implementation details.

## Concurrency and snapshot consistency

GTK object ownership remains on the GTK main thread. Work that can block on procfs, NSS, D-Bus, device I/O or durable persistence is moved to bounded workers where practical. Workers exchange plain data or immutable request snapshots with the application rather than sharing GTK objects.

A completed process or hardware sample is published as a coherent unit. Presentation code may display metrics with different collection cadences, but a value is not reported as newly available until the collector has established it for that sample. Topology generations and stable identities distinguish device replacement from ordinary metric refresh.

Asynchronous persistence uses immutable save requests and generation ordering. An older worker is not allowed to overwrite a newer scheduled application-history generation. Shutdown paths either join owned workers or use explicit detached-lifetime/reference rules so state cannot be freed while it is still reachable.

Application startup follows the same rule: first paint is not held hostage by unrelated page construction. Performance is the only notebook page constructed before the window is shown. Other pages are built on first navigation, and recurring page-specific inventories do not install timers until their owning page exists. Persistent App History accounting is deliberately different from its lazy GTK page: its non-visual model starts after the first frame and consumes process snapshots for the whole application lifetime, so history does not begin only when the user first opens the tab. Startup-application discovery and durable XDG override writes are dispatched through workers; completed plain-data results are the only startup inventory state published back onto GTK.

Linux Pressure Stall Information is sampled with the rest of the native monitor snapshot on the sampler thread. PSI complements utilisation rather than replacing it: utilisation reports activity, while pressure reports time in which runnable work is delayed by CPU, memory or I/O contention.

Friendly process grouping may use the unified cgroup-v2 path as an additional Linux identity hint. Only the standardized desktop `app.slice`, `background.slice` and `app[-launcher]-ApplicationID[...].service/.scope` conventions are interpreted. General systemd cgroup-to-unit reconstruction is deliberately not guessed; XDG executable and ancestor matching remains the fallback.

## Failure model

Optional telemetry fails independently. A failed read clears or withholds that metric's availability and must not invalidate unrelated data. A numeric zero is never used as a substitute for "unavailable" when zero is itself a valid measurement.

Cumulative counters are accepted only across a valid identity and monotonic sampling interval. Rollback, reset, device replacement or invalid elapsed time breaks the baseline; the next valid sample establishes a new baseline instead of producing a fabricated spike.

Malformed external data is rejected or skipped at the narrowest practical boundary. Parsers bound allocation and numeric conversion, path construction is checked, and partial native-interface failure is represented explicitly rather than hidden by guessed values.

## Common

`src/infiltratr-common` is pinned to one exact Common release commit. Common is the authoritative home for reusable project mechanisms; System Monitor owns application, Linux and hardware policy that is genuinely specific to this product.

Common's target is reference-quality, leading-edge and complete reusable code, not merely a lowest-common-denominator helper set. Common 1.19.20 owns the toolkit-neutral POSIX user/XDG path and recursive-directory contracts, deterministic ASCII classification/case-matching/ordering, stable non-cryptographic FNV-1a mixing, monotonic unsigned-counter delta/rate mechanics and normalized absolute POSIX-clock deadline conversion used by System Monitor. Signature composition, Linux counter identity/reset/wrap policy, pthread synchronization policy, GTK/GLib-specific presentation and object-lifecycle policy remain local. If System Monitor contains a stronger implementation of a capability that is fundamentally generic, the correct direction is to improve Common so that its generic contract preserves the local implementation's correctness, performance, resilience and useful capabilities. Once Common is at least as strong, System Monitor should use Common and remove the duplicate implementation.

Do not weaken specialised code merely to increase reuse. Equally, do not leave generic custom code permanently duplicated when its advantages can be incorporated into Common. Changes to Common are made in the Common repository and consumed here through a new exact pin; this repository does not edit the submodule in place.

## Security and trust model

The installed product is one GUI executable with no project-owned privileged helper or daemon. Local kernel, driver, D-Bus and configuration data is treated as external input: it may disappear during a read, contain unsupported values or be inaccessible to the current user.

The Debian package grants only the capability required to bind the read-only Bluetooth HCI monitor channel. That endpoint is acquired during bootstrap and the process clears its effective, permitted and inheritable capability sets before GTK or monitoring workers start. Failure to drop those capabilities aborts startup. No HCI command, reset or controller reconfiguration is issued through the monitor path.

Process-control operations use the native operating-system permission model. Optional vendor libraries are loaded in-process only when present; failure to load them cannot make the core monitor unusable. Information requiring unavailable privilege degrades to unavailable rather than triggering implicit elevation.

## Verification and assurance

Correctness is enforced at several levels rather than by one end-to-end test. Parser and accounting tests exercise deterministic fixtures; lifecycle and worker tests cover shutdown and repeated refresh; topology tests verify stable identity; hardware tests validate known units and discontinuity handling; runtime stability checks look for descriptor, thread and memory-growth regressions.

Both Make and CMake build the same application and exact Common pin. Make owns the canonical executed verification suite. CI uses CMake to prove the alternate application build path without replaying every smoke executable, then runs the Make suite once, applies sanitizer and 32-bit gates, validates generated Doxygen documentation with warnings treated as errors, and constructs release packages. The publication workflow trusts that exact successful Verify SHA and does not rerun the complete test suite.

## Build contract

Direct `make install` is disabled. Installation is owned by the Debian package or native installer. A release is published only from the exact tested `main` commit, and published tags/assets are treated as immutable.
