# Changelog

This changelog records user-visible, compatibility, architecture and validation changes for System Monitor. Detailed commit-by-commit history remains in Git.

## Unreleased

No unreleased changes.

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
