<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Validation

Validation policy is defined by the executable verification suite together with [Architecture](ARCHITECTURE.md). `make check` is the authoritative regression and quality-verification entry point, and `.github/workflows/ci.yml` verifies the exact `main` revision used for release qualification. CMake intentionally validates the application build rather than carrying a second copy of the regression-test graph.

The source/repository audit protects architectural ownership as well as formatting: source manifests must be complete, shared source groups must remain subordinate to the canonical source inventory, the private-application-state allowlist must exactly match its deliberate direct consumers, platform details must not cross declared presentation contracts, and build/release owners are checked by their durable responsibilities rather than retired implementation spellings.

Compilation, deterministic tests, sanitizers, portability checks, runtime GTK smoke tests and physical-device evidence cover different contracts and are not interchangeable. Unsupported or inaccessible telemetry remains unavailable rather than being inferred from an untested environment.

Publish consumes only the sealed Verify bundle. New releases are staged as mutable drafts, their remote assets are byte-compared with the qualified bundle before publication, and an interrupted draft can restore those expected assets in place from the same sealed bundle without deleting repository state. Published releases identify the exact tested revision and remain immutable; publication and idempotent reruns both byte-compare every remote binary with the qualified bundle before succeeding. If a qualified current `main` carries a version whose tag already identifies another commit, publication is a no-op only when the intervening changes are confined to release-workflow maintenance; product-source reuse of an existing version is an error and must advance `support/VERSION`.

Generated build trees, package outputs and qualification scratch state are local workspace state rather than repository material. The Windows release recipe keeps intermediate files under `build/`, emits the qualified executable directly under its versioned release name and removes its startup-smoke marker after qualification; the root `.gitignore` is a narrow final guard against accidentally staging supported build outputs.
