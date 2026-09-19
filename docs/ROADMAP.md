# Roadmap

This is a direction document, not a dated promise. The released source and tests define what is actually supported.

## Current foundation

- maintain process, performance, hardware, service, user and filesystem views
- keep collectors portable behind platform contracts where practical
- qualify direct hardware/accounting behaviour through the support/tests suite

## Near-term priorities

- expand hardware coverage only with explicit availability and provenance
- continue reducing duplicated generic helpers in favour of stronger Common contracts
- preserve UI responsiveness while enriching expensive views asynchronously

## Longer-term direction

- add additional native platform backends where the architecture can preserve the same snapshot contracts
- broaden device telemetry without turning optional vendor libraries into mandatory runtime dependencies

## Admission rule

A proposed capability enters the roadmap only when its ownership is clear and there is a credible way to validate it. Features that require pretending uncertain behaviour is known do not qualify.

## Completion rule

An item is complete when implementation, tests, user-visible behaviour and maintained documentation agree. A checkbox or release number cannot substitute for missing evidence.
