"""Honor Unit.setItem's stopped equipment timelines when baking animations."""
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
CLASSES=("Miner","Swordwrath","Spearton","Archer","Ninja","Magikill","Monk","Giant",
         "EnslavedGiant","Dead","Medusa","MinerChaos","Bomber")

def freeze_default_loadouts(c,overrides=None):
    properties=set()
    for name in CLASSES:
        source=ROOT/"work/extract/scripts/com/brockw/stickwar/engine/units"/(name+".as")
        for receiver,argument in re.findall(r'([\w.]+)\.gotoAndStop\((weapon|armor|misc)\)',source.read_text(encoding="utf-8")):
            properties.add(receiver.split(".")[-1])
    items={}
    for events in c.index["timelines"].values():
        for event in events:
            if event.get("name") in properties and "id" in event:
                symbol=event["id"]
                labels={e["name"]:e["frame"] for e in c.index["timelines"].get(str(symbol),[]) if e["op"]=="label"}
                if "Default" in labels: items[symbol]=(event["name"],labels)
    result=[]
    c.loadout_variants={}
    for symbol,(name,labels) in sorted(items.items()):
        chosen=(overrides or {}).get(symbol,"Default")
        frame=labels[chosen]
        timeline=c.timeline(symbol).getFrames()
        # Keep the authored alternatives available for local overrides such as
        # CastleArchers' Basic Helmet after stopping the equipment timeline.
        c.loadout_variants[symbol]={label:timeline.get(index-1) for label,index in labels.items()}
        timeline.set(0,timeline.get(frame-1)); timeline.subList(1,timeline.size()).clear()
        result.append(dict(symbol=symbol,instance=name,label=chosen,frame=frame))
    return result
