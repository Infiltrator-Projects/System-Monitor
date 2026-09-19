# Design

## First-principles position

System Monitor starts from the behaviour the product must own, studies standards and mature implementations as evidence, and then chooses the strongest justified design rather than copying an existing product or preferring novelty for its own sake.

## Goals

- collect from authoritative native interfaces
- keep slow collection off the GTK main thread
- represent unavailable information explicitly rather than guessing
- keep reusable mechanics in Common while product/hardware policy stays local

## Non-goals and limits

The project does not promise that every metric exists on every machine, and it does not treat command-line utility output as a stable API when a stronger native interface is available.

## Dependency policy

C and C++ are preferred for first-party native implementation where they fit the problem. Platform frameworks and external libraries are used when their documented contract is the stronger engineering choice. A dependency must not silently become the source of product policy, and exact first-party dependencies are pinned where reproducibility requires it.

## Failure philosophy

Unsupported, unavailable or unverified states are represented explicitly. The project prefers a visible refusal or unavailable state to guessed success. Destructive or irreversible behaviour requires a stronger evidence bar than read-only behaviour.

## Decision quality

Design changes should identify the problem, alternatives, evidence, trade-offs and validation method. "Newer" is not a sufficient reason to replace a proven approach. A replacement should improve correctness, safety, performance, resilience, usability or maintainability without weakening an established contract.
