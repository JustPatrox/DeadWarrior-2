# Selection, enemy response and Castle Archers

A click now picks one living friendly unit, choosing the frontmost body when sprites overlap. The same stable depth order is used for drawing and picking. Moving fewer than five logical units while holding the button remains a click; a larger held drag selects the rectangle's units. Units beneath its endpoint are no longer added outside that rectangle. Releasing the button keeps the result, and guided tutorial steps no longer expand an explicit single pick.

Explosive War can issue a macro defend order followed by its controller's attack order in the same step. The native movement command previously reset an enemy's attack animation each time, preventing a Giant from reaching its hit frame. Original `UnitAi.setCommand` skips `stateFixForCutToWalk` for AI teams. The translation now preserves an ongoing enemy strike while updating its formation goal. Completed moves also transition before target acquisition, using the original per-axis distance below five units without a velocity threshold.

Prepared original-player readback at 720p and 1080p confirms that a Giant with residual velocity 1.1 completes a nearby move and immediately starts attacking. Repeated defend/attack orders advance its animation beyond frame 120 and inflict 60 damage. These are isolated component checks; they do not claim full campaign AI parity.

Castle research now follows the campaign's three Order upgrades:

| Tier | Required campaign upgrade | Gold | Research ticks |
| --- | --- | ---: | ---: |
| I | Castle Archer I | 300 | 300 |
| II | Castle Archer II | 400 | 500 |
| III | Castle Archer III | 500 | 700 |

The original technology controller completes on the update after its countdown becomes negative: prepared completion takes 302, 502 and 702 updates. Research advances with simulation steps, including both fast-forward steps. Repeat clicks cannot restart or charge the same research twice. A fourth Order tier is unavailable.

Each defender owns its aim, bow and body animation state. Original placement is `homeX + 180 - 8*i`, battlefield depth `125*i/4`, flight height 390 and the corresponding perspective scale. Original runtime references verify all six tier/defender combinations. Defenders remain untargetable and consume no army population. The next-tier bitmap and tooltips use original artwork and embedded text; all previous PNGs remain byte-identical.

The portable suite contains 17 checks, including overlap/drag selection, interrupted Giant attacks, target replacement, research affordability/locks, original costs/timing/positions and independent firing. Windows input checks exercise overlapping tutorial soldiers and all three castle purchases at 1280×720 and 1920×1080. The SDL executable check also researches and renders all three tiers. The full campaign remains in development.
