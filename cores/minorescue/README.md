# Minotaur Rescue gameplay audit

The rescue/fire-rate report exposed a position/velocity mix-up in
`spaceCraftGun::run`. Original firing cadence is:

```
effectiveRescues = blackHole ? 10 : weapon
interval = int((sunOn ? 3 : 5) + float(length(position)) * max(5, 30 - 2 * effectiveRescues))
relativeShotSpeed = 0.5f - 0.5f * float(length(position))
```

The port used velocity length in both expressions. This produced almost the
minimum interval regardless of rescues. At radius 0.5 in solar mode, zero, five
and ten rescues yield intervals of 18, 13 and 8 ticks. Movement velocity is still
added to each projectile; it does not determine the firing timer. Black-hole
cadence uses ten rescues even if the ship's actual projectile power is higher.

The audit also corrected premature binary32 arithmetic in solar rescue scoring
and sun growth/reset. The original rounds the rescue radius to float, forms its
centre-distance multiplier in double, then stores it as float. For example, a
first-level rescue at radius 0.5 gives 22 points, rather than the port's 21.
Sun mass/radius growth, collapse comparison and end-of-level relaxation likewise
retain their original double operations around float stores.

## Coverage

- Original-execution tables check ship/tank firing, rescue rewards, extra lives,
  shield damage/respawn, entity wrapping, gravity, sun collapse and level-end
  sun recovery. See [fixture provenance](fixtures/README.md).
- The source audit compared projectile ownership/power, tank pickup bursts and
  hit handling, jet damage, planet split/respawn quotas, saucer/mine behavior,
  achievement gates and the solar/deep-space/tank/jet progression branches with
  the original exports and instruction evidence. These reviews are separate
  from the fixture-backed execution checks; they do not establish exhaustive
  campaign equivalence.
- Original quirks are retained where observed: score awards crossing several
  extra-life thresholds grant one life per score call; saucer speed-table integer
  divisions stay integer divisions; mine detonation's displayed combo does not
  itself call the score routine. Modern native input, local scores, zero-distance
  gravity protection and the documented shortest-path turning correction remain
  deliberate host adaptations.

For play validation, compare solar/deep-space firing before and after several
rescues at a similar distance from the centre. Also check weapon reset on death,
black-hole transitions, level-end recovery and tank pickup bursts. The automated
tables isolate mechanics; physical input/display/audio behavior still needs
normal platform testing.
