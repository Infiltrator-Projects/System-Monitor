# Validation

## Purpose

Validation defines the evidence required for System Monitor behaviour. Compilation, deterministic tests, runtime checks and physical-device evidence cover different parts of the product contract and are not treated as interchangeable.

## Automated evidence

The repository uses `.github/workflows/ci.yml` for verification and `.github/workflows/release.yml` for publication.

The verification estate covers accounting, hardware, Bluetooth, GPU, battery, filesystem, history, portability, Common integration, presentation and lifecycle behaviour. Related cases are grouped by subsystem so the maintained suite tests product contracts rather than preserving one-off test structure.

Automated checks include strict C compilation, CMake and Make builds, deterministic subsystem tests, sanitizer checks, 32-bit portability compilation, documentation validation, package construction and release-contract checks. Hosted verification also starts the GTK Overview under Xvfb so application construction and navigation execute against a display server.

Important boundaries include malformed or unavailable inputs, completed-snapshot publication, counter baselines, topology changes, bounded inventories, process identity, persistence ordering and explicit `N/A` availability semantics.

## Manual and environment-dependent evidence

Some behaviour can only be demonstrated on the relevant target environment. Physical GPU, NPU, battery, Bluetooth and thermal telemetry depend on the firmware, driver and kernel interfaces exposed by the machine. Privileged process actions require the affected operating-system path. Native Windows runtime behaviour requires Windows rather than cross-compilation alone.

Manual evidence must identify the environment actually exercised. Simulator, fixture and mocked-provider results remain useful deterministic evidence but are not described as physical-device proof.

## Release criterion

A release is publishable only when the exact current `main` revision passes the required Verify workflow. Release assets are produced from that revision. The version in `support/VERSION`, source tree, tag and published release must identify the same product state.

Published tags and release assets are immutable. Any later source change advances `support/VERSION` before publication.

## Regression coverage

Permanent tests are maintained around product contracts that are useful to protect over time, including:

- CPU, memory, disk, network, GPU and pressure accounting;
- first-sample, unavailable-sample and counter-reset semantics;
- transactional hardware and process inventories;
- stable device and process identity across topology changes;
- Overview completed-snapshot history and navigation;
- persisted history range and ordering rules;
- Linux and Windows presentation contracts;
- installer, package and release-asset construction; and
- Common integration and portability boundaries.

## Evidence boundaries

Passing hosted verification demonstrates the contracts exercised by the hosted environment. It does not manufacture telemetry that the target hardware does not expose. Unsupported or inaccessible metrics remain explicitly unavailable in the product.

Code and tests are authoritative for executable behaviour. This document defines the evidence model used to decide when that behaviour is ready to publish.
