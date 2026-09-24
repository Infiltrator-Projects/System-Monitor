# Changelog

This changelog records user-visible, compatibility, architecture and validation changes for System Monitor. Detailed commit-by-commit history remains in Git.

## 1.0.86 - 2026-09-24

- Reduce periodic App History allocation churn by consolidating per-snapshot live-set, RSS and active-time accounting.
- Replace remaining generic Windows arithmetic, counter-rate, bounded-copy and dynamic-array helpers with the pinned Common 1.19.24 contracts where Common is equal or stronger.
- Remove historical SysMonTask wording and notice requirements after re-checking the current native C/GTK/Win32 implementation against the external Python/Glade project and finding no retained source or asset dependency requiring that notice.
- Replace the imported third-party PCI names corpus with an independently maintained factual registry; unknown devices retain exact numeric PCI identity instead of guessed names.
- Standardise project-owned copyright declarations on Copyright (c) 2000-2026 Shannon Smith while preserving real third-party ownership boundaries.

## 1.0.85 - 2026-09-24

- Make App History open substantially more efficiently by avoiding the duplicate first-navigation refresh that rebuilt the same model immediately after construction.
- Populate the bounded App History model without re-sorting the whole list after every inserted row, then restore the requested sort once after the bulk refresh.
- Track the latest durably written history generation so shutdown does not wait for a periodic save and then write the identical generation a second time.
- Teach synchronous shutdown persistence to treat an already-written matching generation as durable success, preserving the dirty-state contract without duplicate I/O.
- Extend the strict GTK compatibility shim with the standard unsorted tree-model sentinel required by the bulk-refresh path.

## 1.0.84 - 2026-09-23

- Preserve the currently selected Performance device when delayed topology discovery changes a device's presentation stack identity while the semantic device is unchanged.
- Retain a separate semantic selection identity for each Performance page: disk kernel name, network interface, Bluetooth address and the corresponding stable identifier for other device classes.
- Restore topology rebuild selection by exact stack identity first and by same-type semantic identity second; fall back to CPU only when the selected resource genuinely no longer exists.
- Add deterministic regression coverage for Disk identity promotion so Bluetooth discovery cannot move an unchanged Disk selection back to CPU.

## 1.0.83 - 2026-09-23

- Preserve the currently selected Linux Performance device when asynchronous topology discovery adds Bluetooth devices by validating Disk pages with the same stable identity key used to build them, rather than the retired literal `disk-<name>` form.
- Keep CPU/GPU hot-temperature status pills geometrically stable across normal, warning and fault states by reserving their padding and border footprint continuously while leaving the normal border transparent; threshold crossings no longer resize the metric area or graph.
- Hide the GTK main window immediately on a close request and perform preference/layout persistence once during final shutdown, removing the visible teardown stall and duplicate close-path state writes while retaining bounded backend cleanup.

## 1.0.82 - 2026-09-23

- Continue the cross-platform presentation consolidation after 1.0.81 by making the Linux GTK Disk, Network and GPU presenters consume the same toolkit-neutral device projection used by Win32 for overlapping user-facing values.
- Add stable named indices for shared disk, network and GPU view metrics so native renderers and tests do not depend on unexplained numeric positions.
- Keep Linux-only graph mechanics, pressure data, partition tables, cumulative network totals, wireless details and advanced GPU engine telemetry local to GTK while sharing only semantics that are genuinely common to both platforms.
- Preserve the richer Linux GPU telemetry-source fallback in the shared model: native driver telemetry and basic-identification states remain meaningful instead of being weakened to `N/A` for reuse.
- Extend deterministic presentation regression coverage to lock the named device-value contract and GPU telemetry fallbacks.
- Fix the Windows normal-start crash introduced by physical-disk discovery: the collector no longer places multi-megabyte arrays of partition-rich `LsmDiskInfo` objects on the GUI thread's default stack; discovery staging now lives on the heap and is copied into the shared monitor snapshot only after identity reconciliation.
- Remove unnecessary duplicate old-network and old-GPU stack snapshots by comparing newly discovered identities against the retained shared model before publication.
- Strengthen native Windows CI so verification now exercises both the lightweight first-paint smoke path and the real normal startup/backend path for five seconds, preventing collector-start crashes from passing as successful GUI startup.
- Extend the Windows GPU backend from identification-only to live native telemetry without vendor SDKs: correlate active display adapters to Windows adapter LUIDs, sample the built-in GPU Engine performance counters, collapse per-process rows back into physical engines, and publish render, compute, video, video-processing, copy, overall utilisation and active-engine state through the existing shared `LsmGpuInfo` model.
- Add DXGI adapter-memory telemetry keyed by the same LUID, publishing dedicated VRAM used/total/percentage for discrete adapters while explicitly classifying unified/shared-memory adapters instead of presenting system RAM as dedicated VRAM.
- Add conservative SetupAPI display-adapter metadata, publishing driver provider/description, driver version and physical location only when Windows supplies a sufficiently strong device-identity match.
- Keep unsupported generic Windows GPU measurements such as temperature, power and fan state explicitly unavailable rather than synthesising values or adding vendor-specific runtime dependencies.
- Extend the verified Windows system-library contract for PDH, DXGI and SetupAPI while keeping the Windows artifact free of third-party runtime dependencies.
- Recover GitHub Actions verification from a superseded hosted-run concurrency stall by advancing the Verify concurrency epoch without weakening main-only, exact-tested-commit release gating.

## 1.0.81 - 2026-09-23

- Extend the native Windows monitor adapter behind the existing shared `monitor_platform.h` seam instead of creating Windows-only application models.
- Discover physical disks through native storage IOCTLs, retain stable `PhysicalDriveN` counter baselines, publish size/model/bus/media identity, read/write throughput, active time, response time and queue depth, and map Windows volumes back to physical disks for filesystem/usage/system-disk presentation.
- Discover network interfaces through IP Helper, publishing friendly/product identity, IPv4/IPv6, MAC address, connection state, negotiated link speed, cumulative traffic, live receive/send rates and link utilisation into the existing `LsmNetInfo` model.
- Add native Windows graphics-adapter identification without inventing unsupported telemetry: adapter identity is published into `LsmGpuInfo`, while utilisation, temperature and other uncollected GPU metrics remain explicitly unavailable.
- Extend `performance_view.[ch]` with toolkit-neutral disk, network and GPU projections so native renderers consume the same titles, rail summaries, units and availability-to-text policy rather than maintaining Windows-specific formatting rules.
- Replace the Win32 Performance rail's one-device-per-category assumption with type-plus-device-index selection, enumerate every discovered disk/network/GPU instance, add bounded wheel scrolling, and clip hit targets to the visible rail.
- Add deterministic shared-view regression coverage for disk/network/GPU projection and update both MinGW and native Windows verification/release link contracts for IP Helper and Winsock.
- Verify the completed change through the native Windows GUI build/launch gate, MinGW cross-build, CMake application build, canonical verification suite, sanitizers, release-package verification and aggressive PGO installer test.

## 1.0.80 - 2026-09-23

- Correct the cross-platform architecture so Linux and Windows consume one platform-neutral System Monitor presentation contract instead of maintaining independent copies of page identity, tab labels, Performance resource identity, colours, geometry, CPU/Memory field ordering and value-formatting policy.
- Move top-level tab labels, Performance titles/stack identities, canonical rail/graph geometry, resource colours, CPU/Memory metric captions, detail captions and grid positions into `presentation_contract.[ch]`.
- Add `performance_view.[ch]` as a toolkit-neutral CPU/Memory view model. Both GTK and Win32 now receive the same subtitle, rail summary, metric strings and detail strings from one `LsmMonitor` snapshot.
- Make optional CPU telemetry availability explicit for virtualisation, temperature, load average, interrupt rate and context-switch rate so valid zero/false values are distinct from unsupported or uncollected data.
- Convert the GTK CPU/Memory builders and presenter to consume the shared field-placement schema and shared view model instead of hard-coded grid attachments and private value formatting.
- Convert the Win32 renderer to consume the same `LsmTabIndex`, `LsmPageType`, labels, colours, geometry, field-placement schema and formatted CPU/Memory view model.
- Link the pinned Common 1.19.24 portable formatter sources into the native Windows preview/release build, so both platforms use the same generic formatting implementation rather than parallel unit-formatting code.
- Add a toolkit-neutral presentation smoke test that locks shared labels, field positions and availability semantics, including sampled zero values versus `N/A`.
- Build the shared presentation modules into Linux, MinGW cross-builds, native Windows startup verification and Windows release artifacts.
- Keep toolkit mechanics native: GTK remains the Linux renderer and Win32/GDI remains the Windows renderer; collectors remain native OS adapters, while product semantics and presentation policy no longer fork by operating system.

## 1.0.79 - 2026-09-23

- Correct release verification after 1.0.78 by treating central Infiltrator-Repository APT discovery as asynchronous instead of failing a valid GitHub release while the scheduled central publisher has not yet run.
- Keep an immediate catalogue check for already-published packages, but record deferred discovery as a notice rather than a false release failure.
- Preserve the existing immutable-release, exact-source-version and central-publisher ownership boundaries.

## 1.0.78 - 2026-09-23

- Fix the themed Windows About window clipping its second paragraph at the bottom of the content card.
- Measure the wrapped About text with the active native font before creating the window, then derive the client and outer window height from that measurement.
- Preserve the existing themed card/button composition while making the dialog resilient to DPI scaling and font substitution instead of relying on a fixed 285-pixel outer height.

## 1.0.77 - 2026-09-23

- Fix the Windows About surface so its displayed version is supplied from `support/VERSION` at compile time instead of being hard-coded in `main_windows.c`.
- Replace the stock bright Win32 MessageBox About panel with a native themed About window painted from the active System Monitor Day/Night palette.
- Update Windows verification and release compilation to inject the authoritative project version into the Windows executable.

## 1.0.76 - 2026-09-23

- Size Windows CPU and Memory detail-label columns from the actual native font metrics instead of a fixed 116-pixel split, preventing avoidable ellipsis in labels such as "Logical processors", "Maximum speed" and "Context switches/s".
- Match the Linux CPU side-rail value semantics: usage plus current speed when available, otherwise usage plus `N/A`, instead of displaying the Windows-only logical-processor count in the value line.
- Keep the canonical 220-pixel Performance rail while using a dedicated compact native value font so CPU and Memory summaries fit the same geometry more naturally.

## 1.0.75 - 2026-09-23

- Mirror the actual Linux Performance-page composition in the native Windows preview instead of only approximating its colour scheme.
- Rebuild CPU with the compact horizontal title/model header, separate "% Utilisation / 100%" scale row, primary history graph and the Linux-style lower metrics/details panel.
- Rebuild Memory with the title/total and usage-scale header, primary graph, memory-composition bar, usage metrics grid and hardware-information section.
- Keep unimplemented Windows telemetry fields visible as explicit `N/A` values so presentation can reach parity before collector coverage does.
- Correct Windows Performance typography to use the Common UI sans family, matching Linux page titles instead of incorrectly applying the brand serif face.
- Reduce the headline summary and notebook-strip heights to better match the GTK composition and return vertical space to useful page content.

## 1.0.74 - 2026-09-23

- Refine the native Windows presentation toward Linux visual parity without changing backend semantics.
- Add compact live CPU and Memory sparklines to the Performance resource rail and use the Linux resource colours: cyan for CPU and blue for Memory.
- Add View → Theme with Follow system, Day and Night; consume Common 1.19.24's exact Day/Night palette roles and persist the Windows theme choice per user.
- Add hover states to the custom menu strip, top tabs and Performance rail so interactive regions no longer feel like static painted labels.
- Detect unavailable MB Corpo faces and explicitly fall back to Segoe UI instead of relying on opaque GDI font substitution.
- Theme the Windows process-list rows and header and remove harsh stock gridlines while retaining the native read-only list control.
- Refresh Follow system when Windows appearance settings change and update the DWM title-bar preference together with the client palette.

## 1.0.73 - 2026-09-23

- Replace the raw white Win32 preview layout with the current System Monitor/Infiltratr presentation hierarchy: top-level product tabs across the top and a dedicated Performance resource rail on the left.
- Apply Common 1.19.24's Night design contract to the native Windows shell, including the #050608 canvas, graphite panel/card/surface layers, #00ADEF accent, shared spacing/radius metrics and project typography fallbacks.
- Add the Windows headline summary bar and live CPU/memory history graphs so Performance visually follows the Linux product grammar while retaining the existing native Windows data sources.
- Keep Processes as a dark native read-only list and keep unimplemented Windows pages visible as explicit placeholders instead of presenting unrelated native-control layouts.
- Use a dark Windows title-bar hint where supported without making startup depend on that optional non-client styling.

## 1.0.72 - 2026-09-23

- Move the large retained Windows application state, including `LsmMonitor`, from the default GUI-thread stack to heap ownership; native Windows validation reproduced the previous startup failure as `STATUS_STACK_OVERFLOW` before first paint.
- Make first paint independent of Windows backend startup: create and show the Win32 shell before initialising Performance telemetry, and defer the process backend until the Processes page is selected.
- Keep the GUI open when a Windows backend is unavailable instead of allowing backend initialisation or process scanning to block the initial window.
- Link the Windows GUI with the MinGW runtime statically and make CI/release validation prove that the published PE is a Windows GUI-subsystem executable with only approved Windows system DLL imports.

## 1.0.71 - 2026-09-23

- Add the first native Win32 System Monitor GUI so Windows testing starts with the visible product shell rather than a console-only backend probe.
- Present all eight top-level product areas in the Windows window; populate Performance with live CPU/memory telemetry and Processes with the existing read-only Windows process backend.
- Keep App History, Startup Apps, Users, Details, Services and File Systems visible as explicit placeholders until their native Windows backends are implemented.
- Build the Windows preview as a GUI-subsystem executable with no console window and publish `system-monitor-1.0.71-windows.exe` as the Windows release asset.
- Keep Linux Make/CMake builds unchanged by treating the Win32 entry point as platform-specific source, while the Linux i386 gate continues to exclude Windows translation units.

## 1.0.70 - 2026-09-23

- Add the first deliberately limited Windows native monitor backend behind the existing `monitor_platform.h` seam.
- Collect aggregate CPU utilisation, logical processor count, uptime, system process/thread/handle totals and physical/commit memory directly through Win32/PSAPI while leaving unsupported device telemetry unavailable.
- Add a read-only Windows process backend using Tool Help plus native process queries for PID/name/parent/thread inventory and permission-dependent CPU, working-set, I/O, priority, executable and handle details; retain creation-time identity so PID reuse cannot contaminate rates.
- Resolve Windows process account identity and current-user ownership from access-token SIDs so process categorisation and application-history keys do not use a PID-based approximation.
- Keep Windows command-line and GPU/cgroup enrichment plus all process-control operations explicitly unsupported in this first slice rather than fabricating parity.
- Add a MinGW cross-compile gate for both Windows backend translation units and keep the existing i386 ELF portability gate scoped to Linux sources, without claiming a complete or supported Windows application build; Linux remains the released desktop target.
- Publish a small Windows console backend-probe executable with 1.0.70 so the new CPU/memory and read-only process collectors can be exercised on real Windows systems before the full Windows GUI exists.

## 1.0.69 - 2026-09-22

- Pin Make, CMake and the source gitlink to released Common 1.19.24 at `748e089ae175329471d4cf375522c44081371bd5`, keeping all build paths on the same exact shared revision.
- Align maintained README, architecture and portability documentation with the new Common pin.
- Re-audit Common 1.19.24's graphics hardening against System Monitor and keep the GTK/Cairo graph renderer local: Common's changed bitmap-surface clipping/copy/blit/scale/rotation contracts are a different abstraction and do not replace System Monitor presentation code.
- Re-audit the remaining large source files and retain their present module boundaries where splitting would introduce new internal state-sharing APIs without reducing coupling.

## 1.0.68 - 2026-09-22

- Restore the exact `/proc/uptime` first-field boundary contract after the Common token-parser migration: numeric prefixes followed by junk are rejected, while complete standalone and whitespace-delimited records remain accepted; add a permanent process-suite regression.
- Preserve cached storage-metadata framing while retaining Common line-ending trimming: embedded CR/LF still terminates the captured property exactly as before 1.0.67; add a malformed-record regression fixture.
- Split Performance snapshot presentation into core-resource and device-oriented modules, retain one small dispatcher/shared temperature-state boundary, and remove the duplicate hardware-name predicate/title composition in favour of the existing Performance helpers.
- Split exact-file process-owner search out of the Process Inspector and share its sortable text-table construction instead of duplicating GTK table mechanics.
- Split monitor-level Bluetooth membership/traffic baseline handling out of accelerator collection while retaining raw HCI accounting in the existing Bluetooth transport module.
- Align README platform wording with the feature-complete roadmap: additional native backends are optional expansion rather than unfinished mandatory work.

## 1.0.67 - 2026-09-22

- Pin Make, CMake, maintained documentation and the source gitlink to released Common 1.19.23 at `a9cf2957cffeefe6001830916b8a32c2ef58a551`.
- Replace two remaining manual parent-directory slices in block-device identity and partition discovery with Common's lexical dirname contract, preserving the same canonical sysfs inputs while removing duplicate path mechanics.
- Parse the first `/proc/uptime` field directly with Common's locale-independent floating-point token parser instead of mutating the procfs record into a standalone number.
- Route cached storage-metadata CR/LF cleanup through Common's line-ending trim primitive while keeping Linux record grammar and hardware policy local to System Monitor.

## 1.0.66 - 2026-09-22

- Complete the forensic maintenance pass for the declared Linux product scope and document System Monitor as feature-complete rather than treating optional expansion as unfinished work.
- Remove redistribution of proprietary MB Corpo font binaries while preserving the Common typography family preference and required GTK/system fallback.
- Make source policy and release-package validation reject any future bundled proprietary font payload.
- Document the retained-snapshot, PID-instance identity, presentation-only and lazy-shell invariants directly at the implementation boundaries where regressions would be most expensive to diagnose.


- Consume Common 1.19.22's strict bounded UTF-8 validator at genuine firmware/sysfs human-readable metadata boundaries without applying text semantics to arbitrary SSID, process-command or protocol bytes.
- Reject malformed SMBIOS and sysfs identity strings before GTK presentation while preserving existing fallback discovery; UTF-8-safe SMBIOS truncation never splits a multibyte sequence.

- Reduce the smoke-test estate from 49 focused `*_smoke.c` files to 18 physical smoke sources/executables by merging related regression cases into seven coherent subsystem suites while preserving the original case-level assertions and diagnostics.
- Remove the obsolete suite-runner/header and per-case Make/CMake targets; sanitizer and deterministic-coverage gates now consume the consolidated subsystem sources directly instead of rebuilding one-case smoke programs.

- Decouple persistent App History accounting from lazy GTK page construction so process history starts after first paint and is retained even when the App History tab has never been opened.
- Keep App History persistence asynchronous without requiring its GTK list model by anchoring save completion to the application window while immutable save requests retain their own data lifetime.

- Make the Clang and Doxygen documentation gates fail closed so checker errors cannot be masked by a following success message.
- Scope generated Doxygen validation to System Monitor-owned source instead of recursively enforcing this repository's documentation policy on the pinned Common submodule, and remove the obsolete `CLASS_DIAGRAMS` setting.
- Limit generated API documentation to header-defined types so private implementation-only structs in `.c` files remain internal while public API documentation stays warning-clean.
- Treat the existing `io.github.theinfiltratr.SystemMonitor` GApplication ID as an intentional stable compatibility key rather than accidental branding residue.
- Strengthen the source audit so the CMake local smoke/CTest path must retain its source-coverage drift guard and test registration without replaying CTest in hosted CI.

- Remove obsolete pre-rebrand package, configuration-directory and tab-layout migration code so current System Monitor starts and persists only the canonical identity and layout.
- Remove the old `system-monitor` / `linux-system-monitor` Debian compatibility aliases and native-installer removal path; `infiltrator-system-monitor` is now the sole package identity.
- Rename the remaining old `LINUX_SYSTEM_MONITOR_*` include-guard namespace to the current `INFILTRATOR_SYSTEM_MONITOR_*` namespace.
- Complete the Shannon Smith-owned project copyright normalization to the actual 2016 project start across production sources, tests, tools, packaging and icon metadata while preserving third-party ancestry and licence notices unchanged.

- Remove the source-repository PAT dependency from APT publication. System Monitor now publishes only its own immutable release and the central APT repository independently discovers it.
- Verify that the central catalogue advertises the exact System Monitor version within 15 minutes of publication, without requiring a custom cross-repository secret.
- Keep the APT repository authoritative for package discovery and indexing so release synchronisation remains tokenless and self-healing.
- Collapse CI to one hosted Verify job instead of replaying the full regression suite in a second nominally native hosted job.
- Make the Makefile the canonical executed verification suite; CMake now proves the alternate application build path without rerunning every smoke executable through CTest in CI.
- Remove the redundant backend-only strict syntax pass because the project-wide strict gate already compiles those translation units under the same warning policy.
- Stop rerunning Common's own upstream core smoke test inside System Monitor; retain the System Monitor/Common integration smoke and the exact Common version/commit pin instead.
- Keep release publication gated by the exact successful Verify SHA and rebuild/validate the release artifacts without rerunning the complete test suite a third time.

## 1.0.63 - 2026-09-21

- Eliminate the two GCC `-Wstringop-overread` diagnostics exposed by the target-machine PGO/LTO rebuild without weakening bounds: descriptor-kind presentation now uses a directly formatted NUL-terminated label, and the battery-title fallback explicitly bounds reads to the known source array.
- Keep Common's overlap-safe bounded-copy primitive unchanged; the fixes stay at the System Monitor call sites whose source extents are known more precisely than the generic shared contract.
- Extend the end-to-end aggressive native-installer release gate to capture the complete PGO build log and reject any recurrence of `stringop-overread` diagnostics before publication.

## 1.0.62 - 2026-09-21

- Turn the hardware-native `aggressive` profile into a true two-pass target-machine optimisation pipeline: `-O3`, native ISA/tuning, LTO, profile generation, representative native training and a final profile-use rebuild.
- Add a private headless `--pgo-train` execution mode that exercises the real monitor lifecycle, topology refreshes and retained process scans without starting GTK or performing destructive process actions.
- Require the selected compiler to prove support for the PGO generation/use flags and require actual `.gcda` profile output before the final aggressive rebuild; the installer fails rather than silently claiming PGO.
- Use `-fprofile-correction` and `-fprofile-partial-training` so measured hot paths guide layout/inlining/branch decisions while unvisited code retains normal optimisation.
- Preserve correctness-oriented floating-point semantics; `-Ofast` and unsafe global math transformations remain deliberately excluded.
- Record the real selected build profile in application metadata and native BUILD-INFO instead of internally labelling every installer build as merely `native`.
- Gate releases with an end-to-end aggressive `.run` execution on a disposable hosted Linux runner, proving the instrumented build, headless training, generated profile, profile-use rebuild, package creation and final dpkg installation all succeed before publication.

## 1.0.61 - 2026-09-21

- Advance System Monitor from Common 1.19.17 to the current Common 1.19.18 release.
- Pin Make, CMake and the actual `src/infiltratr-common` gitlink to `af4942ab03ceac4b9a3c46519c5670f18344cb8e`.
- Keep System Monitor's existing Common ownership split unchanged; Common 1.19.18 only advances the shared temporal/calendar authority and does not alter the generic counter, path, hashing, ASCII, POSIX deadline or file contracts used by System Monitor.
- Update maintained architecture, portability, roadmap and README references to the exact Common 1.19.18 foundation.

## 1.0.60 - 2026-09-20

- Complete a fourth bidirectional System Monitor/Common ownership pass against Common's maintained architecture, design and consumer-boundary documents.
- Promote generic monotonic unsigned-counter delta handling and normalized absolute POSIX-clock deadline conversion into released Common 1.19.17, then consume those contracts in System Monitor.
- Replace repeated CPU, process and Intel PMU monotonic subtraction/rollback mechanics with Common while preserving System Monitor's domain-specific baseline, high-water, identity and hardware-wrap policies.
- Replace HID++ timeout arithmetic, BlueZ refresh deadline construction and sampler-shutdown timespec normalization with Common's errno-style deadline helpers while retaining pthread/device scheduling and cancellation policy locally.
- Consume Common's existing ordered first-u64 reader for peripheral battery charge-telemetry discovery and use Common's monotonic clock directly in the process-scan benchmark.
- Advance both build systems and the gitlink to exact Common 1.19.17 at `9734c32f37b5863af571ebf2226e23642bc2baa6`.
- Keep transient DRM high-water normalization, Intel energy-counter wrap semantics, Linux hardware interpretation and GTK/pthread ownership in System Monitor because those remain product/domain policy rather than generic Common mechanics.

## 1.0.59 - 2026-09-20

- Complete a third bidirectional System Monitor/Common ownership pass after the 1.0.58 release, reviewing both remaining local generic mechanics and newly released Common contracts.
- Promote deterministic ASCII case-insensitive lexical ordering and stable non-cryptographic FNV-1a byte/text/u64 mixing into released Common 1.19.15, then consume those contracts in System Monitor.
- Remove the final production `strcasecmp` sort and residual `strcasestr` identifier/sensor matching in favour of Common's locale-independent ASCII contracts.
- Replace repeated private FNV-1a text/byte/u64 loops used for stable page identities and refresh signatures while keeping process/group/partition signature composition local to System Monitor.
- Advance both build systems and the gitlink to exact Common 1.19.15 at `e93c7bf55bb2238ad647ecb701ec9614c1055af1`.
- Review Common 1.19.14's temporal-presentation API but leave System Monitor's date/time policy unchanged until the application has a real system-wide clock-profile source; reuse is not forced where the product contract is not yet present.

## 1.0.58 - 2026-09-20

- Complete a second bidirectional System Monitor/Common ownership pass against Common's maintained admission rules rather than treating reuse as a one-way dependency.
- Promote toolkit-neutral current-user HOME, XDG config/data-home resolution and recursive directory creation from System Monitor's remaining GLib-assisted paths into released Common 1.19.13, then consume those contracts throughout configuration, startup, application-catalogue, logging and persistence paths.
- Complete Common's deterministic ASCII family with whitespace, alpha, digit, alphanumeric, hexadecimal and case-insensitive substring contracts, then use them in Linux kernel/protocol/identifier parsers and system-theme detection instead of locale/toolkit helpers.
- Replace remaining production `g_file_get_contents` configuration reads with Common's complete allocated text reader where the data contract is toolkit-neutral.
- Pin both build systems and the gitlink to released Common 1.19.13 at `43f87ce6f8a47425f7823324d38cfdb2c328bb06`.
- Make the project-identity regression assert Common's published version macro instead of embedding a stale release number.
- Keep GTK presentation, GTask/GObject ownership, GDBus semantics, XDG desktop-entry policy, System Monitor history grammar and graph-history policy local where they remain UI/product/domain responsibilities rather than generic Common contracts.

## 1.0.57 - 2026-09-20

- Complete a bidirectional System Monitor/Common ownership pass against Common's maintained admission and consumer-boundary contracts.
- Pin System Monitor to released Common 1.19.11 at `3d42a55195d344cd5fabe1487c0f1515993c47fe`.
- Promote canonical lowercase System/Day/Night persistence keys and parsing into Common, then remove System Monitor's private theme serialization table.
- Promote System Monitor's two-decimal GHz presentation into Common's existing metric-format family and replace the local inline formatter with the shared contract.
- Replace NVML's private required/optional symbol-loading loop with Common's atomic dynamic-library symbol-table binder while preserving the same required and optional NVIDIA entry points.
- Use Common's canonical build-profile labels for About presentation while keeping System Monitor's native-installer profile aliases as product-owned policy.
- Retain Linux HCI/capability security policy, legacy decimal-comma migration, page presentation policy and graph-history semantics locally because they remain product-specific rather than generic Common contracts.

## 1.0.56 - 2026-09-20

- Define zero avoidable third-party dependencies as an explicit architecture target before beginning the dependency-reduction implementation.
- Record the preservation rule: dependency reduction must not remove or silently degrade any documented System Monitor feature.
- Replace the BlueZ development-header dependency with a minimal, compile-time-checked project-owned Linux HCI ABI boundary while preserving exact per-device Bluetooth traffic.
- Replace installation-time `setcap` with a root-only internal mode that writes and verifies the exact CAP_NET_RAW `security.capability` xattr directly; `libcap2-bin` is no longer a build or package requirement.
- Remove custom font-cache maintainer-script calls and the explicit Fontconfig runtime dependency; GTK's platform font stack remains the presentation boundary.
- Add dependency-audit gates that reject reintroduction of BlueZ development headers, libcap tooling, explicit Fontconfig cache helpers and command-wrapper telemetry providers.

## 1.0.55 - 2026-09-20

- Complete a documentation-to-implementation feature pass rather than treating a green build as feature completeness.
- Move Startup Apps discovery off the GTK main thread and coalesce overlapping refresh requests; durable enable/disable override writes now run on a worker as well.
- Keep startup search filtering on the GTK thread over completed worker results, preserving the current query while filesystem discovery is in flight.
- Expand diagnostic snapshots to cover the complete displayed hardware families by adding Bluetooth controllers, connected Bluetooth-device traffic and system/peripheral battery state.
- Add CPU runtime counts and detailed memory commit/cache/kernel accounting to diagnostic snapshots so the exported report matches the information advertised by the Performance views.
- Extend the snapshot regression fixture to prove Bluetooth, battery and detailed-memory sections are emitted while preserving the project's traditional KB/MB/GB labelling contract.

## 1.0.54 - 2026-09-20

- Standardise the application artwork on the canonical non-automotive Infiltrator blue `#00ADEF`, retaining the existing monitor glyph and single packaged icon source across launcher, taskbar, About and Mint metadata.

- Complete the first-paint startup work by making every non-Performance notebook page genuinely first-use lazy instead of constructing all hidden pages from a post-paint idle queue.
- Restore the previously selected tab only after the initial Performance frame is eligible to paint, and ignore notebook switch signals emitted while placeholder pages are still being assembled.
- Start Services, Users and File Systems periodic timers only after those pages have actually been constructed, removing needless wake-ups for pages the user never opens.
- Add native Linux Pressure Stall Information collection for CPU, memory and I/O through `/proc/pressure/*`, sampled on the existing monitor worker rather than the GTK thread.
- Present 10-second PSI pressure on CPU, Memory and Disk pages, retain optional full-pressure semantics, include PSI in diagnostic snapshots, and cover the parser with strict smoke, analyser, sanitizer and deterministic coverage gates.
- Add optional cgroup-v2 process identity collection only while the friendly Processes page needs it.
- Use the standardized cross-desktop systemd application-unit convention and `app.slice`/`background.slice` as additional grouping evidence, while retaining XDG executable and process-ancestor matching as the fallback rather than guessing arbitrary systemd unit mappings.
- Keep System Monitor pinned to the latest released Common 1.19.10 at `33e69c0a462b56d388881d89c4eb49f72fa0b0fe`; the newer Common main commits inspected during this pass contain copyright/documentation normalization only and no functional library changes.

## 1.0.53 - 2026-09-20

- Rework application startup around first paint: construct only the window shell and Performance page before showing the GTK window instead of eagerly building all eight notebook pages.
- Build Processes, Details, Users, Services, File Systems, Startup Apps and App History after the first frame through the original deferred-page path, while first navigation to any page builds that page immediately; 1.0.54 replaces that deferred construction with true first-use laziness.
- Move XDG application-catalog scanning off the GTK main thread into a GTask worker and publish the completed catalogue back to the process model without blocking window creation.
- Stop scanning Startup Apps during page construction; the inventory is collected only when that tab becomes active or the user explicitly refreshes it.
- Preserve restored-tab behaviour under lazy construction, including page-specific refresh, process navigation to Details and user-session navigation to Processes.
- Guard process scan policy while the Details page is not yet constructed so periodic background sampling cannot dereference lazy widgets.
- Cancel deferred page construction and detach asynchronous catalogue publication safely during shutdown.

## 1.0.52 - 2026-09-20

- Move beyond palette matching and apply MBLINK's composition grammar directly to System Monitor performance pages: rounded graphite header and detail cards, card-contained GPU modules and curved graph surfaces.
- Centralise the performance-card and performance-header treatment so CPU, memory, disk, network, Bluetooth, GPU, battery and NPU pages use the same visual hierarchy instead of accumulating page-local styling.
- Derive card radii and padding from Common design metrics rather than hard-coded geometry, preserving Common 1.19.10 as the presentation authority.
- Promote warning and fault temperatures from coloured text into MBLINK-style pill states, using Common gold/red semantic colours while leaving normal telemetry unadorned.
- Keep canonical cyan interaction emphasis, canonical gold battery telemetry and the layered graphite gradient while preserving all existing monitoring behaviour and page functionality.
- Start a clean Verify concurrency epoch after a stale cancelled self-hosted run wedged the previous group; normal cancel-in-progress behaviour remains intact for future main pushes.

## 1.0.51 - 2026-09-20

- Carry the MBLINK visual language beyond palette reuse: rounded Common-metric controls and graphs, subtle graphite card gradients, stronger cyan interaction accents and semantic gold/red status colour.
- Match MBLINK navigation more directly with a translucent cyan selected-row fill, cyan border and cyan selected title instead of relying on a narrow leading edge alone.
- Use canonical Common/MBLINK cyan for CPU telemetry and canonical gold for battery telemetry so the performance face visibly participates in the shared product palette.
- Round and clip Cairo performance graphs using Common design radii, preserving metric traces and fills inside the curved card surface.
- Give the summary strip explicit muted-caption and bright-value roles and round the strip, frames, controls and tab corners from Common design metrics.
- Render CPU and GPU temperatures through Common warning/fault states so hot hardware is visibly gold before the fault threshold and red at the fault threshold rather than remaining plain white.
- Extend strict GTK/Cairo compatibility declarations for the new style-state and rounded-rendering APIs so warning-as-error verification continues to cover the real implementation.
- Keep the graphite card gradient on the proven GTK3 `to bottom right` syntax used by the existing application styling path.

## 1.0.50 - 2026-09-20

- Match the visible System Monitor hierarchy more closely to the established InfiltratorFS/Common Night presentation instead of merely sharing its palette values.
- Move the performance sidebar onto the Common panel layer and change selected device rows to the InfiltratorFS selection treatment: graphite selection fill, normal border and a narrow cyan leading edge.
- Replace boxed cyan notebook selection with the same restrained cyan underline treatment used by the InfiltratorFS stack switcher.
- Restore Common's dedicated button background and foreground roles for ordinary GTK controls, leaving the darker operation surface for components that semantically require it.
- Render primary performance graphs on the Common card surface while keeping compact sidebar graphs on the quieter surface layer, with softer grid/fill emphasis and neutral graphite frames.
- Apply Common detail-label, heading, kicker, summary and selected-summary roles to performance captions, values and device-row metadata so text hierarchy matches the rest of the Infiltrator family.
- Remove the remaining hard-coded purple memory-composition frame and resolve its background/border through the active Common palette.
- Preserve the correct Common foreground roles through normal, hover, checked and disabled button states so custom dark navigation rows never inherit the light-button foreground colour.
- Extend the strict GTK compatibility surface for the style-class API used by the new hierarchy so warning-as-error Make verification covers the same calls as the real GTK build.

## 1.0.49 - 2026-09-20

- Refine GTK Night presentation to follow MBLINK's visual hierarchy rather than only sharing its raw colour values.
- Keep ordinary buttons and controls on graphite operation surfaces with restrained borders; reserve cyan for selection, interaction emphasis and product metrics.
- Remove always-cyan performance navigation borders so unselected device rows remain visually quiet and selected rows use MBLINK's translucent cyan treatment.
- Render performance graphs on Common surface layers with subtle graphite frames and reduced grid emphasis instead of metric-coloured outer boxes.
- Give the performance split panes explicit theme identities so the sidebar, content canvas and separator retain the intended layered graphite structure.
- Keep menus, tabs, scrollbars, text surfaces and About-dialog controls within the same Common 1.19.10 MBLINK-derived Night hierarchy.
- Build the generated GTK theme stylesheet in strict-C-safe fragments so the richer visual contract remains portable under the project's warning-as-error checks.

## 1.0.48 - 2026-09-20

- Pin the project and gitlink to Common 1.19.10 at `33e69c0a462b56d388881d89c4eb49f72fa0b0fe`.
- Replace the temporary single-grey Night override with Common's complete Linux MBLINK reference palette.
- Use the MBLINK canvas, titlebar, connection-bar, card/surface, border, heading/summary and accent-hover roles directly so Night retains the layered graphite appearance instead of collapsing into one black or grey field.
- Style System Monitor summary frames and general frames with those same semantic layers while keeping product-specific performance colours local.

## 1.0.47 - 2026-09-20

- Restore the established MB graphite-grey Night shell instead of presenting the near-black Common canvas as the application background.
- Keep Common 1.19.8 authoritative for semantic component colours and typography while mapping the top-level Night shell to the existing #2B2B30 System Monitor graphite used by graph/drawing fallback rendering.
- Apply the same graphite shell when Follow system resolves a dark host theme, so explicit Night and system-dark presentation remain visually consistent.
- Validate the resolved Common palette and typography before deriving the shell background so the appearance path remains fail-safe.

## 1.0.45 - 2026-09-20

- Complete a second forensic Common 1.19.8 usage pass without changing Common itself.
- Source MB Corpo family names and role weights from Common's typography contract, and use Common's canonical theme names where they match System Monitor's UI wording.
- Replace remaining equivalent private counter-rate arithmetic, allocation-growth overflow checks, bounded string-copy formatting and simple lexical path joining with Common contracts while preserving Linux, hardware and presentation policy locally.

## 1.0.44 - 2026-09-19

- Complete the forensic Common 1.19.8 integration by routing strict numeric-token conversion, checked addition, prefix checks, quantity parsing, trimming and lexical-basename handling through Common where its contract is equal or stronger.
- Keep Linux procfs/sysfs grammars, pthread absolute-deadline semantics, hardware policy and GTK presentation local where Common deliberately does not own them.
- Preserve all existing functionality while reducing private generic mechanics and making the Common ownership boundary explicit.

## 1.0.43 - 2026-09-19

- Pin both Make and CMake builds, the source gitlink, project-identity regression test and maintained documentation to exact Common 1.19.8.
- Consume Common 1.19.8's internal duplicate-code cleanup without changing System Monitor's public behaviour or Linux/hardware ownership boundaries.
- Replace remaining private generic quantity, numeric-token, checked-arithmetic, prefix, trimming and lexical-basename mechanics with Common where Common's contract is equal or stronger; retain Linux record grammar and product policy locally.
- Restore one authoritative Common version across all build paths after the 1.19.6/1.19.7 metadata drift that caused the Make verification gate to fail.
- Repair the strict-check GTK compatibility declarations required by the System-theme observer introduced in the 1.0.43 appearance work.

## 1.0.42 - 2026-09-19

- Advance to Common 1.19.4 and remove remaining private generic parsing, endian, saturation, allocated text-read and CSV-field mechanics where Common now owns an equal or stronger contract.
- Use Common's locale-independent fixed-point formatter for preferences, history, recorder and export persistence while retaining System Monitor's legacy decimal-comma recovery policy.
- Consolidate bounded string copies and lexical path basenames onto Common without moving Linux, hardware or GTK policy out of System Monitor.

## 1.0.41 - 2026-09-19

- Bundle the MB Corpo UI fonts with both Debian and native-installer releases, refresh the font cache during package lifecycle changes, and remove the explicit system Sans fallback.

## 1.0.40 - 2026-09-19

- Replace the application artwork with the shared non-automotive Infiltrator icon language: dark graphite field, #72dcff cyan linework and a simplified system-monitor glyph.
- Keep the same project-owned icon source wired through the desktop launcher and Debian/Mint package aliases so the menu, application and Software Manager remain consistent.

- Documentation baseline aligned with the Infiltrator project family.

## Recording policy

Record additions, removals, behavioural fixes, compatibility changes, dependency changes that affect consumers, and material validation/release changes. Pure refactoring needs an entry only when it changes maintenance or portability expectations.

## Historical releases

Existing Git tags and GitHub Releases remain the authoritative identity for exact historical source and release assets. Do not reconstruct detailed historical claims here without evidence from those immutable records.
