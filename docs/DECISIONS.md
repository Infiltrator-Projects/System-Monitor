# Decisions

This file records durable architectural choices for System Monitor.

## ADR-001 — Native interfaces before utility output

**Decision.** Collect directly from procfs, sysfs, ioctls, D-Bus or documented in-process interfaces wherever practical instead of parsing external monitoring commands.

**Rationale.** Utility formatting is a presentation contract controlled outside this project; native interfaces provide a clearer source of truth.

**Consequence.** Collectors may be more work to implement, but their semantics and failure states remain project-owned.

## ADR-002 — C and C++ are equal first-class choices

**Decision.** Choose C or C++ per component; neither language is preferred over the other by policy.

**Rationale.** Technical quality, control and clarity matter more than language fashion.

**Consequence.** The installed application may remain mostly C while developer or future components use C++ when it is the stronger solution.

## ADR-003 — Snapshots isolate collection from presentation

**Decision.** Linux/platform collectors produce plain data models/snapshots consumed by the GTK presentation layer.

**Rationale.** This keeps slow and platform-specific work away from UI policy and makes collectors testable without GTK.

**Consequence.** GTK objects and Linux handles do not belong in reusable model contracts.

## ADR-004 — Unavailable is a real state

**Decision.** Metrics that cannot be established safely are reported unavailable rather than estimated or invented.

**Rationale.** A plausible value is worse than an explicit unknown when the application is used for diagnosis.

**Consequence.** UI/model contracts must preserve availability separately from numeric value.

## ADR-005 — Shared generic mechanics converge on Common

**Decision.** Generic parsing, formatting, timing, path, allocation and durable-I/O behaviour should live in Common when its contract is at least as strong as the best local version.

**Rationale.** One reference-quality implementation reduces drift without accepting lowest-common-denominator abstractions.

**Consequence.** Strong local code may temporarily lead Common, but the intended end state is one shared implementation.