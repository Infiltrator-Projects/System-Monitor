# Validation

## Purpose

Validation distinguishes implemented behaviour from behaviour that has actually been demonstrated. A build proves compilation; it does not by itself prove runtime, hardware or integration correctness.

## Automated evidence

The repository currently uses:

- .github/workflows/ci.yml
- .github/workflows/release.yml

`support/tests` contains accounting, hardware, Bluetooth, GPU, battery, filesystem, history, portability and Common-integration regression coverage. The estate is intentionally bounded to 18 physical `*_smoke.c` sources/executables: seven coherent subsystem suites plus eleven specialised integration/lifecycle fixtures. Related regression cases live inside their owning subsystem source rather than as separate one-case translation units. Make is the canonical executed suite. CMake retains target registration and local CTest support, while CI builds only the application through CMake so the same smoke programs are not executed twice.

Automated checks should cover ordinary behaviour, important boundaries, malformed/error cases and release/package contracts appropriate to the project.

Generated Doxygen API validation is intentionally scoped to System Monitor-owned source and header-defined API types. Private implementation compounds defined only inside `.c` files are not promoted into the generated API surface; Clang documentation syntax checks still cover the complete owned source set.

## Manual and environment-dependent evidence

Hardware-specific telemetry and privileged actions still require real-device validation because synthetic CI cannot prove a particular firmware, driver or kernel exposes a metric correctly.

Manual evidence supplements automation and must be described at the level actually observed. A simulator, fixture or mocked provider must not be described as physical-device proof.

## Release criterion

The exact revision intended for release must pass the required automated gates. Release assets must be derived from that revision, and documentation must not advertise known-failing or merely planned behaviour as supported.

## Regression rule

Every fixed defect should gain the narrowest useful permanent regression check when reproducible. Tests are part of the product contract rather than disposable scaffolding.
