# Changelog

## 1.0.173 - 2026-10-06

- Centralise GTK top-level page construction, manual refresh ownership and active-page periodic refresh policy in one registry.
- Replace three page-specific slow-refresh timers with one active-page timer, reducing cross-module cadence coupling and duplicate scheduler state.
- Size the native Windows Performance hit-test inventory from every shared resource class so Bluetooth, battery and NPU expansion cannot outgrow a stale local capacity.

## 1.0.172 - 2026-10-06

- Publish a concise, current documentation baseline for architecture, validation, roadmap and capability ownership.
- Keep public release notes focused on supported product behaviour, release contracts and user-visible capabilities.

## 1.0.171 - 2026-10-06

- Unify Overview card borders and strengthen the hero framing.
- Improve graph-scale, metadata and pressure-value readability while preserving the established resource colour accents.

## 1.0.170 - 2026-10-06

- Keep Overview primary statistics on one row when space permits.
- Maintain strict GTK compatibility declarations across the supported build paths.

## 1.0.160 - 2026-10-03

- Install and verify all three MB Corpo faces with the Linux package and hardware-native installer.
- Apply the shared Infiltrator typography and Day/Night icon-colour contracts across the application shell.

## 1.0.151 - 2026-09-29

- Make central APT publication part of the release contract.
- Require the exact released System Monitor version to become visible in the repository catalogue before publication completes.

## 1.0.146 - 2026-09-28

- Move native Windows Performance collection off the Win32 message thread into a completed-snapshot worker.
- Apply explicit availability semantics consistently across CPU, topology, GPU, battery, Bluetooth and Overview presentation paths.
- Expand transactional topology and bounded-enumeration handling across Linux and Windows collectors.

## 1.0.130 - 2026-09-26

- Establish the maintained capability-ownership model across the Infiltrator suite.
- Place process inspection, performance telemetry, mounted-filesystem observation and monitor-local preferences within System Monitor while retaining reusable mechanisms in Common.

## 1.0.96 - 2026-09-25

- Establish the current validation model for deterministic subsystem tests, strict compilation, portability checks, sanitizers, documentation checks and release-package verification.
- Define explicit availability and completed-snapshot contracts for monitor data rather than presenting unsupported values as measured data.

Earlier numbered releases established the core Linux process, performance, hardware, history, service, user and filesystem surfaces that remain part of the current product.
