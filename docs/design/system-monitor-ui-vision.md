<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# System Monitor UI Vision

## Provenance

This UI programme is part of System Monitor's original clean-sheet design. It is not a recreation, port or adaptation of another system monitor's interface. The implementation and product hierarchy remain System Monitor-owned and are built on the project's own monitoring model, presentation contracts and first-party Infiltrator visual language.

## Purpose

This document records the current visual north star for the System Monitor redesign.

## Core intent

System Monitor should feel graphical, premium, colourful, modern, GUI-first, welcoming and visually rich. It should not feel text-heavy, austere, purely administrative, CLI-minded or like an early-alpha control utility.

The intended spirit is direct, object-oriented desktop interaction translated into the modern Infiltrator product language: strong colour, obvious objects, direct manipulation, clear grouping and a coherent visual identity.

## Project mockup

See [image.png](image.png).

The image is this project's own visual direction, not an external product specification. It establishes the product hierarchy: a strong graphical Overview, persistent category navigation, large live resource cards, colour-coded graphs and status, and compact technical detail beneath the primary visual signal.

## Design principles

- Use strong visual hierarchy.
- Prefer cards and panels to flat text-heavy regions.
- Prefer graphical previews, trends, state chips, meters and diagrams where they communicate faster than prose.
- Keep category navigation obvious and stable.
- Use colour semantically and consistently, not decoratively.
- Keep exact values available without making every surface read like a report.
- Preserve native usability, accessibility and keyboard behaviour.
- Preserve diagnostic depth: deliberately technical pages such as Details may remain dense where density serves the task.
- Reuse existing monitoring data and history; presentation must not create duplicate collectors or alternative truth paths.
- Make changes incrementally so every release remains usable and testable.

## Dashboard hierarchy

The Overview is the visual home of System Monitor. CPU, memory and GPU are the primary live cards. Disk, network, battery/temperature and system pressure form the next diagnostic layer. Top processes and overall health should read as graphical summaries rather than paragraphs.

The long-term shell direction is a persistent graphical category rail rather than a row of equally weighted text tabs. That shell change is intentionally staged because persisted navigation identities, first-paint behaviour and accessibility must remain correct during the transition.

## First implementation pass

Version 1.0.99 began the transition by strengthening the existing Overview rather than replacing working architecture. It adds a visual hero, live-state treatment, stronger metric hierarchy, resource-coloured card accents, larger values, visual-priority card ordering and a cleaner top-process summary. Fresh profiles land on Overview after the intentionally fast Performance first paint.

By 1.0.108 the implemented Overview includes the icon-led primary rail, a single-viewport tiered dashboard with equal-width cards within each row, resource icons integrated into the live plots, visibly multicolour native Cairo radial gauges for CPU/Memory/GPU, shared panel surfaces, richer gradient/grid history charts and graphical process activity rows. These are implementation steps toward the project mockup, not a claim that the visual programme is complete.

Subsequent polish should continue toward the project mockup without adding features merely to fill it.
