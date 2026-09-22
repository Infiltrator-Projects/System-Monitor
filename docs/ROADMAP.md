# Roadmap

System Monitor is feature-complete for its current Linux desktop product scope.
This document defines the maintenance and optional-expansion boundary; it is not
a backlog of work required before the application can be considered finished.

## Completion baseline

- preserve process, performance, hardware, service, user and filesystem views
- preserve one retained snapshot model across friendly and technical process views
- keep slow collectors and persistence away from the GTK main thread
- keep Linux-specific collection behind explicit platform contracts
- represent inaccessible or unsupported telemetry as unavailable rather than guessed
- preserve direct native collection where it is stronger than external helper programs
- consume Common only for genuinely generic mechanisms that are at least as strong as the local implementation
- keep proprietary font binaries outside redistributed source and packages while retaining the preferred-family/system-fallback presentation contract
- require the exact release revision to pass warnings-as-errors, sanitizers, portability, documentation and package gates

## Maintenance priorities

Correctness, security, supported-kernel/desktop regressions, hardware evidence and
UI responsiveness take priority over new features. Fixed defects should gain the
narrowest useful permanent regression. Tests completely subsumed by stronger
coverage should be consolidated instead of preserved as historical ceremony.

## Optional expansion

Additional devices, telemetry, native platform backends or presentation work are
optional expansion, not missing completion work. A new collector is admitted
only when its source semantics, availability rules, ownership and validation
strategy are explicit.

## Completion rule

System Monitor is complete when implementation, tests, packaging and maintained
documentation agree for the declared Linux product scope. Completion does not
mean every future kernel, driver or device is already known; it means there is
no known mandatory feature tranche left to implement.
