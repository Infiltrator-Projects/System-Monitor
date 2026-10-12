# Changelog

## 1.0.182 - 2026-10-12

- Make `support/VERSION` the sole release-intent signal instead of coupling publication to a specially spelled commit subject.
- Treat an already-published version on an older commit as a clean no-op for workflow-only follow-up changes while retaining exact-current-main, sealed-manifest and immutable-asset verification for real releases.
- Carry forward the simplified single-producer release path: Verify builds and qualifies the binaries once, while Publish release only verifies and publishes those exact bytes; central APT publication remains independently owned by Infiltrator-Repository.

## 1.0.181 - 2026-10-12

- Publish only the exact release binaries produced and qualified by Verify; the release publisher no longer rebuilds product artifacts or owns central APT orchestration.
- Seal the three release assets with source SHA, version and SHA-256 identity so resumed publication can prove it is publishing the exact qualified bytes.

## 1.0.180 - 2026-10-12

- Make central APT release verification tolerate the hourly publisher when immediate dispatch credentials are unavailable, polling every 30 seconds for up to 90 minutes instead of failing after 20 minutes.

## 1.0.179 - 2026-10-12

- Route Compact Summary, process UI state and Processes-to-Details navigation through existing narrow application context owners instead of direct `LsmApp` layout access.
- Remove `summary_bar_adapter.c`, `process_ui_bridge.c` and `process_navigation.c` from the private application-state allowlist, reducing approved direct consumers from 30 to 27.
- Add a dedicated ThreadSanitizer workflow for the asynchronous process worker ownership and publication path.
- Force process-scan completion callbacks through a real GLib main-loop turn instead of allowing inline worker-thread dispatch, and retain a regression test that verifies the callback executes on the main-loop thread.
- Bound Compact Summary network-rate copies by the formatted source buffer as well as the larger destination, eliminating an aggressive-PGO `-Wstringop-overread` and its underlying over-read possibility.

## 1.0.178 - 2026-10-07

- Extract Technical Details layout persistence, the Preferences GTK editor and process-log plotting into dedicated owners to reduce change ripple through page/menu code.
- Make the extracted page-registry, keyboard and window-mechanics owners the live application wiring and remove the duplicate legacy shell event path.
- Isolate keyboard and page-registry access to private application layout behind a narrow presentation context.
- Split GTK theme construction out of `app_shell.c` so navigation/window chrome and theme policy no longer share one change hotspot.

## 1.0.177 - 2026-10-07

- Route main-loop cadence through a narrow runtime context instead of exposing the complete private application layout.
- Move page-aware keyboard command policy out of the central page registry into its own presentation owner.
- Remove transitive private-application-state exposure from the generic Performance construction contract.

## 1.0.176 - 2026-10-06

- Move shared process snapshot lifecycle and cross-page coordination into a dedicated process workspace owner.
- Localise remaining Processes-to-Details navigation and shared presentation state behind narrow process navigation and UI bridge APIs.
- Make the page registry authoritative for tab-entry refresh, search targeting and page-aware keyboard navigation.

## 1.0.175 - 2026-10-06

- Preserve Performance graph history across topology reconstruction using stable resource identity.
- Keep logical-CPU graph ownership aligned with the exact constructed graph count.
- Centralise shared process sampling, selection, actions, recording and foreground cadence classification.

## 1.0.173 - 2026-10-06

- Centralise top-level page construction, manual refresh ownership and active-page refresh policy.
- Replace three page-specific slow-refresh timers with one active-page timer.
- Size the native Windows Performance hit-test inventory from every shared resource class.

## 1.0.171 - 2026-10-06

- Unify Overview card borders and strengthen hero framing.
- Improve graph-scale, metadata and pressure-value readability while preserving resource accents.

## 1.0.170 - 2026-10-06

- Keep Overview primary statistics on one row when space permits.
- Maintain strict GTK compatibility declarations across supported build paths.

## 1.0.160 - 2026-10-03

- Install and verify all three MB Corpo faces with the Linux package and native installer.
- Apply shared Infiltrator typography and Day/Night icon-colour contracts.

## 1.0.151 - 2026-09-29

- Make central APT publication part of the release contract.
- Require the exact released version to appear in the repository catalogue before publication completes.

## 1.0.146 - 2026-09-28

- Move native Windows Performance collection off the Win32 message thread into a completed-snapshot worker.
- Apply explicit availability semantics across CPU, topology, GPU, battery, Bluetooth and Overview paths.
- Expand transactional topology and bounded-enumeration handling across Linux and Windows collectors.

## 1.0.130 - 2026-09-26

- Establish capability ownership across the Infiltrator suite.
- Keep process inspection, performance telemetry, mounted-filesystem observation and monitor-local preferences in System Monitor while retaining reusable mechanisms in Common.

## 1.0.96 - 2026-09-25

- Establish deterministic subsystem tests, strict compilation, portability checks, sanitizers, documentation checks and release-package verification.
- Define explicit availability and completed-snapshot contracts for monitor data.
