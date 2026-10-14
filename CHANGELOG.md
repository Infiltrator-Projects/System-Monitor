<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Changelog

## 1.0.198 - 2026-10-14

- Advance the exact Infiltratr Common gitlink to published 1.19.42 and consume Common's new shared-design validators instead of duplicating palette and metrics ABI-prefix rules in the Win32 adapter.
- Keep native Win32 integer-range, host-theme detection, registry persistence and palette projection product-owned while making Common the single authority for validity of its shared design records.
- Record the pre-release dependency cadence: System Monitor follows the current Common contract directly, carries no compatibility shims solely for superseded internal Common revisions, and must remain within five published Common releases of the latest qualified release.

## 1.0.197 - 2026-10-13

- Make native Windows process-worker availability reflect actual thread liveness, so backend creation failure cannot leave the UI indefinitely reporting a background refresh after its worker has already exited.
- Extend the native Windows backend probe to enforce the terminated-worker unavailable invariant against the exact live process-runtime implementation used by the product.
- Harden same-version release recovery: when an existing version tag points at another commit, Publish now permits a no-op only for workflow-only changes and fails closed on product-source reuse or an ambiguous GitHub comparison.

## 1.0.196 - 2026-10-13

- Make first publication prove its own postcondition: Publish now downloads the newly created GitHub release and byte-compares all three remote assets with the sealed Verify bundle in the same run, reusing the same verifier used by idempotent reruns.
- Reduce Windows release choreography by keeping generated headers and backend probes under `build/`, compiling the GUI directly to its final version-qualified artifact name, and deterministically removing the startup-smoke marker even on failure.
- Add a deliberately narrow root `.gitignore` for supported build trees, package outputs and legacy Windows scratch names so ordinary qualification work cannot accidentally become repository state.
- Document the strengthened release post-verification and generated-workspace ownership contracts without changing the deliberate Make/CMake/Windows/Common architectural boundaries.

## 1.0.195 - 2026-10-13

- Advance the exact Infiltratr Common gitlink from published 1.19.35 to published 1.19.41, inheriting strengthened POSIX and temporal contracts while keeping monitoring policy, hardware interpretation and product presentation locally owned.
- Make `support/app-internal-allowlist.txt` an exact private-state exception set: every direct `app_internal.h` consumer must be listed and every listed exception must still be a consumer, so completed decoupling cannot leave latent permission to reintroduce private-state coupling.
- Reduce architecture-audit brittleness by replacing retired implementation-spelling sentinels with direct ownership and integrity invariants for Common private sources, Windows source membership and byte-for-byte immutable release verification.
- Preserve deliberate build/release separation after the forensic re-audit: Make remains the regression, packaging and release-quality authority; CMake remains the Linux application-build authority; Windows has one native release recipe; Publish consumes only the sealed qualified artifacts; deterministic source archives remain independently owned by `make dist`.

## 1.0.194 - 2026-10-13

- Integrate the governing engineering philosophy into the maintained-document, generated-reference and licensing contracts instead of leaving it outside repository validation.
- Make the source-style audit keep one maintained-Markdown inventory for approval, required-file and SPDX checks, removing duplicated document-policy lists that could drift independently.
- Remove duplicate `LSM_NAME_LEN` and `LSM_PATH_LEN` ownership from `monitor_types.h`; the lower platform-neutral process model now owns the shared model buffer capacities consumed by the monitor model.

## 1.0.193 - 2026-10-13

- Move the GTK Preferences editor off the complete private `LsmApp` layout by exposing only an editable preference-value snapshot and one preferences-domain commit operation.
- Centralise accepted preference mutation, persistence, dependent control synchronisation and affected-page refreshes so the dialog no longer duplicates cross-subsystem ownership knowledge.
- Reduce approved direct `app_internal.h` consumers from 25 to 24 while preserving the existing preference semantics and refresh sequence.

## 1.0.192 - 2026-10-13

- Remove the obsolete, unwired Win32 process-worker implementation and its duplicate public contract now that `windows_process_runtime.c` owns the live Windows process-worker lifecycle.
- Make canonical Verify run on a clean Ubuntu 24.04 host with declared dependencies instead of depending on persistent self-hosted runner state.
- Restore release-history continuity by documenting the previously published 1.0.191 workflow-hardening release.

## 1.0.191 - 2026-10-13

- Move pinned GitHub artifact upload/download actions to their Node 24-compatible major versions while preserving the sealed single-producer release path.
- Make the release-ownership audit semantic with respect to pinned artifact-action revisions instead of coupling validation to one version spelling.

## 1.0.190 - 2026-10-13

- Remove obsolete include-order-dependent page-registry macro aliases now that the composition root calls the registry explicitly.
- Eliminate the implementation-only preprocessor switch that existed solely to suppress those aliases, leaving page-registry and shell ownership visible in ordinary function calls.

## 1.0.189 - 2026-10-13

- Integrate ThreadSanitizer into the clean Ubuntu Linux release qualification so asynchronous process-worker races block the canonical Verify workflow and therefore block publication.
- Isolate the ThreadSanitizer build under its own build directory so sanitizer instrumentation cannot leak into release artifacts.
- Remove the redundant standalone ThreadSanitizer workflow and its duplicate checkout/dependency orchestration.

## 1.0.188 - 2026-10-13

- Make the parent repository gitlink the single Infiltratr Common revision authority for both Make and CMake, removing duplicate tag, version and commit pins and the source-archive network fallback.
- Centralise metrics, storage, process and accelerator test source membership so ordinary, sanitizer and coverage builds cannot drift independently.
- Remove the self-hosted CI Git package bootstrap and validate the runner toolchain contract before checkout instead.
- Make `support/VERSION` the only maintained source-version value by removing duplicate current-version strings from README and CI.

## 1.0.187 - 2026-10-13

- Remove the unused duplicate CMake smoke-test and developer-tool graph so `make check` is the single regression and quality-test authority while CMake owns only the Linux application build.
- Move native Windows release compilation and qualification out of workflow YAML into `support/windows/build-release.ps1`, centralising compiler warnings, link libraries, Common construction, source membership and backend probes.
- Replace brittle source-audit checks for obsolete CI spelling with semantic build and release ownership checks, eliminating legacy marker comments that existed only to satisfy the checker.
- Make CMake verify Infiltratr Common against the parent repository gitlink instead of using its own pin for application-build correctness, preventing a stale configured submodule from being accepted.
- Harden Technical Details layout persistence by validating saved column order as a complete permutation, deriving width bounds from column metadata and centralising identity-order reset logic.

## 1.0.186 - 2026-10-13

- Add explicit regression coverage for the Overview page-scroll preference boundary so the tab-count-derived parser remains protected at the current highest tab index.

## 1.0.185 - 2026-10-13

- Make persisted page-scroll parsing use a numeric tab index bounded by the actual tab count rather than a fixed single-digit range.
- Reuse the canonical bundled-font verifier for Windows instead of downloading and independently parsing the font metadata during the Windows build.
- Generate Windows font role aliases from the already verified canonical font assets so there is only one font-selection authority.

## 1.0.184 - 2026-10-13

- Make Publish release consume the sealed Verify bundle as its release contract without a second source checkout or product rebuild.
- Replace release-workflow implementation-string auditing with durable sealed-bundle, source-identity and no-rebuild contract checks.
- Treat superseded verified commits and already-published versions as clean no-ops while preserving immutable release asset verification.

## 1.0.183 - 2026-10-13

- Route exact-file process-owner search through the existing presentation-window contract instead of exposing the complete private `LsmApp` layout.
- Route process-log plotting through the presentation and runtime context contracts for window and shutdown state.
- Remove `process_file_users.c` and `process_log_plot.c` from the private application-state allowlist, reducing approved direct consumers from 27 to 25.

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