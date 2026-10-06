# Roadmap

System Monitor is feature-complete for its current Linux desktop product scope. This document defines the maintenance and optional-expansion boundary; it is not a backlog of work required before the application can be considered finished.

## Completion baseline

- preserve process, performance, hardware, service, user and filesystem views;
- preserve one retained snapshot model across friendly and technical process views;
- keep slow collectors and persistence away from the GTK main thread;
- keep Linux-specific collection behind explicit platform contracts;
- represent inaccessible or unsupported telemetry as unavailable rather than guessed;
- preserve direct native collection where it is stronger than external helper programs;
- consume Common only for genuinely generic mechanisms that are at least as strong as the local implementation;
- keep packaging, typography and release behaviour aligned with the maintained Infiltrator contracts; and
- require the exact release revision to pass the project verification and package gates.

## Maintenance priorities

Correctness, security, supported-kernel and desktop compatibility, hardware evidence and UI responsiveness take priority over feature count. Tests should remain focused on durable product contracts and may be consolidated when stronger coverage fully subsumes older cases.

## Product-coherence gate

Optional work is filtered through the product-admission policy in [Design](DESIGN.md) and ADR-006 in [Decisions](DECISIONS.md). Competitor features are evidence and idea sources rather than a parity backlog. A new capability should solve an existing monitoring or diagnostic problem, strengthen an established product quality and fit the current navigation and presentation model.

Presentation work follows the same rule. Summary and monitoring surfaces should favour visual hierarchy, graphs, compact state/value treatment and meaningful grouping when those communicate faster than prose. Dense textual tables remain appropriate where the surface is deliberately technical.

## Optional expansion

Additional devices, telemetry providers, native platform backends and presentation work are optional expansion. A new collector is admitted only when its source semantics, availability rules, ownership and validation strategy are explicit.

The Windows GUI remains a native preview with live Performance, Processes and Overview paths while other product pages remain explicit placeholders until their corresponding native backends are implemented.

## Completion rule

System Monitor is complete when implementation, tests, packaging and maintained documentation agree for the declared Linux product scope. Future kernels, drivers and devices can extend the supported evidence surface without changing that product boundary.
