<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Security

Security fixes target current `main` and, where appropriate, the latest published release. Older releases should not be assumed to receive backports.

## Reporting

Do not open a public issue for vulnerabilities that could expose users, local data, credentials, private process information, package integrity or release infrastructure.

Use GitHub private vulnerability reporting when available. Otherwise contact `infiltratr@yandex.com` with the subject `System Monitor security report`.

Include the affected version/commit, environment, privilege level, subsystem, impact and reliable reproduction. Remove unrelated private information.

## Boundaries

The installed Linux product is one GUI executable with no project-owned privileged daemon or helper. Kernel, driver, D-Bus and configuration data is untrusted input.

Process control uses native operating-system permissions. Optional privileged or vendor telemetry becomes unavailable rather than triggering implicit elevation.

Bluetooth traffic monitoring uses only the `CAP_NET_RAW` file capability needed to open the read-only HCI monitor channel. The endpoint is opened during bootstrap, capability sets are then cleared, and failure to drop them aborts startup. The monitor path sends no HCI commands.

Security-sensitive boundaries include process control, external-input parsing, durable writes and exports, native library/driver interaction, package/release integrity and memory safety. Fix defects at the underlying boundary and add regression coverage where practical. Do not test against third-party systems or data without authorisation.
