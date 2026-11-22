# GoatUp 2

## Gameplay audit

The broader gameplay audit adds 6,700 independent original-execution comparisons
and 144,000 native campaign frames with death enabled across all 80 stages. It
checks player movement/jumps, terrain interactions, all patroller variants,
sixteen machine behaviors, damage, pickups, carried-object consumption,
checkpoint restoration, death and exit decisions. Controlled live routes also
check that collection, attacks, kills and level completion remain connected.

No additional gameplay discrepancy was found in those checks after the fart
attack correction below. This is bounded evidence, not proof of complete
campaign equivalence. See [witness provenance and audit limits](tests/fixtures/ORIGINAL-GAMEPLAY.md).
CTest registers the added checks as `goatup2-original-gameplay` and
`goatup2-live-campaign-audit`.

## Fart attack collision correction

The EF173 gameplay investigation found two missing parts of the original fart
attack: its particle factory left attack power at zero, and its update ran only
the movement/lifetime routine without the enemy-contact wrapper. Particles were
visible but could not damage enemies.

Fart particles now deal one point per contact and check the enemy list after
movement and lifetime handling. Positive-power explosion particles use that
same update wrapper; zero-power decorative particles skip enemy contact. The
existing target eligibility, health, immunity, freeze and reward callbacks apply.
Particles remain alive after hitting, as in the original.

The original ARMv7 image has SHA-256
`c17852a2803d533fc73c03511c22946d4991f57328a73f21b5c592ac4334c394`.
Instruction review verifies the factory writes float `1.0` to attack power at
`0x5aaba..0x5aacc`. The virtual update at `0x5b7c0` calls movement/lifetime at
`0x5b7f0`, then calls collision at `0x9538` against the enemy list when attack
power is positive. No attack range or timing constants were adjusted.

`tests/runtime_headless.cpp` exercises the real factory and owned particle
updates through enemy damage, freeze, repeated hits, death and scoring for both
facing directions and gravity signs. Additional cases cover zero/negative
power, delayed particles and delayed/inactive/dead/distant targets. The test
failed first on zero attack power, then on missing enemy damage after correcting
the factory alone; it passes with both corrections. The existing emission
checks also verify the corrected factory field.
