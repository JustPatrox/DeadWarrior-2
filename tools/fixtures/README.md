# Original controller fixtures

`original-defence.tsv` contains goal coordinates read immediately after executing the recovered original `Team.defend(true)` and `UnitMove.execute` in Adobe Player. Both 1280x720 and 1920x1080 runs produced identical rows.

Columns: relative spawn index, AS3 unit type, original command type, goal X, goal Y, starting X, starting Y. Command 2 is ATTACK_MOVE; command 3 is MOVE. The fixture includes ten mixed units, two separate five-unit defence groups, same-type spawn ordering, and one unit initially inside the castle's reduced formation height.

Reproduce with `tools/reference_economy_orders.py`. Captures and detailed state reports are stored locally in `evidence/adobe-economy-orders/`. The original sources are `engine/Team/Team.as`, `engine/multiplayer/moves/UnitMove.as`, and `engine/Ai/command/UnitCommand.as` in the recovered AS3 tree.

`economy_orders_probe` compares native goals against these observed coordinates. Facing right after the player's defenders settle is a separately requested presentation change, not an original-controller observation.
