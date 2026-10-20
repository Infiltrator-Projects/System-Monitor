<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Changelog

## 1.0.240 - 2026-10-20

- Restore the Win32 process worker's start/stop/restart lifecycle: a clean stop no longer leaves a stale stop request that immediately terminates its replacement.
- Treat a terminated worker's retained thread handle as unavailable; reap its resources before attempting a fresh backend startup, without restarting an abandoned worker.
- Extend the native Windows backend probe with two complete snapshot/stop cycles, terminated-worker recovery, and document ownership and recovery semantics.
- Retry failed or exited Windows process workers from the shell after a bounded five-second delay rather than permanently suppressing recovery after the first attempt.
- Reject unterminated, malformed and wrong-type Win32 registry strings rather than exposing uninitialised GPU driver metadata; exercise the production reader against volatile registry fixtures.

## 1.0.239 - 2026-10-20

- Reject duplicate Linux PSI measurement keys and repeated `some` or `full` records rather than silently replacing earlier kernel telemetry.
- Reject malformed and ambiguous PSI tokens while preserving well-formed unknown extension fields; recognize tab-separated records and fail atomically on incomplete known records.
- Expand the existing metrics regression with malformed, duplicate, extension and failure-atomic PSI fixtures.

## 1.0.238 - 2026-10-20

- Resolve Linux XDG application catalogue overrides by desktop-file basename before parsing; hidden, malformed and non-application higher-priority entries now suppress lower-priority copies rather than resurrecting obsolete identities.
- Prevent a system desktop entry with a superseded Exec from leaking its executable alias after a user override changes the command; retain Common-owned generic helpers and native product-owned XDG discovery.
- Extend isolated catalogue regression coverage for hidden, malformed, changed-Exec and non-application overrides, and close test fixture streams independently on write failure.

## 1.0.237 - 2026-10-20

- Validate recorded-process CSV timestamps before graph import rather than accepting an arbitrary short first field as a valid recording date.
- Reject impossible Gregorian dates, invalid local clock/UTC-offset components and trailing timestamp data while accepting historical offsets containing seconds.
- Extend the existing recorder/reader regression with leap-day and historical-offset acceptance and malformed timestamp rejection; preserve failure-atomic metric outputs.

## 1.0.236 - 2026-10-20

- Match Linux TryExec path lookup to executable-file semantics: empty PATH entries denote the current working directory, and a searchable directory is not an executable target.
- Preserve absolute and relative executable paths, including executable symlinks; retain all TryExec policy in the native startup backend.
- Extend the isolated startup regression with real executable and directory fixtures, including an entirely empty PATH, without running the programs.

## 1.0.235 - 2026-10-20

- Refuse incomplete, oversized or embedded-NUL Linux device-manager cache records rather than publishing truncated filesystem identities; reject overlong recognized properties without clipping their names.
- Close the metadata stream and check its result independently of read success, preserving failure-atomic label publication. Reject caller output buffers too small to hold a complete label instead of claiming success with a shortened name.
- Extend storage regressions for missing record terminators, long unknown records following valid properties and oversized filesystem type values; correct close handling in the storage test fixtures.

## 1.0.234 - 2026-10-20

- Preserve distinct CPU thermal warning and critical temperatures when a native critical point is at the 50 C sensor-acceptance floor; derive the warning below that floor rather than publishing equal thresholds.
- Cover critical-only, equal sensor limits and maximum warning-sensor saturation in the canonical metrics regression suite.
- Clarify that one available native limit determines the other by policy, rather than mixing partial sensor data with a model-table limit.

## 1.0.233 - 2026-10-20

- Close Debian package staging streams and descriptors independently of earlier read, write or permission errors. Preserve the originating failure code so cleanup cannot replace a useful diagnostic with an unrelated close result.
- Reject zero-byte writes in package copying and native-installer file publication instead of permitting non-advancing loops; retry interrupted native writes.
- Make the native installer's compiler probe and source-line reader close their streams even on I/O failures, preserving their explicit success/failure contract.

## 1.0.232 - 2026-10-20

- Close the Linux meminfo stream even when a read fails, eliminating the second confirmed short-circuit descriptor leak in the native accounting collectors.
- Preserve failure-atomic memory publication: only fully read and closed source data can update the caller's memory model.

## 1.0.231 - 2026-10-20

- Close the Linux diskstats stream on both successful and failed reads. A short-circuited stream close previously leaked a descriptor when procfs reported a read error; incomplete samples continue to invalidate rate baselines without publishing partial measurements.
- Show the actual number of retained samples in recorded-process graph captions, including the singular case, rather than always claiming a full history window.

## 1.0.230 - 2026-10-20

- Correct recorded-process graph fidelity for CPU use above one logical core. The playback plot now grows its vertical maximum in 100% increments rather than clipping all samples at 100%, while retaining a minimum 100% range for the memory trace.
- Use the existing GTK graph-scale contract rather than introducing another axis implementation; report graph allocation failure instead of dereferencing an unavailable graph.
- Add a GTK/Xvfb regression to canonical `make check` and document the recorder-versus-renderer responsibility boundary.

## 1.0.229 - 2026-10-20

- Fix XDG autostart precedence: a higher-priority desktop filename masks lower-priority entries even when its contents are malformed; scanning no longer treats parsed rows as the sole source of identity authority.
- Reject invalid or mismatched desktop-file IDs at the per-user override boundary, preventing path traversal and writes under an unrelated desktop-file name.
- Add isolated user/system autostart and override regression coverage to the canonical verification suite, and document filename priority separately from desktop-entry parsing.


## 1.0.228 - 2026-10-20

- Reject incomplete or malformed Linux mountinfo snapshots instead of silently omitting the affected mount and claiming a successful filesystem inventory.
- Validate complete records and decoded mount identities, reject embedded NUL and invalid octal escaping, and preserve legitimate optional mount propagation fields.
- Extend storage regressions for invalid mount records, partial-read rollback and valid empty namespaces; document the parser/collector publication boundary.


## 1.0.227 - 2026-10-20

- Make the process-log graph read the exact CSV schema emitted by the asynchronous process recorder instead of using locale-dependent sscanf and narrowing 64-bit process identities to signed int.
- Centralise schema validation and full-row finite ASCII numeric parsing at the recorder boundary using Common's authoritative conversion helpers; reject interrupted/malformed data and fail visibly when a file has no usable samples.
- Add regression coverage for actual writer output, PIDs above INT_MAX, per-core CPU percentages, locale-independent decimals, CRLF, invalid columns, numeric overflow and output preservation on failure.


## 1.0.226 - 2026-10-20

- Eliminate duplicate process-selection predicates and counts from GTK clipboard copying and plain-C CSV export. One export-domain authority now decides exact PID and process-instance membership for both outputs.
- Correct group-only selection: explicitly selected group identities are valid without a highlighted representative PID. Reject stale, zero or structurally incomplete identities without choosing a different process.
- Extend CSV/export regression coverage for group-only, single-instance, stale and malformed selections, and document the domain/UI ownership boundary.


## 1.0.225 - 2026-10-20

- Make native locale timestamp formatting transactional: libc strftime renders privately, and System Monitor publishes text only after complete output fits. Prevent a failed short-buffer conversion from replacing "N/A" with partial or undefined contents.
- Share one all-or-nothing output-commit helper for native Gregorian date and date/time paths while keeping literal strftime format strings, strict compiler diagnostics, OS locale choices and Common .so boundaries.
- Test both provider-absent fallback and explicit Standard/Gregorian selection with insufficient destination capacity, full-width success and a write-bound sentinel.


## 1.0.224 - 2026-10-20

- Prevent process recording from silently overwriting a previous CSV with the same PID/name/second-resolution timestamp. Exclusive creation remains the recorder's atomic filesystem boundary; the process UI selects a bounded, human-readable suffix for collisions without swallowing other errors.
- Check civil-time conversion and filename bounds before opening recording destinations and avoid replacing an already-active recorder instance.
- Add a regression proving an existing recording is preserved byte-for-byte, while a new recording still queues and persists normally. Document ownership, failure and recovery semantics.


## 1.0.223 - 2026-10-20

- Introduce one presentation capacity across History, Users and system snapshots so valid long installed-calendar results are not misreported as unavailable by 64–256-byte UI buffers.
- Statically verify the display capacity against Common's calendar/clock resource bounds and unify native civil-offset acquisition while keeping clock and calendar ownership unchanged.
- Test local-midnight rollover into Common's installed calendar module, independent time-only presentation when the selected calendar is absent and explicit failure on undersized destinations; align temporal API documentation with actual date/zone semantics.


## 1.0.222 - 2026-10-20

- Respect the selected System Settings calendar when presenting dated timestamps, invoking the installed Common Calendar Resource .so instead of always formatting Gregorian.
- Compose selected calendar and clock independently (including Standard clock), preserving ordinary OS Gregorian output and failing closed for missing modules.
- Validate the real installed Hebrew Calendar and French Revolutionary Decimal Clock modules together during Linux release qualification, with updated ownership and failure contracts.


## 1.0.221 - 2026-10-20

- Advance Libraries/Common from 1.19.51 to the published 1.19.68 exact gitlink after reviewing the complete API and implementation delta, including the intentional retirement of compiled absolute clock faces.
- Retire the obsolete three-face 1.000 build archive; instead verify three exact 1.010 source-pinned font files against the updated Common manifest, keeping them build-time only.
- Move nonstandard civil-clock display to Common's validated installed native resource presentations, caching the selected module under the existing policy mutex and reporting unavailable resources without creating local clock fallbacks; retain Common-owned SI duration projection and exact historical UTC-offset handling.
- Qualify the real GTK typography/header behaviour probe after clean-host package installation rather than relying on brittle CI CSS string checks; exercise a real Common-built decimal `.so` under the canonical system location and preserve absent-resource tests. Reapply executable mode to the downloaded GTK probe because artifact transport does not retain it; assert the actual Text body-font role rather than Common's Title fallback. Correct performance-heading CSS to use S Title Bold rather than the product-identity A Title Cond role, with separate widget-level validation.

## 1.0.220 - 2026-10-19

- Preserve seconds-bearing historical timezone offsets when projecting Unix instants into System Settings-selected clock modes; obtain the exact Linux civil offset rather than parsing minute-resolution `strftime("%z")` text, and retain those seconds in explicit clock-mode timezone labels.
- Add a tzdata-independent +00:19:32 regression fixture that compares the application adapter with Common's authoritative clock formatter; document the exact-offset boundary while retaining the existing pinned Common version and all release-qualification guarantees.

## 1.0.219 - 2026-10-19

- Preserve the installed System Monitor package during privileged native upgrades: let one `dpkg --install` own the replacement instead of removing the working version first.
- Correct detached Linux process-scan coalescing so a request satisfied by the current scan cancels obsolete queued enrichment; latest UI demand wins even when it returns to the in-flight flags.
- Add an isolated controlled backend fixture proving both stale-scan cancellation and delivery of the newest distinct request.
- Remove a redundant Debian icon payload assertion without weakening package validation; keep current Common integration and the qualified release workflow intact.

## 1.0.218 - 2026-10-18

- Correct Day-mode contrast in the deliberately dark System Monitor header: use Common's Night title/summary foreground colours for the fixed navy chrome, independent of the workspace theme.
- Keep title, subtitle and window controls legible in both modes, with restrained header-specific hover treatment, rather than changing the hero/dashboard or the global Day palette.
- Extend the GTK chrome regression test and CI source contract to reject dark Day-mode lettering on the fixed dark header; leave telemetry and window geometry unchanged.

## 1.0.217 - 2026-10-18

- Improve System Monitor's small descriptive type after reviewing a running 1.0.216 Night screenshot: slightly larger header subtitle, summary and overview gauge/stat captions.
- Use existing Common summary contrast roles for short descriptions and metrics, preserving the stronger MB Corpo S Title headings and A Screen1 numerical values.
- Keep dashboard card geometry, colour-coded sensor signals, monitoring data, hardware access, packaging ownership and verified CAP_NET_RAW setup unchanged.

## 1.0.216 - 2026-10-18

- Correct the executable's capability-installation target to the authoritative `/Programs/system-monitor` path. The Debian post-install script has used that path since 1.0.209, but the application-side `LSM_INSTALLED_EXECUTABLE_PATH` still incorrectly pointed at `/usr/bin/system-monitor`, causing a misleading warning and disabling optional raw Bluetooth capture.
- Add a source/package path-contract assertion and run the real `--install-capability` mode against the installed executable in clean-host qualification, so this mismatch cannot silently pass another release.
- Preserve the narrow `CAP_NET_RAW` handling and explicitly avoid recreating the retired `/usr/bin` application alias.

## 1.0.215 - 2026-10-18

- Extend the Windows executable beyond the older three privately embedded fonts, using six SHA-256-qualified Text, Text Bold, Text Cond, Title Bold, A Title Cond and A Screen1 roles.
- Verify every selected asset against the same immutable 23-font InfiltratorOS manifest, register all six before first paint and fail rather than substituting an unrelated font.
- Improve Windows telemetry, headings, labels and table readability while preserving Linux's pinned Common three-face verification baseline and standalone 23-face Linux font provisioning.

## 1.0.214 - 2026-10-18

- Correct Debian package content qualification to expect the verified 23-face `ensure-extended-fonts` helper actually staged by the package builder, rather than the retired `ensure-native-fonts` path.
- Correct the installed-executable allowlist to check that same helper name. Preserve the complete native typography bootstrap and immutable release qualification.

## 1.0.213 - 2026-10-18

- Keep the exact Libraries/Common 1.19.51 runtime revision and package a verified snapshot of Common 1.19.57's additive installer-only 23-face font helper, avoiding unrelated calendar/clock semantic changes.
- Standalone System Monitor installation now provisions the same role-specific S Text/Cond, S Title and A Screen1 faces used under InfiltratorOS, with source hashes verified before publication and persistent machine-cache ownership preserved.
- Do not reinstall retired 1.000 TTF files into the 23-font cache; older applications that still need the old bootstrap remain independently responsible for it, without new package-owned TTF duplication.

## 1.0.212 - 2026-10-18

- Optimise Linux typography by task from the 23-face InfiltratorOS font catalogue: MB Corpo S Text for legible UI labels, S Title Bold for headings, S Text Cond for dense process rows and A Screen1 Cond for numerical performance readouts.
- Retain Common's three-face fallback when separately installed before the extended font resources; preserve Windows' already verified private three-face font embedding until its asset bundle is migrated.
- Keep Common-owned Day/Night semantic colours, metric rules and hardware/process behaviour unchanged.

## 1.0.211 - 2026-10-18

- Restore the standard root `README.md` and `docs/README.md` documentation entry points, linking System Monitor to the InfiltratorOS product-family design contract.
- Update the maintained-document audit, Doxygen main page and shipped package README source together, so new repository layout is verified rather than bypassing quality gates.
- Keep Common's versioned palette, native font cache and single-source build/release implementation unchanged; this is a repository/documentation integration release.

## 1.0.210 - 2026-10-17

- Remove application-owned MB Corpo TTFs from the Linux package and establish the canonical shared `/System/Resources/Fonts` cache through the exact pinned Common bootstrap.
- Preserve standalone Mint/Ubuntu installation by packaging only the installer-side native-font bootstrap under `/usr/lib/system-monitor`; System Monitor no longer owns a private font payload.
- Make Debian package qualification reject `/usr/share/fonts/truetype/system-monitor/` and require the shared-font bootstrap dependencies.

## 1.0.209 - 2026-10-17

- Install the Linux application only at `/Programs/system-monitor`; no System Monitor executable or compatibility link is installed under `/usr/bin`.
- Make the standalone Debian and hardware-native installation paths establish the native InfiltratorOS filesystem skeleton when needed.
- Add the native `/System/Commands:/Programs` login-shell PATH bridge and use `/Programs/system-monitor` directly from desktop and package capability setup.
- Release qualification rejects the obsolete `/System/Programs` split and any `/usr/bin/system-monitor` package entry.

## 1.0.205 — 2026-10-16

- Replace the self-extracting native installer with a small source downloader that fetches the exact release and pinned Common revisions over HTTPS before compiling and installing; keep help available offline.
- Organise repository guidance under `.github` and project documentation under `docs`, keeping build entry points and `LICENSE` at the repository root.
- Update relative links, Doxygen inputs, maintained-document checks and Debian package README sourcing for the new document locations.

## 1.0.204 — 2026-10-16

Keep presentation-contract checks executable independently of `NDEBUG`, so optimized qualification cannot erase label, layout and unit assertions.

## 1.0.203 - 2026-10-15

- Remove Process Inspector from the complete private `LsmApp` layout by routing retained process data, CPU display policy, graph direction and destructive-action confirmation through the process context, and window parenting through the presentation context.
- Make process presentation policy fail closed for destructive-action confirmation while preserving existing CPU and history semantics.
- Reduce the exact private-application-state allowlist from 24 to 23 direct consumers without weakening the build, test or sealed-release ownership model.

## 1.0.202 - 2026-10-15

- Repair the default Make build by removing the orphaned `build-all` indirection and making `all` depend directly on the application target.
- Simplify verification and release orchestration by removing a redundant `style-check` dependency and expressing Debian/native-installer release artifacts as direct Make prerequisites instead of recursive Make invocations.
- Make the repository audit protect durable contracts rather than theme/CSS, README-heading and Doxygen-layout spellings; keep GTK behavioural qualification and Doxygen itself authoritative for those semantics while guarding the real default-build and release dependency graph.

## 1.0.201 - 2026-10-15

- Reduce repository-audit coupling to incidental build and workflow spelling: retain source manifests, ownership boundaries, licensing, shell limits, Common encapsulation and sealed-release invariants while allowing semantically equivalent command layout and orchestration refactors.
- Make Doxygen's fail-closed undocumented/API-contract settings the documentation authority instead of maintaining a second hand-written public-declaration parser in the source-style auditor.
- Preserve the deliberate single-authority build model: Make owns verification and packaging, CMake owns the Linux application build, the Windows release script owns the native Windows build, and Publish consumes only the sealed qualified artifact bundle.

## 1.0.200 - 2026-10-14

- Make Common dependency preparation explicit: CMake and normal Make verification now validate the parent-pinned submodule without hidden network or source-tree mutation, while `make common-bootstrap` is the single opt-in fetch path for an incomplete Git checkout.
- Move every persisted graphical preference default into the preferences owner so the application composition root retains lifecycle ordering without duplicating preference policy.
- Make the authoritative Clang and Doxygen documentation gates fail closed when their required tools are unavailable, and give deterministic coverage one manifest for both its exact module scope and expected count.
- Remove redundant source checkouts from clean-host installer qualification and release sealing by carrying `support/VERSION` with the already-qualified Linux artifact bundle.

## 1.0.199 - 2026-10-14

- Complete the Common 1.19.42 adoption audit: verify that the 1.19.41-to-1.19.42 implementation delta changes no pre-existing System Monitor parsing, I/O, arithmetic, timing or persistence contract, then retain the full exact-pin qualification as the integration proof.
- Remove the pre-1.0.39 decimal-comma persistence compatibility shim and consume Common's strict bounded decimal parser directly for preferences and App History, with malformed or superseded rows marked for canonical rewrite.
- Route disk latency operation/time rollback handling through Common's monotonic counter-delta contract instead of maintaining a second private unsigned-counter rule.
- Require every future Common advancement to include both an old-to-new behavioural regression review and a product-side Common-adoption audit, preventing dependency repins from accumulating avoidable local rework.

## 1.0.198 - 2026-10-14

- Advance the exact Libraries gitlink to published 1.19.42 and consume Common's new shared-design validators instead of duplicating palette and metrics ABI-prefix rules in the Win32 adapter.
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

- Advance the exact Libraries gitlink from published 1.19.35 to published 1.19.41, inheriting strengthened POSIX and temporal contracts while keeping monitoring policy, hardware interpretation and product presentation locally owned.
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

- Make the parent repository gitlink the single Libraries revision authority for both Make and CMake, removing duplicate tag, version and commit pins and the source-archive network fallback.
- Centralise metrics, storage, process and accelerator test source membership so ordinary, sanitizer and coverage builds cannot drift independently.
- Remove the self-hosted CI Git package bootstrap and validate the runner toolchain contract before checkout instead.
- Make `support/VERSION` the only maintained source-version value by removing duplicate current-version strings from README and CI.

## 1.0.187 - 2026-10-13

- Remove the unused duplicate CMake smoke-test and developer-tool graph so `make check` is the single regression and quality-test authority while CMake owns only the Linux application build.
- Move native Windows release compilation and qualification out of workflow YAML into `support/windows/build-release.ps1`, centralising compiler warnings, link libraries, Common construction, source membership and backend probes.
- Replace brittle source-audit checks for obsolete CI spelling with semantic build and release ownership checks, eliminating legacy marker comments that existed only to satisfy the checker.
- Make CMake verify Libraries against the parent repository gitlink instead of using its own pin for application-build correctness, preventing a stale configured submodule from being accepted.
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
