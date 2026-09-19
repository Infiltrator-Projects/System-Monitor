# Changelog

This changelog records user-visible, compatibility, architecture and validation changes for System Monitor. Detailed commit-by-commit history remains in Git.

## Unreleased

No unreleased changes.

## 1.0.50 - 2026-09-20

- Match the visible System Monitor hierarchy more closely to the established InfiltratorFS/Common Night presentation instead of merely sharing its palette values.
- Move the performance sidebar onto the Common panel layer and change selected device rows to the InfiltratorFS selection treatment: graphite selection fill, normal border and a narrow cyan leading edge.
- Replace boxed cyan notebook selection with the same restrained cyan underline treatment used by the InfiltratorFS stack switcher.
- Restore Common's dedicated button background and foreground roles for ordinary GTK controls, leaving the darker operation surface for components that semantically require it.
- Render primary performance graphs on the Common card surface while keeping compact sidebar graphs on the quieter surface layer, with softer grid/fill emphasis and neutral graphite frames.
- Apply Common detail-label, heading, kicker, summary and selected-summary roles to performance captions, values and device-row metadata so text hierarchy matches the rest of the Infiltrator family.
- Remove the remaining hard-coded purple memory-composition frame and resolve its background/border through the active Common palette.
- Preserve the correct Common foreground roles through normal, hover, checked and disabled button states so custom dark navigation rows never inherit the light-button foreground colour.

## 1.0.49 - 2026-09-20

- Refine GTK Night presentation to follow MBLINK's visual hierarchy rather than only sharing its raw colour values.
- Keep ordinary buttons and controls on graphite operation surfaces with restrained borders; reserve cyan for selection, interaction emphasis and product metrics.
- Remove always-cyan performance navigation borders so unselected device rows remain visually quiet and selected rows use MBLINK's translucent cyan treatment.
- Render performance graphs on Common surface layers with subtle graphite frames and reduced grid emphasis instead of metric-coloured outer boxes.
- Give the performance split panes explicit theme identities so the sidebar, content canvas and separator retain the intended layered graphite structure.
- Keep menus, tabs, scrollbars, text surfaces and About-dialog controls within the same Common 1.19.10 MBLINK-derived Night hierarchy.
- Build the generated GTK theme stylesheet in strict-C-safe fragments so the richer visual contract remains portable under the project's warning-as-error checks.

## 1.0.48 - 2026-09-20

- Pin the project and gitlink to Common 1.19.10 at `33e69c0a462b56d388881d89c4eb49f72fa0b0fe`.
- Replace the temporary single-grey Night override with Common's complete Linux MBLINK reference palette.
- Use the MBLINK canvas, titlebar, connection-bar, card/surface, border, heading/summary and accent-hover roles directly so Night retains the layered graphite appearance instead of collapsing into one black or grey field.
- Style System Monitor summary frames and general frames with those same semantic layers while keeping product-specific performance colours local.

## 1.0.47 - 2026-09-20

- Restore the established MB graphite-grey Night shell instead of presenting the near-black Common canvas as the application background.
- Keep Common 1.19.8 authoritative for semantic component colours and typography while mapping the top-level Night shell to the existing #2B2B30 System Monitor graphite used by graph/drawing fallback rendering.
- Apply the same graphite shell when Follow system resolves a dark host theme, so explicit Night and system-dark presentation remain visually consistent.
- Validate the resolved Common palette and typography before deriving the shell background so the appearance path remains fail-safe.

## 1.0.45 - 2026-09-20

- Complete a second forensic Common 1.19.8 usage pass without changing Common itself.
- Source MB Corpo family names and role weights from Common's typography contract, and use Common's canonical theme names where they match System Monitor's UI wording.
- Replace remaining equivalent private counter-rate arithmetic, allocation-growth overflow checks, bounded string-copy formatting and simple lexical path joining with Common contracts while preserving Linux, hardware and presentation policy locally.

## 1.0.44 - 2026-09-19

- Complete the forensic Common 1.19.8 integration by routing strict numeric-token conversion, checked addition, prefix checks, quantity parsing, trimming and lexical-basename handling through Common where its contract is equal or stronger.
- Keep Linux procfs/sysfs grammars, pthread absolute-deadline semantics, hardware policy and GTK presentation local where Common deliberately does not own them.
- Preserve all existing functionality while reducing private generic mechanics and making the Common ownership boundary explicit.

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
