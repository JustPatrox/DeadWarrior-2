"""Compile the fourth campaign mission and its spell effects."""
from xml.etree import ElementTree as ET
import shutil
import jpype
from build_menu_assets import ArtCompiler, ROOT, WORK
from build_archidon_assets import table
from build_campaign_assets import set_field


def build():
    c=ArtCompiler(ROOT/"assets/magic")
    data=ET.parse(next((WORK/"extract/binaryData").glob("10208_*")))
    campaign=ET.parse(next((WORK/"extract/binaryData").glob("29_*")))
    level=campaign.findall("level")[3]
    rules=[("map.length",5536),("normalHealthScale",level.findtext("normalHealthScale")),
           ("normalDamageScale",level.findtext("normalDamageScale")),
           ("enemy.handicap",level.findtext("insane")),("enemy.castleArchers",level.findtext("oponent/castleArcher","0"))]
    # SwampLands uses the existing tutorial background and mining layout.
    bounds=c.swf.getCharacter(4098).getRect()
    rules[0]=("map.length",int((bounds.Xmax-bounds.Xmin)/20))
    for side in ("player","oponent"):
        rules.extend(((side+".statueHealth",level.find(side).get("statueHealth")),
                      (side+".gold",level.findtext(side+"/gold","500")),
                      (side+".mana",level.findtext(side+"/mana","0"))))
    for name in ("Monk","Magikill"):
        rules.append(("composition."+name,level.findtext("oponent/UnitComposition/"+name)))
    for name in ("monk","magikill"):
        unit=data.find("Order/Units/"+name)
        for node in unit.iter():
            if len(node) or node.text is None: continue
            try: value=float(node.text)
            except ValueError: continue
            section=next((s.tag+"." for s in unit if node in list(s)),"")
            rules.append((name+"."+section+node.tag,value))
    rules.append(("nuke.radius",data.findtext("Chaos/Units/bomber/explosionRadius")))
    staff=next(s for s in c.timeline(7922).getFrame(43).layers.values() if str(s.instanceName)=="wizstaff")
    rules.extend((("poison.originX",-77.65+staff.matrix.translateX/20),
                  ("poison.originY",-82.8+staff.matrix.translateY/20)))
    poses=[]
    tips=[]
    for owner in (7926,8233):
        label=None
        for event in c.index["timelines"][str(owner)]:
            if event["op"]=="label": label=event["name"]
            if event.get("name")!="mc" or label=="stone": continue
            symbol=event["id"]
            timeline=c.timeline(symbol)
            if not any(r[0]==symbol for r in c.rows): c.render(symbol,scale=1)
            poses.append((owner,label,symbol,*event["matrix"][4:],timeline.getFrameCount(),0))
            for number,frame in enumerate(timeline.getFrames(),1):
                for state in frame.layers.values():
                    if str(state.instanceName)=="tip":
                        tips.append((symbol,number,state.matrix.translateX/20,state.matrix.translateY/20))
    for symbol in (10054,1531,1480):
        c.render(symbol,[1],scale=1)
    source=next((WORK/"extract/images").glob("8807_*.png"))
    shutil.copyfile(source,c.directory/"8807-1.png")
    from PIL import Image
    with Image.open(source) as image:
        c.rows.append((8807,1,"8807-1.png",-image.width/2,-image.height/2,image.width,image.height))
    c.render(1904,[7],scale=1)
    frames=c.timeline(1904).getFrames()
    frames.set(0,frames.get(6)); frames.subList(1,frames.size()).clear()
    c.render(1922,scale=.8,frame_offset=267)
    for symbol in (3120,3968):
        frames=c.timeline(symbol).getFrameCount()
        c.render(symbol,scale=1,child_ticks=list(range(frames)))
        rules.append((f"effect.{symbol}.frames",c.timeline(symbol).getFrameCount()))
    length=c.timeline(3170).getFrameCount()
    c.render(3172,[1]*length,scale=1,child_ticks=list(range(length)))
    rules.append(("effect.3172.frames",length))
    for symbol,value in ((292,"Order"),(293,level.findtext("oponent/raceName")),(326,data.findtext("Order/Units/magikill/info")),
                         (334,data.findtext("Order/Units/magikill/name")),(337,level.findtext("tip"))):
        set_field(c,symbol,value)
    for symbol,depths in ((343,(2,3,19,22,25,30,31,32,34,36,40,41,57,68)),(335,(4,)),(338,(4,))):
        for depth in depths: c.timeline(symbol).getFrame(0).layers.remove(jpype.JInt(depth))
        c.render(symbol,[1],scale=1,frame_offset=3)
    set_field(c,326,data.findtext("Order/Units/monk/info"))
    set_field(c,334,data.findtext("Order/Units/monk/name"))
    c.render(335,[1],scale=1,frame_offset=1003)
    for sound in ("fulminateSound","electricWallSound","ElectricWallSoundEffect","wizardPoisonSound",
                  "AcidSpraySound","HealSpellFinish","mediumExplosion1"):
        source=next((WORK/"extract/sounds").glob(f"*_{sound}_{sound}.mp3"))
        shutil.copyfile(source,c.directory/(sound+".mp3"))
    table(c.directory/"textures.tsv",c.rows)
    table(c.directory/"rules.tsv",rules)
    table(c.directory/"animations.tsv",poses)
    table(c.directory/"tips.tsv",tips)
    table(c.directory/"starting.tsv",((side,n.text) for side in ("player","oponent") for n in level.findall(side+"/startingUnit")))


if __name__=="__main__":
    build()
