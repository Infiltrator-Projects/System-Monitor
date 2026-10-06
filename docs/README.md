<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Documentation

System Monitor keeps a deliberately small maintained documentation set. Code and tests remain authoritative for executable behaviour; these documents record durable engineering contracts.

System Monitor is an original clean-sheet implementation designed and written from the ground up for this project. It is not forked, copied, translated, adapted, ported or derived from another system-monitoring application.

## Canonical documents

- [Architecture](ARCHITECTURE.md) — ownership, data flow, concurrency, failure and security boundaries.
- [Decisions](DECISIONS.md) — durable architectural decisions and rationale.
- [Hardware collection](HARDWARE.md) — hardware evidence, telemetry, units and availability rules.
- [Portability](PORTABILITY.md) — platform, ABI and representation boundaries.
- [Validation](VALIDATION.md) — automated/manual evidence and release criteria.
- [Project README](../README.md) — product overview, provenance, capabilities, build and release entry point.
- [Changelog](../CHANGELOG.md) — public product milestones.
- [Contributing](../CONTRIBUTING.md) — contribution and repository rules.
- [Security](../SECURITY.md) — vulnerability scope and reporting.

## Maintenance rule

Each durable rule should have one authoritative home. Link to that rule instead of copying its explanation into multiple files. Temporary design programmes, one-off comparison audits and obsolete roadmaps do not belong in the maintained documentation set once their useful conclusions are represented by the current code, tests or canonical contracts.
