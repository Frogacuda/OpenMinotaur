# Minotron

This core implements Minotron 2112. The local `engine/` directory contains its
game rules and platform bindings; shared mechanisms live in the top-level engine.

The release-readiness gameplay audit compares 4,224 focused cases with execution
of the original iOS ARMv7 binary and verifies 11 original level/tuning tables.
It covers shots, rescues, pickups, damage, death gating, level completion and
shared enemy movement. The audit corrected heading wrapping to retain the
original double arithmetic before its float store. No other discrepancies were
found in those cases.

See [fixture provenance and scope](fixtures/README.md) for evidence and limits.
CTest registers the checks as `minotron_gameplay_original`; the collection's
`check` build target builds and runs them with the rest of the suite. Platform
playtesting remains necessary for controls, presentation and full enemy waves.
