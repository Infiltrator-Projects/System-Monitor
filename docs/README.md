# System Monitor documentation

The root [README](../README.md) introduces the application. [InfiltratorOS product-family and UI contract](https://github.com/Infiltrator-Projects/InfiltratorOS/blob/main/docs/PRODUCT-FAMILY.md) governs shared application identity, semantic appearance, MB Corpo roles and independent installation.

## Maintained documents

- [Engineering philosophy](PHILOSOPHY.md) — this project's obligations under [Libraries/Common philosophy](https://github.com/Infiltrator-Projects/Libraries/blob/main/docs/PHILOSOPHY.md).
- [Architecture](ARCHITECTURE.md) — ownership, concurrency, platform boundaries and runtime semantics.
- [Hardware](HARDWARE.md) — hardware interfaces, units and provenance.
- [Validation](VALIDATION.md) — tests and release qualification.
- [Changelog](CHANGELOG.md) — recorded product changes.
- [Contributing](../.github/CONTRIBUTING.md) — development and verification discipline.
- [Security](../.github/SECURITY.md) — responsible reporting and trust boundaries.

## Documentation ownership

Architecture describes current ownership; hardware documents device evidence; validation establishes what is actually tested. Domain and historical information must not masquerade as active executable truth. Changes that alter an invariant or public behaviour update the owning document as well as its tests.
