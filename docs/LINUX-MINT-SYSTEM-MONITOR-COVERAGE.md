<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Linux Mint System Monitor Function Ownership Audit

## Purpose

This document began as a forensic inventory of the Linux Mint System Monitor
baseline and a placement map for the Infiltrator 22-repository software family.
It is now also the maintained implementation ledger for that replacement work.
It remains a behavioural coverage specification, not a one-for-one UI-cloning
specification.

The objective is to ensure that every real capability in the Mint baseline has
an explicit home in the Infiltrator suite: an existing equivalent, a stronger
existing capability, or a clearly owned future System Monitor responsibility.
A function must not be moved to another application merely because its noun
resembles that application's name.

The governing rule is simple: monitoring, process inspection/control, resource
telemetry and mounted-filesystem observation belong to **System Monitor**.
Common may own reusable primitives, but it must not own monitor product policy.

## Audited baseline

The Mint 22.x GTK3 System Monitor baseline is GNOME System Monitor 45.0.2. This
audit pins the source examined:

- upstream repository: `GNOME/gnome-system-monitor`;
- release tag: `45.0.2`;
- tag object: `1887b54c4b1c1b183dd8a4c7465b3d015bdd01d2`;
- release commit: `e74545dbdebd8e591c1679181c5ff3d53c53386e`;
- release tree: `68f31f4d429a2f8fcbcff74e9378e81384ab7ebf`;
- Mint package baseline: `gnome-system-monitor 45.0.2-1+wilma`.

The audit read the application, process, resource, filesystem and preference
implementations rather than inferring behaviour from screenshots. Principal
source files included `application.cpp`, `proctable.cpp`,
`procactions.cpp`, `procdialogs.cpp`, `procproperties.cpp`,
`memmaps.cpp`, `openfiles.cpp`, `lsof.cpp`, `setaffinity.cpp`,
`load-graph.cpp`, `smooth_refresh.cpp`, `disks.cpp`, `cgroups.cpp`,
`systemd.cpp`, `selinux.cpp`, the GSettings schema, the GTK UI/menu files,
and the task-oriented help pages.

The implementation comparison point is System Monitor 1.0.130. Version 1.0.129
implemented the main replacement tranche; 1.0.130 closes the remaining
inspection-detail gaps found by the follow-up audit. Current-code observations
are evidence only; code and tests remain authoritative.

## Executive ownership decision

Almost every actual Mint System Monitor capability belongs in
**Infiltrator System Monitor**. The suite should not split this application up.

| Repository | Correct boundary |
| --- | --- |
| **System-Monitor** | Owns monitor UX, process inspection/control, resource telemetry, mounted-filesystem capacity, monitor preferences, monitor help and monitor state. |
| **Infiltrator-Libraries (Common)** | Generic formatting, timing, parsing, safe file/config access, design primitives and other genuinely reusable mechanisms only. |
| **System-Settings** | Operating-system-wide policy only. It must not absorb System Monitor refresh rates, graph options, process columns, kill warnings or monitor-local units. |
| **Filesystem-Support** | Filesystem-driver/support availability and install/uninstall. It does not own mounted-filesystem capacity monitoring. |
| **Defragmenter** | Fragmentation analysis and defragmentation only. |
| **Software** | Package/application discovery, install, update and removal. |
| **Infiltrator-OS** | OS integration/defaults/meta-packaging, not monitor implementation. |

## Placement inside System Monitor

| System Monitor surface | Mint responsibility placed here |
| --- | --- |
| **Overview** | Headline CPU, memory, disk, network and busiest-process summary only. |
| **Processes** | Friendly process/application view, search and ordinary end-task workflow. |
| **Details** | Complete technical process inventory, scope filters, flat/tree dependency view, columns and advanced process actions. |
| **Process Inspector → Overview** | Full selected-process properties/identity. |
| **Process Inspector → Performance** | Selected-process live CPU/memory/I/O/GPU behaviour. |
| **Process Inspector → Open Files** | Per-process descriptors, sockets, pipes and objects. |
| **Process Inspector → Memory Map** | Virtual-memory map. |
| **Process Inspector → Threads** | Thread inventory; stronger than the Mint floor. |
| **Process Inspector → Process Family** | Parent/child context. |
| **Performance → CPU** | CPU history and per-core activity. |
| **Performance → Memory** | RAM/cache/swap history and composition. |
| **Performance → Network** | Receive/send rates, totals and units. |
| **Performance → Disks/Partitions** | Physical/storage I/O and unmounted-device telemetry. |
| **File Systems** | Mounted source/device, mount point, filesystem type and capacity. |
| **Tools → Find process using file** | Reverse lookup from file/path to processes holding it open. |
| **Preferences** | Monitor-local sampling, process presentation, graph presentation, warning and column defaults. |
| **Help** | Process states, memory/swap, units, filesystems and task workflows. |
| **Application shell** | Refresh, search focus, shortcuts, About/Help, window/page state and direct-entry routes. |

This preserves the product model: Performance answers “what is the resource
doing?”, Processes/Details/Inspector answer “what is this workload doing?”, and
File Systems answers “how are mounted filesystems consuming capacity?”.

## Complete function map

### Application shell, navigation and state

| Mint 45.0.2 function | Infiltrator destination |
| --- | --- |
| Processes page | System-Monitor → Processes / Details |
| Resources page | System-Monitor → Performance |
| File Systems page | System-Monitor → File Systems |
| Refresh now | System-Monitor shell |
| Process search | Processes / Details |
| Remember current page | System-Monitor private UI state |
| Remember window geometry/maximised state | System-Monitor private UI state |
| Remember table sort/order/width/visibility | Owning System-Monitor view |
| Command-line/direct open Processes | System-Monitor shell/deep link |
| Command-line/direct open Resources | System-Monitor → Performance |
| Command-line/direct open File Systems | System-Monitor → File Systems |
| Version output | System-Monitor application identity |
| Help | System-Monitor Help |
| Keyboard shortcuts | System-Monitor shell |
| About | System-Monitor metadata; Common may provide presentation mechanics |
| Quit/lifecycle | System-Monitor shell |

### Process scopes, search and hierarchy

| Mint function | Destination |
| --- | --- |
| Active Processes | Details → process-scope filter |
| All Processes | Details → process-scope filter |
| My Processes | Details → process-scope filter using portable ownership identity |
| Search process name | Processes + Details |
| Search user | Processes + Details |
| Search PID | Processes + Details |
| Search command arguments | Details |
| Show Dependencies | Details → Flat/Tree toggle |
| Parent/child inspection | Process Inspector → Process Family |
| Multi-process actions | Details |

Mint's Show Dependencies is not a reason for another page. Details already owns
parent PID and technical process hierarchy; the friendly Processes page can
remain application-oriented.

### All 27 Mint process columns

A hidden-by-default column is still a feature and is included here.

| Mint column | Destination | Current 1.0.128 evidence |
| --- | --- | --- |
| Process Name | Details: Name | Covered |
| User | Details: User | Covered |
| Status | Details: Status | Covered |
| Virtual Memory | Details + Inspector | Covered by Linux technical process enrichment |
| Resident Memory | Details: RAM + Inspector | Covered as RSS |
| Writable Memory | Details + Inspector | Covered from Linux smaps_rollup when available |
| Shared Memory | Details + Inspector | Covered from Linux resident shared/file accounting when available |
| X Server Memory | Not reproduced as a normal field | Deliberately superseded: upstream 45.0.2 builds with WNCK disabled by default and its own build option warns that enabling it is unstable; modern Wayland sessions have no equivalent X-server allocation concept |
| % CPU | Details: CPU | Covered |
| CPU Time | Details: CPU time + Inspector | Covered |
| Started | Details: Start time + Inspector | Covered |
| Nice | Details + Inspector | Covered with exact Linux nice value plus portable priority classes |
| ID | Details: PID | Covered |
| Security Context | Details + Inspector | Covered through the active Linux procfs security attribute when available |
| Command Line | Details: Command + Inspector | Covered |
| Memory | Details: Memory % / RAM | Covered with stronger split values |
| Waiting Channel | Details + Inspector | Covered through procfs wchan |
| Control Group | Details + Inspector | Covered as a selectable technical column and Inspector property |
| Unit | Details + Inspector | Covered from unified-cgroup/systemd identity |
| Session | Details + Inspector | Covered when login-session metadata is available |
| Seat | Details + Inspector | Covered when login-session metadata is available |
| Owner | Details + Inspector | Covered and kept distinct from the process user where the platform exposes it |
| Disk read total | Details: Read total | Covered |
| Disk write total | Details: Written total | Covered |
| Disk read | Details: Read/s | Covered |
| Disk write | Details: Write/s | Covered |
| Priority | Details: Priority + Inspector | Covered at portable product level |

Current Details also has fields beyond Mint: Parent PID, Threads, GPU,
GPU engine, GPU memory, handle count, context switches, page faults, elapsed
time and executable path. Those stay. Mint parity is a floor, not a ceiling.

### Process Properties

Mint's Process Properties dialog maps into the existing Process Inspector:

- name, PID, user/UID and status → Inspector Overview;
- CPU %, CPU time and start time → Overview/Performance;
- memory, virtual, resident, writable, shared and conditional X-server memory →
  Overview/Performance;
- nice/priority → Overview + process-control strip;
- security context, command line, waiting channel and control group →
  Overview/technical details.

### Per-process Open Files

Mint lists FD, type and object for a selected process and distinguishes regular
files, pipes, IPv4 sockets, IPv6 sockets, local sockets and unknown objects.

**Owner:** `System-Monitor → Process Inspector → Open Files`.

This should stay native to System Monitor and should not depend on launching the
external `lsof` command.

### Find which process is using a file

Mint's system-wide Search for Open Files is a separate workflow: start with a
filename fragment, optionally ignore case, and return Process, PID and Filename.

**Owner:** `System-Monitor → Tools → Find process using file`.

Do not merge this with Inspector → Open Files. One begins with a file; the
other begins with a process. Current System Monitor Help already documents the
file-to-process workflow.

### Memory Maps

Mint exposes:

- Filename;
- VM Start;
- VM End;
- VM Size;
- Flags;
- VM Offset;
- Private clean;
- Private dirty;
- Shared clean;
- Shared dirty;
- Device; and
- Inode.

**Owner:** `System-Monitor → Process Inspector → Memory Map`.

The current Memory Map tab is the correct home. The list above is the Linux
technical parity checklist.

### Process control

| Mint action | Destination |
| --- | --- |
| End/Terminate (SIGTERM) | Processes ordinary End task; Details/Inspector advanced control |
| Stop (SIGSTOP) | Details + Inspector → portable Suspend |
| Continue (SIGCONT) | Details + Inspector → portable Resume |
| Kill (SIGKILL) | Details + Inspector → Force Terminate |
| Confirm before end/kill | System-Monitor Preferences → Process control |
| Very High/High/Normal/Low/Very Low priority | Details/Inspector |
| Custom Unix nice value -20…19 | Inspector → Advanced priority on Linux |
| Run on all CPUs | Inspector → CPU affinity |
| Select allowed CPUs | Inspector → CPU affinity |
| Privilege escalation for a specific action | System-Monitor process-control backend; never run the whole monitor privileged |

The current portable priority model already owns the cross-platform categories.
If exact custom nice parity is added, the Unix number remains a Linux-native
advanced field rather than polluting the portable enum.

### Process-state meanings

Mint displays/explains Running, Sleeping, Stopped and Zombie.

**Owner:** native state collection in the System-Monitor process backend,
display in Details/Inspector, explanation in System-Monitor Help.

Additional native states are fine, but these meanings must not be collapsed
into an ambiguous generic state.

### CPU resources

| Mint function | Destination | Current disposition |
| --- | --- | --- |
| CPU history graph | Performance → CPU | Covered |
| Per-core/logical CPU activity | Performance → CPU | Covered/richer |
| Current CPU percentages | Performance → CPU | Covered |
| CPU colours | Canonical Infiltrator graph palette | Deliberately superseded by suite-wide semantic colours rather than per-core arbitrary colour pickers |
| Stacked CPU area chart | Preferences → Graphs → CPU | Covered |
| Smooth/line graph choice | Preferences → Graphs | Covered |
| Resource graph update interval | Preferences → Sampling | Covered by refresh-speed choice |
| Chart data-point/history length | Preferences → Sampling/Graphs | Covered without discarding retained history |
| Divide process CPU by CPU count (“Solaris mode”) | Preferences → Process CPU scale | Covered by whole-machine max-100% mode |
| Per-core/Irix-style process CPU | Preferences → Process CPU scale | Covered by mode allowing >100% |

The current explanatory labels are clearer than exposing the historical name
“Solaris mode” as the primary UI term.

### Memory and Swap resources

| Mint function | Destination |
| --- | --- |
| Memory history | Performance → Memory |
| Used memory/percentage/total | Performance → Memory |
| Cache amount | Performance → Memory composition/details |
| Swap used/percentage/total | Performance → Memory |
| Memory and swap colours | Preferences → Graph appearance if admitted |
| Process-memory IEC units | Preferences → Units → Process memory |
| Resource-memory/swap IEC units | Preferences → Units → Memory/Swap |
| Logarithmic memory graph | Preferences → Graphs → Memory |
| Smooth graph choice | Preferences → Graphs |

These are System Monitor presentation settings, not System Settings settings,
unless the suite deliberately creates an OS-wide measurement-unit policy.

### Network resources

| Mint function | Destination | Current disposition |
| --- | --- | --- |
| Network history | Performance → Network | Covered/richer per adapter |
| Receiving rate | Performance → Network | Covered |
| Sending rate | Performance → Network | Covered |
| Total received | Performance → Network | Covered |
| Total sent | Performance → Network | Covered |
| Receive/send colours | Preferences → Graph appearance if admitted | Placement only |
| Rate in bits vs bytes | Preferences → Network units | Covered |
| Totals use same unit as rate | Preferences → Network units | Covered common path |
| Separate totals unit | Preferences → Network units → Advanced | Covered |
| Totals in bits | Preferences → Network units → Advanced | Goes with separate-total-unit control |
| Smooth graph choice | Preferences → Graphs | System-Monitor local |

### Mounted File Systems

This page stays in **System Monitor**, not Filesystem-Support, Defragmenter or
InfiltratorFS.

| Mint function/column | Destination | Current evidence |
| --- | --- | --- |
| Device/source | File Systems: Device or source | Covered |
| Directory/mount point | File Systems: Mount point | Covered |
| Type | File Systems: Type | Covered |
| Total | File Systems: Total | Covered |
| Free | File Systems: Free | Covered distinctly from Available |
| Available | File Systems: Available | Covered |
| Used | File Systems: Used | Covered |
| Used percentage | File Systems: Use | Covered |
| Normal/user filesystems by default | File Systems default | Covered |
| Show all virtual/system filesystems | File Systems toggle + Preferences default | Covered |
| Respond to mount add/change/remove | File Systems collector | System-Monitor backend responsibility |
| Separate filesystem refresh interval | Preferences → Sampling → File Systems | Placement if exact Mint control is retained |
| Filesystem column visibility/order/sort | File Systems → Columns/View state | Covered with persistent configurable columns |
| Activate row to open mount directory | File Systems row action | Covered through the normal desktop file manager |

Physical disk I/O belongs in Performance → Disks/Partitions. Fragmentation
belongs in Defragmenter. Driver/install support belongs in Filesystem-Support.

### Refresh and adaptive sampling

Mint has process, graph and filesystem intervals plus an optional “smooth
refresh” algorithm that adjusts process refresh based on the monitor's own CPU
cost.

**Owner:** System-Monitor scheduler/preferences.

The current architecture's asynchronous collectors, completed-snapshot
generations, hidden-page suppression and bounded presentation may supersede the
exact Mint algorithm. The behavioural requirement remains: refresh choices must
not make the monitor itself wasteful, slow collection must not block GTK,
hidden views must not cause pointless redraw work, and explicit Refresh must
produce a fresh coherent view.

Nothing here belongs in System Settings.

## Exact preference ownership

| Mint preference | Destination |
| --- | --- |
| Process update interval | System-Monitor Preferences → Sampling |
| Smooth refresh | System-Monitor scheduler/preferences |
| Alert before ending/killing | System-Monitor Preferences → Process control |
| Divide CPU usage by CPU count | System-Monitor Preferences → Process CPU scale |
| Process memory IEC | System-Monitor Preferences → Units |
| Process information fields | Details → Columns |
| Resource update interval | System-Monitor Preferences → Sampling |
| Chart data points | System-Monitor Preferences → Sampling/Graphs |
| Smooth graphs | System-Monitor Preferences → Graphs |
| CPU stacked chart | System-Monitor Preferences → Graphs → CPU |
| Memory/swap IEC | System-Monitor Preferences → Units |
| Logarithmic memory | System-Monitor Preferences → Graphs → Memory |
| Network speed bits | System-Monitor Preferences → Network units |
| Separate network totals unit | System-Monitor Preferences → Network units → Advanced |
| Network totals bits | System-Monitor Preferences → Network units → Advanced |
| CPU/memory/swap/network colours | System-Monitor Preferences → Graph appearance if admitted |
| File-system update interval | System-Monitor Preferences → Sampling |
| Show all file systems | File Systems toggle + Preferences default |
| File-system information fields | File Systems → Columns |
| Window/current-page/table state | System-Monitor private persistence |

**System Settings is not the home for another application's preferences.**

## Native/system metadata ownership

| Upstream information source | Information | Infiltrator placement |
| --- | --- | --- |
| `/proc/<pid>/cgroup` | Control group | Linux process enrichment → Details/Inspector |
| systemd/logind APIs | Unit, session, seat, owner | Linux process enrichment → Details/Inspector |
| SELinux context | Security Context | Generalise to authoritative active Linux security context/profile → Details/Inspector |
| proc kernel state | Waiting Channel | Linux process enrichment → Details/Inspector |
| process memory APIs | virtual/resident/shared/writable | Linux process accounting → Details/Inspector |
| process I/O APIs | read/write totals/rates | process backend → Details/Inspector |
| scheduler APIs | nice/priority | process backend/control → Details/Inspector |
| affinity APIs | CPU mask | process backend/control → Inspector |

Platform-specific concepts must remain explicitly unavailable where there is no
meaningful equivalent. Do not fake values to fill a cross-platform table.

## Help topics: functions versus documentation

Several upstream help pages are explanatory, not executable features.

| Upstream help subject | Destination |
| --- | --- |
| What is a process? | System-Monitor Help → Processes |
| Running/Sleeping/Stopped/Zombie | System-Monitor Help → Processes |
| Find CPU/memory hogs | System-Monitor Help → Troubleshooting; link to Details sorting |
| End vs Kill | System-Monitor Help → Process control |
| Nice/priority explanation | System-Monitor Help → Process control |
| CPU/core explanation | System-Monitor Help → Performance |
| Memory vs swap | System-Monitor Help → Performance |
| Memory-map explanation | System-Monitor Help → Inspector |
| Network bits vs bytes | System-Monitor Help → Performance/Units |
| Mount/filesystem/capacity explanation | System-Monitor Help → File Systems |
| IEC units | System-Monitor Help → Units |
| `top`, `lsof`, `free`, `vmstat`, `df`, `pmap` | Documentation only; do not shell out merely because Mint help names them |
| Uninstall apps to free space | Handoff/deep-link to Software if desired; package removal belongs to Software |
| Fragmentation | Handoff to Defragmenter; never fold it into File Systems |
| Filesystem support/driver installation | Handoff to Filesystem-Support |

## The 22-repository suite

| # | Repository | Ownership of Mint System Monitor functions |
| ---: | --- | --- |
| 1 | **Infiltrator-Libraries** | No user-visible monitor function; generic reusable primitives only |
| 2 | **Calendar** | None |
| 3 | **System-Monitor** | **Primary owner of the audited feature set** |
| 4 | **Defragmenter** | None; fragmentation/defrag only |
| 5 | **InfiltratorFS** | None of the generic monitor functions; filesystem implementation/check/scrub only |
| 6 | **Character-Profiler** | None |
| 7 | **MBLINK** | None |
| 8 | **Jaglink** | None |
| 9 | **LINK** | None |
| 10 | **Infiltrator-Repository** | No runtime feature; release/package publication only |
| 11 | **BMWLink** | None |
| 12 | **AUDILINK** | None |
| 13 | **FORDLINK** | None |
| 14 | **RunnerScope** | None of the host System Monitor feature set; runner/workflow monitoring is a separate domain |
| 15 | **Egypt** | None |
| 16 | **backyard-racer** | None |
| 17 | **ssmithnet.net** | No runtime feature; documentation/distribution only |
| 18 | **Calculator** | None |
| 19 | **Software** | Package/app management only; receives handoffs for uninstall/update tasks |
| 20 | **System-Settings** | OS-wide policy only; no System Monitor-local preference |
| 21 | **Filesystem-Support** | Filesystem-support/driver capability only; not mounted-capacity monitoring |
| 22 | **Infiltrator-OS** | OS integration/default selection/meta-packaging only |

## Implementation status at 1.0.130

The replacement ledger is now closed for the audited Mint 45.0.2 behaviour.
The implementation deliberately preserves the stronger Infiltrator information
architecture instead of reproducing the older three-tab application literally.

### Implemented equivalents or stronger replacements

- All / Active / My process scopes and flat/dependency-tree Details views;
- every normally meaningful Mint technical process field: virtual, resident,
  writable and shared memory, exact Linux nice, waiting channel, cgroup,
  systemd-derived unit/session/seat/owner and active Linux security context;
- portable priority classes plus exact Linux custom nice values, CPU affinity,
  terminate/suspend/resume/force-terminate and configurable action confirmation;
- Process Inspector Open Files with file, pipe, local-socket, IPv4-socket,
  IPv6-socket, anonymous-inode and kernel-object classification when procfs
  exposes the corresponding namespace tables;
- Process Inspector Memory Map with filename, VM start/end/size, flags, offset,
  private clean/dirty, shared clean/dirty, device and inode. Detailed residency
  accounting comes from `smaps`; restricted systems fall back to `maps` and
  report the unavailable accounting explicitly;
- reverse “find process using file” lookup based on device/inode identity;
- CPU, Memory and Network histories, configurable visible history length,
  smooth/line rendering, stacked CPU presentation, logarithmic Memory
  presentation and independent network-total units;
- mounted File Systems with Total, Free, Used and Available kept distinct,
  show-all system mounts, independent refresh cadence, persistent configurable
  columns and direct mount-point activation;
- stronger existing Infiltrator capabilities including Overview, per-device
  Performance pages, Process Family, Threads, GPU/process GPU fields, PSI,
  hardware telemetry, App History, Services, Users and Startup Apps.

### Deliberate supersessions rather than omissions

- **X Server Memory:** not treated as a required 45.0.2 baseline field. Upstream
  disables its WNCK support by default and its own build option warns that
  enabling it is unstable. It also has no honest Wayland-wide equivalent.
- **Per-series arbitrary graph colour pickers:** replaced by the suite's
  canonical semantic Infiltrator/Common palette so the same metric carries the
  same visual meaning throughout the product.
- **IEC on/off memory toggles:** Common already owns one suite-wide memory and
  storage policy: binary base-2 scaling with the project's established compact
  B/KB/MB/GB labels. System Monitor does not create a second contradictory
  unit policy merely to duplicate an older preference switch.
- **GNOME's adaptive “smooth refresh” algorithm:** superseded by System
  Monitor's asynchronous/coalescing collectors, completed-snapshot generations,
  active-page presentation suppression and explicit independent cadences. The
  user-facing requirement—responsive monitoring without the monitor becoming
  the workload—is retained without copying that implementation.
- **GNOME UI geometry and three-tab composition:** not copied. Functional
  replacement is integrated into Overview, Performance, Processes, Details,
  Inspector and File Systems according to this document.

## Replacement acceptance rule

A Mint function is replaced when:

1. the same task, or a strictly richer form of it, can be completed in the
   assigned Infiltrator surface;
2. collection/control uses System Monitor's platform contracts rather than
   invoking GNOME System Monitor or parsing helper commands when a stronger
   native interface exists;
3. unavailable or permission-denied data is explicit rather than disguised as
   zero;
4. privilege is requested only for the specific action that requires it;
5. the feature fits the current System Monitor navigation instead of creating a
   duplicate top-level application;
6. implementation receives appropriate backend/presentation regression tests;
   and
7. Help describes Infiltrator behaviour in Infiltrator terminology.

## Source-use boundary

This audit uses upstream source to determine behaviour and coverage. It is not
permission to transplant upstream source, UI definitions, help prose or
implementation structure. Future implementation should continue System
Monitor's existing first-principles/native-interface architecture and preserve
the repository's own provenance and licence requirements.

## Maintenance

When a baseline capability changes, regresses or gains a stronger replacement,
update the corresponding row here with evidence. If ownership itself changes,
update [Architecture](ARCHITECTURE.md) in the same change.

This specialist audit complements [Design](DESIGN.md) and
[Roadmap](ROADMAP.md). It is an ownership/completeness ledger, not an
instruction to clone the competitor UI.
