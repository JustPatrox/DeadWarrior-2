# Economy and defensive-order correction

The ten-mission campaign keeps the original finite gold deposits and untinted castle defenders. This correction translates missing miner-command and formation behavior and adds the requested player-only idle orientation.

## Gold

Original `Gold.mine` removes a bounded amount from a deposit, then scales its visible ore by remaining amount divided by its initial total. The original total is 3500 in the supplied data. Very small remaining amounts can be visually obscured by the unchanged foreground rock or become smaller than a pixel. This presentation is preserved.

An exhausted deposit returns zero gold. A miner can still deposit an already carried bag afterward, and researched passive income continues independently of mines. The native probe checks the last one-gold stroke, no further extraction, one carried-bag deposit, and Passive Income I's five-gold award over its original 150-tick interval.

Ore hit testing now follows `Gold.hitTest` using the current depleted width, rather than an unchanged full-deposit rectangle.

## Miner commands

The previous native command accepted a mining target only if it could immediately reserve a slot. When the clicked deposit had two miners, this left the old prayer order active.

Original `UnitAi` changes `MinerAi.targetOre` independently of slot availability. `MinerAi` waits beside a full deposit and searches the home-side mines for a free spot. The native command now follows that distinction, immediately exits the prayer pose, and either uses another available deposit or waits for a slot. A miner returning with a full bag also releases its spot, as in the original. Automatic searches remain on the miner's own half of the map; explicit clicks can still target another deposit.

## Castle archer

The original ordinary enemy tint is applied by `Team.spawn`. `CastleArchers.addUnit` constructs its defender separately and does not apply that tint. Original-player snapshots at both resolutions report zero red, green, and blue offsets, and show a black archer with its castle helmet. The native appearance is therefore preserved. The new probe guards against introducing the regular enemy tint here.

## Defence

The native defence now executes the original separate MOVE and ATTACK_MOVE groups. Each group uses `UnitMove.formationOrder`: descending AS3 unit type, then ascending spawn ID. The six-row geometry, alternating inside rows, castle height reduction, and integer command coordinates follow the recovered controller. Captured original targets are checked by `tools/fixtures/original-defence.tsv`.

At rest, only the player's defenders face right, as requested. Attacking, moving elsewhere, hold/stop commands, and enemy units do not inherit that override.

## Verification

Original-player gold depletion, prayer-to-full-mine commands, defender color, and mixed formation references passed at 1280x720 and 1920x1080. Native regression cases cover finite resources, deposits and passive income, free-alternative and all-spots-full retargeting, a subsequently freed slot, original formation goals, and player/enemy facing isolation.

The real Windows game also passed click-driven prayer-to-mining and retreat-to-defence checks at both resolutions. Its six new screenshots contain no exact or tinted diagnostic clear. Logs and snapshots are under `work/economy-orders-*` and `evidence/windows-economy-orders/`. The full CTest suite now contains twelve checks. Wider original-game combat parity and the remaining campaign are still in progress.
