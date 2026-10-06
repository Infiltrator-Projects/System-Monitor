<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Security

Security fixes target current `main` and, where appropriate, the latest published release. Older releases should not be assumed to receive backports.

## Reporting

Do not open a public issue for a vulnerability that could expose users, local system data, credentials, private process information, package integrity or release infrastructure.

Use GitHub private vulnerability reporting when available. Otherwise contact `infiltratr@yandex.com` with the subject `System Monitor security report`.

Include the affected version/commit, environment, privilege level, affected subsystem or hardware, impact and reliable reproduction. Sanitise unrelated private information.

## Security boundaries

The installed Linux product is one GUI executable with no project-owned privileged daemon or helper. Kernel, driver, D-Bus and configuration data is untrusted external input.

Process control uses the native operating-system permission model. Optional privileged or vendor-specific telemetry degrades to unavailable instead of triggering implicit elevation.

Bluetooth traffic monitoring uses only the `CAP_NET_RAW` file capability required to open the read-only HCI monitor channel. The endpoint is opened during bootstrap and process capability sets are cleared before normal GTK and monitoring work begins; failure to drop them aborts startup. The monitor path issues no HCI commands, resets or controller reconfiguration.

Security-sensitive code includes process-control permissions; procfs/sysfs/ioctl/D-Bus/device input parsing; local export and durable writes; native library/driver interaction; package/release integrity; and memory-safety faults reachable from untrusted local state.

Security defects should be fixed at the underlying boundary and covered by regression tests where practical. Public details should follow a fix or clear mitigation. Testing must not damage or access third-party systems or data without authorisation.
