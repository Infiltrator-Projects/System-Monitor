<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Validation

Validation policy is defined by the executable verification suite together with [Architecture](ARCHITECTURE.md). `make check` is the authoritative regression and quality-verification entry point, and `.github/workflows/ci.yml` verifies the exact `main` revision used for release qualification. CMake intentionally validates the application build rather than carrying a second copy of the regression-test graph.

The source/repository audit protects architectural ownership as well as formatting: source manifests must be complete, shared source groups must remain subordinate to the canonical source inventory, the private-application-state allowlist must exactly match its deliberate direct consumers, platform details must not cross declared presentation contracts, and build/release owners are checked by their durable responsibilities rather than retired implementation spellings.

Compilation, deterministic tests, sanitizers, portability checks, runtime GTK smoke tests and physical-device evidence cover different contracts and are not interchangeable. Unsupported or inaccessible telemetry remains unavailable rather than being inferred from an untested environment.

Published releases identify the exact tested revision and remain immutable.
