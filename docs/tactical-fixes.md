# Tactical commands, support units and the center tower

These corrections preserve the current ten-mission campaign. Remaining missions and full original-game parity are still in progress.

| Original source | Native translation |
| --- | --- |
| UnitAi.setCommand / HoldCommand / targeted MoveCommand | A direct enemy target replaces Hold, including both Giant types. |
| MonkAi / Monk.stateFixForCutToWalk | A plain Move suppresses new automatic casts; a live heal or cure finishes its event and then resumes the pending movement, including global Attack/Defend orders. |
| MonkAi.baseUpdate / SpatialHash.mapInArea | Allied Merics retain the original squared support distance of 50000. Healing candidates come from the original cell query, rather than an extra exact 100-unit rectangle. |
| UnitAi.baseUpdate, non-mage branch | Melee pursuit stops lateral convergence inside the original 40-unit attack lane; caster movement follows its separate rules. |
| Spearton / SpeartonAi / BlockCommand | Q or the original shield icon toggles Block when its campaign upgrade is unlocked. The stance holds frame 15 and exits over 15 updates. Movement and Hold leave the stance; Stand preserves it. |
| Hill / HillControlBar / Team.spawnMiddleUnit | Original capture bar, contested ownership, passive tower income, delayed ghost spawn and replacement. |
| CampaignUpgradeScreen / CampaignUpgrade.parents | Purchases require the entire root chain, including when an older save contains an inconsistent child. Existing saves are retained. |
| SoundManager.playSoundInBackground | Requesting the current music track preserves its playhead, including entry into the WIP screen. |

Block uses the original 30-frame animation, bitmap, embedded-font tooltip and green/red toggle dot. The original source reduces damage from a unit/projectile by 40%; its separate raw-damage path performs an additional integer conversion and multiplier. Prepared original readback verifies 12 to 7.2 and raw 50 to 18. The tooltip keeps the original three measured text lines because FFDec's automatic HTML wrapping clips that particular description.

Tower ownership takes 301 prepared updates from neutral, including the original floating-point boundary. The 60 by 10 control bar sits at original y -206.05 relative to the tower. Tower Spawn I emits an uncontrolled Spearton; II emits an Enslaved Giant. The original strict 500-frame delay applies to the first spawn and replacement after death. Ghosts cost no resources or army population, ignore selection and army commands, and still contribute team vision. Their original blue transform, cyan glow, spawn and drip effects are exported separately, preserving ordinary actor artwork.

Meric healing and researched Cure start enabled, as in Monk.init. The current original EnemyGoodTeamAi grants Cloak and the Magikill spells, not MONK_CURE; the player's upgrade does not grant enemy research or spend the player's mana on an enemy spell.

Prepared original references at 720p and 1080p cover Giant command replacement, Meric cast/movement priority, support distance and spatial healing, Block frames/damage, melee lanes, tower capture and both ghost tiers. The 19 deterministic checks include these cases and all ten mission simulations. Some battle probes now use explicitly prepared enemy funds to guarantee spell coverage; the separate unmodified campaign victory and castle-fire checks remain. A fixed automated purchase/order strategy is not a proof of original battle balance.

Windows checks use isolated saves and assets. Published packages and player installations are changed only after the corresponding validation checkpoint. Portable release builds must pass the same deterministic suite and executable reports before publication.
