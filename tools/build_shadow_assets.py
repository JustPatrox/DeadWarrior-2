"""Compile Mountains and the Shadowrath unit from the local campaign SWF."""
from xml.etree import ElementTree as ET
import shutil
from build_menu_assets import ArtCompiler, ROOT, WORK
from build_archidon_assets import extend, table
from build_campaign_assets import set_field
import jpype


def build():
    c=ArtCompiler(ROOT/"assets/shadow")
    for sound in ("ninjaCloakSound","PoisonedSound"):
        shutil.copyfile(next((WORK/"extract/sounds").glob(f"*_{sound}_{sound}.mp3")),c.directory/(sound+".mp3"))
    level=ET.parse(next((WORK/"extract/binaryData").glob("29_*"))).findall("level")[2]
    data=ET.parse(next((WORK/"extract/binaryData").glob("10208_*")))
    ninja=data.find("Order/Units/ninja")
    rules=[]
    for symbol in (4044,4048,4053):
        bounds=c.swf.getCharacter(symbol).getRect()
        rules.append((f"layer.{symbol}.width",(bounds.Xmax-bounds.Xmin)/20))
        extend(c,symbol,ground=symbol==4053)
    rules.append(("map.length",int(dict(rules)["layer.4053.width"])))
    for side in ("player","oponent"):
        rules.extend(((side+".statueHealth",level.find(side).get("statueHealth")),
                      (side+".gold",level.findtext(side+"/gold","500")),
                      (side+".mana",level.findtext(side+"/mana","0"))))
    rules.extend((("normalHealthScale",level.findtext("normalHealthScale")),
                  ("enemy.handicap",level.findtext("insane"))))
    for node in level.findall("oponent/UnitComposition/*"):
        rules.append(("composition."+node.tag,node.text))
    for node in ninja.iter():
        if len(node) or node.text is None: continue
        try: value=float(node.text)
        except ValueError: continue
        section=next((s.tag+"." for s in ninja if node in list(s)),"")
        rules.append(("ninja."+section+node.tag,value))
    poses=[]
    tips=[]
    events=c.index["timelines"]["7289"]
    label=None
    for event in events:
        if event["op"]=="label": label=event["name"]
        if event.get("name")!="mc" or label not in ("stand","run","stealth","attack_1","attack_2","death_1"): continue
        symbol=event["id"]
        timeline=c.timeline(symbol)
        c.render(symbol,scale=1)
        poses.append((7289,label,symbol,*event["matrix"][4:],timeline.getFrameCount(),0))
        if label.startswith("attack"):
            for number,frame in enumerate(timeline.getFrames(),1):
                for state in frame.layers.values():
                    if str(state.instanceName)=="tip":
                        tips.append((symbol,number,state.matrix.translateX/20,state.matrix.translateY/20))
    if not tips or len(poses)!=6: raise ValueError("Incomplete Shadowrath animation data")
    portrait=int(next(k for k,v in c.index["linkage"].items() if "profileAssassin" in v))
    c.render(portrait,[1],scale=1)
    c.render(1168,scale=1)
    rules.append(("poison.frames",c.timeline(1168).getFrameCount()))
    for symbol,value in ((292,"Order"),(293,level.findtext("oponent/raceName")),(326,ninja.findtext("info")),
                         (334,ninja.findtext("name")),(337,level.findtext("tip"))):
        set_field(c,symbol,value)
    for symbol,depths in ((343,(2,3,19,22,25,30,31,32,34,36,40,41,57,68)),(335,(4,)),(338,(4,))):
        for depth in depths: c.timeline(symbol).getFrame(0).layers.remove(jpype.JInt(depth))
        c.render(symbol,[1],scale=1,frame_offset=2)
    table(c.directory/"textures.tsv",c.rows)
    table(c.directory/"rules.tsv",rules)
    table(c.directory/"animations.tsv",poses)
    table(c.directory/"tips.tsv",tips)
    table(c.directory/"starting.tsv",((side,n.text) for side in ("player","oponent") for n in level.findall(side+"/startingUnit")))


if __name__=="__main__":
    build()
