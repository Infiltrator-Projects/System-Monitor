# Validation

Validation policy is defined by the executable verification suite together with [Architecture](ARCHITECTURE.md). `make check` is the authoritative local verification entry point, and `.github/workflows/ci.yml` verifies the exact `main` revision used for release qualification.

Compilation, deterministic tests, sanitizers, portability checks, runtime GTK smoke tests and physical-device evidence cover different contracts and are not interchangeable. Unsupported or inaccessible telemetry remains unavailable rather than being inferred from an untested environment.

Published releases identify the exact tested revision and remain immutable.
