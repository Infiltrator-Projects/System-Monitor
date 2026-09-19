<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Security

## Supported source

Security fixes target current `main` and, where appropriate, the latest published release. Older releases should not be assumed to receive backports.

## Reporting

Do not open a public issue for a vulnerability that could expose users, local system data, credentials, private process information, package integrity or release infrastructure.

Use GitHub private vulnerability reporting when available. Otherwise contact `infiltratr@yandex.com` with the subject `System Monitor security report`.

Include the affected version/commit, Linux distribution, desktop/session context, privilege level, affected subsystem/hardware, impact and reliable reproduction. Sanitise unrelated private information.

## Security-sensitive boundaries

Particular attention belongs to:

- process-control operations, affinity/priority and privilege boundaries;
- procfs/sysfs/ioctl/D-Bus/device input parsing;
- local export and durable-write behaviour;
- native dynamic-library and driver interaction;
- package/release integrity and dependency pinning;
- memory-safety faults reachable from untrusted local state.

Absence of privileged telemetry is not itself a vulnerability. The application deliberately degrades inaccessible information to unavailable rather than installing a hidden privileged helper.

## Response

Security defects are correctness defects. Reproduce, add regression coverage where practical, fix the underlying boundary and test the environment actually affected.

## Disclosure

Public details should follow a fix or clear mitigation. Testing must not damage or access third-party systems/data without authorisation.