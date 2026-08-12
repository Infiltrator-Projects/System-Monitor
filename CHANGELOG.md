<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Changelog

This file records current releases and consolidated milestones. Fine-grained
historical implementation detail is intentionally not duplicated here; the
source history remains the authoritative record for individual intermediate
changes.

## 1.13.2 — 2026-08-12

- Relicensed all Shannon Smith-owned application, test, build, documentation
  and vendored Infiltratr Common source under `GPL-3.0-or-later`; every C and
  header file now begins with the exact SPDX identifier and retains a matching
  Doxygen licence tag.
- Replaced both project BSD licence files with the unmodified GNU GPL version 3
  text, advanced Infiltratr Common to 1.1.1 for its licensing-only patch, and
  made the source-quality gate reject missing or inconsistent licence headers.
- Separated third-party ownership from project ownership. The retained
  SysMonTask icon and compiled PCI-name data keep their BSD 3-Clause notices in
  `THIRD_PARTY_NOTICES`, file-specific SPDX sidecars and Debian copyright data.
- Added the canonical Debian `copyright` file to the package, updated the About
  dialog and package metadata to `GPL-3.0-or-later`, and changed all project
  links to `The-First-Infiltrator/System-Monitor`.
- Corrected the glibc compatibility header for both `C2X_STRTOL` and
  `C23_STRTOL` feature-macro spellings, preventing current glibc headers from
  silently importing 2.38-only parsing symbols into the glibc 2.34 package.
- Installed the existing 96x96 icon in the matching icon-theme directory rather
  than incorrectly labelling it as a 256x256 asset.

## 1.13.1 — 2026-08-12

- Added a dependency-free release gate that inspects the finished application
  binary for imported `GLIBC_M.N` symbol versions before a Debian package can
  be assembled. Packages now fail closed if the binary exceeds the supported
  glibc 2.34 baseline instead of publishing metadata that understates the real
  runtime requirement.
- Centralised the Debian `libc6` dependency floor and the binary ABI check on
  the same version constants so package metadata and compatibility validation
  cannot drift independently.
- Added regression fixtures covering supported, too-new and non-version GLIBC
  strings without introducing a readelf/objdump runtime dependency.

## 1.13.0 — 2026-08-11

- Expanded the shared Infiltratr Common component to 1.1.0 with strict
  unsigned and finite-double parsing, null-safe string equality, prefix and
  suffix predicates, and toolkit-neutral floating-point clamping.
- Replaced the Intel PMU backend's local suffix and numeric parser helpers and
  adopted the common range helper in disk, NPU and Intel GPU accounting.
- Extended both the standalone common-library suite and the stable `lsm_`
  facade suite across invalid values, overflow, null strings and boundaries.

## 1.12.0 — 2026-08-11

- Introduced Infiltratr Common 1.0.0 as a versioned C11 component shared with
  Calendar Plus, covering project identity, bounded strings, safe numeric
  primitives, binary quantity formatting and POSIX file/path/clock adapters.
- Converted the established `lsm_` utility interface into a compatibility
  facade over the shared implementation, preserving collector call sites and
  behaviour while removing the independent duplicate implementation.
- Centralised About-dialog and diagnostic-snapshot release identity in one
  toolkit-neutral project record and included the common-library version in
  generic and native package build metadata.
- Added common-component compilation, strict-source, CMake, smoke, sanitizer,
  coverage, source-archive and hardware-native-installer integration.

## 1.11.1 — 2026-08-11

- Increased every Performance sidebar card uniformly from 62 to 68 pixels and the compact graph frame from 60x42 to 64x44 pixels, preserving the compact 1.10.13 redesign while giving multi-line battery and device status text more breathing room.
- Increased the sidebar/button width slightly to preserve text space beside the wider thumbnail rather than creating new wrapping.
- Vertically centred thumbnail graphs and label stacks and kept each live-value summary to one ellipsized line so one device status cannot make a card visually taller than its neighbours.
- Excluded transient `build-*` directories from both standard source ZIPs and native-installer payloads so release source cannot accidentally carry a previous local binary.

## 1.11.0 — 2026-08-11

- Refactored the formerly flat `LsmApp` into explicit shell/runtime, Performance, process-workspace, Processes, Details, History, File Systems, Startup, Services, Users and path state blocks. The root application object now has 15 owned members instead of more than one hundred unrelated fields.
- Split the former 1,800-line Performance implementation into a 623-line lifecycle/controller module, a 536-line CPU/memory/disk/network builder and a 670-line GPU/battery/NPU builder.
- Split process mutation, filtering, recording and action-menu logic out of the Details renderer into `process_actions.c`; the Details module is now below 1,000 lines and focused on table/model presentation.
- Expanded the formal deterministic line-coverage gate from five backend parsers/accounting modules to eleven core modules, adding memory accounting, sample history, GPU metric selection, Performance selection, process grouping and mountinfo. Every gated module must retain at least 65 percent line coverage.
- Extended sample-history regression cases to exercise partially-filled history in both graph directions and null-input guards.
- Preserved all 1.10.13 Performance sidebar sizing, colours and compact summary behaviour through the architecture-only refactor.

## 1.10.12 — 2026-08-10

- GPU engine graph selectors now list only metrics actually supplied by the active GPU backend. Intel PMU adapters therefore offer native Render, Compute, Video, Video Enhance and Copy counters without unrelated NVIDIA-only Video Encode/Video Decode entries.
- GPU selector state now maps visible choices explicitly to `LsmGpuMetric` values, so filtering the list cannot misroute a selection to a different metric.
- Added regression coverage for Intel-style and NVIDIA-style selectable GPU metric sets.

## 1.10.11 — 2026-08-10

- Added native MMC/SD product-name discovery from the kernel's
  `device/name` attribute when SCSI-style `device/model` is unavailable.
  Performance disk titles now use the card's reported product name instead of
  falling back to raw identifiers such as `mmcblk0`.
- Added a synthetic MMC regression fixture to the native system-source smoke
  test so this identity path remains covered without external utilities.

## 1.10.10 — 2026-08-10

- Moved Wi-Fi metadata collection and its retained ioctl cache out of
  `performance_present.c` and into the Linux monitor backend. The GTK
  presentation layer is again a read-only consumer of the retained snapshot.
- Added a source-quality guard that rejects direct Wi-Fi, ioctl, socket or
  D-Bus collection calls in Performance snapshot presentation code.
- Made Debian package assembly byte-for-byte reproducible by normalising the
  staged payload timestamps and supplying the same `SOURCE_DATE_EPOCH` to
  `dpkg-deb`. `make deb` now rebuilds a second package and compares it.
- Changed `make dist` to create the standard deterministic
  `Linux-System-Monitor-VERSION-source.zip` instead of an extra tarball, and
  added `make release` to create the generic `.deb`, native `.run` and source
  `.zip` release set.
- Tightened backend selection so the build manifest may contain multiple native
  monitor/process providers while the executable selects exactly one provider
  for the requested platform.

## 1.10.9 — 2026-08-10

- Restored a GPU graph context menu on the generic fallback graph. Drivers that
  do not expose independent engine counters now still respond to right-click;
  Single engine and Multiple engines remain visible but disabled and the menu
  states that detailed engine counters are unavailable.
- Kept the existing Task-Manager-style Single/Multiple engine modes unchanged
  for GPUs that really expose engine telemetry.
- Audited source comments across the project. Public API Doxygen contracts and
  file-level purpose comments remain mandatory; implementation comments were
  kept for ownership, accounting, portability and failure invariants rather
  than obvious line-by-line narration.
- Consolidated project documentation into README, CHANGELOG and required
  licence texts. Removed the duplicate architecture document, separate AUTHORS
  file and document manifest; attribution remains in README and LICENSE.
- Shortened README and CHANGELOG while preserving build, package, architecture,
  portability, security, unit and release-gate contracts.
- Renamed the Details process column from Linux-specific file-descriptor wording
  to the neutral Handles terminology already used by the process model.

## 1.10.8 — 2026-08-10

- Introduced a platform-neutral process model and moved Linux PID/UID, procfs
  start identity, nice values, signals and affinity semantics below the process
  backend contract.
- Normalised process CPU time to nanoseconds, replaced public descriptor counts
  with handle counts and made account/process-instance identity opaque.
- Added backend-free process-model tests and source guards against reintroducing
  POSIX process primitives into common contracts.

## 1.10.7 — 2026-08-10

- Purified the Performance snapshot by moving CPU, disk, network and GPU sample
  baselines plus Linux battery transport state into private Linux backend state.
- Replaced presentation-visible Intel/backend markers with neutral GPU
  capabilities and opaque hardware identities.
- Added source checks that reject native collector state in the common monitor
  snapshot.

## 1.10.6 — 2026-08-10

- Removed `/dev/accel` construction from NPU presentation and made the NPU
  identity/display contract platform-neutral. Linux may still provide a native
  device identifier as optional display-ready information below the HAL.

## 1.10.5 — 2026-08-10

- Moved the Linux Performance lifecycle behind `monitor_platform.h`; `monitor.c`
  now delegates to a replaceable operating-system backend.
- Added a backend-free lifecycle test proving the common monitor can link and
  operate without Linux collectors.

## 1.10.4 — 2026-08-10

- Added Task-Manager-style GPU Single engine / Multiple engines graph modes for
  adapters with independent engine telemetry; GPU-memory history remains below
  either layout.
- Added a binary-progression dynamic network graph ceiling and live midpoint
  scale label.

## 1.10.3 — 2026-08-10

- Corrected negotiated network-rate conversion: the decimal rate reported by
  Linux is converted back to raw bits before the application's 1024-based
  Kb/Mb/Gb/Tb formatter is applied. Unit promotion retains decimal detail but
  never produces a leading `0.` value.

## 1.10.1 — 2026-08-10

- Replaced permanent CPU Overall/Logical buttons with the graph context menu
  used by Task Manager.
- Repacked GPU detail fields into denser groups without dropping telemetry.

## 1.10.0 — 2026-08-10

- Added backend-neutral selectable GPU engine histories and a separate GPU
  memory graph, with a truthful single-graph fallback when a driver exposes no
  detailed engine counters.

## 1.9 — August 2026

- Completed responsive Performance-page sizing, maximised/restored window
  renegotiation and page-specific scrolling behaviour.
- Improved live CPU/hardware identity, Hyper-V device naming and Performance
  side-selection synchronisation.
- Added native unmounted-filesystem/partition identification, including FAT
  variants and common filesystem-less partition roles, without mounting or
  invoking external storage utilities.
- Added the persistent cross-tab live summary, richer CPU/memory/disk/network/GPU
  fields, system snapshot export and process-table export.
- Began formal HAL work by moving collector state behind an opaque backend and
  replacing direct topology-timestamp manipulation with a neutral request API.

## 1.8 — August 2026

- Promoted the proven 1.7 application and corrected native installer to stable
  1.8.0 after long-running lifecycle, page-switch and backend stress testing.
- Preserved the GUI-only one-executable package and 1024-based unit policy.

## 1.7 — July/August 2026

- Reworked Performance, Processes and Details toward the Windows Task Manager
  interaction model while retaining native Linux hardware information.
- Added CPU/memory/disk/network/GPU/NPU/battery collectors, process inspection,
  process GPU accounting, startup/services/users/filesystem pages and extensive
  deterministic smoke tests.
- Replaced command-line telemetry dependencies with direct C collectors/parsers
  where practical, including CPUID, SMBIOS, PCI names, wireless metadata and
  Logitech HID++.
- Standardised the three release artifacts: generic Debian package,
  hardware-native installer and complete source archive.
- Fixed the native installer privilege boundary so compilation/testing runs as
  the normal user and only package installation requests administrator rights.

## Earlier 1.x milestones

- Replaced the original Python-oriented SysMonTask implementation with a native
  C/GTK application.
- Established Performance as the first/default tab, the GUI-only package
  contract, truthful unavailable-value handling and traditional KB/MB/GB/TB
  labels using 1024-based scaling.
- Added strict compiler, packaging, sanitizer, static-analysis, lifecycle and
  source-architecture gates that remain part of the release process.
