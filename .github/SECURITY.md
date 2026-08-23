<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Security Policy

## Supported versions

Security fixes are applied to the current `main` branch and, where appropriate, the latest published release. Older releases should not be assumed to receive security backports.

## Reporting a vulnerability

Please do not open a public issue for a vulnerability that could expose users, local system data, credentials, private process information, package/install integrity, release infrastructure or other sensitive material.

If GitHub private vulnerability reporting is available for this repository, use the repository's **Security** reporting flow. Otherwise, report the issue privately to `infiltratr@yandex.com` with the subject `Linux System Monitor security report`.

Include enough information to reproduce and assess the problem where possible: affected version or commit, Linux distribution, desktop/session context, privilege level, affected hardware or subsystem, impact, reproduction steps, relevant logs, and any suggested mitigation. Remove unrelated private information from logs or screenshots.

## Security-sensitive areas

Reports are especially useful for problems involving:

- process-control operations or privilege boundaries;
- unsafe handling of device, procfs or sysfs data;
- local file export or durable-write behaviour;
- D-Bus or native library interaction;
- native installer or Debian package integrity;
- release automation, artifact identity or dependency pinning;
- crashes or memory-safety faults reachable from untrusted local data.

## Handling

Reports will be assessed before public disclosure. Please allow maintainers a reasonable opportunity to investigate and prepare a fix. Reporters are asked not to access data or systems beyond what is necessary to demonstrate the issue and not to perform testing that could damage third-party systems or data.

The project does not treat normal absence of privileged telemetry as a vulnerability. Linux System Monitor deliberately has no project-owned privileged helper or daemon; optional information that the current user cannot read should degrade to unavailable.
