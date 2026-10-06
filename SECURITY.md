<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Security

Security fixes target current `main` and, where appropriate, the latest published release. Older releases should not be assumed to receive backports.

## Reporting

Do not open a public issue for a vulnerability that could expose users, local system data, credentials, private process information, package integrity or release infrastructure.

Use GitHub private vulnerability reporting when available. Otherwise contact `infiltratr@yandex.com` with the subject `System Monitor security report`.

Include the affected version/commit, environment, privilege level, affected subsystem or hardware, impact and reliable reproduction. Sanitise unrelated private information.

## Security-sensitive boundaries

Particular attention belongs to process-control permissions; procfs/sysfs/ioctl/D-Bus/device input parsing; local export and durable writes; native library/driver interaction; package/release integrity; and memory-safety faults reachable from untrusted local state.

Unavailable privileged telemetry is not itself a vulnerability. The application deliberately reports inaccessible information as unavailable rather than installing a hidden privileged helper.

Security defects should be reproduced, fixed at the underlying boundary and covered by regression tests where practical. Public details should follow a fix or clear mitigation. Testing must not damage or access third-party systems or data without authorisation.
