# Documentation

This directory is the canonical documentation entry point for System Monitor. The Infiltrator project family uses the same baseline document roles across repositories so readers can move between projects without relearning the structure.

## Canonical baseline

- [Architecture](ARCHITECTURE.md) — ownership, layers, dependencies and system boundaries.
- [Design](DESIGN.md) — first-principles goals, non-goals, trade-offs and failure philosophy.
- [Decisions](DECISIONS.md) — durable architectural decisions, alternatives and consequences.
- [Roadmap](ROADMAP.md) — current foundation, maintenance priorities and optional expansion.
- [Validation](VALIDATION.md) — automated, manual and environment-specific evidence boundaries.
- [Project README](../README.md) — product overview, capabilities, build/use entry point and engineering ethos.
- [Changelog](../CHANGELOG.md) — public product milestones and contract-relevant release history.
- [Contributing](../CONTRIBUTING.md) — development, ownership and verification rules.
- [Security](../SECURITY.md) — vulnerability scope, reporting and response policy.

## Documentation authority

The baseline files have distinct responsibilities and should not compete as alternate sources of truth. Architecture describes where behaviour belongs; Design explains why; Roadmap describes direction; Validation defines the evidence required for publication. Code and tests remain authoritative for executable behaviour, while release tags identify published source states.

Specialist documents may go deeper into one subsystem, protocol, platform or reference comparison. They should link back to the canonical baseline when a reader needs the wider project context.

## Specialist documentation

- [Hardware collection](HARDWARE.md) — native hardware identity, telemetry and availability contracts.
- [Portability](PORTABILITY.md) — supported build/runtime boundaries and platform-specific implementation notes.
- [Linux Mint System Monitor capability coverage](LINUX-MINT-SYSTEM-MONITOR-COVERAGE.md) — reference capability map and ownership placement across the Infiltrator suite.

## Maintenance rule

When a change moves an ownership boundary, support boundary, validation claim or major design decision, update the corresponding canonical document in the same change. Avoid copying the same status statement into several files; link to the authoritative document instead.
