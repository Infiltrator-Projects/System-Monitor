# Validation

## Purpose

Validation distinguishes implemented behaviour from behaviour that has actually been demonstrated. A build proves compilation; it does not by itself prove runtime, hardware or integration correctness.

## Automated evidence

The repository currently uses:

- .github/workflows/ci.yml
- .github/workflows/release.yml

`support/tests` contains accounting, hardware, Bluetooth, GPU, battery, filesystem, history, portability and Common-integration regression coverage. The estate currently has 20 physical `*_smoke.c` sources: consolidated subsystem suites and specialised presentation, integration and lifecycle fixtures. Related regression cases live inside their owning subsystem source rather than as separate one-case translation units. Make is the canonical executed suite. CMake retains target registration and local CTest support, while CI builds only the application through CMake so the same smoke programs are not executed twice.

Automated checks should cover ordinary behaviour, important boundaries, malformed/error cases and release/package contracts appropriate to the project.

Generated Doxygen API validation is intentionally scoped to System Monitor-owned source and header-defined API types. Private implementation compounds defined only inside `.c` files are not promoted into the generated API surface; Clang documentation syntax checks still cover the complete owned source set.

## Manual and environment-dependent evidence

Hardware-specific telemetry and privileged actions still require real-device validation because synthetic CI cannot prove a particular firmware, driver or kernel exposes a metric correctly.

Manual evidence supplements automation and must be described at the level actually observed. A simulator, fixture or mocked provider must not be described as physical-device proof.

## Release criterion

The exact revision intended for release must pass the required automated gates. Release assets must be derived from that revision, and documentation must not advertise known-failing or merely planned behaviour as supported.

## Regression rule

Every fixed defect should gain the narrowest useful permanent regression check when reproducible. Tests are part of the product contract rather than disposable scaffolding.

## 1.0.96 forensic audit record — 2026-09-25

Baseline: System Monitor 1.0.95, commit
`b9660fa417708f3b0d68e967c85bd9400a8ab1fe`. Common remains pinned to
1.19.27, `3ef3710df6563df305b6d8e2dc9d1a41c61843ba`.

### Scope and method

The review examined source contracts, collector arithmetic and failure paths,
worker ownership, persistence, selected presentation paths, documentation,
comments, build rules and packaging. Manual inspection traced the affected
contracts from producer through retained state to presentation and tests.
Strict compilation, Clang analysis, documentation syntax and source-contract
checks provide broader automated coverage. Generated PCI lookup data and the
separately maintained Common submodule were not independently re-audited.

This is a bounded engineering audit, not an exhaustive manual sign-off of every
line or proof of defect freedom. In particular, the complete GTK/Win32 event and
rendering surface has not received a fresh manual line-by-line review. Passing
source-style or documentation checks is not evidence that every comment is
semantically correct. The findings below distinguish implemented corrections
from remaining assurance work; no academic certification is implied.

### Corrected findings

| Finding | Failure and correction | Permanent regression/evidence |
| --- | --- | --- |
| Sampler ownership | A worker could exit before a timed-out caller transferred cleanup responsibility, leaking the sampler/backend. Independent references now cover either exit order; startup retry writes use the publication mutex. | `backend_smoke.c` forces worker exit before a reported timeout and requires exactly one source-context destruction; runtime lifecycle suite. |
| CPU rollback | Failed idle deltas could appear as 100% busy, while total rollback retained stale usage. Reset affected percentages before accepting a valid interval. | `metrics_smoke.c`: idle rollback, total rollback and recovery. |
| Disk/network sample gaps | Retained baselines could turn missing intervals into a recovery spike. Missing records invalidate baselines; first recovery establishes a baseline. Sysfs missing/malformed counters are omitted, while measured zero remains valid. | Metrics, backend and storage suites cover missing samples and recovery. |
| GPU memory semantics | DXGI `CurrentUsage` is per application, not adapter-wide. Remove that misleading value; add independent used-memory availability to the shared model, Linux providers and both presentations. Missing Linux reads no longer imply measured zero. | Presentation suite tests capacity-only, measured zero and unavailable stale usage; NVML suite asserts valid usage availability; Windows cross-build. |
| CPU frequency availability | Nominal/base clock was used when current or maximum clock was unavailable. Those fields now remain unavailable. | Backend fixture has a known nominal clock and no current/max provider. |
| Persisted durations | Negative and excessively large durations were accepted; signed rounding could overflow during display. Validate the persisted range and clamp before unsigned conversion. | History retention suite adds negative and oversized fixtures while preserving legacy decimal-comma migration. |
| CPUID representation | Brand extraction wrote through an unsigned pointer into a character buffer. Use aligned register storage and copy the resulting bytes. | CPUID fixture, strict builds, ASan/UBSan and i386 compilation. |
| File-user identity | Path-string comparison missed hard links and could confuse an old open file with a new file at its former path. Compare device/inode identity through the procfs descriptor. | Process suite covers hard links and a replaced pathname; API comments document descriptor churn. |
| Netlink/source contracts | Missing statistics looked like zero; interface-name copies assumed a terminator; message headers assumed alignment/minimum payload size. Track counter presence and validate bounds/alignment; source initialization clears its output on failure. | Storage missing/malformed/zero fixtures, native backend smoke, strict and i386 builds. Malformed raw-netlink fuzz coverage remains outstanding. |
| Release tooling | Parallel package creation changed the source directory during tar traversal. CI/publication now use the existing serial `release` target. Source payloads exclude `.exe` artifacts; version inspection avoids an early-closing pipe; font extraction ignores archived UID/GID. | Exact workflow build commands; deterministic installer/package checks; archive-content assertion with the Windows executable present. |
| Documentation/comments | Async requests were described as synchronous collection; cleanup ownership, CPU kernel counters, temperature discovery and file matching had inaccurate comments. Windows GPU and typography documentation also lagged implementation. | Updated API/internal comments, Architecture, Portability, Roadmap, README and changelog; Doxygen/Clang documentation gates. |

The source semantics behind the CPU and GPU corrections are documented by the
[Linux procfs interface](https://www.kernel.org/doc/html/v6.9/filesystems/proc.html)
and Microsoft's
[DXGI memory-info contract](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/ns-dxgi1_4-dxgi_query_video_memory_info).

### Executed local validation

Environment: Linux x86-64 container with GTK 3 development libraries, GCC,
Clang, CMake, Doxygen, MinGW and i386 development support.

- `make check`: strict builds, analyzer, documentation, deterministic subsystem,
  backend/worker/lifecycle, coverage and installer checks passed. A prior
  development run produced a gcov profile-stamp mismatch; an
  isolated coverage rebuild and subsequent complete suite passed.
- `cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build-cmake --target system-monitor --parallel 2`: passed.
- `make portability-check`: every Linux application translation unit compiled
  to an ELF i386 object. This is compilation evidence, not 32-bit runtime proof.
- ASan/UBSan: runtime, metrics, storage, process, history and application-catalog
  suites passed with leak detection disabled in a temporary local Makefile.
  The committed sanitizer gate is unchanged. Ordinary `make sanitizer-check`
  could not complete here because LeakSanitizer rejected the traced execution
  environment; this is **not** a LeakSanitizer pass.
- Release-package commands built the Debian package and native installer, with
  byte-reproducibility checks. MinGW built the native Windows GUI using verified
  embedded font hashes; PE subsystem and system-DLL checks passed.
- A real GTK startup attempt under Xvfb was blocked because this environment
  could not create X server listening sockets. No visual/runtime desktop result
  is claimed from that attempt. Installation on the host was not performed.

The coverage gate applies to 17 selected deterministic modules, each at least
65% line coverage. It is neither whole-application coverage nor a measure of
manual audit completeness. Hosted verification/publication must still report
its own result for the exact candidate revision.

### Remaining assurance and investigation

These items are not closed by this patch or by successful cross-compilation:

- Native Windows runtime/metric comparison, GTK navigation and visual checks,
  privileged process/service actions, physical GPU/NPU/battery/Bluetooth devices,
  and actual device hotplug require target-system evidence.
- CPU numbering with sparse online IDs/hotplug needs targeted fixtures: parsing
  uses kernel CPU IDs while projection is bounded by logical processor count.
- Network replacement under an unchanged interface name needs an identity test;
  current Linux rate-state reconciliation is name-based. Process I/O permission
  loss/recovery and Windows counter-gap handling also need dedicated fixtures.
- Raw netlink truncation/interrupted dumps, Windows IOCTL payload bounds and the
  wider UI callback/lifetime surface warrant further manual review and fault
  injection. These are investigation areas, not claims of reproduced exploits.
- Some optional Linux providers are process-global; concurrent monitor instances
  or immediate reinitialization while a detached collector remains blocked have
  not been established as supported. Bounded shutdown cannot cancel a permanently
  blocked kernel or filesystem operation.

A rigorous completeness claim requires closing these review and runtime gates,
not merely increasing test counts or restating the feature-complete roadmap.
