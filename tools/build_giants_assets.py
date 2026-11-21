"""Compile Massive Battle's terrain, actors and voiced campaign scene."""
from xml.etree import ElementTree as ET
import re
import subprocess
import wave
import imageio_ffmpeg
import jpype
import csv
import sys
from build_menu_assets import ArtCompiler, ROOT, WORK
from build_archidon_assets import extend, table
from build_campaign_assets import set_field


def build():
    c=ArtCompiler(ROOT/"assets/giants",loadout_overrides={2121:"Wrapped Helmet"})
    freeze_bag(c)
    data=ET.parse(next((WORK/"extract/binaryData").glob("10208_*")))
    level=ET.parse(next((WORK/"extract/binaryData").glob("29_*"))).findall("level")[5]
    rules=[]
    for symbol in (4094,4080,4087,4091,4084):
        bounds=c.swf.getCharacter(symbol).getRect()
        rules.append((f"layer.{symbol}.width",(bounds.Xmax-bounds.Xmin)/20))
        extend(c,symbol,ground=symbol==4084)
    rules.extend((("map.length",int(dict(rules)["layer.4084.width"])),
                  ("campaign.points",level.get("points")),
                  ("normalHealthScale",level.findtext("normalHealthScale")),
                  ("normalDamageScale",level.findtext("normalDamageScale","1")),
                  ("enemy.handicap",level.findtext("insane"))))
    for side in ("player","oponent"):
        rules.extend(((side+".statueHealth",level.find(side).get("statueHealth")),
                      (side+".gold",level.findtext(side+"/gold","500")),
                      (side+".mana",level.findtext(side+"/mana","0"))))
    for prefix,path in (("giant","Chaos/Units/giant"),("enslavedGiant","Order/Units/giant"),
                        ("chaosMiner","Chaos/Units/miner"),("medusa","Chaos/Units/medusa"),
                        ("dead","Chaos/Units/dead")):
        unit=data.find(path)
        for node in unit:
            if len(node) or node.text is None: continue
            try: value=float(node.text)
            except ValueError: continue
            rules.append((prefix+"."+node.tag,value))
    # MinerChaos explicitly takes these values from the Order XML branch.
    for field in ("bagSize","miningRate","manaMineRate","maxVelocity"):
        rules.append(("chaosMiner."+field,data.findtext("Order/Units/miner/"+field)))
    poses=[]; events=[]
    for owner,labels in ((7025,{"stand","run","attack_1","attack_2","death_1"}),
                         (7323,{"stand","run","mine","pray","bendDownToPray","attack_1","death_1"}),
                         (7526,{"stand","run","death_1"}),(8061,{"stand","run"}),
                         (8529,{"stand","run","attack_1","death_1"})):
        label=None
        for event in c.index["timelines"][str(owner)]:
            if event["op"]=="label": label=event["name"]
            if event.get("name")!="mc" or label not in labels: continue
            symbol=event["id"]
            if owner in (7025,7526): c.render_detailed(symbol)
            else: c.render(symbol,scale=1)
            poses.append((owner,label,symbol,*event["matrix"][4:],c.timeline(symbol).getFrameCount(),0))
            events.extend((symbol,e["frame"],e["name"]) for e in c.index["timelines"].get(str(symbol),[]) if e["op"]=="label")
    for symbol in (2318,2423,2426,3914,1314,1290): c.render(symbol,[1],scale=1)
    c.render(1904,[8,11],scale=1)
    frames=c.timeline(1904).getFrames()
    chaos=frames.get(10)
    frames.set(0,frames.get(7)); frames.subList(1,frames.size()).clear()
    c.render(1922,scale=.8,frame_offset=356)
    frames.set(0,chaos)
    c.render(1922,scale=.8,frame_offset=445)
    for symbol,value in ((292,"Order"),(293,"Giants"),(334,data.findtext("Order/Units/giant/name")),
                         (326,data.findtext("Order/Units/giant/info")),(337,level.findtext("tip"))):
        set_field(c,symbol,value)
    for symbol,depths in ((343,(2,3,19,22,25,30,31,32,34,36,40,41,57,68)),(335,(4,)),(338,(4,))):
        for depth in depths: c.timeline(symbol).getFrame(0).layers.remove(jpype.JInt(depth))
        c.render(symbol,[1],scale=1,frame_offset=5)
    # Original dialogue order and WAV sample lengths drive the voiced states.
    script=(WORK/"extract/scripts/com/brockw/stickwar/campaign/controllers/CampaignCutScene1.as").read_text()
    dialogue=re.findall(r'this\.message\.setMessage\("((?:\\.|[^"\\])*)","",0,"([^"]+)"',script)
    rows=[]
    for message,name in dialogue:
        source=next((WORK/"extract/sounds").glob(f"*_{name}_{name}.*"))
        target=c.directory/(name+".wav")
        subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(),"-v","error","-y","-i",source,"-c:a","pcm_s16le",target],check=True)
        with wave.open(str(target)) as audio: seconds=audio.getnframes()/audio.getframerate()
        rows.append((name,seconds,message.replace("\\'","'")))
    if len(rows)!=7: raise ValueError("Incomplete Massive Battle dialogue")
    table(c.directory/"textures.tsv",c.rows)
    table(c.directory/"rules.tsv",rules)
    table(c.directory/"animations.tsv",poses)
    table(c.directory/"events.tsv",events)
    table(c.directory/"dialogue.tsv",rows)
    table(c.directory/"starting.tsv",((side,n.text) for side in ("player","oponent") for n in level.findall(side+"/startingUnit")))


def freeze_bag(c):
    # EnslavedGiant.setItem holds giantbag at the loadout's Default frame.
    # Leaving this child running cycles Default/Wood/Bones/Stone every tick.
    frames=c.timeline(1950).getFrames()
    frames.subList(1,frames.size()).clear()


def refresh_enslaved():
    c=ArtCompiler(ROOT/"assets/giants",loadout_overrides={2121:"Wrapped Helmet"})
    freeze_bag(c)
    with (c.directory/"textures.tsv").open() as file:
        previous=list(csv.reader(file,delimiter="\t"))
    for row in (c.directory/"animations.tsv").read_text().splitlines():
        owner,name,symbol,*rest=row.split()
        if int(owner)==7526: c.render_detailed(int(symbol))
    textures={(int(row[0]),int(row[1])):row for row in previous}
    textures.update({(row[0],row[1]):row for row in c.rows})
    table(c.directory/"textures.tsv",textures.values())


if __name__=="__main__":
    refresh_enslaved() if "--enslaved-only" in sys.argv else build()
