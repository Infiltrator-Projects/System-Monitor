# Design

## Clean-sheet position

System Monitor is an original clean-sheet implementation designed and written from the ground up for this project. Its application source was not forked, copied, translated, adapted, ported or derived from another system-monitoring application. Other monitoring applications and their source code are not implementation or design authorities for this project.

## First-principles position

System Monitor starts from the behaviour the product must own, project requirements, documented standards and authoritative operating-system or hardware interfaces. It then chooses the strongest justified design from those contracts rather than inheriting another application's architecture, code, assumptions or implementation behaviour.

A later comparison with another product may be used only as a post-implementation audit of user-visible capability coverage. Such comparison does not define architecture, source structure, algorithms, implementation technique or product ancestry.

## Goals

- collect from authoritative native interfaces
- keep slow collection off the GTK main thread
- represent unavailable information explicitly rather than guessing
- keep reusable mechanics in Common while product/hardware policy stays local

## Product identity and feature admission

System Monitor is a coherent diagnostic instrument, not a catalogue of every system utility feature that could be implemented. Its product question is: **what is the computer doing, where are the resources going, and what trustworthy information does the user need to understand or act on that state?**

New capabilities are admitted from System Monitor's own monitoring and diagnostic requirements. They must fit System Monitor's architecture, interaction model and presentation instead of being imported from another product. Post-implementation comparison may reveal a user-visible capability worth considering, but the resulting implementation must be independently designed from project requirements and authoritative interfaces rather than copied or adapted from the compared product.

A feature or expansion should have a clear connection to at least one established product quality: measurement correctness, diagnostic usefulness, responsiveness, resource efficiency, reliability, platform completeness, interaction clarity or maintainability. Integration matters more than count: strengthening the relationship between measurements, navigation and actions is preferable to accumulating independent panels or tools.

Presentation follows the same rule. The interface should communicate state through hierarchy, graphs, compact status/value treatments, colour and spatial grouping where those forms are clearer than prose. Text remains appropriate for names, exact values, technical tables, explanations and exceptional states, but ordinary monitoring surfaces should not narrate information that can be understood more quickly from the visual structure itself. Technical density is intentional on explicitly technical surfaces such as Details; it should not leak into approachable summary surfaces without a diagnostic reason.

## Non-goals and limits

The project does not promise that every metric exists on every machine, and it does not treat command-line utility output as a stable API when a stronger native interface is available.

## Language and dependency policy

C and C++ are preferred for first-party native implementation where they fit the problem. The project does not treat C, procedural C++ or object-oriented C++ as a hierarchy of better and worse languages or styles. They are different expressive tools: direct C may best match a native ABI or simple state transform; C++ may better express ownership, invariants or generic algorithms; OO C++ may be appropriate when encapsulated state or real polymorphism exists. The design should use whichever form makes the actual problem clearest without adding abstraction for its own sake.

Platform frameworks and external libraries are used when their documented contract is the stronger engineering choice. A dependency must not silently become the source of product policy, and exact first-party dependencies are pinned where reproducibility requires it. Using a documented platform/library contract is not source ancestry: System Monitor's product implementation remains project-owned and independently designed.

## Failure philosophy

Unsupported, unavailable or unverified states are represented explicitly. The project prefers a visible refusal or unavailable state to guessed success. Destructive or irreversible behaviour requires a stronger evidence bar than read-only behaviour.

## Decision quality

Design changes should identify the problem, alternatives, evidence, trade-offs and validation method. "Newer" is not a sufficient reason to replace a proven approach. A replacement should improve correctness, safety, performance, resilience, usability or maintainability without weakening an established contract.
