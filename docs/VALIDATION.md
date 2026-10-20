<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Validation

Validation policy is defined by the executable verification suite together with [Architecture](ARCHITECTURE.md). `make check` is the authoritative regression and quality-verification entry point, and `.github/workflows/ci.yml` verifies the exact `main` revision used for release qualification. CMake intentionally validates the application build rather than carrying a second copy of the regression-test graph.

The source/repository audit protects architectural ownership as well as formatting: source manifests must be complete, shared source groups must remain subordinate to the canonical source inventory, the private-application-state allowlist must exactly match its deliberate direct consumers, platform details must not cross declared presentation contracts, and build/release owners are checked by their durable responsibilities rather than retired implementation spellings.

Compilation, deterministic tests, sanitizers, portability checks, runtime GTK smoke tests and physical-device evidence cover different contracts and are not interchangeable. The Linux Verify job compiles the actual GTK typography/chrome probe and the clean-host installer job executes it after installing the real package and font catalogue. The test checks Pango-resolved families, Day/Night header contrast, window-control state colours and Settings activation, not CSS source spelling. Real canonical-name decimal clock and Hebrew calendar modules from the
pinned Common revision are installed during release qualification, and the
production presentation API is exercised with both Standard and decimal time
plus non-Gregorian dates. Missing installed calendar IDs must fail closed; they must not affect
time-only output. The regression tests cover civil-date rollover across a
deterministic UTC+10 zone and insufficient destination capacity without
truncated output. The application's shared timestamp capacity is statically
checked against Common's resource ABI maxima. The ordinary source smoke
independently exercises absent resources. Native Gregorian fallback and
System Settings' explicit Standard/Gregorian selection are both exercised
with undersized output buffers: failed libc formatting must leave "N/A"
intact and must not write beyond the supplied capacity. The authoritative
documentation gate fails closed if Clang or Doxygen is unavailable. Unsupported or inaccessible telemetry remains unavailable rather than being inferred from an untested environment.

Dependency preparation is outside verification. Git checkouts must already contain the parent-pinned Common revision, normally through recursive checkout or an explicit `make common-bootstrap`; Make and CMake validate that revision without performing hidden network or source-tree mutation.

Three offline build-time fonts must match the exact Common SHA-256 manifest; the retired, incompatible 1.000 archive is rejected. These build inputs are not installed as duplicate product-owned font resources, and no implicit network fetching occurs during ordinary verification.

Linux release qualification builds the installable artifacts once. Its artifact bundle also carries the authoritative `support/VERSION` and verified typography assets needed by downstream qualification, so clean-host installer testing and release sealing consume the qualified installer and typography artifacts. The source-download installer fetches System Monitor and Common at their embedded exact commit identities; clean-host qualification exercises those real HTTPS downloads, compilation and package installation before publication.

The production native installer has one optimisation policy: CPU-native tuning, LTO and trained two-pass PGO. The bootstrap fixes that policy for every production invocation, so caller options cannot select a weaker build; `--system-package-mode` changes only the privilege handoff used by managed installation and does not change optimisation.

Publish consumes only the sealed Verify bundle. New releases are staged as mutable drafts, their remote assets are byte-compared with the qualified bundle before publication, and an interrupted draft can restore those expected assets in place from the same sealed bundle without deleting repository state. Published releases identify the exact tested revision and remain immutable; publication and idempotent reruns both byte-compare every remote binary with the qualified bundle before succeeding. If a qualified current `main` carries a version whose tag already identifies another commit, publication is a no-op only when the intervening changes are confined to release-workflow maintenance; product-source reuse of an existing version is an error and must advance `support/VERSION`.

Generated build trees, package outputs and qualification scratch state are local workspace state rather than repository material. The Windows release recipe keeps intermediate files under `build/`, emits the qualified executable directly under its versioned release name and removes its startup-smoke marker after qualification; the root `.gitignore` is a narrow final guard against accidentally staging supported build outputs.

Optimized qualification retains executable checks in the desktop/presentation regression paths. These checks report a failing expression and terminate unsuccessfully independently of `NDEBUG`; Release compilation cannot silently turn the suite into a successful no-op.

## Windows process-worker restart

The native Windows backend probe exercises two complete worker
start/snapshot/stop cycles on the same runtime, verifying that a clean stop
does not poison the next worker with a stale stop request. It also checks
that an already-terminated worker handle is unavailable and that start()
reaps and replaces that terminated worker. The Win32 shell retries a failed
or terminated backend from its UI timer after a five-second monotonic delay
rather than retaining a permanent attempted flag. If cancellation
cannot finish within the bounded shutdown wait, the probe retains the
worker-owned allocation until process exit instead of risking use-after-free.

## Linux pressure telemetry integrity

The metrics regression rejects duplicate PSI keys and repeated `some`/`full`
records, malformed tokens and incomplete known records. It verifies that
well-formed unknown extension fields and tab-separated records still parse,
and that rejected snapshots leave both availability flags cleared. The parser
owns native PSI semantics; generic numeric parsing remains Common-owned.

## Cached storage metadata completeness

The storage regression requires newline-terminated device-manager records and complete recognized property values. An unterminated record, oversized unrelated record after a valid prefix, or filesystem type exceeding its representation limit must reject the cache file and clear the output label. The reader checks both read and close results, and short caller buffers must fail without publishing a partial label; exact-fit buffers succeed. Test fixtures also close their streams on write failure.

## Thermal warning/fault boundary

The metrics regression checks that independently reported sensor warning and critical limits never collapse to equal thresholds at the accepted 50 C critical-input floor, including critical-only and equal-pair inputs. It separately verifies the 150 C warning-only saturation case. The input acceptance floor is not an output floor for a derived warning; a valid critical point at that floor must remain distinguishable from the earlier warning point.

## Recorded-process graph fidelity

The GTK graph-scale smoke verifies the existing renderer's real maximum-axis
contract under Xvfb. A dual-series plot starts at 100% for ordinary memory
percentages but must expand to represent a single process using more than one
logical core; the scale is quantised in 100-point increments. This test is
part of `make check`, not an alternative production renderer or a
source-spelling assertion. Recording format and parser validation remain
owned by the asynchronous-worker suite.

## Linux mountinfo completeness

The storage regression validates optional propagation fields and kernel
escaped paths, then checks corrupt IDs, device numbers, incomplete or
unrecognised octal escapes, embedded NUL, unexpected trailing fields, missing
the required separator and truncated final records. Valid prefixes followed
by malformed records must fail the visitor contract; the filesystem
collector must discard the prefix and reset its outputs. A valid empty file
is separately distinguished from incomplete input and a failed file read.
Independent statvfs failures continue to invalidate only their capacity.

## Maintained documentation updates

The already-published version guard compares executable and release-affecting source, while allowing standalone README and `docs/**` maintenance without fabricating a new binary version. Workflow-only fixes also remain exempt. All application source, build configuration, packaging and other non-documentation changes still require an advanced `support/VERSION` after publication. These exclusions must stay narrow; a document or build file must not be used to smuggle altered release assets under an old version.

## Exact civil UTC offsets

The temporal-presentation smoke fixture supplies a synthetic POSIX zone with a
seconds-bearing historical UTC offset (+00:19:32). It compares System Monitor's
rendering against Common's clock formatter with the exact +1172-second offset
and checks that an explicitly requested timezone label retains +001932.
This protects the Linux native-time adapter and the installed Common clock-resource path against the minute-only truncation
of `strftime("%z")` without coupling the test to a distribution's tzdata
release. Common remains the authority for nonstandard clock semantics.

## XDG TryExec lookup semantics

The isolated startup regression checks the native executable-availability
predicate against a real executable reachable only through a trailing empty
PATH component and an entirely empty PATH (current working directory). It also verifies that a directory
with search permission is not accepted as TryExec, without executing either
candidate. The backend owns this interpretation; GTK receives only the
resulting enabled state.

## XDG startup precedence and write containment

The startup backend smoke uses isolated XDG user/system autostart directories.
It verifies that a malformed higher-priority desktop entry masks a valid
system entry, a valid user Hidden override remains authoritative, an absent
user override reveals the system entry, and disabling a system entry writes
only a user-owned override. Invalid desktop IDs and mismatched source/origin
basenames must not result in writes or create paths outside autostart. The
regression runs as part of the canonical Make verification graph.

## XDG application catalogue precedence

The isolated catalogue regression verifies user-versus-system desktop-file
basename precedence independently of executable names. Hidden, malformed and
non-application user entries must suppress valid lower-priority files; a
user entry changing Exec must not retain the superseded system executable
alias. Ordinary executable, window-class and cgroup lookups must continue to
resolve only effective entries. Discovery never executes desktop commands.

## Unified process export selection

The selected-process regression exercises the same exported membership and
count functions consumed by both CSV and clipboard projection. It covers an
explicit group without a highlighted representative PID, a stale group member,
single-process identity matching, missing instance identity and incomplete
process/selection arrays. Empty or malformed selection never becomes a
plausible successful export; no GTK code owns another membership predicate.

## Process recording collision safety

The asynchronous recording smoke creates a previous CSV fixture, attempts to
open that exact destination through the production recorder, requires EEXIST,
and byte-verifies that its original content survives. The same suite then
opens a fresh destination and verifies queued samples drain in order. Filename
suffix selection remains with the GTK process-action owner and is bounded;
only an existing-name collision is retried. Permission and I/O failures are
reported rather than reclassified as collisions.

The same production asynchronous-worker smoke now verifies the recorder's
real emitted header and sample rows using the graph import parser. It checks
a PID exceeding 32-bit signed range, CPU usage exceeding one core, ASCII
decimal conversion independent of libc scanf locale, LF/CRLF records and
failure-atomic rejection of missing fields, excess columns, unterminated
rows, comma decimals, negative/NaN percentages, out-of-range memory and
integer overflow. The importer separately discards oversized physical
lines in full, refusing to reinterpret their fragments as new samples. Its
schema regression also rejects malformed local timestamps, impossible
Gregorian days, invalid clock/offset components and trailing timestamp data,
while accepting leap days and seconds-bearing historical UTC offsets.

## Background scan coalescing and upgrade recovery

The `scanner-coalescing-smoke` fixture blocks a synthetic backend scan, changes
requested enrichment through the public scanner API and proves that a request
already satisfied by in-flight work invalidates stale queued work. It also
verifies that the latest distinct request survives coalescing. The test uses
the production scanner implementation without depending on procfs timing;
live-backend and sanitizer qualifications remain independent.

Native installation delegates package replacement to a single `dpkg --install`
operation. Removing the working package first is prohibited because it creates
an unnecessary failure window. This does not imply fully transactional rollback
of failed Debian maintainer scripts: their recovery remains dpkg-owned.
