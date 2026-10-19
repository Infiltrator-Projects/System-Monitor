<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Validation

Validation policy is defined by the executable verification suite together with [Architecture](ARCHITECTURE.md). `make check` is the authoritative regression and quality-verification entry point, and `.github/workflows/ci.yml` verifies the exact `main` revision used for release qualification. CMake intentionally validates the application build rather than carrying a second copy of the regression-test graph.

The source/repository audit protects architectural ownership as well as formatting: source manifests must be complete, shared source groups must remain subordinate to the canonical source inventory, the private-application-state allowlist must exactly match its deliberate direct consumers, platform details must not cross declared presentation contracts, and build/release owners are checked by their durable responsibilities rather than retired implementation spellings.

Compilation, deterministic tests, sanitizers, portability checks, runtime GTK smoke tests and physical-device evidence cover different contracts and are not interchangeable. The Linux Verify job compiles the actual GTK typography/chrome probe and the clean-host installer job executes it after installing the real package and font catalogue. The test checks Pango-resolved families, Day/Night header contrast, window-control state colours and Settings activation, not CSS source spelling. A separately staged canonical-name decimal clock resource from the exact Common gitlink also exercises the production module-based presentation path; the ordinary source smoke separately verifies absent-resource fail-closed behaviour. The authoritative documentation gate fails closed if Clang or Doxygen is unavailable. Unsupported or inaccessible telemetry remains unavailable rather than being inferred from an untested environment.

Dependency preparation is outside verification. Git checkouts must already contain the parent-pinned Common revision, normally through recursive checkout or an explicit `make common-bootstrap`; Make and CMake validate that revision without performing hidden network or source-tree mutation.

Linux release qualification builds the installable artifacts once. Its artifact bundle also carries the authoritative `support/VERSION` and verified typography assets needed by downstream qualification, so clean-host installer testing and release sealing consume the qualified installer and typography artifacts. The source-download installer fetches System Monitor and Common at their embedded exact commit identities; clean-host qualification exercises those real HTTPS downloads, compilation and package installation before publication.

The production native installer has one optimisation policy: CPU-native tuning, LTO and trained two-pass PGO. The bootstrap fixes that policy for every production invocation, so caller options cannot select a weaker build; `--system-package-mode` changes only the privilege handoff used by managed installation and does not change optimisation.

Publish consumes only the sealed Verify bundle. New releases are staged as mutable drafts, their remote assets are byte-compared with the qualified bundle before publication, and an interrupted draft can restore those expected assets in place from the same sealed bundle without deleting repository state. Published releases identify the exact tested revision and remain immutable; publication and idempotent reruns both byte-compare every remote binary with the qualified bundle before succeeding. If a qualified current `main` carries a version whose tag already identifies another commit, publication is a no-op only when the intervening changes are confined to release-workflow maintenance; product-source reuse of an existing version is an error and must advance `support/VERSION`.

Generated build trees, package outputs and qualification scratch state are local workspace state rather than repository material. The Windows release recipe keeps intermediate files under `build/`, emits the qualified executable directly under its versioned release name and removes its startup-smoke marker after qualification; the root `.gitignore` is a narrow final guard against accidentally staging supported build outputs.

Optimized qualification retains executable checks in the desktop/presentation regression paths. These checks report a failing expression and terminate unsuccessfully independently of `NDEBUG`; Release compilation cannot silently turn the suite into a successful no-op.

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
