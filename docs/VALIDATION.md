# Validation

Validation defines the evidence required for System Monitor behaviour. Compilation, deterministic tests, runtime checks and physical-device evidence cover different contracts and are not interchangeable.

## Automated evidence

`.github/workflows/ci.yml` is the verification workflow. The maintained suite covers accounting, hardware, Bluetooth, GPU, battery, filesystems, history, portability, Common integration, presentation and lifecycle behaviour.

Automated gates include strict compilation, Make and CMake builds, deterministic subsystem tests, sanitizers, 32-bit portability checks, documentation validation, package construction and release-contract checks. Hosted verification also starts the GTK Overview under Xvfb so application construction and navigation execute against a display server.

Important regression boundaries include malformed or unavailable input, completed-snapshot publication, counter discontinuities, topology changes, bounded inventories, process identity, persistence ordering and explicit `N/A` semantics.

## Environment-dependent evidence

Physical GPU, NPU, battery, Bluetooth and thermal telemetry depends on the firmware, driver and kernel interfaces exposed by the target machine. Privileged process actions require the affected operating-system path. Native Windows runtime behaviour requires Windows rather than cross-compilation alone.

Manual evidence must identify the environment actually exercised. Fixtures, simulators and mocked providers are useful deterministic evidence but are not described as physical-device proof.

## Release readiness

A release is publishable only when the exact current `main` revision passes the required Verify workflow and the version in `support/VERSION`, source tree, tag and published release identify the same product state. Published tags and release assets are immutable.

Passing hosted verification proves only the contracts exercised by that environment. It does not manufacture telemetry unsupported by the target hardware; unsupported or inaccessible metrics remain unavailable.

Code and tests remain authoritative for executable behaviour.
