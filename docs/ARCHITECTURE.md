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

Private application storage remains owned by the composition root. Coordinators that do not own that storage consume narrow views through dedicated runtime, process and presentation context bridges rather than including the complete `LsmApp` layout. Top-level page activation, keyboard commands, window-manager mechanics and theme projection have separate presentation owners so a change in one policy does not require the others to know its private state. Persisted preference defaults, parsing, validation and persistence belong to the preferences domain; the composition root selects lifecycle order but does not duplicate preference policy.

Coupling boundaries are mechanically enforced. `support/app-internal-allowlist.txt` is the exact closed set of implementation files permitted to include `app_internal.h`: unlisted direct consumers fail verification, and stale entries fail once a consumer has been decoupled so an old exception cannot silently authorise future re-coupling. New coordinators must use a narrow context bridge. Native Windows theme projection and process-worker lifetime are separate owners from the Win32 shell. Shared source groups under `support/source-groups/` define source membership where multiple build contexts genuinely need the same subset rather than maintaining parallel source lists.

## Build ownership

The build system deliberately has one authority per concern. Make owns the regression, static-analysis, packaging and release-quality graph through `make check` and the release targets. CMake owns the Linux application build used by development and GTK runtime qualification; it does not duplicate Make's smoke-test registry. The native Windows release recipe lives in `support/windows/build-release.ps1`, while GitHub Actions only orchestrates that recipe and transports its qualified artifact.

`src/infiltratr-common` is a Git submodule. In a Git checkout, Make and CMake validate the checked-out Common revision against the parent repository gitlink instead of maintaining an independent application-build pin. Dependency preparation is explicit: recursive checkout is preferred and `make common-bootstrap` is the sole project-owned opt-in fetch path for a checkout that omitted submodules. Configuration, builds and verification never mutate or fetch dependencies implicitly. Complete source archives produced by `make dist` carry the exact Common tree they were built with. The native `.run` downloads separate System Monitor and Common HTTPS archives using the tested parent commit and its exact Common gitlink before invoking the existing build and package-install owner.

While the Infiltrator estate remains pre-release, System Monitor follows the current shared Common contract rather than carrying product-local compatibility machinery for superseded internal revisions. The gitlink must remain within five published Common releases of the latest qualified Common release; normal dependency maintenance should advance directly to the newest reviewed release. This cadence rule does not weaken Common's own API/ABI discipline or justify moving product semantics into the shared library.

Every Common advancement is also a dependency review. The previous and proposed exact Common revisions must be compared for behavioural-contract changes, the canonical System Monitor qualification must run against the proposed pin, and product-local helpers must be audited against the current Common API surface. When semantics genuinely match and Common owns the stronger generic contract, System Monitor consumes Common and retires the local mechanism; monitoring policy, hardware interpretation and product presentation remain local.

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

## Clock resource integration

Canonical Unix instants and duration measurements remain System Monitor state. System Settings owns the selected clock ID, and Common owns the validated native resource ABI. Explicit clock faces render only through the installed `/System/Resources/Clocks` provider for that stable ID; a missing resource is unavailable rather than an invitation to reimplement its notation. The Linux presentation adapter caches one validated Common clock presentation handle for the selected ID, and serialises selection replacement and invocation through the policy mutex so a module cannot be unloaded during formatting. Changing the selection replaces that handle; installing system clock modules is visible on the next application start. Canonical duration projection remains independent of installed clock faces.

## Calendar resource integration

The active System Settings calendar ID is independent of its clock ID.
System Monitor resolves selected non-Gregorian dates through Common's
validated native Calendar Resource provider in /System/Resources/Calendars,
rather than implementing chronology locally. One lazy provider is retained
behind the temporal policy mutex; Common owns replacement by stable ID and
bounded retries when an installed module is absent. Unavailable custom
calendars never become an unlabelled Gregorian date. Gregorian remains the
normal OS locale; clock and calendar are composed independently without
changing canonical Unix timestamps or SI measurements. The local civil day,
not the UTC day, is passed into the selected chronology. Human-readable
timestamp callers share one bounded presentation capacity; its minimum
against Common's combined module maxima is checked at compile time, so a
valid resource is not silently lost to an undersized UI string.
Native Gregorian formatting follows the same all-or-unavailable commit rule:
libc renders into a bounded private buffer before a complete result is
published to its caller. In particular, a failed strftime cannot corrupt
the preinitialised unavailable presentation.

## Process selection and export ownership

The plain-C process export domain owns one authoritative membership predicate
for exact (PID, instance) identities. Both CSV serialization and GTK clipboard
projection consume it; the UI never reimplements selection semantics. Explicit
group members are independent of the currently highlighted representative PID,
and malformed or stale selections resolve to no matching rows. The GTK adapter
owns only application-state translation, user interaction and clipboard lifetime.
This prevents exports from disagreeing about selected processes after navigation,
group selection or PID reuse.

## Process recording ownership

CSV recording is deliberately asynchronous, but opening the destination is a
synchronous, exclusive operation. The recorder owns the atomic create-if-absent
boundary and rejects existing names or symlinks with EEXIST; it must never
truncate an earlier session. UI action policy owns the readable filename and
chooses a bounded numeric suffix on collision, retrying only EEXIST rather than
hiding permission, capacity or filesystem failures. Once created, the detached
recorder worker owns bounded queuing, ordered flush and its lifetime across
shutdown. A successful start therefore proves a distinct destination was
reserved without exposing an earlier recording to modification.

The recorder owns its eight-column CSV schema and validates the same schema
for asynchronous graph import. Numeric fields use Common's locale-independent
ASCII parsing (matching Common's fixed-ASCII writer), with a full-width
unsigned process identity rather than a scanner-local signed int. Invalid,
interrupted or overlong physical CSV rows are ignored in full; unsupported
headers or recordings containing no valid samples report an explicit error
instead of displaying an apparently successful but empty graph.

## Common

`src/infiltratr-common` is pinned by the parent repository gitlink to an exact Libraries commit. Generic mechanisms may move to Common; monitoring policy, hardware interpretation and product presentation remain local. Common owns validation of its versioned shared design records; native adapters retain only host-specific constraints and projection.

Hardware-specific rules live in [Hardware collection](HARDWARE.md). Security boundaries live in [Security](../.github/SECURITY.md).

## Native Linux filesystem ownership

On Linux, the user-launchable application has one authoritative executable path: `/Programs/system-monitor`. `/Programs` is the single application namespace for Infiltrator software regardless of whether a program is bundled with InfiltratorOS or installed independently. `/System` is reserved for operating-system infrastructure, and `/System/Commands` is reserved for genuine command helpers. System Monitor therefore creates the common native skeleton when necessary but does not create `/System/Programs` and does not install an executable or compatibility link under `/usr/bin`.

## Extended typography provisioning

System Monitor's Debian package copies a source-identical, release-pinned Common extended font bootstrap script. Its configure hook installs the 23-face catalogue and does not reintroduce the three retired 1.000 font binaries. The extended 23-face snapshot is source-pinned to the immutable InfiltratorOS inventory and verifies each TTF before populating the persistent machine cache; no application package owns those fonts. The Linux GTK theme selects S Text, S Text Cond, S Title and A Screen1 roles from that catalogue. The Windows native build obtains six verified roles from the pinned 23-face InfiltratorOS manifest, embeds them as process-private resources (S Text Regular/Bold, S Text Cond Regular, S Title Bold, A Title Cond and A Screen1 Cond) and verifies actual GDI family resolution before using them. The Linux build's three source-pinned 1.010 font faces are checked individually against Common's exact hash manifest without hidden network operations; the obsolete 1.000 archive is retired. This independent asset check remains separate from Windows extended preparation so build-tool ownership does not drift. Real-machine appearance/scale qualification remains separate from native compile and packaging checks.
