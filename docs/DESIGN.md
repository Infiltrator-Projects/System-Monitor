# Design

## First-principles position

System Monitor starts from the behaviour the product must own, studies standards and mature implementations as evidence, and then chooses the strongest justified design rather than copying an existing product or preferring novelty for its own sake.

## Goals

- collect from authoritative native interfaces
- keep slow collection off the GTK main thread
- represent unavailable information explicitly rather than guessing
- keep reusable mechanics in Common while product/hardware policy stays local

## Product identity and feature admission

System Monitor is a coherent diagnostic instrument, not a catalogue of every system utility feature that could be implemented. Its product question is: **what is the computer doing, where are the resources going, and what trustworthy information does the user need to understand or act on that state?**

Competitor applications, operating-system tools and research implementations are evidence and idea sources, not feature checklists. A particularly strong feature may be adopted when it materially improves System Monitor's existing purpose, but the implementation must fit System Monitor's architecture, interaction model and presentation instead of arriving as an isolated imitation.

A feature or expansion should have a clear connection to at least one established product quality: measurement correctness, diagnostic usefulness, responsiveness, resource efficiency, reliability, platform completeness, interaction clarity or maintainability. Integration matters more than count: strengthening the relationship between measurements, navigation and actions is preferable to accumulating independent panels or tools.

Presentation follows the same rule. The interface should communicate state through hierarchy, graphs, compact status/value treatments, colour and spatial grouping where those forms are clearer than prose. Text remains appropriate for names, exact values, technical tables, explanations and exceptional states, but ordinary monitoring surfaces should not narrate information that can be understood more quickly from the visual structure itself. Technical density is intentional on explicitly technical surfaces such as Details; it should not leak into approachable summary surfaces without a diagnostic reason.

## Non-goals and limits

The project does not promise that every metric exists on every machine, and it does not treat command-line utility output as a stable API when a stronger native interface is available.

## Language and dependency policy

C and C++ are preferred for first-party native implementation where they fit the problem. The project does not treat C, procedural C++ or object-oriented C++ as a hierarchy of better and worse languages or styles. They are different expressive tools: direct C may best match a native ABI or simple state transform; C++ may better express ownership, invariants or generic algorithms; OO C++ may be appropriate when encapsulated state or real polymorphism exists. The design should use whichever form makes the actual problem clearest without adding abstraction for its own sake.

Platform frameworks and external libraries are used when their documented contract is the stronger engineering choice. A dependency must not silently become the source of product policy, and exact first-party dependencies are pinned where reproducibility requires it.

## Failure philosophy

Unsupported, unavailable or unverified states are represented explicitly. The project prefers a visible refusal or unavailable state to guessed success. Destructive or irreversible behaviour requires a stronger evidence bar than read-only behaviour.

## Decision quality

Design changes should identify the problem, alternatives, evidence, trade-offs and validation method. "Newer" is not a sufficient reason to replace a proven approach. A replacement should improve correctness, safety, performance, resilience, usability or maintainability without weakening an established contract.
