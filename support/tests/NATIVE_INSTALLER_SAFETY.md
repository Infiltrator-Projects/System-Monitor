# Native installer package safety fixture

`native_installer_safety.c` verifies the hardware-native installer's privilege and package boundary without installing the real System Monitor package.

The fixture's mock `dpkg --install` must model the externally visible result of a successful package transaction. The Linux package contract places the application at `/Programs/system-monitor`, so the mock installation materialises that executable before the production installer performs its post-install verification. The fixture removes only the payload it created and leaves an already-existing `/Programs` namespace intact.

The test does not alter the production installer profile or optimisation policy. Production aggressive-native installation continues to use CPU-native optimisation, LTO and trained PGO; this fixture exercises the package/privilege boundary with the portable profile solely to keep the safety test deterministic and focused.

No System Monitor executable or compatibility alias is permitted under `/usr/bin`.
